// #1265: production-derived observer. Only mapped SDL buttons/axes drive play;
// no F6, direct request, teleport, actor health, card or save-state writes.
#include <SDL2/SDL.h>
#include "App.h"
#include "Node.h"
#include "Navi.h"
#include "NaviMgr.h"
#include "NaviState.h"
#include "Camera.h"
#include "MoviePlayer.h"
#include "Piki.h"
#include "PikiMgr.h"
#include "GameStat.h"
#include "PlayerState.h"
#include "gameflow.h"
#include "pc_bbft.h"
#include "pc_randomizer.h"
#include "pc_window.h"
#include "pc_p2_authored_cave_campaign.h"
#include "pc_p2_cave_campaign_cache.h"
#include "pc_p2_cave_campaign_party_engine.h"
#include "settings/pc_settings.h"
#include "settings/pc_settings_p2d.h"
#include "system.h"
#include "p2_fixture_captain_guard.h"
#include <cstdio>
#include <cstdlib>
#include <chrono>
namespace {
SDL_Joystick* pad=nullptr;
int frames=0,stage=0,ticks=0,entryClicks=0,exitClicks=0;
std::uint64_t baselineGeneration=0;
bool entered=false,returned=false,baselineKnown=false;
bool gatherArrived=false,surfaceGathered=false;
bool coldFloor=false;
int entryPulse=0;
float gatherX=0,gatherZ=0;
P2CaveCampaignParty floorParty;
auto start=std::chrono::steady_clock::now();
void finish(int code){std::fflush(nullptr);std::_Exit(code);}
bool same(const P2CaveCampaignParty& a,const P2CaveCampaignParty& b,bool owners=true){
    if(a.bodies.size()!=b.bodies.size() || a.captains.size()!=b.captains.size() || a.active!=b.active)return false;
    for(size_t i=0;i<a.captains.size();++i)if(a.captains[i].slot!=b.captains[i].slot
        || a.captains[i].health!=b.captains[i].health || a.captains[i].maxHealth!=b.captains[i].maxHealth)return false;
    for(auto& x:a.bodies){
        const P2CavePartyBody* found=nullptr;
        for(auto& body:b.bodies)if(body.key==x.key){found=&body;break;}
        if(!found)return false;auto& y=*found;
        if(x.key!=y.key || !x.sameOrigin(y) || x.species!=y.species || x.growth!=y.growth
            || x.health!=y.health || x.maxHealth!=y.maxHealth || (owners&&(x.owner!=y.owner || x.player!=y.player)))return false;}
    return true;
}
class Observer:public PlugPikiApp {
public:int idle()override{
    Navi* n=naviMgr?naviMgr->getActiveNavi():nullptr;
    const auto boundary=pc_p2_cave_campaign_boundary();
    const auto choice=pc_p2_cave_campaign_save_choice();
    const bool movie=gameflow.mMoviePlayer&&gameflow.mMoviePlayer->mIsActive;
    const bool walk=n&&n->getCurrState()&&n->getCurrState()->getID()==NAVISTATE_Walk;
    const bool ready=walk&&!movie&&!gameflow.mPauseAll&&!gameflow.mIsUIOverlayActive;
    const bool controlling=n&&n->getCurrState()
        &&(walk||n->getCurrState()->getID()==NAVISTATE_Gather)
        &&!movie&&!gameflow.mPauseAll&&!gameflow.mIsUIOverlayActive;
    int living=0,formation=0;float liveX=0,liveZ=0;
    if(pikiMgr){Iterator it(pikiMgr);CI_LOOP(it){auto* p=static_cast<Piki*>(*it);if(p->isAlive()){
        ++living;liveX+=p->mSRT.t.x;liveZ+=p->mSRT.t.z;
        if(p->mMode==PikiMode::FormationMode)++formation;}}}
    if(n&&n->getCurrState())p2_fixture_require_captain(GameStat::orimaDead,
        n->getCurrState()->getID()==NAVISTATE_Dead,n->mHealth,frames);
    int a=0,b=0,x=0;float sx=0,sy=0;
    auto move=[&](float tx,float tz){
        const float dx=tx-n->mSRT.t.x,dz=tz-n->mSRT.t.z,len=std::hypot(dx,dz);
        if(len>8&&n->mNaviCamera){const auto& axis=n->mNaviCamera->mViewXAxis;
            sx=65*(dx*axis.x+dz*axis.z)/len;sy=65*(dx*axis.z-dz*axis.x)/len;}
        return len;
    };
    if(choice.active){a=frames%40<4?1:0;}
    else if(gameflow.mIsTutorialTextActive){a=frames%40<4?1:0;}
    else if(gameflow.mIsUIOverlayActive&&!movie){b=frames%40<4?1:0;}
    else if(controlling&&living==20){
        ++ticks;
        if(stage==0&&boundary.ready&&boundary.floor==1&&!baselineKnown){
            std::uint8_t digest[32]{};
            if(!pc_randomizer_checkpoint_info(&baselineGeneration,digest))finish(16);
            baselineKnown=true;coldFloor=true;entered=true;stage=2;ticks=0;
            std::puts("CAVE_VISIBLE_BASELINE genuine_cold_floor=1 live20=1");
        }
        if(stage==0&&boundary.ready&&boundary.floor==0){
            if(!baselineKnown){
                std::uint8_t digest[32]{};
                baselineGeneration=pc_randomizer_active_campaign_generation();
                if(baselineGeneration&&!pc_randomizer_checkpoint_info(&baselineGeneration,digest))finish(12);
                baselineKnown=true;
                gatherX=liveX/living;gatherZ=liveZ/living;
                std::printf("CAVE_VISIBLE_BASELINE actual_surface_SAVE=%llu fresh_or_genuine_resume=1 live20=1\n",(unsigned long long)baselineGeneration);
            }
            if(!surfaceGathered){
                if(!gatherArrived){
                    if(move(gatherX,gatherZ)<20){gatherArrived=true;ticks=0;
                        std::puts("CAVE_VISIBLE_INPUT approach_actual_squad ordinary_movement=1");}
                }else{
                    b=1;
                    if(ticks>=120&&formation==20){surfaceGathered=true;ticks=0;
                        std::puts("CAVE_VISIBLE_INPUT gather_surface20 ordinary_B=1");}
                    else if(ticks>=360){std::puts("P2_CAVE_VISIBLE_RUNTIME FAIL gather_surface20=0");finish(15);}
                }
            }else if(move(boundary.x,boundary.z)<45){stage=1;ticks=0;std::puts("CAVE_VISIBLE_INPUT near_hole ordinary_movement=1");}
        }else if(stage==1){
            if(ticks<45)b=1;
            if(ticks>=90&&ready&&entryClicks==0){++entryClicks;entryPulse=4;
                std::puts("CAVE_VISIBLE_INPUT enter_A=1 F6=0");}
            if(entryPulse>0){a=1;--entryPulse;}
            if(boundary.floor==1){stage=2;ticks=0;entered=true;}
        }else if(stage==2){
            if(ticks==45){
                floorParty=pc_randomizer_authored_cave_session().party;
                if(!floorParty.present || floorParty.bodies.size()!=20 || !floorParty.inside)finish(10);
                if(!coldFloor&&std::getenv("PIKMIN_AUTHORED_FIXTURE_STOP_AFTER_FLOOR_SAVE")){
                    std::uint64_t generation=0;std::uint8_t digest[32]{};P2CaveCampaignParty actual;
                    const bool passed=pc_randomizer_checkpoint_info(&generation,digest)&&generation==baselineGeneration+1
                        &&pc_p2_cave_campaign_party_capture(actual,true)&&same(actual,floorParty)
                        &&GameStat::mapPikis==20&&pc_randomizer_generated_cave_cache().inside;
                    std::printf("P2_AUTHORED_FLOOR_SAVE_RUNTIME %s generation=%llu live20=1 actual_native_SAVE=1\n",
                        passed?"PASS":"FAIL",(unsigned long long)generation);finish(passed?0:17);
                }
                std::puts("CAVE_VISIBLE_INPUT gather_on_landing ordinary_B=1");
            }
            if(ticks>=45&&ticks<135)b=1;
            if(ticks>=135&&formation==20){
                if(move(320,0)<12){stage=3;ticks=0;}
            }
            if(ticks>=240&&formation!=20){std::puts("P2_CAVE_VISIBLE_RUNTIME FAIL recruit20_on_landing=0");finish(14);}
        }else if(stage==3){
            if(ticks==1)std::puts("CAVE_VISIBLE_INPUT dry_bank formation20_before_X=1");
            x=ticks<8?1:0;
            if(ticks>120&&formation==0){
                P2CaveCampaignParty dismissed;
                if(!pc_p2_cave_campaign_party_capture(dismissed,true)||!same(dismissed,floorParty,false)||!dismissed.valid())finish(13);
                floorParty=dismissed;stage=4;ticks=0;
                std::puts("CAVE_VISIBLE_INPUT dismiss20_on_dry_bank=1 actual_owner_reference_captured=1 Red_water_crossing_claim=0");
            }
        }else if(stage==4){if(move(500,0)<20){stage=5;ticks=0;}}
        else if(stage==5){if(move(boundary.x,boundary.z)<45){stage=6;ticks=0;}}
        else if(stage==6){
            if(ticks>=60&&ticks<64)a=1;
            if(ticks==60){++exitClicks;std::puts("CAVE_VISIBLE_INPUT return_A=1 F6=0");}
            if(boundary.floor==0){stage=7;ticks=0;returned=true;}
        }else if(stage==7){
            // Surface weeds/nearby actors can legitimately start work before a
            // read-only snapshot. Settle through ordinary whistle, never FSM writes.
            if(ticks==1)std::puts("CAVE_VISIBLE_INPUT gather_after_return ordinary_B=1");
            if(ticks<90)b=1;
            if(ticks>=150&&ticks%15==0){
            P2CaveCampaignParty actual;const bool captured=pc_p2_cave_campaign_party_capture(actual,false);
            if(captured||ticks>=300){
            const auto& bank=pc_randomizer_generated_cave_cache();
            std::uint64_t generation=0;std::uint8_t sha[32]{};
            const bool checkpoint=pc_randomizer_checkpoint_info(&generation,sha)&&generation==baselineGeneration+(coldFloor?1:2);
            const bool preserved=captured&&same(actual,floorParty);
            const bool population=int(GameStat::mapPikis)==20&&int(GameStat::allPikis[Red])==20;
            const bool passed=entered&&returned&&entryClicks==(coldFloor?0:1)&&exitClicks==1&&preserved&&population&&checkpoint&&!bank.inside;
            std::printf("P2_CAVE_VISIBLE_RUNTIME %s entry_A=%d return_A=%d identity_health=%d live20=%d generation=%llu authored_segment=1 F6=0\n",
                passed?"PASS":"FAIL",entryClicks,exitClicks,int(preserved),int(population),(unsigned long long)generation);
            finish(passed?0:11);
            }
            }
        }
    }
    SDL_JoystickSetVirtualButton(pad,SDL_CONTROLLER_BUTTON_A,a);
    SDL_JoystickSetVirtualButton(pad,SDL_CONTROLLER_BUTTON_B,b);
    SDL_JoystickSetVirtualButton(pad,SDL_CONTROLLER_BUTTON_X,x);
    SDL_JoystickSetVirtualButton(pad,SDL_CONTROLLER_BUTTON_START,
        movie&&!gameflow.mIsUIOverlayActive&&!gameflow.mIsTutorialTextActive&&frames%40<4?1:0);
    SDL_JoystickSetVirtualAxis(pad,SDL_CONTROLLER_AXIS_LEFTX,Sint16(int(sx)*256));
    SDL_JoystickSetVirtualAxis(pad,SDL_CONTROLLER_AXIS_LEFTY,Sint16(-int(sy)*256));SDL_JoystickUpdate();
    if(frames%30==0&&n){std::printf("CAVE_VISIBLE_OBSERVER frame=%d stage=%d floor=%d ready=%d living=%d formation=%d xyz=%.3f,%.3f,%.3f UI=%d\n",
        frames,stage,boundary.floor,int(ready),living,formation,n->mSRT.t.x,n->mSRT.t.y,n->mSRT.t.z,int(choice.active));std::fflush(nullptr);}

    if(std::chrono::steady_clock::now()-start>std::chrono::seconds(180)){std::puts("P2_CAVE_VISIBLE_RUNTIME TIMEOUT");finish(2);}
    ++frames;return PlugPikiApp::idle();
}};
}
int main(int argc,char**argv){
    setvbuf(stdout,nullptr,_IONBF,0);SDL_SetMainReady();pc_bbft_init(argc,argv);
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,"1");
    if(!pc_window_init("Visible cave interaction1265",960,540))return 3;
    pc_settings_init();pc_window_set_control_mode(PC_CONTROL_CLASSIC);
    pc_window_set_display_mode(PC_WINDOW_FULLSCREEN_WINDOWED);pc_window_set_window_size(960,540);pc_window_center();
    int px,py,w,h;SDL_GetWindowPosition(SDL_GL_GetCurrentWindow(),&px,&py);SDL_GetWindowSize(SDL_GL_GetCurrentWindow(),&w,&h);
    std::printf("CAVE_VISIBLE_WINDOW width=%d height=%d x=%d y=%d centered=1\n",w,h,px,py);
    const int device=SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,6,15,0);if(device<0)return 4;
    char guid[64];SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(device),guid,sizeof(guid));
    const std::string mapping=std::string(guid)+",Cave1265 observer,a:b0,b:b1,x:b2,y:b3,back:b4,guide:b5,start:b6,leftstick:b7,rightstick:b8,leftshoulder:b9,rightshoulder:b10,dpup:b11,dpdown:b12,dpleft:b13,dpright:b14,leftx:a0,lefty:a1,rightx:a2,righty:a3,lefttrigger:a4,righttrigger:a5,";
    if(SDL_GameControllerAddMapping(mapping.c_str())<0)return 5;
    pad=SDL_JoystickOpen(device);if(!pad)return 6;
    pc_window_input_assign(0,PC_INPUT_DEV_GAMEPAD,SDL_JoystickInstanceID(pad));pc_window_input_assign(1,PC_INPUT_DEV_NONE,-1);
    pc_window_set_gamepad_binding(PC_KEY_ACT_A,SDL_CONTROLLER_BUTTON_A);pc_window_set_gamepad_binding(PC_KEY_ACT_B,SDL_CONTROLLER_BUTTON_B);
    pc_window_set_gamepad_binding(PC_KEY_ACT_START,SDL_CONTROLLER_BUTTON_START);pc_window_set_gamepad_binding(PC_KEY_ACT_X,SDL_CONTROLLER_BUTTON_X);
    gsys->Initialise();pc_settings_p2d_init();nodeMgr=new NodeMgr();start=std::chrono::steady_clock::now();
    gsys->run(new Observer());return 0;
}
