// #1154: ordinary imported-surface movement and the production F6 dialog.
// The supervisor owns OS-dialog confirmation and a <=60 second child deadline.
// No actor writes, direct cave requests, checkpoint calls or transfer writes.
#include <SDL2/SDL.h>
#include "system.h"
#include "App.h"
#include "Node.h"
#include "FlowController.h"
#include "MoviePlayer.h"
#include "Navi.h"
#include "NaviMgr.h"
#include "NaviState.h"
#include "Camera.h"
#include "Piki.h"
#include "PikiMgr.h"
#include "MapMgr.h"
#include "Collision.h"
#include "Shape.h"
#include "PlayerState.h"
#include "GameStat.h"
#include "pc_window.h"
#include "pc_bbft.h"
#include "pc_gpu_preference.h"
#include "pc_p2_cave.h"
#include "pc_p2_cave_route_policy.h"
#include "pc_p2_species.h"
#include "../../../scripts/p2_fixture_captain_guard.h"
#include "settings/pc_settings.h"
#include "settings/pc_settings_p2d.h"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <thread>

namespace {
SDL_Joystick* pad=nullptr;
P2CaveSurfaceRoute route;
void require(bool value,const char* why) {
    if(!value){std::printf("FAIL P2_CAVE_ROUTE_RUNTIME %s\n",why);std::fflush(nullptr);std::_Exit(1);}
}
bool transferExists() {
    std::error_code error;
    bool exists=std::filesystem::exists("p2-cave-surface-transfer.txt",error);
    require(!error,"transfer existence query failed");return exists;
}
void controls(int x=0,int y=0,bool whistle=false) {
    require(SDL_JoystickSetVirtualAxis(pad,SDL_CONTROLLER_AXIS_LEFTX,x*256)==0,"left X input");
    require(SDL_JoystickSetVirtualAxis(pad,SDL_CONTROLLER_AXIS_LEFTY,-y*256)==0,"left Y input");
    require(SDL_JoystickSetVirtualButton(pad,SDL_CONTROLLER_BUTTON_B,whistle?1:0)==0,"whistle input");
    SDL_JoystickUpdate();
}
void f6(const char* kind) {
    for(int down=1;down>=0;--down){
        SDL_Event event{};event.type=down?SDL_KEYDOWN:SDL_KEYUP;
        event.key.windowID=SDL_GetWindowID(SDL_GL_GetCurrentWindow());
        event.key.state=down?SDL_PRESSED:SDL_RELEASED;
        event.key.keysym.scancode=SDL_SCANCODE_F6;event.key.keysym.sym=SDLK_F6;
        require(SDL_PushEvent(&event)==1,"F6 SDL event rejected");
    }
    std::printf("P2_CAVE_ROUTE_F6 where=%s path=SDL_event confirmation=external_native_dialog\n",kind);std::fflush(nullptr);
}
void snapshot(Navi* n,const char* label) {
    int total=0,red=0;Iterator it(pikiMgr);
    CI_LOOP(it){Piki* p=static_cast<Piki*>(*it);if(!p||!p->isAlive())continue;
        require(std::isfinite(p->mSRT.t.x)&&std::isfinite(p->mSRT.t.y)&&std::isfinite(p->mSRT.t.z),"nonfinite squad position");
        int species=pc_p2_species(p);if(species==P2SpeciesRed)++red;
        std::printf("P2_CAVE_ROUTE_LIVE label=%s actor=%d species=%d maturity=%d mode=%d state=%d x=%.6f y=%.6f z=%.6f\n",label,total++,species,int(p->mHappa),int(p->mMode),p->getState(),p->mSRT.t.x,p->mSRT.t.y,p->mSRT.t.z);
    }
    require(total==20&&red==20,"starting Red squad changed");
    require(C_NAVI_PARM(n,mHealth)>0,"captain health denominator");
    std::printf("P2_CAVE_ROUTE_SNAPSHOT label=%s survivors=%d red=%d hp=%.9g health=%.9g x=%.6f y=%.6f z=%.6f\n",label,total,red,n->mHealth,n->mHealth/C_NAVI_PARM(n,mHealth),n->mSRT.t.x,n->mSRT.t.y,n->mSRT.t.z);std::fflush(nullptr);
}
class CaveRouteApp final:public PlugPikiApp {
    bool captainSeen=false;int ticks=0,ready=0,phase=0,wait=0;Vector3f origin;
public:
    int idle() override {
        int result=PlugPikiApp::idle();
        Navi* n=naviMgr?naviMgr->getNavi():nullptr;
        bool initialized=n&&n->getCurrState();
        if(initialized)captainSeen=true;
        bool forced=std::getenv("P2_CAVE_ROUTE_FORCE_CAPTAIN_DOWN")!=nullptr;
        if(captainSeen&&!initialized)p2_fixture_require_captain(true,true,0,ticks);
        if(initialized){
            p2_fixture_require_captain(GameStat::orimaDead,
                naviMgr->isNaviDead(n)||n->getCurrState()->getID()==NAVISTATE_Dead,n->mHealth,ticks);
            if(forced)p2_fixture_require_captain(true,true,0,ticks);
        }
        ++ticks;
        // No demo-flag writes and no automatic movie skip: original lifecycle.
        if(!initialized||!pikiMgr||!mapMgr||!playerState)return result;
        if(gameflow.mPauseAll||gameflow.mIsUIOverlayActive||(gameflow.mMoviePlayer&&gameflow.mMoviePlayer->mIsActive))return result;
        require(!pc_settings_get_debug_keys(),"debug keys must remain disabled");
        if(n->getCurrState()->getID()!=NAVISTATE_Walk)return result;
        require(std::isfinite(n->mSRT.t.x)&&std::isfinite(n->mSRT.t.y)&&std::isfinite(n->mSRT.t.z),"nonfinite captain position");
        if(phase==0){
            if(!pc_p2_cave_surface_route_active()||++ready<30)return result;
            require(flowCont.mCurrentStage&&!std::strcmp(flowCont.mCurrentStage->mFileName,"stages/p2_tutorial.ini"),"wrong surface stage");
            require(!gameflow.mIsChallengeMode&&!pc_pikipelago_room_preview(),"wrong lifecycle");
            require(mapMgr->mMapModel&&mapMgr->mMapModel->mTriCount==5332,"imported face count changed");
            require(mapMgr->mMapModel->mTriList[673].mMapCode!=mapMgr->mMapModel->mTriList[4914].mMapCode,"duplicate slip overlay collapsed");
            require(!route.entrance.contains(n->mSRT.t.x,n->mSRT.t.y,n->mSRT.t.z),"starting captain already inside entrance");
            require(!transferExists(),"stale transfer exists");origin=n->mSRT.t;snapshot(n,"outside");controls();f6("outside");phase=1;wait=0;
        }else if(phase==1){
            require(!transferExists(),"distant F6 wrote transfer");
            if(++wait>=30){std::puts("P2_CAVE_ROUTE_DISTANT_REFUSED observed_ticks=30 transfer=absent");phase=2;wait=0;}
        }else if(phase==2){
            controls(0,0,true);if(++wait>=60){controls();phase=3;}
        }else if(phase==3){
            const float dx=route.entrance.x-n->mSRT.t.x,dz=route.entrance.z-n->mSRT.t.z,d=std::sqrt(dx*dx+dz*dz);
            if(d<10&&route.entrance.contains(n->mSRT.t.x,n->mSRT.t.y,n->mSRT.t.z)){
                controls();phase=4;wait=0;
            }else{
                require(n->controlCamera()!=nullptr,"movement camera missing");const Vector3f& a=n->controlCamera()->mViewXAxis;
                require(d>0,"entrance height unreachable");controls(int(std::lround(65*(dx*a.x+dz*a.z)/d)),int(std::lround(65*(dx*a.z-dz*a.x)/d)));
            }
        }else if(phase==4){
            controls();if(++wait<30)return result;
            require(route.entrance.contains(n->mSRT.t.x,n->mSRT.t.y,n->mSRT.t.z),"captain drifted outside entrance");
            float dx=n->mSRT.t.x-origin.x,dz=n->mSRT.t.z-origin.z;
            require(dx*dx+dz*dz>120*120,"insufficient ordinary movement");
            require(!transferExists(),"transfer preceded inside F6");snapshot(n,"inside_before_F6");
            std::printf("P2_CAVE_ROUTE_DIALOG_READY distance=%.6f faces=5332 actor_writes=0 direct_checkpoint=0\n",std::sqrt(dx*dx+dz*dz));std::fflush(nullptr);
            f6("inside");phase=5;wait=0;
        }else if(phase==5){
            // Production normally exits42 from the actual modal confirmation.
            // A cancelled/refused dialog is never retried or treated as success.
            controls();require(++wait<120,"inside F6 cancelled or refused");
        }
        if(ticks%60==0){std::printf("P2_CAVE_ROUTE_FRAME tick=%d phase=%d x=%.3f y=%.3f z=%.3f hp=%.3f\n",ticks,phase,n->mSRT.t.x,n->mSRT.t.y,n->mSRT.t.z,n->mHealth);std::fflush(nullptr);}
        return result;
    }
};
}
int main(int argc,char** argv){
    // Backstop remains live while SDL_ShowMessageBox blocks the engine thread.
    // The external supervisor still owns child-only termination and run records.
    std::thread([]{std::this_thread::sleep_for(std::chrono::seconds(60));std::puts("FAIL P2_CAVE_ROUTE_RUNTIME wall_timeout60");std::fflush(nullptr);std::_Exit(2);}).detach();
    require(!transferExists(),"stale transfer at startup");
    std::ifstream input("p2-cave-route-surface.txt");require(bool(input)&&p2_cave_surface_route_read(input,route),"surface route sidecar invalid");
    require(route.party.squad.size()==20,"requires20 staged Pikmin");for(const auto& p:route.party.squad)require(p.species==P2SpeciesRed,"requires staged Red squad");
    SDL_setenv("SDL_AUDIODRIVER","dummy",1);SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,"1");SDL_SetMainReady();pc_gpu_preference_apply();
    _putenv_s("PIKMIN_RANDOMIZER_TEST_BACKGROUND","1");_putenv_s("PIKMIN_DEBUG_KEYS","0");pc_bbft_init(argc,argv);
    require(pc_pikipelago_surface_course()!=nullptr,"ordinary surface option required");
    require(pc_window_init("P2 Cave route runtime",960,540),"window init");pc_settings_init();
    // Settings has no process-local debug-key setter. Refuse unsafe saved config;
    // the runner must stage debugKeys=0 in its private settings file.
    require(!pc_settings_get_debug_keys(),"stage private settings debugKeys=0");
    pc_window_set_display_mode(0);pc_window_set_window_size(960,540);pc_window_center();pc_window_set_control_mode(PC_CONTROL_CLASSIC);
    SDL_Window* window=SDL_GL_GetCurrentWindow();int w,h,x,y;SDL_GetWindowSize(window,&w,&h);SDL_GetWindowPosition(window,&x,&y);SDL_Rect bounds{};SDL_GetDisplayBounds(SDL_GetWindowDisplayIndex(window),&bounds);
    require(w==960&&h==540&&std::abs(x-(bounds.x+(bounds.w-w)/2))<=2&&std::abs(y-(bounds.y+(bounds.h-h)/2))<=2,"window baseline");
    std::puts("P2_CAVE_ROUTE_WINDOW width=960 height=540 centered=1 after_settings=1");
    SDL_VirtualJoystickDesc desc{};desc.version=SDL_VIRTUAL_JOYSTICK_DESC_VERSION;desc.type=SDL_JOYSTICK_TYPE_GAMECONTROLLER;desc.naxes=SDL_CONTROLLER_AXIS_MAX;desc.nbuttons=SDL_CONTROLLER_BUTTON_MAX;desc.axis_mask=(1u<<SDL_CONTROLLER_AXIS_MAX)-1;desc.button_mask=(1u<<SDL_CONTROLLER_BUTTON_MAX)-1;desc.name="Cave route fixture P1";
    int device=SDL_JoystickAttachVirtualEx(&desc);require(device>=0,"virtual attach");pad=SDL_JoystickOpen(device);require(pad!=nullptr,"virtual open");
    PADStatus statuses[4]{};pc_window_poll_events(statuses);int id=SDL_JoystickInstanceID(pad);pc_window_input_assign(0,PC_INPUT_DEV_GAMEPAD,id);pc_window_input_assign(1,PC_INPUT_DEV_NONE,-1);
    for(int a=0;a<PC_KEY_ACT_COUNT;++a)pc_window_set_gamepad_binding(a,-1);
    pc_window_set_stick_dead_zone(15);pc_window_set_stick_invert(0);pc_window_set_cstick_invert(0);controls();pc_window_poll_events(statuses);
    SDL_GameController* resolved=pc_window_get_controller();require(resolved&&SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(resolved))==id,"virtual P1 assignment");
    std::printf("P2_CAVE_ROUTE_GAMEPAD instance=%d player=1 assigned=1 input=SDL_virtual\n",id);std::fflush(nullptr);
    gsys->Initialise();pc_settings_p2d_init();nodeMgr=new NodeMgr();gsys->run(new CaveRouteApp());return 0;
}
