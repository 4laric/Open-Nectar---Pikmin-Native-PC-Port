// #1096 generated source44 -> ordinary native combat/carry -> durable AP check.
// Existing production TEST_BACKGROUND withdraws20. No creature/health/check writes.
#include <SDL2/SDL.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include "system.h"
#include "App.h"
#include "Node.h"
#include "Section.h"
#include "FlowController.h"
#include "MoviePlayer.h"
#include "Navi.h"
#include "NaviMgr.h"
#include "NaviState.h"
#include "Camera.h"
#include "KeyConfig.h"
#include "Piki.h"
#include "PikiMgr.h"
#include "PikiState.h"
#include "Pellet.h"
#include "teki.h"
#include "GameStat.h"
#include "GameFlow.h"
#include "PlayerState.h"
#include "Demo.h"
#include "pc_randomizer.h"
#include "pc_window.h"
#include "pc_bbft.h"
#include "pc_gpu_preference.h"
#include "settings/pc_settings.h"
#include "settings/pc_settings_p2d.h"
#include "C:/Users/alari/pikmin-randomizer/scripts/p2_fixture_captain_guard.h"

namespace {
constexpr unsigned Target=1849273021;
const char* Check="Bestiary: Deliver P2 Dwarf Orange Bulborb";
SDL_Joystick* pad=nullptr;
void require(bool yes,const char* reason){if(!yes){std::printf("FAIL P2_GENERATED_DELIVERY %s\n",reason);std::fflush(nullptr);std::_Exit(1);}}
void input(unsigned keys=0,int x=0,int y=0,int cx=0,int cy=0){
 SDL_JoystickSetVirtualButton(pad,SDL_CONTROLLER_BUTTON_A,(keys&KBBTN_A)!=0);
 SDL_JoystickSetVirtualButton(pad,SDL_CONTROLLER_BUTTON_B,(keys&KBBTN_B)!=0);
 SDL_JoystickSetVirtualAxis(pad,SDL_CONTROLLER_AXIS_LEFTX,Sint16(x*32767/74));
 SDL_JoystickSetVirtualAxis(pad,SDL_CONTROLLER_AXIS_LEFTY,Sint16(-y*32767/74));
 SDL_JoystickSetVirtualAxis(pad,SDL_CONTROLLER_AXIS_RIGHTX,Sint16(cx*32767/74));
 SDL_JoystickSetVirtualAxis(pad,SDL_CONTROLLER_AXIS_RIGHTY,Sint16(-cy*32767/74));
 SDL_JoystickUpdate();
}
void point(Navi* n,const Vector3f& goal,bool walk,unsigned keys=0){
 const Vector3f& from=walk?n->mSRT.t:n->mCursorWorldPos;
 float dx=goal.x-from.x,dz=goal.z-from.z,d=std::sqrt(dx*dx+dz*dz);
 int x=0,y=0;
 if(d>(walk?15.f:6.f)){
  const Vector3f& axis=n->controlCamera()->mViewXAxis;float power=walk?65:22;
  x=int(std::lround(power*(dx*axis.x+dz*axis.z)/d));y=int(std::lround(power*(dx*axis.z-dz*axis.x)/d));
 }
 input(keys,x,y);
}
class DeliveryApp:public PlugPikiApp {
 int frames=0,observed=0,phase=0,phaseStart=0;bool captainSeen=false;Teki* enemy=nullptr;
 public:
 int idle()override{
  int result=PlugPikiApp::idle();Navi* n=naviMgr?naviMgr->getNavi():nullptr;
  if(n){captainSeen=true;p2_fixture_require_captain(GameStat::orimaDead,!n->getCurrState() || naviMgr->isNaviDead(n) || n->getCurrState()->getID()==NAVISTATE_Dead,std::getenv("P2_GENERATED_FORCE_CAPTAIN_DOWN")?0:n->mHealth,frames);}
  else if(captainSeen)p2_fixture_require_captain(true,true,0,frames);
  require(++frames<5000,"frame ceiling");
  if(gameflow.mMoviePlayer && gameflow.mMoviePlayer->mIsActive){gameflow.mMoviePlayer->requestSkip();return result;}
  if(gameflow.mIsUIOverlayActive){input(frames%30<15?KBBTN_A:0);return result;}
  if(gameflow.mPauseAll || !pc_randomizer_ready() || !n || !n->getCurrState() || !tekiMgr || !pikiMgr || !n->controlCamera())return result;
  ++observed;
  if(phase==0 && n->getCurrState()->getID()==NAVISTATE_Starting)return result;
  int alive=0;Iterator pikis(pikiMgr);CI_LOOP(pikis){if(static_cast<Piki*>(*pikis)->isAlive())++alive;}
  if(!enemy){Iterator it(tekiMgr);CI_LOOP(it){Teki* t=static_cast<Teki*>(*it);if(pc_randomizer_p2_source_for(static_cast<PelletView*>(t))==44 && pc_randomizer_p2_generator_for(static_cast<PelletView*>(t))==Target){require(!enemy,"duplicate singleton source");enemy=t;}}}
  if(phase==0){
   if(observed<60)return result;
   require(enemy,"generated source44 singleton not born");require(alive==20,"starting field squad differs from disclosed20");
   require(gameflow.mWorldClock.mCurrentDay==2,"unexpected initial day");
   std::printf("P2_GENERATED_BASELINE source=44 uid=%u day=2 alive=%d enemy=%.3f,%.3f,%.3f navi=%.3f,%.3f,%.3f hp=%.3f\n",Target,alive,enemy->mSRT.t.x,enemy->mSRT.t.y,enemy->mSRT.t.z,n->mSRT.t.x,n->mSRT.t.y,n->mSRT.t.z,n->mHealth);
   for(int f=0;f<DEMOFLAG_COUNT;++f)playerState->mDemoFlags.setFlagOnly(f); // Disclosed tutorial instrumentation.
   phase=1;phaseStart=observed;
  }
  if(observed%60==0){std::printf("P2_GENERATED_PROGRESS frame=%d phase=%d alive=%d enemy_alive=%d hp=%.3f navi=%.3f,%.3f enemy=%.3f,%.3f\n",frames,phase,alive,int(enemy->isAlive()),n->mHealth,n->mSRT.t.x,n->mSRT.t.z,enemy->mSRT.t.x,enemy->mSRT.t.z);std::fflush(nullptr);}
  if(pc_randomizer_checked(Check)){std::puts("PASS P2_GENERATED_DELIVERY actual_native_check=1 direct_event_writes=0 scripted_virtual_P1=1");std::fflush(nullptr);std::_Exit(0);}
  if(phase==1){input(KeyConfig::_instance->mSetCursorKey.mBind);if(observed-phaseStart>100){phase=2;phaseStart=observed;}return result;}
  float dx=enemy->mSRT.t.x-n->mSRT.t.x,dz=enemy->mSRT.t.z-n->mSRT.t.z;
  if(phase==2){point(n,enemy->mSRT.t,true);if(dx*dx+dz*dz<120*120){phase=3;phaseStart=observed;input();}return result;}
  if(phase==3){
   unsigned keys=(observed-phaseStart)%60<15?KeyConfig::_instance->mThrowKey.mBind:0;
   point(n,enemy->mSRT.t,false,keys);
   if(!enemy->isAlive()){phase=4;phaseStart=observed;std::puts("P2_GENERATED_NATURAL_DEATH observed=1 health_writes=0");}
   return result;
  }
  // Approach the actual corpse and swarm to assign carry through normal input.
  if(phase==4){point(n,enemy->mSRT.t,true);if(dx*dx+dz*dz<65*65){phase=5;phaseStart=observed;input();}return result;}
  if(phase==5){const Vector3f& axis=n->controlCamera()->mViewXAxis;float d=std::sqrt(dx*dx+dz*dz);int x=0,y=0;if(d>1){x=int(65*(dx*axis.x+dz*axis.z)/d);y=int(65*(dx*axis.z-dz*axis.x)/d);}input(0,0,0,x,y);return result;}
  return result;
 }
};
}
int main(int argc,char**argv){
 SDL_setenv("SDL_AUDIODRIVER","dummy",1);SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,"1");SDL_SetMainReady();pc_gpu_preference_apply();
 _putenv_s("PIKMIN_RANDOMIZER_TEST_BACKGROUND","1");pc_bbft_init(argc,argv);require(pc_randomizer_enabled(),"requires generated full-session bootstrap");
 if(!pc_window_init("Generated P2 delivery acceptance",960,540))return 3;
 pc_settings_init();pc_window_set_control_mode(PC_CONTROL_CLASSIC);pc_window_set_display_mode(PC_WINDOW_FULLSCREEN_WINDOWED);pc_window_set_window_size(960,540);pc_window_center();
 std::puts("Experimental preview window set to 960x540 windowed and centered");
 int device=SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,SDL_CONTROLLER_AXIS_MAX,SDL_CONTROLLER_BUTTON_MAX,0);require(device>=0,"virtual pad attach");
 char guid[64];SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(device),guid,sizeof(guid));std::string mapping=std::string(guid)+",Generated delivery pad,a:b0,b:b1,x:b2,y:b3,back:b4,guide:b5,start:b6,leftstick:b7,rightstick:b8,leftshoulder:b9,rightshoulder:b10,dpup:b11,dpdown:b12,dpleft:b13,dpright:b14,leftx:a0,lefty:a1,rightx:a2,righty:a3,lefttrigger:a4,righttrigger:a5,";
 require(SDL_GameControllerAddMapping(mapping.c_str())>=0,"virtual pad mapping");pad=SDL_JoystickOpen(device);require(pad!=nullptr,"virtual pad open");
 pc_window_input_assign(0,PC_INPUT_DEV_GAMEPAD,SDL_JoystickInstanceID(pad));pc_window_input_assign(1,PC_INPUT_DEV_NONE,-1);pc_window_set_stick_invert(0);pc_window_set_cstick_invert(0);
 pc_window_set_gamepad_binding(PC_KEY_ACT_A,SDL_CONTROLLER_BUTTON_A);pc_window_set_gamepad_binding(PC_KEY_ACT_B,SDL_CONTROLLER_BUTTON_B);
 std::puts("P2_GENERATED_INPUT SDL_virtual_P1 native_polling=1 ordinary_combat_carry=1");
 gsys->Initialise();pc_settings_p2d_init();nodeMgr=new NodeMgr();gsys->run(new DeliveryApp());return 0;
}
