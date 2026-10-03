// Actual headers and descriptor member access. Asset loader/model constructors
// are replaced with explicit test stubs; not production factory acceptance.
#include "pc_midday_render_mutable.h"
#include "Shape.h"
#include <cstdio>
BaseShape::BaseShape(){}
void BaseShape::read(RandomAccessStream&){}
RouteGroup::RouteGroup():EditNode("test routes"){}
RoutePoint::RoutePoint(){}
void RouteGroup::render2d(Graphics&,int&){}
void Material::read(RandomAccessStream&){}
void Material::attach(){}

// Physical SDL/reward fence methods are explicit header-test stubs. The real
// production pc_sim_rng.cpp and pc_netplay_det.cpp are linked below.
namespace pc_midday {
AudioConstructionFence::~AudioConstructionFence()=default;
ConstructorFence::~ConstructorFence(){if(held_){std::string e;finish(false,e);}}
bool ConstructorFence::begin(std::string& e){held_=true;return pc_sim_rng_constructor_suppression(true,e);}
bool ConstructorFence::finish(bool,std::string& e){held_=false;return pc_sim_rng_constructor_suppression(false,e);}
}
using namespace pc_midday;
namespace {
int checks=0,failures=0;void check(bool b,const char* s){++checks;if(!b){++failures;std::printf("FAIL %s\n",s);}}
struct Asset {
 BaseShape model;Material material;PVWTevInfo tev;PVWTextureData texture;
 PVWAnimKey3<PVWKeyInfoU8> colorKeys[1];PVWAnimKey1<PVWKeyInfoU8> alphaKeys[1];
 PVWAnimKey3<PVWKeyInfoS10> tevColorKeys[1];PVWAnimKey1<PVWKeyInfoS10> tevAlphaKeys[1];
 PVWAnimKey3<PVWKeyInfoF32> textureKeys[1];PVWTexGenData texgen[1];PVWTevStage stages[1];u8 display[8]{};
 Asset(){
  model.mMaterialCount=1;model.mMaterialList=&material;model.mTevInfoCount=1;model.mTevInfoList=&tev;
  material.mIndex=0;material.mFlags=MATFLAG_PVW;material.mDisplayListPtr=display;material.mDisplayListSize=8;
  material.mColourInfo.mTotalFrameCount=30;material.mColourInfo.mColourInfo.mAnimInfo.mSize=1;material.mColourInfo.mColourInfo.mAnimInfo.mKeyframes=colorKeys;material.mColourInfo.mAlphaInfo.mAnimInfo.mSize=1;material.mColourInfo.mAlphaInfo.mAnimInfo.mKeyframes=alphaKeys;
  material.mPeInfo.mControlFlags=1;material.mPeInfo.mAlphaCompareFlags=2;material.mPeInfo.mDepthTestFlags=3;material.mPeInfo.mBlendModeFlags=4;
  material.mTevInfoIndex=0;material.mTevInfo=&tev;material.mLightingInfo._UNUSED08=0.25f;
  material.mTextureInfo.mTextureDataCount=1;material.mTextureInfo.mTextureData=&texture;material.mTextureInfo.mTexGenDataCount=1;material.mTextureInfo.mTexGenData=texgen;material.mTextureInfo.mUseScale=1;material.mTextureInfo.mTevStageCount=1;
  tev.mTevStageCount=1;tev.mTevStages=stages;for(auto& c:tev.mTevColRegs){c.mAnimFrameCount=23;c.mColorAnimData.mInfo.mSize=1;c.mColorAnimData.mInfo.mKeyframes=tevColorKeys;c.mAlphaAnimData.mInfo.mSize=1;c.mAlphaAnimData.mInfo.mKeyframes=tevAlphaKeys;}
  for(auto* k:{&texture.mScaleInfo.mInfo,&texture.mRotationInfo.mInfo,&texture.mTranslationInfo.mInfo}){k->mSize=1;k->mKeyframes=textureKeys;}
  texture.mSourceAttrIndex=2;texture.mTotalFrameCount=17;texture._UNUSED0C=1;texture._UNUSED0E=2;texture._UNUSED10=3;texture._UNUSED11=4;texture._UNUSED12=5;texture._UNUSED13=6;
 }
};
void index(Asset& a,RenderDescriptorIndex& i,std::string& e){check(i.declare(101,RenderKind::Materials,a.model,{0},e),"declare source material");check(i.declare(102,RenderKind::Tev,a.model,{0},e),"declare source TEV");check(i.declare(103,RenderKind::Textures,a.model,{0},e),"declare source textures");}
void drawAdvances(const char* label){std::string e;PcSimRngCheckpoint before,after;check(pc_sim_rng_capture(before,e),"RNG unsuppressed before invalid request");pc_sim_rand();check(pc_sim_rng_capture(after,e)&&after.simDraws==before.simDraws+1&&after.simState!=before.simState,label);}
}
int main(){
 std::string e;pc_sim_rng_note_main_thread();check(pc_sim_rng_begin_offline(123,456,e),"actual portable RNG bootstrap");
 Asset a,b;RenderDescriptorIndex first,foreign;index(a,first,e);index(b,foreign,e);
 ActorBytes payload;check(encode_actor_fields({},payload,e),"valid empty payload only for preplanner provenance refusal");RenderGraph graph;graph.generation=7;graph.nodes={{1,101,RenderKind::Materials,1,{payload}},{2,102,RenderKind::Tev,1,{payload}},{3,103,RenderKind::Textures,1,{payload}}};graph.links={{1,0,true,2,3,1}};
 ConstructorFence idle;IsolatedRenderAllocations empty;int factories=0;auto unavailable=[&](u64,u32)->LogicalResolver*{++factories;return nullptr;};
 drawAdvances("baseline actual draw advances");check(!first.initializeBacking(graph,empty,idle,e),"invalid initializer fence refuses");drawAdvances("invalid initializer does not suppress actual RNG");check(!bindRenderMutable(graph,empty,first,unavailable,idle,e),"invalid binder fence refuses");drawAdvances("invalid binder does not suppress actual RNG");
 ConstructorFence held;check(held.begin(e),"stub physical fence enables real RNG barrier");RestoreGate gate{true,true,true,true,true,true,true};
 {
  IsolatedRenderAllocations owner;check(owner.prepare(graph,gate,held,e),"actual native-header owned allocations prepare");check(first.initializeBacking(graph,owner,held,e),"initialize all actual owned immutable fields");check(first.matchesInitializedBacking(graph,owner,held,e),"original source provenance matches");check(foreign.validate(graph,e),"foreign asset retains identical logical geometry");
  check(!foreign.matchesInitializedBacking(graph,owner,held,e),"same-geometry foreign immutable backing refused");check(!bindRenderMutable(graph,owner,foreign,unavailable,held,e)&&factories==0&&!owner.mutableReady(),"foreign binder refuses before resolver/attempt/Apply");check(first.matchesInitializedBacking(graph,owner,held,e),"foreign refusal leaves initialized destination unchanged");
  auto* mat=static_cast<Material*>(owner.allocation(1));auto* tev=static_cast<PVWTevInfo*>(owner.allocation(2));auto* texture=static_cast<PVWTextureData*>(owner.allocation(3));
  mat->mDisplayListPtr=b.display;check(!first.matchesInitializedBacking(graph,owner,held,e),"tampered display-list backing refused");mat->mDisplayListPtr=a.display;
  mat->mTextureInfo.mTexGenData=b.texgen;check(!first.matchesInitializedBacking(graph,owner,held,e),"same-count foreign texgen backing refused");mat->mTextureInfo.mTexGenData=a.texgen;
  tev->mTevStages=b.stages;check(!first.matchesInitializedBacking(graph,owner,held,e),"same-count foreign TEV stage backing refused");tev->mTevStages=a.stages;
  tev->mTevColRegs[2].mAlphaAnimData.mInfo.mKeyframes=b.tevAlphaKeys;check(!first.matchesInitializedBacking(graph,owner,held,e),"third TEV alpha key backing refused");tev->mTevColRegs[2].mAlphaAnimData.mInfo.mKeyframes=a.tevAlphaKeys;
  texture->mTranslationInfo.mInfo.mKeyframes=b.textureKeys;check(!first.matchesInitializedBacking(graph,owner,held,e),"texture translation key backing refused");texture->mTranslationInfo.mInfo.mKeyframes=a.textureKeys;
  mat->mColourInfo.mAlphaInfo.mAnimInfo.mKeyframes=b.alphaKeys;check(!first.matchesInitializedBacking(graph,owner,held,e),"material alpha key backing refused");mat->mColourInfo.mAlphaInfo.mAnimInfo.mKeyframes=a.alphaKeys;
  texture->_UNUSED10=99;check(!first.matchesInitializedBacking(graph,owner,held,e),"actual source envmap marker backing refused");texture->_UNUSED10=a.texture._UNUSED10;
  check(first.matchesInitializedBacking(graph,owner,held,e),"all correct aliases preserved after refusal controls");
  // Temporarily remove only real suppression while retaining the stub's held
  // flag, so an invalid/wrong fence call cannot hide a setter behind existing true.
  check(pc_sim_rng_constructor_suppression(false,e),"test unsuppression for invalid fence observation");check(!bindRenderMutable(graph,owner,first,unavailable,idle,e)&&factories==0,"wrong binder fence refuses before real setter");drawAdvances("wrong binder fence leaves real draws unsuppressed");check(!first.initializeBacking(graph,owner,idle,e),"wrong initializer fence refuses");drawAdvances("wrong initializer fence leaves real draws unsuppressed");
  check(pc_sim_rng_constructor_suppression(true,e),"restore test cleanup barrier");
 }
 check(held.finish(false,e),"release test barrier after owned disposal");drawAdvances("released test barrier advances actual RNG");
 std::printf("%d checks, %d failures\n",checks,failures);return failures?1:0;
}
