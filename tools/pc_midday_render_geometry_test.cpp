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
 for(auto& c:tev.mTevColRegs){c.mAnimFrameCount=23;c.mColorAnimData.mInfo.mSize=0;c.mAlphaAnimData.mInfo.mSize=0;}tev.mTevStageCount=0;
 for(auto& t:textures){t.mSourceAttrIndex=2;t.mTotalFrameCount=17;t.mScaleInfo.mInfo.mSize=0;t.mRotationInfo.mInfo.mSize=0;t.mTranslationInfo.mInfo.mSize=0;}
 RenderDescriptorIndex index;std::string e;check(index.declare(101,RenderKind::Materials,model,{0,1},e),"installed material-slot factory");check(index.declare(102,RenderKind::Tev,model,{0},e),"installed cloned TEV backing factory");check(index.declare(104,RenderKind::Textures,model,{0},e),"installed shared texture-array factory");
 RenderGraph graph;graph.generation=7;graph.nodes={{1,101,RenderKind::Materials,2,{}},{2,102,RenderKind::Tev,1,{}},{3,102,RenderKind::Tev,1,{}},{4,104,RenderKind::Textures,2,{}}};graph.links={{1,0,true,2,4,2},{1,1,true,3,4,2}};
 check(index.validate(graph,e),"distinct cloned TEV nodes share installed backing; texture mutable alias exact");
 materials[1].mTevInfoIndex=1;materials[1].mTevInfo=&tevs[1];check(!index.validate(graph,e),"valid in-range TEV from different source material refuses alias factory mismatch");materials[1].mTevInfoIndex=0;materials[1].mTevInfo=&tev;
 PVWTextureData foreignTextures[2];materials[1].mTextureInfo.mTextureData=foreignTextures;check(!index.validate(graph,e),"valid same-count foreign texture allocation cannot replace original shared backing");materials[1].mTextureInfo.mTextureData=textures;

 ActorFields state;auto value=[&](const char* k,ScalarKind kind,u64 bits){ActorField f;f.scalar=kind;f.bits=bits;state[k]=f;};
 value("mIndex",ScalarKind::U32,0);value("mFlags",ScalarKind::U32,MATFLAG_PVW);value("textureCount",ScalarKind::S32,2);
 check(index.validateStateGeometry(graph.nodes[0],0,state,e),"actual source material immutable index/PVW/texture geometry accepted");
 state.at("mIndex").bits=1;check(!index.validateStateGeometry(graph.nodes[0],0,state,e),"valid in-range wrong source material index refused");state.at("mIndex").bits=0;
 state.at("textureCount").bits=1;check(!index.validateStateGeometry(graph.nodes[0],0,state,e),"saved texture count differs from installed asset");state.at("textureCount").bits=2;
 state.at("mFlags").bits=0;check(!index.validateStateGeometry(graph.nodes[0],0,state,e),"saved non-PVW cannot access PVW asset route");
 state.clear();for(int i=0;i<3;++i)value(("mTevColRegs."+std::to_string(i)+".mAnimFrameCount").c_str(),ScalarKind::U32,23);
 check(index.validateStateGeometry(graph.nodes[1],0,state,e),"all three native TEV frame geometries accepted");state.at("mTevColRegs.2.mAnimFrameCount").bits=22;check(!index.validateStateGeometry(graph.nodes[1],0,state,e),"third TEV register cannot substitute frame geometry");
 state.clear();value("mSourceAttrIndex",ScalarKind::U32,2);value("mTotalFrameCount",ScalarKind::U32,17);
 check(index.validateStateGeometry(graph.nodes[3],1,state,e),"actual installed second texture element geometry accepted");state.at("mSourceAttrIndex").bits=3;check(!index.validateStateGeometry(graph.nodes[3],1,state,e),"saved texture cannot change installed attribute route");state.at("mSourceAttrIndex").bits=2;state.at("mTotalFrameCount").bits=18;check(!index.validateStateGeometry(graph.nodes[3],1,state,e),"saved texture frame extent must match source backing");
 check(!index.validateStateGeometry(graph.nodes[3],2,state,e),"out-of-allocation slot refused before source access");auto foreign=graph.nodes[3];foreign.factory=999;check(!index.validateStateGeometry(foreign,0,state,e),"unknown installed factory refuses state geometry");
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
 std::printf("%d checks, %d failures\n",checks,failed);return failed?1:0;
}
