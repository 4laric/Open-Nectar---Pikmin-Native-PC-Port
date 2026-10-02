// Actual native UFO/material/light constructors and failure disposal only.
// Descriptor fixtures are synthetic; no actual checkpoint capture or resume.
#include "system.h"
#include "App.h"
#include "NaviMgr.h"
#include "NaviState.h"
#include "PikiMgr.h"
#include "GameStat.h"
#include "ItemMgr.h"
#include "PikiHeadItem.h"
#include "ObjType.h"
#include "Node.h"
#include "MoviePlayer.h"
#include "pc_midday_player_resource_stage.h"
#include "PlayerState.h"
#include "pc_midday_player_root.h"
#include "Material.h"
#include "UtEffect.h"
#include "sysNew.h"
#include "pc_midday_constructor.h"
#include "pc_bbft.h"
#include "pc_window.h"
#include "pc_gpu_preference.h"
#include "pc_coop.h"
#include "pc_randomizer.h"
#include "settings/pc_settings.h"
#include "settings/pc_settings_p2d.h"
#include <SDL2/SDL.h>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <tuple>
using namespace pc_midday;
namespace {
unsigned checks=0;
void require(bool ok,const std::string& e){++checks;if(!ok){std::printf("FAIL MIDDAY_PLAYER_RESOURCES %s\n",e.c_str());std::fflush(nullptr);std::_Exit(1);}}
void requireError(bool ok,const char* stage,const std::string& error){require(ok,std::string(stage)+": "+error);}
using Nodes=std::vector<std::tuple<const CoreNode*,const CoreNode*,const CoreNode*>>;
Nodes generatorRoots(){Nodes out;std::set<const CoreNode*>seen;require(gameflow.mFlowManager!=nullptr,"flow root initialized");for(auto*n=gameflow.mFlowManager->mChild;n;n=n->mNext){require(seen.size()<4096&&seen.insert(n).second,"bounded generator roots");out.emplace_back(n,n->mNext,n->mParent);}return out;}
void scalar(ActorFields& f,const std::string& key,ScalarKind kind,u64 bits=0){ActorField v;v.scalar=kind;v.bits=bits;f[key]=v;}
void ref(ActorFields& f,const std::string& key,RefKind role,u64 id=0,u32 slot=0){ActorField v;v.category=FieldCategory::Reference;v.reference=role;v.target={0,id,slot};f[key]=v;}
void vec(ActorFields& f,const std::string& key){for(auto axis:{".x",".y",".z"})scalar(f,key+axis,ScalarKind::F32);}
void animation(ActorFields& f,const std::string& key){scalar(f,key+".active",ScalarKind::Bool);for(auto name:{"mMgr","mContext","mMotionTable"})ref(f,key+"."+name,RefKind::Animation,10);ref(f,key+".mAnimInfo",RefKind::Animation);}
ActorBytes descriptors(){ActorFields f;scalar(f,"player.resources.version",ScalarKind::S32,1);scalar(f,"player.resources.total",ScalarKind::S32,30);scalar(f,"player.resources.registered",ScalarKind::S32,3);scalar(f,"player.resources.repair",ScalarKind::S32,0xffffffffu);scalar(f,"player.resources.preload",ScalarKind::Bool);
 for(int i=0;i<30;++i){scalar(f,"player.resources.replay."+std::to_string(i),ScalarKind::Bool);scalar(f,"player.resources.part."+std::to_string(i)+".visibility",ScalarKind::U8,i%3);}
 const int counts[]={1,2,256};for(int i=0;i<3;++i){auto p="player.resources.part."+std::to_string(i)+".";scalar(f,p+"joint",ScalarKind::S32,0xffffffffu);scalar(f,p+"model",ScalarKind::U32,i+100);scalar(f,p+"pellet",ScalarKind::U32,i+200);scalar(f,p+"shaped",ScalarKind::Bool);ref(f,p+"shape",RefKind::ItemShape);vec(f,p+"repairPosition");ref(f,p+"listenerIdentity",RefKind::AnimListener,99,i+1);scalar(f,p+"materials.count",ScalarKind::S32,counts[i]);ref(f,p+"materials.next",RefKind::ShapeDynMaterials);ref(f,p+"materials.model",RefKind::Shape,8);for(int j=0;j<counts[i];++j)ref(f,p+"materials.material."+std::to_string(j),RefKind::Material,1000+i*256+j);}
 ref(f,"player.resources.olimarShape",RefKind::Shape,4);scalar(f,"player.resources.olimarSpeed",ScalarKind::F32);animation(f,"player.resources.olimarLower");animation(f,"player.resources.olimarUpper");ref(f,"player.resources.light",RefKind::Effect,5);ref(f,"player.resources.lightGlow",RefKind::Effect,6);vec(f,"player.resources.lightPosition");ActorBytes bytes;std::string e;requireError(encode_actor_fields(f,bytes,e),"descriptor encode",e);return bytes;
}
// Fixture metadata only: no pointer resolve/bind, and no real world capture.
struct DescriptorResolver:LogicalResolver {
 bool identify(const char*,RefKind,const void*,LogicalRef&,std::string&)override{return false;}
 bool validate(const char*,RefKind,const LogicalRef&,std::string&)const override{return true;}
 bool validateTyped(const FieldSchema&s,const LogicalRef&r,std::string&)const override{return !s.targetType.empty()&&!r.owner;}
 bool resolve(const char*,RefKind,const LogicalRef&,void*&,std::string&)override{return false;}
 bool identifyHandle(const char*,RefKind,u32,LogicalRef&,std::string&)override{return false;}
 bool resolveHandle(const char*,RefKind,const LogicalRef&,u32&,std::string&)override{return false;}
};
bool sameHeap(const PikiPcAllocationStats&a,const PikiPcAllocationStats&b){return a.liveBlocks==b.liveBlocks&&a.liveBytes==b.liveBytes&&a.unknownFrees==b.unknownFrees;}
void run(){
 std::string error;auto*source=playerState;require(source&&source->mUfoParts,"initialized source player array");auto*parts=source->mUfoParts;const int registered=source->mTotalRegisteredParts;auto*light=source->mNaviLightEfx;auto*glow=source->mNaviLightGlowEfx;auto*captains=naviMgr;auto*pikis=pikiMgr;auto*items=itemMgr;const auto generators=generatorRoots();bool preload=preloadUFO;std::array<bool,30> replay{};PcMiddayPlayerReplayAccess::read(replay);
 auto bytes=descriptors();PlayerCoreFields core;core.totalParts=30;core.totalRegisteredParts=3;core.living=20;core.totalPlucked=-7;core.hour.entries={{{-2,4,18}}};core.day.entries={{{20,-1,1}}};
 PlayerCoreTopology topology;const uint16_t entries[]={0,1,8,9,4096};for(size_t i=0;i<5;++i){topology.courseEntries[i]=entries[i];core.courses[i].entries=entries[i];if(entries[i])core.courses[i].bits.assign(entries[i]/8+1,uint8_t(i+37));}
 Bytes coreBytes,demoBytes,resultBytes;requireError(encodePlayerCore(core,coreBytes,error),"core descriptor encode",error);
 DemoFields demo;require(source->mDemoFlags.mCurrentDataIndex==62&&source->mDemoFlags.mFlagDataList&&source->mDemoFlags.mStoredFlags,"compiled source Demo descriptors");for(int i=0;i<62;++i){auto*d=source->mDemoFlags.mFlagDataList[i];require(d!=nullptr,"source Demo registration");demo.descriptors.push_back({d->mIndex,d->mMovieIndex,d->_08,d->_0A});}std::copy(source->mDemoFlags.mStoredFlags,source->mDemoFlags.mStoredFlags+32,demo.stored.begin());requireError(encodeDemo(demo,demoBytes,error),"compiled Demo encode",error);
 PlayerCoreReadFence stopped{true,true,1,1};requireError(captureResult(source->mResultFlags,stopped,resultBytes,error),"actual Result capture",error);
 DescriptorResolver resolver;RestoreGate gate{true,true,true,true,true,true,true};
 PcSimRngCheckpoint before,after;requireError(pc_sim_rng_capture(before,error),"actual RNG before",error);ConstructorFence fence;requireError(fence.begin(error),"physical constructor barrier",error);
 unsigned failures=0;
 for(size_t fail=1;fail<=7;++fail){auto heap=piki_pc_allocation_stats();
  {std::string e;IsolatedPlayerResources stage;require(!stage.allocate(bytes,core,resolver,gate,fence,e,fail),"injected native allocation boundary refuses");require(stage.allocationAttempts()==fail&&!stage.parts()&&!stage.light(false)&&!stage.light(true),"failed stage exposes no partial native allocation");++failures;}
  // Capture before require's std::string conversion: argument evaluation order
  // may otherwise count the assertion message itself as a live allocation.
  const auto afterHeap=piki_pc_allocation_stats();
  const bool exactHeap=sameHeap(heap,afterHeap);
  std::printf("MIDDAY_PLAYER_HEAP failure_site=%zu before_blocks=%zu after_blocks=%zu before_bytes=%zu after_bytes=%zu before_unknown=%zu after_unknown=%zu\n",fail,heap.liveBlocks,afterHeap.liveBlocks,heap.liveBytes,afterHeap.liveBytes,heap.unknownFrees,afterHeap.unknownFrees);
  require(exactHeap,"failed native allocation returns heap to exact baseline");
 }
 for(int round=0;round<2;++round){auto heap=piki_pc_allocation_stats();
  {std::string e;IsolatedPlayerResources stage;requireError(stage.allocate(bytes,core,resolver,gate,fence,e),"actual UFO material light construction",e);require(stage.allocationAttempts()==7&&stage.heldBy(fence),"seven owned native allocation sites");auto*staged=static_cast<PlayerState::UfoParts*>(stage.parts());require(staged&&staged!=parts,"private UFO array");const int counts[]={1,2,256};
   for(int i=0;i<30;++i){require(staged[i].mPartVisType==i%3&&staged[i].mPelletShape==nullptr,"exact descriptor visibility and unbound shape");require(staged[i].mAnimatedMaterials.mMatCount==(i<3?counts[i]:0),"bounded native material geometry");if(i<3)require(staged[i].mAnimatedMaterials.mMaterials==stage.materials(i),"owned native material backing");}
   require(stage.light(false)&&stage.light(true)&&stage.light(false)!=light&&stage.light(true)!=glow&&stage.light(false)!=stage.light(true),"private distinct native effect wrappers");require(!stage.light(false)->mPtclGen&&!stage.light(true)->mPtclGen,"constructors create no particle generator");require(!stage.allocate(bytes,core,resolver,gate,fence,e),"second staging refused");
   // Root is declared after resource owner: its inline resource views die first.
   IsolatedPlayerRoot player;requireError(player.prepare(*source,coreBytes,demoBytes,resultBytes,topology,{},gate,fence,e),"actual partial PlayerState construction",e);auto*root=player.root();require(root&&root!=source&&player.heldBy(fence),"private exact-fence PlayerState root");
   require(root->mUfoParts==nullptr&&root->mOlimarShapeObj==nullptr&&root->mNaviLightEfx==nullptr&&root->mNaviLightGlowEfx==nullptr,"unbound partial root cannot masquerade as complete restore");require(root->mDemoFlags.mStoredFlags!=source->mDemoFlags.mStoredFlags&&root->mDemoFlags.mFlagDataList!=source->mDemoFlags.mFlagDataList,"independent inline Demo backing");
   Bytes observedCore,observedResult;requireError(capturePlayerCore(*root,topology,stopped,observedCore,e),"actual staged core readback",e);require(observedCore==coreBytes,"native named core graph and course backing roundtrip");requireError(captureResult(root->mResultFlags,stopped,observedResult,e),"actual staged Result readback",e);require(observedResult==resultBytes,"native inline Result named-field roundtrip");

  }
  const auto afterHeap=piki_pc_allocation_stats();
  const bool exactHeap=sameHeap(heap,afterHeap);
  std::printf("MIDDAY_PLAYER_HEAP disposal_round=%d before_blocks=%zu after_blocks=%zu before_bytes=%zu after_bytes=%zu before_unknown=%zu after_unknown=%zu\n",round,heap.liveBlocks,afterHeap.liveBlocks,heap.liveBytes,afterHeap.liveBytes,heap.unknownFrees,afterHeap.unknownFrees);
  require(exactHeap,"successful native disposal returns heap to exact baseline");
 }
 std::array<bool,30> current{};PcMiddayPlayerReplayAccess::read(current);require(current==replay&&preloadUFO==preload,"live replay and preload unchanged");require(playerState==source&&source->mUfoParts==parts&&source->mTotalRegisteredParts==registered&&source->mNaviLightEfx==light&&source->mNaviLightGlowEfx==glow,"live player resource roots unchanged");require(naviMgr==captains&&pikiMgr==pikis&&itemMgr==items&&generatorRoots()==generators,"live manager generator roots unchanged");requireError(fence.finish(false,error),"fence abort",error);requireError(pc_sim_rng_capture(after,error),"actual RNG after",error);require(before.profile==after.profile&&before.simState==after.simState&&before.cosmeticState==after.cosmeticState&&before.simDraws==after.simDraws&&before.cosmeticDraws==after.cosmeticDraws,"exact RNG after native constructors and aborts");
 std::printf("PASS MIDDAY_PLAYER_RESOURCES checks=%u injected_failures=%u abort_rounds=2 native_constructors=1 disposal=1 heap_exact=1 player_root=1 core_result_readback=1 source_unchanged=1 synthetic_descriptors=1 typed_bind=0 fresh_process_resume=0\n",checks,failures);std::fflush(nullptr);std::_Exit(0);
}
class TestApp:public PlugPikiApp {
 std::chrono::steady_clock::time_point started=std::chrono::steady_clock::now();
public:int idle()override {
 require(std::chrono::steady_clock::now()-started<std::chrono::seconds(55),"bounded startup");
 if(naviMgr&&naviMgr->getActiveNavi()){auto*n=naviMgr->getActiveNavi();if(GameStat::orimaDead||n->mHealth<=1||(n->getCurrState()&&n->getCurrState()->getID()==NAVISTATE_Dead)){std::printf("P2_FIXTURE_CAPTAIN_DOWN outcome=BLOCKED\n");std::fflush(nullptr);std::_Exit(86);}}
 int result=PlugPikiApp::idle();if(gameflow.mMoviePlayer&&gameflow.mMoviePlayer->mIsActive){gameflow.mMoviePlayer->requestSkip();return result;}
 if(!pc_randomizer_ready()||!naviMgr||!pikiMgr||!itemMgr||gameflow.mPauseAll||gameflow.mIsUIOverlayActive)return result;
 auto*n=naviMgr->getActiveNavi();if(!n||!n->getCurrState()||n->getCurrState()->getID()!=NAVISTATE_Walk)return result;
 int count=0;Iterator it(pikiMgr);for(it.first();!it.isDone();it.next())++count;if(count!=20)return result;
 run();return result;
 }
};
}
int main(int argc,char**argv){
 SDL_setenv("PIKMIN_RANDOMIZER_TEST_BACKGROUND","1",1);SDL_setenv("SDL_AUDIODRIVER","dummy",1);SDL_SetMainReady();
 pc_sim_rng_note_main_thread();std::string error;requireError(pc_sim_rng_begin_offline(0x68,0x168,error),"portable bootstrap",error);
 pc_gpu_preference_apply();pc_bbft_init(argc,argv);require(pc_randomizer_enabled(),"ordinary generated assets required");
 if(!pc_window_init("Midday player resources fixture",960,540))return 3;
 pc_settings_init();pc_window_set_display_mode(0);pc_window_set_window_size(960,540);pc_window_center();int w=0,h=0;SDL_GetWindowSize(SDL_GL_GetCurrentWindow(),&w,&h);require(w==960&&h==540,"960x540 baseline");
 pc_coop_set_pending(false);gsys->Initialise();pc_settings_p2d_init();nodeMgr=new NodeMgr();gsys->run(new TestApp());return 0;
}
