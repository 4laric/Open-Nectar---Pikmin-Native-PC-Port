// Actual native death/Piki carry/Onion diagnostic. The source registry binding,
// nearby placement and initial health depletion are explicit fixture controls.
// Only ordinary SDL input performs pickup/carry; no carrier/action/stock writes.
#include <SDL2/SDL.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "system.h"
#include "App.h"
#include "Node.h"
#include "MoviePlayer.h"
#include "Navi.h"
#include "NaviMgr.h"
#include "NaviState.h"
#include "Camera.h"
#include "Piki.h"
#include "PikiMgr.h"
#include "PikiHeadItem.h"
#include "ItemMgr.h"
#include "GoalItem.h"
#include "Pellet.h"
#include "teki.h"
#include "TekiPersonality.h"
#include "MapMgr.h"
#include "Generator.h"
#include "GameStat.h"
#include "KeyConfig.h"
#include "pc_bbft.h"
#include "pc_window.h"
#include "pc_gpu_preference.h"
#include "pc_p2_chappy.h"
#include "pc_p2_original_actor.h"
#include "pc_p2_original_corpse_native.h"
#include "settings/pc_settings.h"
#include "settings/pc_settings_p2d.h"
namespace {
using namespace p2original;
SDL_Joystick* pad=nullptr;bool manual=false;
void require(bool ok,const char* text){if(!ok){std::fprintf(stderr,"FAIL P2_ORIGINAL_CORPSE %s\n",text);std::fflush(nullptr);std::_Exit(1);}}
void checked(bool ok,const std::string& e){if(!ok)std::fprintf(stderr,"P2_ORIGINAL_CORPSE_ERROR %s\n",e.c_str());require(ok,"native call");}
float distance(const Vector3f& a,const Vector3f& b){float x=a.x-b.x,z=a.z-b.z;return std::sqrt(x*x+z*z);}
void input(unsigned keys=0,int x=0,int y=0){
 pc_window_input_assign(0,PC_INPUT_DEV_GAMEPAD,SDL_JoystickInstanceID(pad));pc_window_input_assign(1,PC_INPUT_DEV_NONE,-1);
 SDL_JoystickSetVirtualButton(pad,SDL_CONTROLLER_BUTTON_A,(keys&KBBTN_A)!=0);
 SDL_JoystickSetVirtualButton(pad,SDL_CONTROLLER_BUTTON_B,(keys&KBBTN_B)!=0);
 SDL_JoystickSetVirtualAxis(pad,SDL_CONTROLLER_AXIS_LEFTX,Sint16(x*32767/74));
 SDL_JoystickSetVirtualAxis(pad,SDL_CONTROLLER_AXIS_LEFTY,Sint16(-y*32767/74));SDL_JoystickUpdate();
}
void point(Navi* n,const Vector3f& goal,bool walk,unsigned keys=0){
 const auto& from=walk?n->mSRT.t:n->mCursorWorldPos;float dx=goal.x-from.x,dz=goal.z-from.z,d=std::sqrt(dx*dx+dz*dz);int x=0,y=0;
 if(d>(walk?15.f:6.f)){const auto& a=n->controlCamera()->mViewXAxis;float power=walk?65:22;
  x=int(std::lround(power*(dx*a.x+dz*a.z)/d));y=int(std::lround(power*(dx*a.z-dz*a.x)/d));}input(keys,x,y);
}
CatalogRow row(){CatalogRow r;r.course="tutorial";r.member="nonloop/5-29.txt";r.index=1;r.sourceKey="tutorial/nonloop/5-29.txt#1";
 r.enemy.uid=1385033769u;r.enemy.source=2;r.enemy.count=1;r.enemy.spawnType=1;r.enemy.generatorVersion="????";
 // This diagnostic isolates corpse yield; number drops and authored placement
 // remain the original provider/campaign tests' responsibility.
 r.enemy.pelletProbability=0;return r;}
class CorpseApp:public PlugPikiApp {
 BTeki* actor=nullptr;Pellet* corpse=nullptr;GoalItem* onion=nullptr;
 Generator* generator=nullptr;std::uint64_t generatorHandle=0,handle=0;Vector3f release;int frame=0,phase=0,age=0,baseline=0,settled=0;
 bool carried=false,nearGoal=false;float travel=0;
 int population(){int total=onion->mHeldPikis[0]+onion->mHeldPikis[1]+onion->mHeldPikis[2]+onion->mSAICtx.mCurrAnimId;
  Iterator p(pikiMgr);CI_LOOP(p){if(static_cast<Piki*>(*p)->isAlive()&&static_cast<Piki*>(*p)->mColor==Red)++total;}
  Iterator h(itemMgr->getPikiHeadMgr());CI_LOOP(h){auto* s=static_cast<PikiHeadItem*>(*h);if(s->isAlive()&&s->mSeedColor==Red)++total;}return total;}
 bool receipt(const CorpseRecord& r,bool consumed){
  const InstanceIdentity expected{std::string(64,'a'),row().enemy.uid,0,1,1};
  return r.identity==expected&&r.sourceType==2&&r.yield==12&&r.consumed==consumed;
 }
 Pellet* live(){Iterator it(pelletMgr);CI_LOOP(it){if(*it==corpse){auto* body=static_cast<Pellet*>(*it);
  const auto* profile=pc_p2_original_corpse_profile(body);require(profile&&profile->source==2,"live body retains original source binding");return body;}}return nullptr;}
 void birth(Navi* n){
  onion=itemMgr->getContainer(Red);
  require(onion,"actual Red Onion");std::string e;checked(pc_p2_original_corpse_resources(2,e),e);
  const int old=gsys->setHeap(SYSHEAP_App);checked(pc_p2_chappy_prepare_original({2},e),e);
  auto* t=tekiMgr->newTeki(TEKI_Swallow);require(t,"actual chassis birth");
  // Audited flat camp used by the actual native five-pellet carry diagnostic.
  t->mPersonality->reset();Vector3f pos(-600,n->mSRT.t.y,2155);pos.y=mapMgr->getMinY(pos.x,pos.z,true);
  t->mPersonality->mPosition=pos;t->mPersonality->mNestPosition=pos;t->mPersonality->mFaceDirection=0;t->reset();t->startAI(0);actor=t;
  generator=new Generator;checked(originalActors().generator(generator,row().enemy.uid,generatorHandle,e),e);
  unsigned token=0;checked(originalActors().actorActivation(actor,row().enemy.uid,0,1,1,token,handle,e),e);
  require(pc_p2_chappy_bind_original(actor,token,2),"genuine staged original P2 Chappy bank/FSM bind");gsys->setHeap(old);
  baseline=population();std::printf("P2_ORIGINAL_CORPSE_CONTROL source=2 uid=%u fixture_registry_binding=1 fixture_placement=1 initial_damage_control=%d population=%d\n",row().enemy.uid,int(!manual),baseline);
  if(!manual)actor->mHealth=0;phase=1;age=0;
 }
public:
 int idle()override{
  int result=PlugPikiApp::idle();require(++frame<24000||manual,"bounded engine frame budget");
  if(gameflow.mMoviePlayer&&gameflow.mMoviePlayer->mIsActive){gameflow.mMoviePlayer->requestSkip();return result;}
  auto* n=naviMgr?naviMgr->getNavi():nullptr;if(!n||!n->getCurrState()||!tekiMgr||!pikiMgr||!pelletMgr||gameflow.mPauseAll||gameflow.mIsUIOverlayActive)return result;
  require(n->mHealth>0&&!GameStat::orimaDead,"captain remains alive");
  if(!phase){if(n->getCurrState()->getID()!=NAVISTATE_Walk)return result;unsigned count=0;Iterator it(pikiMgr);CI_LOOP(it){if(static_cast<Piki*>(*it)->isAlive())++count;}require(count==20,"20 live Pikmin baseline");birth(n);}
  if(manual)return result;++age;
  if(phase==1){input();if(!actor->mPellet)return result;corpse=actor->mPellet;
   const auto* p=pc_p2_original_corpse_profile(corpse);require(p&&p->source==2&&corpse->mConfig->mCarryMinPikis()==10&&corpse->mConfig->mCarryMaxPikis()==20&&corpse->mConfig->mNonMatchingOnyonSeeds()==12,"actual typed source profile before carry");
   require(corpse->mPelletView==static_cast<PelletView*>(actor)&&corpse->mCollInfo,"retained dead P2 view and real collision");
   CorpseSnapshot snapshot;std::string e;checked(pc_p2_original_corpse_snapshot(snapshot,e),e);require(snapshot.records.size()==1&&receipt(snapshot.records[0],false),"full birth source identity/yield and no reward");
   require(!pc_p2_original_corpse_set_death_cause(actor,CorpseDeathCause::StoneShatter,e),"late Stone cause refuses a body already born");
   require(!pc_p2_original_corpse_unload(e),"pending physical corpse refuses course unload");
   require(population()==baseline,"death creates no stock");release=corpse->mSRT.t;phase=2;age=0;return result;}
  auto* body=live();
  if(phase==2){
   Piki* nearest=nullptr;float close=1e9f;Iterator it(pikiMgr);CI_LOOP(it){auto* p=static_cast<Piki*>(*it);if(p->isAlive()&&p->mColor==Red&&distance(n->mSRT.t,p->mSRT.t)<close){nearest=p;close=distance(n->mSRT.t,p->mSRT.t);}}
   if(n->getPlatePikis()<10&&nearest&&close>50)point(n,nearest->mSRT.t,true);else input(KeyConfig::_instance->mSetCursorKey.mBind);
   if(n->getPlatePikis()>=10&&age>30){phase=3;age=0;input();}return result;}
  if(body){
   if(body->mCarrierCount>=10){carried=true;travel=std::max(travel,distance(body->mSRT.t,release));}
   if(carried&&body->mTargetGoal==static_cast<Suckable*>(onion)&&distance(body->mSRT.t,onion->mSRT.t)<100)nearGoal=true;
   if(age%60==0)std::printf("P2_ORIGINAL_CORPSE_CARRY carriers=%u strength=%u max=%d state=%d travel=%.2f population=%d delta=%d navi_distance=%.2f\n",body->mCarrierCount,body->mCarrierCounter,body->mConfig->mCarryMaxPikis(),body->getState(),travel,population(),population()-baseline,distance(n->mSRT.t,body->mSRT.t));
   if(body->mCarrierCount>=10){input();return result;}
   if(distance(n->mSRT.t,body->mSRT.t)>100){point(n,body->mSRT.t,true);age=0;return result;}
   point(n,body->mSRT.t,false,age%60<15?KeyConfig::_instance->mThrowKey.mBind:0);return result;
  }
  input();require(carried&&nearGoal&&travel>25,"actual carry and ordinary Onion approach before removal");if(++settled<90)return result;
  require(population()-baseline==12,"correct ordinary Onion population delta");CorpseSnapshot snapshot;std::string e;checked(pc_p2_original_corpse_snapshot(snapshot,e),e);
  require(snapshot.records.size()==1&&receipt(snapshot.records[0],true),"one full consumed source receipt retained after actor kill");
  checked(pc_p2_original_corpse_unload(e),e);
  CorpseSnapshot retained;checked(pc_p2_original_corpse_snapshot(retained,e),e);require(retained.records.size()==1&&receipt(retained.records[0],true),"successful unload preserves receipt");
  unsigned source=0,token=0;if(originalActors().query(actor,source,token))require(originalActors().retire(actor,handle),"fixture registry retirement");
  require(originalActors().retireGenerator(generator,generatorHandle),"fixture generator binding retirement");
  std::puts("PASS P2_ORIGINAL_CORPSE_RUNTIME actual_P2_dead_bank=1 native_death_END=1 ordinary_SDL_Piki_carry=1 ordinary_Onion_suction=1 population_delta=12 receipts=1 original_binding_control=1 initial_damage_control=1 natural_combat=0 pool_reuse=0 save_resume=0");std::fflush(nullptr);std::_Exit(0);
 }
};
}
int main(int argc,char**argv){
 for(int i=1;i<argc;++i)if(!std::strcmp(argv[i],"--manual-encounter"))manual=true;
 SDL_setenv("SDL_AUDIODRIVER","dummy",1);SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,"1");SDL_SetMainReady();pc_gpu_preference_apply();pc_bbft_init(argc,argv);
 require(pc_pikipelago_surface_course()&&!std::strcmp(pc_pikipelago_surface_course(),"tutorial"),"imported tutorial course");
 require(pc_window_init("P2 original corpse native carry diagnostic",960,540),"window init");pc_settings_init();pc_window_set_control_mode(PC_CONTROL_CLASSIC);pc_window_set_display_mode(0);pc_window_set_window_size(960,540);pc_window_center();
 SDL_Window* w=SDL_GL_GetCurrentWindow();int width,height,x,y;SDL_GetWindowSize(w,&width,&height);SDL_GetWindowPosition(w,&x,&y);SDL_Rect b{};SDL_GetDisplayBounds(SDL_GetWindowDisplayIndex(w),&b);
 require(width==960&&height==540&&std::abs(x-(b.x+(b.w-width)/2))<=2&&std::abs(y-(b.y+(b.h-height)/2))<=2,"960x540 centered baseline");
 if(!manual){int device=SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,SDL_CONTROLLER_AXIS_MAX,SDL_CONTROLLER_BUTTON_MAX,0);require(device>=0,"SDL virtual controller");
 char guid[64];SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(device),guid,sizeof(guid));std::string mapping=std::string(guid)+",Corpse fixture pad,a:b0,b:b1,x:b2,y:b3,back:b4,guide:b5,start:b6,leftstick:b7,rightstick:b8,leftshoulder:b9,rightshoulder:b10,dpup:b11,dpdown:b12,dpleft:b13,dpright:b14,leftx:a0,lefty:a1,rightx:a2,righty:a3,lefttrigger:a4,righttrigger:a5,";
 require(SDL_GameControllerAddMapping(mapping.c_str())>=0,"controller mapping");pad=SDL_JoystickOpen(device);require(pad,"controller open");pc_window_set_stick_invert(0);pc_window_set_cstick_invert(0);pc_window_set_gamepad_binding(PC_KEY_ACT_A,SDL_CONTROLLER_BUTTON_A);pc_window_set_gamepad_binding(PC_KEY_ACT_B,SDL_CONTROLLER_BUTTON_B);input();}
 gsys->Initialise();pc_settings_p2d_init();nodeMgr=new NodeMgr();std::string e;
 checked(originalActors().install(std::string(64,'a'),{row()},[](const CatalogRow& r,std::string&){return r.enemy.source==2;},e),e);
 gsys->run(new CorpseApp());return 0;
}
