// Native birth/resource/lifecycle diagnostic. Human mode leaves the actual
// authored Burrowing Snagret encounter running; no HP/state/transport writes.
#include <SDL2/SDL.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include "system.h"
#include "App.h"
#include "Node.h"
#include "MoviePlayer.h"
#include "Navi.h"
#include "NaviMgr.h"
#include "NaviState.h"
#include "Piki.h"
#include "PikiMgr.h"
#include "Pellet.h"
#include "teki.h"
#include "Generator.h"
#include "GameStat.h"
#include "pc_bbft.h"
#include "pc_window.h"
#include "pc_gpu_preference.h"
#include "pc_p2_original_bulblax_snagret_native.h"
#include "pc_p2_original_group_engine.h"
#include "pc_p2_snakejoint.h"
#include "settings/pc_settings.h"
#include "settings/pc_settings_p2d.h"
namespace {
using namespace p2original;
bool human=false;
void require(bool yes,const char* text){if(!yes){std::fprintf(stderr,"FAIL P2_ORIGINAL_SNAGRET %s\n",text);std::fflush(nullptr);std::_Exit(1);}}
void checked(bool yes,const std::string& e){if(!yes)std::fprintf(stderr,"P2_ORIGINAL_SNAGRET_ERROR %s\n",e.c_str());require(yes,"native provider call");}
CatalogRow retail(){
 CatalogRow r;r.course="tutorial";r.member="nonloop/5-29.txt";r.index=2;r.sourceKey="tutorial/nonloop/5-29.txt#2";
 auto& a=r.enemy;a.uid=1389661387u;a.source=34;a.birthType=0;a.count=1;a.spawnType=1;a.directionDegrees=0;
 a.position={-325.493164f,111.026802f,1937.845825f};a.appearRadius=100;a.enemySize=0;a.generatorVersion="????";
 a.pelletColor=3;a.pelletSize=5;a.pelletMinimum=1;a.pelletMaximum=3;a.pelletProbability=.5f;return r;
}
class SnagretApp:public PlugPikiApp {
 std::unique_ptr<bulblax_snagret::Native> native;std::unique_ptr<Generator> generator;
 GeneratorState state;Creature* actor=nullptr;unsigned token=0;int frame=0,age=0,entries=0,before=0;
 void enter(){
  struct Heap{int prior;Heap():prior(gsys->setHeap(SYSHEAP_App)){}~Heap(){gsys->setHeap(prior);}} heap;
  if(!native)native=std::make_unique<bulblax_snagret::Native>();
  if(!generator)generator=std::make_unique<Generator>();
  require(!generator->mGenType,"genuine original null P1 GenType boundary");
  generator->mRespawnInterval=state.resurrectionDays;
  generator->mCarryOverFlags=state.reserved;
  std::string e;checked(pc_p2_original_course_install({{generator.get(),state}},native->provider(),e),e);
  bool handled=false;checked(pc_p2_original_generator_init(generator.get(),handled,e),e);require(handled,"actual original generator handled");
  actor=nullptr;Iterator it(tekiMgr);CI_LOOP(it){auto* t=static_cast<BTeki*>(*it);unsigned source=0,found=0;
   if(originalActors().query(t,source,found)&&source==34){require(!actor,"one literal source birth");actor=t;token=found;}}
  require(actor&&token&&native->provider().lookup(actor)!=nullptr,"physical actor registered");
  auto* t=static_cast<BTeki*>(actor);const char* fsm="stay",*clip=nullptr;float phase=0;
  require(pc_p2_snakejoint_clip(t,clip,phase)&&pc_p2_snakejoint_suppress_ai(t)&&pc_p2_snakejoint_model_hidden(t)&&t->mHealth==1500&&t->mTekiType==TEKI_Chappy,"actual P2 family health/FSM/chassis");
  InstanceIdentity id;unsigned source=0,found=0;require(originalActors().query(actor,source,found,&id)&&id.generator==retail().enemy.uid&&id.ordinal==0&&id.activation==unsigned(entries+1),"complete original registry identity");
  require(t->mSRT.t.x==retail().enemy.position.x&&t->mSRT.t.z==retail().enemy.position.z,"authored source XZ preserved");
  std::printf("P2_ORIGINAL_SNAGRET_RUNTIME_BIRTH uid=%u token=%u activation=%llu state=%s clip=%s health=%.1f x=%.3f y=%.3f z=%.3f\n",id.generator,token,(unsigned long long)id.activation,fsm,clip,t->mHealth,t->mSRT.t.x,t->mSRT.t.y,t->mSRT.t.z);
  age=0;
 }
public:
 SnagretApp(){state.uid=retail().enemy.uid;state.count=1;state.reserved=5;state.resurrectionDays=5;}
 int idle()override{
  int result=PlugPikiApp::idle();require(++frame<18000||human,"bounded frame budget");
  if(gameflow.mMoviePlayer&&gameflow.mMoviePlayer->mIsActive){gameflow.mMoviePlayer->requestSkip();return result;}
  Navi* n=naviMgr?naviMgr->getNavi():nullptr;
  if(!n||!n->getCurrState()||!pikiMgr||!tekiMgr||!pelletMgr||gameflow.mPauseAll||gameflow.mIsUIOverlayActive)return result;
  require(n->mHealth>0&&!GameStat::orimaDead,"captain remains alive");
  if(!native){if(n->getCurrState()->getID()!=NAVISTATE_Walk)return result;
   unsigned live=0;Iterator it(pikiMgr);CI_LOOP(it){auto* p=static_cast<Piki*>(*it);if(p->isAlive())++live;}
   require(live==20,"20 live Pikmin fixture baseline");before=tekiMgr->getSize();enter();}
  if(human)return result;
  if(++age<180)return result;
  unsigned alive=0;require(pc_p2_original_groups().state(generator.get(),state,alive)&&alive==1,"original group retained live actor");
  std::string e;checked(pc_p2_original_course_unload(e),e);
  unsigned source=0,found=0;require(!originalActors().query(actor,source,found)&&native->provider().lookup(actor)==nullptr&&tekiMgr->getSize()==before,"real pool/registry cleanup");
  if(++entries==1){enter();return result;}
  std::puts("PASS P2_ORIGINAL_SNAGRET_RUNTIME actual_native_birth=1 source_row=1 resource_bank=1 cleanup_reentry=1 natural_attack=0 save_resume=0");std::fflush(nullptr);std::_Exit(0);
 }
};
}
int main(int argc,char** argv){
 for(int i=1;i<argc;++i)if(!std::strcmp(argv[i],"--manual-encounter"))human=true;
 SDL_setenv("SDL_AUDIODRIVER","dummy",1);SDL_SetMainReady();pc_gpu_preference_apply();pc_bbft_init(argc,argv);
 require(pc_pikipelago_surface_course()&&!std::strcmp(pc_pikipelago_surface_course(),"tutorial"),"ordinary imported tutorial course");
 require(pc_window_init("Original Burrowing Snagret provider fixture",960,540),"window init");
 pc_settings_init();pc_window_set_control_mode(PC_CONTROL_CLASSIC);pc_window_set_display_mode(0);pc_window_set_window_size(960,540);pc_window_center();
 SDL_Window* w=SDL_GL_GetCurrentWindow();int width,height,x,y;SDL_GetWindowSize(w,&width,&height);SDL_GetWindowPosition(w,&x,&y);SDL_Rect b{};SDL_GetDisplayBounds(SDL_GetWindowDisplayIndex(w),&b);
 require(width==960&&height==540&&std::abs(x-(b.x+(b.w-width)/2))<=2&&std::abs(y-(b.y+(b.h-height)/2))<=2,"960x540 centered baseline");
 gsys->Initialise();pc_settings_p2d_init();nodeMgr=new NodeMgr();
 std::string e;checked(originalActors().install(std::string(64,'a'),{retail()},bulblax_snagret::decode,e),e);
 gsys->run(new SnagretApp());return 0;
}
