// #1164: ordinary SDL throws; no creature position, health or FSM writes.
// Build-only fixture target. Run under a separate <=60-second wall supervisor.
#include <SDL2/SDL.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <map>
#include <set>
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
#include "PikiState.h"
#include "PikiMgr.h"
#include "Generator.h"
#include "teki.h"
#include "Collision.h"
#include "GameStat.h"
#include "KeyConfig.h"
#include "pc_bbft.h"
#include "pc_window.h"
#include "pc_gpu_preference.h"
#include "pc_p2_elecbug.h"
#include "settings/pc_settings.h"
#include "settings/pc_settings_p2d.h"
#include "p2_fixture_captain_guard.h"

namespace {
SDL_Joystick* pad=nullptr;
constexpr unsigned Target=346002, Partner=346010;
// Production SDL->PAD conversion divides by256; preserve intended PAD strength.
constexpr int contact_sdl_axis(int padAxis) { return padAxis * 256; }
void require(bool ok,const char* why){
    if(!ok){std::printf("FAIL P2_ELECBUG_CONTACT %s\n",why);std::fflush(nullptr);std::_Exit(1);}
}
void input(unsigned keys=0,int x=0,int y=0){
    pc_window_input_assign(0,PC_INPUT_DEV_GAMEPAD,SDL_JoystickInstanceID(pad));
    pc_window_input_assign(1,PC_INPUT_DEV_NONE,-1);
    SDL_JoystickSetVirtualButton(pad,SDL_CONTROLLER_BUTTON_A,(keys&KBBTN_A)!=0);
    SDL_JoystickSetVirtualButton(pad,SDL_CONTROLLER_BUTTON_B,(keys&KBBTN_B)!=0);
    SDL_JoystickSetVirtualAxis(pad,SDL_CONTROLLER_AXIS_LEFTX,Sint16(contact_sdl_axis(x)));
    SDL_JoystickSetVirtualAxis(pad,SDL_CONTROLLER_AXIS_LEFTY,Sint16(-contact_sdl_axis(y)));
    SDL_JoystickUpdate();
}
float distance(const Vector3f& a,const Vector3f& b){return std::hypot(a.x-b.x,a.z-b.z);}
void point(Navi* n,const Vector3f& goal,bool walk,unsigned keys=0){
    const Vector3f from=walk?n->mSRT.t:n->mCursorWorldPos;
    const float dx=goal.x-from.x,dz=goal.z-from.z,d=std::hypot(dx,dz);
    int x=0,y=0;
    if(d>(walk?15.f:6.f)){
        const Vector3f axis=n->controlCamera()->mViewXAxis;
        const float power=walk?65.f:22.f;
        x=int(std::lround(power*(dx*axis.x+dz*axis.z)/d));
        y=int(std::lround(power*(dx*axis.z-dz*axis.x)/d));
    }
    input(keys,x,y);
}
Teki* find(unsigned token){
    Iterator it(tekiMgr);CI_LOOP(it){Teki* t=static_cast<Teki*>(*it);
        if(t&&t->mGenerator&&t->mGenerator->_70==token)return t;}
    return nullptr;
}
class ContactApp:public PlugPikiApp {
    int frame=0,age=0,ready=0,throwTicks=0;
    bool captainSeen=false,started=false,offContactSeen=false,reverseSeen=false;
    int contactSamples=0,offContactSamples=0;
    // Fixture observation only: held, released, rising, descending, off-contact.
    std::map<Piki*,int> flight;
    std::map<Piki*,int> releasedAt;
    bool aHeld=false;
    void witness(Piki* p,const char* phase){
        std::printf("P2_ELECBUG_THROW frame=%d piki=%p phase=%s\n",frame,static_cast<void*>(p),phase);
    }
public:
    int idle() override {
        const int result=PlugPikiApp::idle();
        // First post-idle boundary: guard even when paused/in a movie. Do not
        // treat an allocated but uninitialized captain as initialized gameplay.
        Navi* n=naviMgr?naviMgr->getNavi():nullptr;
        const bool initialized=n&&n->getCurrState();
        if(initialized)captainSeen=true;
        const bool forced=std::getenv("P2_ELECBUG_FORCE_CAPTAIN_DOWN")!=nullptr;
        if(captainSeen&&!initialized){
            std::puts("P2_FIXTURE_CAPTAIN_MISSING outcome=BLOCKED");
            p2_fixture_require_captain(true,true,0,frame);
        }
        if(initialized)p2_fixture_require_captain(GameStat::orimaDead||forced,
            naviMgr->isNaviDead(n)||n->getCurrState()->getID()==NAVISTATE_Dead,n->mHealth,frame);
        ++frame;
        require(frame<3600,"frame bound; requires separate60s wall supervisor");
        if(gameflow.mMoviePlayer&&gameflow.mMoviePlayer->mIsActive){gameflow.mMoviePlayer->requestSkip();return result;}
        if(!initialized||!pikiMgr||!tekiMgr||gameflow.mPauseAll||gameflow.mIsUIOverlayActive)return result;
        if(!started&&(n->getCurrState()->getID()!=NAVISTATE_Walk||++ready<45))return result;
        Teki* enemy=find(Target);Teki* partner=find(Partner);
        require(enemy&&partner&&pc_p2_elecbug_registered(enemy)&&pc_p2_elecbug_registered(partner),"two bound ElecBugs");
        int live=0,red=0;
        Iterator squad(pikiMgr);CI_LOOP(squad){Piki* p=static_cast<Piki*>(*squad);if(p&&p->isAlive()){++live;if(p->mColor==Red)++red;}}
        if(!started){
            require(live==20&&red==20,"fresh20 nativeRed1 baseline");
            require(enemy->mCollInfo&&enemy->mCollInfo->hasInfo(),"initialized enemy geometry");
            started=true;
            std::printf("P2_ELECBUG_CONTACT_READY live=%d red=%d captain_hp=%.3f source_id=28 generators=%u,%u\n",live,red,n->mHealth,Target,Partner);
            if(std::getenv("P2_ELECBUG_READY_ONLY")){
                std::puts("PASS P2_ELECBUG_READY_ONLY combat=UNTESTED");std::fflush(nullptr);std::_Exit(0);
            }
        }
        ++age;
        const char* state=pc_p2_elecbug_state_name(enemy);
        require(state,"registered state exists");
        if(!std::strcmp(state,"reverse"))reverseSeen=true;
        std::set<Piki*> living;
        Iterator candidates(pikiMgr);CI_LOOP(candidates){
            Piki* p=static_cast<Piki*>(*candidates);
            if(!p||!p->isAlive())continue;
            if(!p->getCurrState()){
                auto previous=flight.find(p);
                if(previous!=flight.end()){witness(p,"invalidated");flight.erase(previous);}
                continue;
            }
            living.insert(p);
            int& phase=flight[p];
            const int pstate=p->getState();
            if(aHeld&&p->mNavi==n&&pstate==PIKISTATE_Hanged&&phase!=1){phase=1;witness(p,"held");}
            if(phase==2){
                if(pstate==PIKISTATE_Flying&&p->mVelocity.y>.01f){phase=3;witness(p,"rising");}
                else if(pstate!=PIKISTATE_Hanged||p->mNavi!=n||
                        n->getCurrState()->getID()!=NAVISTATE_Throw||frame-releasedAt[p]>60){
                    phase=0;witness(p,"invalidated");
                }
            }
            if(phase==3&&pstate==PIKISTATE_Flying&&p->mVelocity.y<-.01f){phase=4;witness(p,"descending");}
            if(((phase==3||phase==4)&&pstate!=PIKISTATE_Flying)||
               (phase==5&&!reverseSeen&&pstate!=PIKISTATE_Flying)){
                phase=0;witness(p,"invalidated");
            }
            if(p->mVelocity.y>=-.01f)continue;
            const float xz=distance(p->getPosition(),enemy->getPosition());
            if(xz>30.f)continue;
            Vector3f ignored;
            const bool contact=enemy->mCollInfo&&enemy->mCollInfo->hasInfo()&&enemy->mCollInfo->checkCollision(p,ignored);
            const float dy=p->getPosition().y-enemy->getPosition().y;
            if(contact)++contactSamples;
            else if(dy>30.f&&!reverseSeen&&phase==4&&pstate==PIKISTATE_Flying){
                offContactSeen=true;++offContactSamples;phase=5;
                std::printf("P2_ELECBUG_OFF_CONTACT frame=%d generator=%u piki=%p contact=0\n",frame,Target,static_cast<void*>(p));
            }
            std::printf("P2_ELECBUG_CONTACT_SAMPLE age=%d dy=%.3f xz=%.3f vy=%.3f contact=%d state=%s piki=%p\n",
                age,dy,xz,p->mVelocity.y,int(contact),state,static_cast<void*>(p));
        }
        for(auto it=flight.begin();it!=flight.end();){
            if(!living.count(it->first)){witness(it->first,"invalidated");it=flight.erase(it);}
            else ++it;
        }
        if(age%30==0){
            std::printf("P2_ELECBUG_CONTACT_PROGRESS age=%d live=%d target_distance=%.2f state=%s throw_ticks=%d off_contact=%d contacts=%d\n",
                age,live,distance(n->mSRT.t,enemy->mSRT.t),state,throwTicks,offContactSamples,contactSamples);
            std::fflush(nullptr);
        }
        if(offContactSeen&&reverseSeen){
            for(const auto& entry:flight)if(entry.second==5){
                // Exit supplies a candidate only. The launcher must correlate
                // production contact-dispatch evidence to this exact Pikmin.
                std::printf("P2_ELECBUG_CONTACT_CANDIDATE frame=%d generator=%u piki=%p observed_reverse=1\n",frame,Target,static_cast<void*>(entry.first));
            }
            std::fflush(nullptr);std::_Exit(0);
        }
        // Only virtual-pad input. Gather, approach, aim during A hold, release.
        if(age<90){input(KBBTN_B);return result;}
        if(distance(n->mSRT.t,enemy->mSRT.t)>140.f){
            if(aHeld){for(const auto& entry:flight)witness(entry.first,"invalidated");flight.clear();aHeld=false;}
            point(n,enemy->mSRT.t,true);return result;
        }
        const int cycle=throwTicks++%45;
        if(cycle<22){aHeld=true;point(n,enemy->mSRT.t,false,KBBTN_A);}
        else {
            input();
            if(aHeld)for(auto& entry:flight)if(entry.second==1){entry.second=2;releasedAt[entry.first]=frame;witness(entry.first,"released");}
            aHeld=false;
        }
        return result;
    }
};
}
int main(int argc,char** argv){
    SDL_setenv("SDL_AUDIODRIVER","dummy",1);
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,"1");
    SDL_SetMainReady();pc_gpu_preference_apply();pc_bbft_init(argc,argv);
    require(pc_pikipelago_room_preview(),"requires experimental room argument");
    if(!pc_window_init("P2 Anode landing-contact acceptance",960,540))return 3;
    pc_settings_init();pc_window_set_control_mode(PC_CONTROL_CLASSIC);
    pc_window_set_display_mode(0);pc_window_set_window_size(960,540);pc_window_center();
    SDL_Window* window=SDL_GL_GetCurrentWindow();int width,height,x,y;SDL_Rect bounds{};
    SDL_GetWindowSize(window,&width,&height);SDL_GetWindowPosition(window,&x,&y);
    SDL_GetDisplayBounds(SDL_GetWindowDisplayIndex(window),&bounds);
    require(width==960&&height==540&&std::abs(x-(bounds.x+(bounds.w-width)/2))<=2&&std::abs(y-(bounds.y+(bounds.h-height)/2))<=2,"measured centered960x540");
    std::puts("P2_ELECBUG_CONTACT_WINDOW size=960x540 centered=1");
    const int device=SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,SDL_CONTROLLER_AXIS_MAX,SDL_CONTROLLER_BUTTON_MAX,0);
    require(device>=0,"virtual pad attach");
    char guid[64];SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(device),guid,sizeof(guid));
    const std::string mapping=std::string(guid)+",Anode acceptance pad,a:b0,b:b1,x:b2,y:b3,back:b4,guide:b5,start:b6,leftstick:b7,rightstick:b8,leftshoulder:b9,rightshoulder:b10,dpup:b11,dpdown:b12,dpleft:b13,dpright:b14,leftx:a0,lefty:a1,rightx:a2,righty:a3,lefttrigger:a4,righttrigger:a5,";
    require(SDL_GameControllerAddMapping(mapping.c_str())>=0,"virtual pad mapping");
    pad=SDL_JoystickOpen(device);require(pad,"virtual pad open");
    pc_window_set_stick_invert(0);pc_window_set_cstick_invert(0);
    pc_window_set_gamepad_binding(PC_KEY_ACT_A,SDL_CONTROLLER_BUTTON_A);
    pc_window_set_gamepad_binding(PC_KEY_ACT_B,SDL_CONTROLLER_BUTTON_B);input();
    gsys->Initialise();pc_settings_p2d_init();nodeMgr=new NodeMgr();gsys->run(new ContactApp());return 0;
}
