// Actual headers and descriptor member access. Asset loader/model constructors
// are replaced with explicit test stubs; not production factory acceptance.
#include "pc_midday_render_descriptors.h"
#include "Shape.h"
#include <cstdio>
BaseShape::BaseShape(){}
void BaseShape::read(RandomAccessStream&){}
RouteGroup::RouteGroup():EditNode("test routes"){}
RoutePoint::RoutePoint(){}
void RouteGroup::render2d(Graphics&,int&){}
void Material::read(RandomAccessStream&){}
void Material::attach(){}
using namespace pc_midday;
namespace {int checks=0,failed=0;void check(bool b,const char*s){++checks;if(!b){++failed;std::printf("FAIL %s\n",s);}}}
int main(){BaseShape model;Material materials[2];PVWTevInfo tevs[2];auto& tev=tevs[0];PVWTextureData textures[2];
 model.mMaterialCount=2;model.mMaterialList=materials;model.mTevInfoCount=2;model.mTevInfoList=tevs;
 for(int i=0;i<2;++i){auto& m=materials[i];m.mIndex=i;m.mFlags=MATFLAG_PVW;m.mTevInfoIndex=0;m.mTevInfo=&tev;m.mColourInfo.mColourInfo.mAnimInfo.mSize=0;m.mColourInfo.mAlphaInfo.mAnimInfo.mSize=0;m.mTextureInfo.mTextureDataCount=2;m.mTextureInfo.mTextureData=textures;m.mTextureInfo.mTexGenDataCount=0;m.mTextureInfo.mTevStageCount=0;}
 for(auto& c:tev.mTevColRegs){c.mColorAnimData.mInfo.mSize=0;c.mAlphaAnimData.mInfo.mSize=0;}tev.mTevStageCount=0;
 for(auto& t:textures){t.mScaleInfo.mInfo.mSize=0;t.mRotationInfo.mInfo.mSize=0;t.mTranslationInfo.mInfo.mSize=0;}
 RenderDescriptorIndex index;std::string e;check(index.declare(101,RenderKind::Materials,model,{0,1},e),"installed material-slot factory");check(index.declare(102,RenderKind::Tev,model,{0},e),"installed cloned TEV backing factory");check(index.declare(104,RenderKind::Textures,model,{0},e),"installed shared texture-array factory");
 RenderGraph graph;graph.generation=7;graph.nodes={{1,101,RenderKind::Materials,2,{}},{2,102,RenderKind::Tev,1,{}},{3,102,RenderKind::Tev,1,{}},{4,104,RenderKind::Textures,2,{}}};graph.links={{1,0,true,2,4,2},{1,1,true,3,4,2}};
 check(index.validate(graph,e),"distinct cloned TEV nodes share installed backing; texture mutable alias exact");
 materials[1].mTevInfoIndex=1;materials[1].mTevInfo=&tevs[1];check(!index.validate(graph,e),"valid in-range TEV from different source material refuses alias factory mismatch");materials[1].mTevInfoIndex=0;materials[1].mTevInfo=&tev;
 PVWTextureData foreignTextures[2];materials[1].mTextureInfo.mTextureData=foreignTextures;check(!index.validate(graph,e),"valid same-count foreign texture allocation cannot replace original shared backing");materials[1].mTextureInfo.mTextureData=textures;
 check(!index.declare(101,RenderKind::Materials,model,{0,1},e),"duplicate factory declaration refused");
 auto wrong=graph;wrong.generation=0;check(!index.validate(wrong,e),"missing world generation refused");wrong=graph;wrong.nodes[3].factory=999;check(!index.validate(wrong,e),"unknown installed content factory refused");wrong=graph;wrong.nodes[1].factory=104;check(!index.validate(wrong,e),"compiled factory kind mismatch refused");wrong=graph;wrong.nodes[0].count=1;check(!index.validate(wrong,e),"saved material geometry differs from selected source slots");wrong=graph;wrong.nodes[3].count=1;check(!index.validate(wrong,e),"saved texture count differs from original allocation");wrong=graph;wrong.nodes[1].id=0;check(!index.validate(wrong,e),"zero allocation identity refused");wrong=graph;wrong.nodes[1].id=1;check(!index.validate(wrong,e),"duplicate allocation identity refused");wrong=graph;wrong.links.pop_back();check(!index.validate(wrong,e),"missing material relationship refused");wrong=graph;wrong.links[1].slot=0;check(!index.validate(wrong,e),"duplicate source material relationship refused");wrong=graph;wrong.links[0].pvw=false;check(!index.validate(wrong,e),"saved PVW discriminator agrees with installed original");wrong=graph;wrong.links[0].textures=0;check(!index.validate(wrong,e),"missing canonical texture target refused");wrong=graph;wrong.links[0].textureCount=1;check(!index.validate(wrong,e),"material count relationship agrees with installed backing");
 materials[0].mIndex=1;check(!index.validate(graph,e),"later changed source index refused");materials[0].mIndex=0;
 materials[0].mColourInfo.mColourInfo.mAnimInfo.mSize=1;materials[0].mColourInfo.mColourInfo.mAnimInfo.mKeyframes=nullptr;check(!index.validate(graph,e),"nonempty material keys require backing");materials[0].mColourInfo.mColourInfo.mAnimInfo.mSize=0;
 tev.mTevColRegs[2].mAlphaAnimData.mInfo.mSize=65537;check(!index.validate(graph,e),"third TEV register backing is bounded");tev.mTevColRegs[2].mAlphaAnimData.mInfo.mSize=0;
 textures[1].mTranslationInfo.mInfo.mSize=1;textures[1].mTranslationInfo.mInfo.mKeyframes=nullptr;check(!index.validate(graph,e),"second texture element key backing checked");textures[1].mTranslationInfo.mInfo.mSize=0;
 tev.mTevStageCount=17;check(!index.validate(graph,e),"native TEV stage bound");tev.mTevStageCount=0;
 materials[0].mTextureInfo.mTexGenDataCount=9;check(!index.validate(graph,e),"native texgen bound");materials[0].mTextureInfo.mTexGenDataCount=0;
 materials[0].mTevInfoIndex=2;check(!index.validate(graph,e),"TEV content must be a member of this installed model");materials[0].mTevInfoIndex=0;
 model.mMaterialCount=-1;check(!index.validate(graph,e),"uninitialized source model refused");model.mMaterialCount=2;
 RenderDescriptorIndex bad;check(!bad.declare(0,RenderKind::Materials,model,{0},e),"zero factory refused");check(!bad.declare(1,RenderKind::Materials,model,{0,0},e),"duplicated selected source material refused");check(!bad.declare(1,RenderKind::Materials,model,{2},e),"foreign installed material slot refused");check(!bad.declare(1,RenderKind::Tev,model,{0,1},e),"TEV factory is singleton");check(!bad.declare(1,static_cast<RenderKind>(99),model,{0},e),"unknown descriptor kind refused");
 check(index.validate(graph,e),"correct original graph still validates after all refusal paths");
 for(auto& t:tevs){t.mTevStageCount=0;for(auto& c:t.mTevColRegs){c.mColorAnimData.mInfo.mSize=0;c.mAlphaAnimData.mInfo.mSize=0;}}
 RenderDescriptorIndex full;check(full.declare(201,RenderKind::Materials,model,{0,1},e)&&full.declareTevArray(202,model,e)&&full.declare(204,RenderKind::Textures,model,{0},e),"whole model TEV array factory declared");
 RenderGraph array;array.generation=11;array.nodes={{11,201,RenderKind::Materials,2,{}},{12,202,RenderKind::Tev,2,{},true},{14,204,RenderKind::Textures,2,{}}};array.links={{11,0,true,12,14,2,0},{11,1,true,12,14,2,1}};
 materials[1].mTevInfoIndex=1;materials[1].mTevInfo=&tevs[1];check(full.validate(array,e),"prototype links resolve distinct elements of exact original TEV array");
 auto wrongArray=array;wrongArray.links[1].tevSlot=0;check(!full.validate(wrongArray,e),"in-range wrong original TEV element refused");
 wrongArray=array;wrongArray.nodes[1].contentRoot=false;check(!full.validate(wrongArray,e),"caller cannot remove compiled prototype root provenance");
 wrongArray=array;wrongArray.nodes[1].count=1;check(!full.validate(wrongArray,e),"partial original TEV allocation refused");
 materials[1].mTevInfoIndex=0;materials[1].mTevInfo=&tevs[0];array.links[1].tevSlot=0;check(full.validate(array,e),"initialized unused second TEV entry retained by full factory");
 tevs[1].mTevColRegs[2].mAlphaAnimData.mInfo.mSize=1;tevs[1].mTevColRegs[2].mAlphaAnimData.mInfo.mKeyframes=nullptr;check(!full.validate(array,e),"unused initialized array entry backing is still validated");tevs[1].mTevColRegs[2].mAlphaAnimData.mInfo.mSize=0;
 check(full.validate(array,e),"whole array remains valid after refusals");
 RenderDescriptorIndex clone;check(clone.declare(301,RenderKind::Tev,model,{0},e),"old clone factory declaration remains supported");RenderGraph forged;forged.generation=11;forged.nodes={{31,301,RenderKind::Tev,1,{},true}};check(!clone.validate(forged,e),"clone factory cannot authorize invented unreachable prototype root");
 std::printf("%d checks, %d failures\n",checks,failed);return failed?1:0;
}
