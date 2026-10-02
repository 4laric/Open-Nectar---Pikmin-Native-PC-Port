// #1155 engineering-preview real receiver diagnostic; no positive actor/state writes.
#include <SDL2/SDL.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include "system.h"
#include "App.h"
#include "Node.h"
#include "FlowController.h"
#include "MoviePlayer.h"
#include "Navi.h"
#include "Kontroller.h"
#include "NaviMgr.h"
#include "NaviState.h"
#include "Camera.h"
#include "Piki.h"
#include "PikiState.h"
#include "PikiMgr.h"
#include "PikiHeadItem.h"
#include "ItemMgr.h"
#include "GoalItem.h"
#include "Pellet.h"
#include "teki.h"
#include "TekiPersonality.h"
#include "Generator.h"
#include "MapMgr.h"
#include "Shape.h"
#include "Collision.h"
#include "Route.h"
#include "GameStat.h"
#include "PlayerState.h"
#include "KeyConfig.h"
#include "pc_bbft.h"
#include "pc_window.h"
#include "pc_gpu_preference.h"
#include "pc_p2_kochappy.h"
#include "pc_p2_kochappy_fsm.h"
#include "pc_p2_surface_water.h"
#include "pc_p2_purple.h"
#include "pc_p2_kochappy_stun.h"
#include "pc_p2_purple_flight.h"
#include "Boss.h"
#include "Pom.h"
#include "PaniAnimator.h"
#include "settings/pc_settings.h"
#include "settings/pc_settings_p2d.h"
#include "p2_fixture_captain_guard.h"
namespace {
constexpr unsigned Target=0x50323101;
// Read-only inherited member pointer applied to the genuine Boss instance.
struct BossObserver:Boss {
 static float frame(Boss& b){return (b.*(&BossObserver::mAnimator)).getCounter();}
 static int motion(Boss& b){return (b.*(&BossObserver::mAnimator)).getCurrentMotionIndex();}
};
SDL_Joystick* pad=nullptr;
bool human(){return std::getenv("P2_PURPLE_KOCHAPPY_HUMAN")!=nullptr;}
void require(bool yes,const char* message){if(!yes){std::printf("FAIL P2_PURPLE_KOCHAPPY %s\n",message);std::fflush(nullptr);std::_Exit(1);}}
float distance(const Vector3f& a,const Vector3f& b){float x=a.x-b.x,z=a.z-b.z;return std::sqrt(x*x+z*z);}
void input(unsigned keys=0,int x=0,int y=0,int cx=0,int cy=0){
 if(!pad)return;
 pc_window_input_assign(0,PC_INPUT_DEV_GAMEPAD,SDL_JoystickInstanceID(pad));pc_window_input_assign(1,PC_INPUT_DEV_NONE,-1);
 SDL_JoystickSetVirtualButton(pad,SDL_CONTROLLER_BUTTON_A,(keys&KBBTN_A)!=0);
 SDL_JoystickSetVirtualButton(pad,SDL_CONTROLLER_BUTTON_B,(keys&KBBTN_B)!=0);
 SDL_JoystickSetVirtualButton(pad,SDL_CONTROLLER_BUTTON_DPAD_RIGHT,(keys&KBBTN_DPAD_RIGHT)!=0);
 SDL_JoystickSetVirtualAxis(pad,SDL_CONTROLLER_AXIS_LEFTX,Sint16(x*32767/74));
 SDL_JoystickSetVirtualAxis(pad,SDL_CONTROLLER_AXIS_LEFTY,Sint16(-y*32767/74));
 SDL_JoystickSetVirtualAxis(pad,SDL_CONTROLLER_AXIS_RIGHTX,Sint16(cx*32767/74));
 SDL_JoystickSetVirtualAxis(pad,SDL_CONTROLLER_AXIS_RIGHTY,Sint16(-cy*32767/74));SDL_JoystickUpdate();
}
void point(Navi* n,const Vector3f& goal,bool walk,unsigned keys=0){
 const Vector3f& from=walk?n->mSRT.t:n->mCursorWorldPos;
 float dx=goal.x-from.x,dz=goal.z-from.z,d=std::sqrt(dx*dx+dz*dz);int x=0,y=0;
 if(d>(walk?15.f:6.f)){const Vector3f& axis=n->controlCamera()->mViewXAxis;float power=walk?65:22;
  x=int(std::lround(power*(dx*axis.x+dz*axis.z)/d));y=int(std::lround(power*(dx*axis.z-dz*axis.x)/d));}
 input(keys,x,y);
}
class PurpleKochappyApp:public PlugPikiApp {
 int frame=0,age=0,phase=0,start=0,settled=0,throwCount=0;
 bool seenCaptain=false,wasActive=false,sawFit=false,sawPause=false,recovered=false,deathDuringStun=false;
 Teki* enemy=nullptr;Pom* violet=nullptr;Piki* purple=nullptr;
 // Fixture-local observation ledger; pointer/slot is process-local, not a durable Pikmin UID.
 Piki* initialBodies[20]={};unsigned initialGeneratorIds[20]={};int initialBodyCount=0,lastObservedLive=-1;
 void observePopulation(int live) {
  if(age%10!=0&&live==lastObservedLive)return;
  lastObservedLive=live;bool present[20]={};int purpleHeads=0,otherHeads=0,captured=0;
  Iterator observed(pikiMgr);CI_LOOP(observed){Piki* p=static_cast<Piki*>(*observed);if(!p)continue;
   int slot=-1;for(int i=0;i<initialBodyCount;++i)if(initialBodies[i]==p){slot=i;present[i]=true;break;}
   unsigned generator=p->mGenerator?unsigned(p->mGenerator->_70):0;
   if(p->getStickObject()==violet)++captured;
   const Vector3f normal=p->mGroundTriangle?p->mGroundTriangle->mTriangle.mNormal:Vector3f(0,0,0);
   std::printf("P2_PURPLE_KOCHAPPY_BODY age=%d slot=%d ptr=%p generator_present=%d generator=%u baseline_generator=%u alive=%d health=%.3f state=%d mode=%d purple=%d mouth=%d sticker=%p violet_sticker=%d water_timer=%u xyz=%.4f,%.4f,%.4f velocity=%.4f,%.4f,%.4f terrain=%.4f ground=%d normal=%.4f,%.4f,%.4f\n",
    age,slot,static_cast<void*>(p),int(p->mGenerator!=nullptr),generator,slot>=0?initialGeneratorIds[slot]:0,int(p->isAlive()),p->mHealth,p->getCurrState()?p->getState():-1,int(p->mMode),int(pc_p2_is_purple(p)),int(p->isStickToMouth()),static_cast<void*>(p->getStickObject()),int(p->getStickObject()==violet),unsigned(p->mInWaterTimer),p->mSRT.t.x,p->mSRT.t.y,p->mSRT.t.z,p->mVelocity.x,p->mVelocity.y,p->mVelocity.z,mapMgr->getMinY(p->mSRT.t.x,p->mSRT.t.z,true),int(p->mGroundTriangle!=nullptr),normal.x,normal.y,normal.z);
  }
  // Compare pointer tokens only; never dereference a body absent from the current manager.
  for(int i=0;i<initialBodyCount;++i)if(!present[i])std::printf("P2_PURPLE_KOCHAPPY_BODY_MISSING age=%d slot=%d ptr=%p baseline_generator=%u durable_identity=0\n",age,i,static_cast<void*>(initialBodies[i]),initialGeneratorIds[i]);
  Iterator sprouts(itemMgr->getPikiHeadMgr());CI_LOOP(sprouts){PikiHeadItem* h=static_cast<PikiHeadItem*>(*sprouts);if(!h)continue;
   if(h->isAlive()){if(h->mP2Purple)++purpleHeads;else ++otherHeads;}
   std::printf("P2_PURPLE_KOCHAPPY_HEAD age=%d ptr=%p alive=%d purple=%d white=%d bulbmin=%d seed_color=%d state=%d generator_present=%d generator=%u owner=%d parent_onion=%p pullable=%d xyz=%.4f,%.4f,%.4f velocity=%.4f,%.4f,%.4f terrain=%.4f input_UID_mapping=unavailable\n",
    age,static_cast<void*>(h),int(h->isAlive()),int(h->mP2Purple),int(h->mP2White),int(h->mP2Bulbmin),h->mSeedColor,h->getCurrState()?h->getCurrState()->getID():-1,int(h->mGenerator!=nullptr),h->mGenerator?unsigned(h->mGenerator->_70):0,h->mPcOwner,static_cast<void*>(h->mParentOnion),int(h->canPullout()),h->mSRT.t.x,h->mSRT.t.y,h->mSRT.t.z,h->mVelocity.x,h->mVelocity.y,h->mVelocity.z,mapMgr->getMinY(h->mSRT.t.x,h->mSRT.t.z,true));
  }
  std::printf("P2_PURPLE_KOCHAPPY_POPULATION age=%d field=%d purple_heads=%d other_heads=%d field_plus_heads=%d captured=%d dead=%d fall=%d victim=%d born=%d map=%d all=%d violet_generator=%u violet_state=%d violet_motion=%d violet_frame=%.4f loaded_cycle_capacity=%d loaded_min_cycles=%d loaded_max_cycles=%d source_violet_lifetime_capacity=5 remaining_budget=private_unobserved enemy_health=%.3f enemy_xyz=%.4f,%.4f,%.4f\n",
   age,live,purpleHeads,otherHeads,live+purpleHeads+otherHeads,captured,int(GameStat::deadPikis),int(GameStat::fallPikis),int(GameStat::victimPikis),int(GameStat::bornPikis),int(GameStat::mapPikis),int(GameStat::allPikis),violet->mGenerator?unsigned(violet->mGenerator->_70):0,violet->getCurrentState(),BossObserver::motion(*violet),BossObserver::frame(*violet),C_POM_PARM(violet,mMaxPikiPerCycle),C_POM_PARM(violet,mMinCycles),C_POM_PARM(violet,mMaxCycles),enemy->mHealth,enemy->mSRT.t.x,enemy->mSRT.t.y,enemy->mSRT.t.z);
  std::fflush(nullptr);
 }
 float pausedCounter=0,lastCounter=0,activeSeconds=0;
public:
 int idle() override {
  int result=PlugPikiApp::idle();
  // First post-idle boundary, also during movies, pause and disappearance.
  Navi* n=naviMgr?naviMgr->getNavi():nullptr;
  const bool initialized=n&&n->getCurrState();if(initialized)seenCaptain=true;
  const bool forced=std::getenv("P2_PURPLE_KOCHAPPY_FORCE_DOWN")!=nullptr;
  const bool paused=std::getenv("P2_PURPLE_KOCHAPPY_PAUSED_DOWN")!=nullptr;
  if(initialized&&paused)gameflow.mPauseAll=true; // negative test only
  if(seenCaptain&&!initialized){std::puts("P2_FIXTURE_CAPTAIN_MISSING outcome=BLOCKED");p2_fixture_require_captain(true,true,0,frame);}
  if(initialized)p2_fixture_require_captain(GameStat::orimaDead||forced||paused,
   naviMgr->isNaviDead(n)||n->getCurrState()->getID()==NAVISTATE_Dead,n->mHealth,frame);
  ++frame;require(frame<3600,"frame bound; supervisor additionally caps60wallseconds");
  if(gameflow.mMoviePlayer&&gameflow.mMoviePlayer->mIsActive){gameflow.mMoviePlayer->requestSkip();return result;}
  if(!initialized||!pikiMgr||!tekiMgr||!itemMgr||!bossMgr)return result;
  if(gameflow.mPauseAll||gameflow.mIsUIOverlayActive)return result;
  if(phase==0&&(n->getCurrState()->getID()!=NAVISTATE_Walk||++settled<45))return result;
  ++age;int live=0,red=0,purples=0;
  Iterator bodies(pikiMgr);CI_LOOP(bodies){Piki* p=static_cast<Piki*>(*bodies);if(!p->isAlive())continue;++live;
   if(pc_p2_is_purple(p)){++purples;purple=p;}else if(p->mColor==Red)++red;}
  if(phase==0){
   require(live==20&&red==20&&purples==0,"current20nativeRed baseline, no injectedPurple");
   Iterator baseline(pikiMgr);CI_LOOP(baseline){Piki* p=static_cast<Piki*>(*baseline);if(p&&p->isAlive()&&initialBodyCount<20){initialBodies[initialBodyCount]=p;initialGeneratorIds[initialBodyCount]=p->mGenerator?unsigned(p->mGenerator->_70):0;++initialBodyCount;}}
   require(pc_p2_purples_enabled()&&pc_p2_purple_flight_enabled(),"actual Purplebank/flight profiles");
   Iterator ts(tekiMgr);CI_LOOP(ts){Teki* t=static_cast<Teki*>(*ts);if(t->mGenerator&&t->mGenerator->_70==Target){require(!enemy,"duplicateRed");enemy=t;}}
   Iterator bs(bossMgr);CI_LOOP(bs){Boss* b=static_cast<Boss*>(*bs);if(b->isAlive()&&b->mObjType==OBJTYPE_Pom&&pc_p2_violet(static_cast<Pom*>(b))){require(!violet,"duplicateViolet");violet=static_cast<Pom*>(b);}}
   require(enemy&&violet&&pc_p2_kochappy_registered(enemy)&&pc_p2_kochappy_fsm_suppress_ai(enemy),"actualRedownFSM and nativeViolet");
   require(std::fabs(enemy->mHealth-200)<.01f,"sourceRedhealth200");
   std::puts("P2_PURPLE_KOCHAPPY_READY engineering_preview=1 startingRed=20 startingPurple=0 actor_writes=0 tutorial_AP_gate=OPEN");std::fflush(nullptr);
   if(std::getenv("P2_PURPLE_KOCHAPPY_READY_ONLY"))std::_Exit(0);
   phase=1;start=age;
  }
  if(human())return result;
  if(age%60==0){std::printf("P2_PURPLE_KOCHAPPY_PROGRESS phase=%d age=%d hp=%.2f live=%d red=%d purple=%d followers=%d\n",phase,age,n->mHealth,live,red,purples,n->getPlatePikis());std::fflush(nullptr);}
  if(phase==1){
   observePopulation(live);
   const float radius=C_NAVI_PARM(n,mCursorMaxRadius);
   require(std::isfinite(radius)&&radius>20,"loaded cursor radius permits ordinary approach");
   const float approach=std::min(65.f,radius*.5f);
   if(age%30==0){
    std::printf("P2_PURPLE_KOCHAPPY_APPROACH_OBSERVE age=%d state=%d actual_pad_b=%d actual_mainstick=%.4f,%.4f loaded_radius=%.4f loaded_neutral=%.4f loaded_move_threshold=%.4f distance=%.4f approach=%.4f captain=%.4f,%.4f,%.4f velocity=%.4f,%.4f,%.4f violet=%.4f,%.4f,%.4f terrain_mid=%.4f terrain_bud=%.4f followers=%d\n",
     age,n->getCurrState()->getID(),int(SDL_JoystickGetButton(pad,SDL_CONTROLLER_BUTTON_B)),n->mKontroller->getMainStickX(),n->mKontroller->getMainStickY(),radius,C_NAVI_PARM(n,mNeutralStickThreshold),C_NAVI_PARM(n,mCursorMoveStickThreshold),distance(n->mSRT.t,violet->mSRT.t),approach,n->mSRT.t.x,n->mSRT.t.y,n->mSRT.t.z,n->mVelocity.x,n->mVelocity.y,n->mVelocity.z,violet->mSRT.t.x,violet->mSRT.t.y,violet->mSRT.t.z,mapMgr->getMinY((n->mSRT.t.x+violet->mSRT.t.x)*.5f,(n->mSRT.t.z+violet->mSRT.t.z)*.5f,true),mapMgr->getMinY(violet->mSRT.t.x,violet->mSRT.t.z,true),n->getPlatePikis());
    std::fflush(nullptr);
   }
   if(n->getPlatePikis()==20&&age-start>30){
    if(distance(n->mSRT.t,violet->mSRT.t)>approach){point(n,violet->mSRT.t,true,KeyConfig::_instance->mSetCursorKey.mBind);return result;}
    std::printf("P2_PURPLE_KOCHAPPY_APPROACH loaded_cursor_radius=%.4f captain_bud_xz=%.4f target_distance=%.4f SDL_walk=1\n",radius,distance(n->mSRT.t,violet->mSRT.t),approach);
    phase=2;start=age;
   }
   input(KeyConfig::_instance->mSetCursorKey.mBind);return result;
  }
  if(phase==2){
   PikiHeadItem* head=nullptr;Iterator hs(itemMgr->getPikiHeadMgr());CI_LOOP(hs){PikiHeadItem* h=static_cast<PikiHeadItem*>(*hs);if(h->isAlive()&&h->mP2Purple){require(!head,"more than one convertedPurple");head=h;}}
   if(head){input();phase=3;start=age;return result;}
   // Hold/release the ordinary A throw. Never call throwPiki or transit actors.
   int cycle=(age-start)%120;point(n,violet->mSRT.t,false,cycle>=30&&cycle<48?KeyConfig::_instance->mThrowKey.mBind:0);
   if(cycle<70||age%30==0){
    int captured=0,index=0;Iterator samples(pikiMgr);
    CI_LOOP(samples){Piki* p=static_cast<Piki*>(*samples);if(!p->isAlive())continue;
     if(p->getStickObject()==violet)++captured;
     if(p->getState()==PIKISTATE_Flying||p->mMode!=PikiMode::FormationMode){
      std::printf("P2_PURPLE_KOCHAPPY_THROW_OBSERVE age=%d index=%d state=%d mode=%d violet_sticker=%d xyz=%.4f,%.4f,%.4f velocity=%.4f,%.4f,%.4f\n",
       age,index,p->getState(),int(p->mMode),int(p->getStickObject()==violet),p->mSRT.t.x,p->mSRT.t.y,p->mSRT.t.z,p->mVelocity.x,p->mVelocity.y,p->mVelocity.z);
     }++index;
    }
    std::printf("P2_PURPLE_KOCHAPPY_AIM_OBSERVE age=%d cycle=%d actual_pad_a=%d captain=%.4f,%.4f,%.4f cursor=%.4f,%.4f,%.4f violet=%.4f,%.4f,%.4f registered=%d state=%d motion=%d anim_frame=%.4f captured=%d enemy_health=%.2f\n",
     age,cycle,int(SDL_JoystickGetButton(pad,SDL_CONTROLLER_BUTTON_A)),n->mSRT.t.x,n->mSRT.t.y,n->mSRT.t.z,n->mCursorWorldPos.x,n->mCursorWorldPos.y,n->mCursorWorldPos.z,
     violet->mSRT.t.x,violet->mSRT.t.y,violet->mSRT.t.z,int(pc_p2_violet(violet)),violet->getCurrentState(),BossObserver::motion(*violet),BossObserver::frame(*violet),captured,enemy->mHealth);
    std::fflush(nullptr);
   }
   require(age-start<600,"ordinaryViolet conversion did not complete");return result;
  }
  if(phase==3){
   if(purples==1&&purple&&purple->getState()==PIKISTATE_Normal){require(live==20&&red==19,"one-for-one ordinary20body conversion");
    std::puts("P2_PURPLE_KOCHAPPY_CONVERSION live=20 Red=19 Purple=1 SDL_throw_pluck=1 species_writes=0");phase=4;start=age;input();return result;}
   PikiHeadItem* head=nullptr;Iterator hs(itemMgr->getPikiHeadMgr());CI_LOOP(hs){PikiHeadItem* h=static_cast<PikiHeadItem*>(*hs);if(h->isAlive()&&h->mP2Purple)head=h;}
   if(!head&&purples==1){input();require(age-start<600,"ordinaryPurple birth/formation timeout");return result;}
   require(head,"ordinaryPurple sprout disappeared");
   if(distance(n->mSRT.t,head->mSRT.t)>20)point(n,head->mSRT.t,true);
   else input(head->canPullout()?KeyConfig::_instance->mExtractKey.mBind:0);
   require(age-start<600,"ordinary approach/pluck timeout");return result;
  }
  require(purple&&purple->isAlive(),"naturalPurple lifetime lost");
  if(phase==4){
   if(n->getPlatePikis()<20){input(KeyConfig::_instance->mSetCursorKey.mBind);return result;}
   if(distance(n->mSRT.t,enemy->mSRT.t)>120){point(n,enemy->mSRT.t,true);return result;}
   phase=5;start=age;input();
  }
  const bool active=pc_p2_kochappy_stun_active(enemy);
  const float counter=enemy->mTekiAnimator->getCounter();
  if(active){
   if(!wasActive){pausedCounter=counter;activeSeconds=0;std::printf("P2_PURPLE_KOCHAPPY_ACTIVE counter=%.6f health=%.2f\n",counter,enemy->mHealth);}
   require(enemy->getTekiOption(TEKIOPT_ManualAnimation)&&enemy->mMotionSpeed==0,"actual ownmotion overlay notpaused");
   require(std::fabs(counter-pausedCounter)<.001f,"actual animator advanced during receiveroverlay");
   sawPause=true;activeSeconds+=gsys->getFrameTime();if(activeSeconds>1.5f)sawFit=true;
   // Whistle through the normal control path to prevent continuous Pikmin
   // attacking from hiding the source10s recovery boundary with early death.
   if(phase==5)input(KeyConfig::_instance->mSetCursorKey.mBind);
  }
  if(wasActive&&!active&&phase==5){
   std::printf("P2_PURPLE_KOCHAPPY_RECOVERY elapsed=%.3f counter=%.6f fit=%d\n",activeSeconds,counter,int(sawFit));
   if(sawFit){require(activeSeconds>=10,"RedFit recovered before source10s");phase=6;start=age;lastCounter=counter;input();}
  }
  if(phase==7&&wasActive&&!active&&enemy->mHealth<=0)deathDuringStun=true;
  wasActive=active;
  if(phase==5&&!active){
   const int cycle=(age-start)%150;
   // Cycle once while holding A: the native selection path chooses Purple.
   Vector3f aim=enemy->mSRT.t;aim.x+=40; // ordinary cursor target near quake radius, no actor relocation
   point(n,aim,false,cycle<18?(KeyConfig::_instance->mThrowKey.mBind|(cycle==10&&throwCount==0?KBBTN_DPAD_RIGHT:0)):0);
   if(cycle==18)++throwCount;
   require(throwCount<8,"natural Purple receiver/Fit not observed after bounded throws");
  }
  if(phase==6){input();if(age-start>60){require(std::fabs(counter-lastCounter)>.001f,"animator failed toresume");recovered=true;phase=7;start=age;}}
  if(phase==7){
   // Further ordinary throws allow natural damage/death to interrupt another
   // overlay. Success requires the observed active->terminal boundary.
   if(active&&enemy->mHealth<=0)deathDuringStun=true;
   if(!enemy->isAlive()||enemy->mHealth<=0){
    require(sawPause&&sawFit&&recovered,"receiver recovery prerequisite missing");
    std::printf("P2_PURPLE_KOCHAPPY_MECHANIC_RECOVERY_PASS natural_impact=1 motion_pause_resume=1 RedFit10=1 actor_writes=0 death_during_stun=%d tutorial_AP_gate=OPEN\n",int(deathDuringStun));
    std::fflush(nullptr);std::_Exit(deathDuringStun?0:3);
   }
   int cycle=(age-start)%60;point(n,enemy->mSRT.t,false,cycle<18?KeyConfig::_instance->mThrowKey.mBind:0);
   // No forced source selection/state/damage. Unobserved terminal interruption
   // remains a failed diagnostic rather than a production approval.
   require(age-start<600,"natural damage/death interruption timeout");
  }
  return result;
 }
};
}
int main(int argc,char**argv){
 require(std::getenv("PIKMIN_P2_TEST_START_DAY")&&!std::strcmp(std::getenv("PIKMIN_P2_TEST_START_DAY"),"5"),"inherit existing test-only actual day5 bootstrap");
 SDL_setenv("SDL_AUDIODRIVER","dummy",1);SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,"1");SDL_SetMainReady();pc_gpu_preference_apply();pc_bbft_init(argc,argv);
 require(pc_pikipelago_room_preview(),"engineering room-preview required; tutorial/AP acceptance OPEN");
 if(!pc_window_init("Purple own Kochappy engineering diagnostic",960,540))return 3;
 pc_settings_init();pc_window_set_control_mode(PC_CONTROL_CLASSIC);pc_window_set_display_mode(0);pc_window_set_window_size(960,540);pc_window_center();
 SDL_Window* w=SDL_GL_GetCurrentWindow();int width,height,x,y;SDL_GetWindowSize(w,&width,&height);SDL_GetWindowPosition(w,&x,&y);SDL_Rect b{};SDL_GetDisplayBounds(SDL_GetWindowDisplayIndex(w),&b);
 require(width==960&&height==540&&std::abs(x-(b.x+(b.w-width)/2))<=2&&std::abs(y-(b.y+(b.h-height)/2))<=2,"centered960x540 baseline");
 std::printf("P2_PURPLE_KOCHAPPY_WINDOW size=%dx%d centered=1\n",width,height);
 if(!human()){
  int device=SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,SDL_CONTROLLER_AXIS_MAX,SDL_CONTROLLER_BUTTON_MAX,0);require(device>=0,"virtual pad attach");
  char guid[64];SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(device),guid,sizeof(guid));std::string mapping=std::string(guid)+",Tutorial acceptance pad,a:b0,b:b1,x:b2,y:b3,back:b4,guide:b5,start:b6,leftstick:b7,rightstick:b8,leftshoulder:b9,rightshoulder:b10,dpup:b11,dpdown:b12,dpleft:b13,dpright:b14,leftx:a0,lefty:a1,rightx:a2,righty:a3,lefttrigger:a4,righttrigger:a5,";
  require(SDL_GameControllerAddMapping(mapping.c_str())>=0,"virtual pad mapping");pad=SDL_JoystickOpen(device);require(pad,"virtual pad open");
  pc_window_set_stick_invert(0);pc_window_set_cstick_invert(0);pc_window_set_gamepad_binding(PC_KEY_ACT_A,SDL_CONTROLLER_BUTTON_A);pc_window_set_gamepad_binding(PC_KEY_ACT_B,SDL_CONTROLLER_BUTTON_B);pc_window_set_gamepad_binding(PC_KEY_ACT_DPAD_RIGHT,SDL_CONTROLLER_BUTTON_DPAD_RIGHT);input();
 }
 gsys->Initialise();pc_settings_p2d_init();nodeMgr=new NodeMgr();gsys->run(new PurpleKochappyApp());return 0;
}
