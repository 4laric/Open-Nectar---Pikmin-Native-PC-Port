// Actual native construction/abort only; no saved-state bind or full restore.
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
#include "pc_midday_item_manager.h"
#include "pc_midday_constructor.h"
#include "pc_bbft.h"
#include "pc_window.h"
#include "pc_gpu_preference.h"
#include "pc_coop.h"
#include "pc_randomizer.h"
#include "settings/pc_settings.h"
#include "settings/pc_settings_p2d.h"
#include <SDL2/SDL.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <tuple>
using namespace pc_midday;
namespace {
unsigned checks=0;
void require(bool ok,const std::string& e){++checks;if(!ok){std::printf("FAIL MIDDAY_ITEM_FACTORY %s\n",e.c_str());std::fflush(nullptr);std::_Exit(1);}}
void requireError(bool ok,const char* stage,const std::string& error){require(ok,std::string(stage)+": "+error);}
using Nodes=std::vector<std::tuple<const CoreNode*,const CoreNode*,const CoreNode*>>;
Nodes generatorRoots(){Nodes out;std::set<const CoreNode*>seen;require(gameflow.mFlowManager!=nullptr,"flow root initialized");for(auto*n=gameflow.mFlowManager->mChild;n;n=n->mNext){require(seen.size()<4096&&seen.insert(n).second,"bounded generator roots");out.emplace_back(n,n->mNext,n->mParent);}return out;}
bool equal(const PolyPoolView&a,const PolyPoolView&b){if(a.capacity!=b.capacity||a.count!=b.count||a.stride!=b.stride||a.statuses!=b.statuses||a.objects!=b.objects||a.templates.size()!=b.templates.size())return false;for(size_t i=0;i<a.templates.size();++i)if(a.templates[i].classId!=b.templates[i].classId||a.templates[i].bytes!=b.templates[i].bytes||a.templates[i].prototype!=b.templates[i].prototype)return false;return true;}
void run(){
 std::string error;auto*original=itemMgr;auto*captains=naviMgr;auto*pikis=pikiMgr;const bool bury=PikiHeadMgr::buryMode;const auto generators=generatorRoots();
 PolyPoolView source;requireError(readPolyPoolView(*original,source,error),"source observation",error);
 ItemPolyConcreteTypes types;requireError(types.bind(*original,error),"actual factory contract",error);
 PolyPoolPlan plan;plan.capacity=source.capacity;plan.classes=types.allocatedClasses();plan.count=uint32_t(plan.classes.size());plan.slots.resize(plan.capacity);
 require(plan.classes.size()==12&&plan.capacity>=plan.count,"actual twelve factory registrations");
 unsigned slot=0;for(int id:plan.classes){plan.slots[slot]={slot%2?SlotLife::Retained:SlotLife::Active,slot+1,id};++slot;}
 PcSimRngCheckpoint before,after;requireError(pc_sim_rng_capture(before,error),"actual RNG before",error);
 ConstructorFence fence;requireError(fence.begin(error),"physical constructor barrier",error);
 for(int round=0;round<2;++round){
  {IsolatedItemManager stage;requireError(stage.prepare(*original,plan,fence,error),"actual isolated constructor",error);auto*m=stage.manager();require(m&&m!=original&&stage.roots().size()==12,"independent complete factory roots");
   PolyPoolView fresh;requireError(readPolyPoolView(*m,fresh,error),"staged observation",error);require(fresh.count==0&&fresh.capacity==source.capacity&&fresh.stride==source.stride,"unpublished fresh channel geometry");
   for(unsigned i=0;i<fresh.capacity;++i){require(fresh.statuses[i]==-1,"all manager slots unpublished");require(fresh.objects[i]!=source.objects[i],"independent pool allocation");}
   for(const auto&saved:plan.slots)if(saved.actor){auto*root=stage.roots().at(saved.actor);bool match=false;require(types.matches(saved.classId,root,match,error)&&match,"actual concrete placed subtype");require(root->mCount==0,"fresh constructor reference count");}
   require(m->mPikiHeadMgr==nullptr&&m->mMeltingPotMgr==nullptr,"forwarded managers not reused");require(m->mItemShapes!=original->mItemShapes,"independent shape-pointer array");
   for(unsigned i=0;i<11;++i)require(m->mItemShapes[i]==original->mItemShapes[i],"exact borrowed source content");
   require(generatorRoots()==generators&&itemMgr==original&&naviMgr==captains&&pikiMgr==pikis&&PikiHeadMgr::buryMode==bury,"live roots unchanged while stage exists");
  } // Exact typed destructors execute under the still-held physical fence.
  PolyPoolView observed;require(readPolyPoolView(*original,observed,error)&&equal(source,observed),"source pool unchanged after actual abort");
  require(generatorRoots()==generators&&itemMgr==original&&PikiHeadMgr::buryMode==bury,"source roots unchanged after actual abort");
 }
 requireError(fence.finish(false,error),"actual fence abort",error);requireError(pc_sim_rng_capture(after,error),"actual RNG after",error);
 require(before.profile==after.profile&&before.simState==after.simState&&before.cosmeticState==after.cosmeticState&&before.simDraws==after.simDraws&&before.cosmeticDraws==after.cosmeticDraws,"RNG exact after two aborts");
 std::printf("PASS MIDDAY_ITEM_FACTORY checks=%u classes=12 abort_rounds=2 native_constructors=1 disposal=1 source_unchanged=1 synthetic_bind=0 fresh_process_resume=0\n",checks);std::fflush(nullptr);std::_Exit(0);
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
 if(!pc_window_init("Midday item factory fixture",960,540))return 3;
 pc_settings_init();pc_window_set_display_mode(0);pc_window_set_window_size(960,540);pc_window_center();int w=0,h=0;SDL_GetWindowSize(SDL_GL_GetCurrentWindow(),&w,&h);require(w==960&&h==540,"960x540 baseline");
 pc_coop_set_pending(false);gsys->Initialise();pc_settings_p2d_init();nodeMgr=new NodeMgr();gsys->run(new TestApp());return 0;
}
