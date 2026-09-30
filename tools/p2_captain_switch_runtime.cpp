// Lane 12 (#130) live slot-0 captain/squad adapter runtime fixture.
// Private real-GL display fixture; no production registration or gameplay claims.
//
// Drives the LIVE pc_p2_captain adapter (p2_captain::setup_from_navi_mgr is now
// auto-bound by the GameCoreSection constructor) through the four semantics a
// captor family needs: target identity, claim/release, interrupted capture and
// cleanup.
// A separate --knockout-roster scenario exercises the survivor-gated game-over /
// NaviMgr::informOrimaDead hook added to NaviDeadState::init.
// The --survivor-path scenario (with PIKMIN_P2_SECOND_CAPTAIN=1) drives a real
// second captain, knocks the active captain down through the integrated
// InteractAttack receiver, and verifies the survivor rebind, observed squad
// release and final stage end.
// The --two-captain-ppm scenario draws both captains and saves a PPM.
#include <SDL2/SDL.h>
#include <GL/gl.h>
#include "App.h"
#include "CPlate.h"
#include "GameCoreSection.h"
#include "GameStat.h"
#include "Graphics.h"
#include "Interactions.h"
#include "Navi.h"
#include "NaviMgr.h"
#include "NaviState.h"
#include "Kontroller.h"
#include "Pcam/Camera.h"
#include "Pcam/CameraManager.h"
#include "pc_p2_input_script.h"
#include "pc_coop.h"
#include "Node.h"
#include "Piki.h"
#include "PikiMgr.h"
#include "PikiState.h"
#include "MoviePlayer.h"
#include "pc_bbft.h"
#include "pc_gfx.h"
#include "pc_gpu_preference.h"
#include "pc_p2_captain.h"
#include "pc_p2_preview.h"
#include "pc_window.h"
#include "settings/pc_settings.h"
#include "settings/pc_settings_p2d.h"
#include "system.h"
#include "teki.h"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {
bool sSingle = false;
bool sCoop = false;
bool sKnockoutScenario = false;
bool sSurvivorScenario = false;
bool sPpmScenario = false;
bool sMamutaScenario = false;

void require(bool value, const char* message)
{
    if (!value) { std::printf("FAIL P2_CAPTAIN_RUNTIME %s\n", message); std::fflush(stdout); std::_Exit(1); }
}

// Reusable P6 PPM capture after a real draw (mirrors the other room fixtures).
// Returns true (and writes the file) only when the captured frame is non-black,
// so a caller can retry across the setup fade-in.
bool capture(const char* path)
{
    pc_gfx_flush_batch();
    auto bind = reinterpret_cast<PFNGLBINDFRAMEBUFFERPROC>(SDL_GL_GetProcAddress("glBindFramebuffer"));
    require(bind != nullptr, "framebuffer entry point unavailable");
    GLint previous = 0; glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previous); bind(GL_FRAMEBUFFER, 0);
    int width = 0, height = 0; SDL_GL_GetDrawableSize(SDL_GL_GetCurrentWindow(), &width, &height);
    std::vector<unsigned char> pixels(size_t(width) * size_t(height) * 3);
    glReadBuffer(GL_BACK); glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    bind(GL_FRAMEBUFFER, previous);
    bool visible = false; for (unsigned char value : pixels) visible |= value > 8;
    if (!visible) return false;
    FILE* file = std::fopen(path, "wb"); require(file != nullptr, "capture file");
    std::fprintf(file, "P6\n%d %d\n255\n", width, height);
    for (int y = height - 1; y >= 0; --y) std::fwrite(pixels.data() + size_t(y) * width * 3, 1, size_t(width) * 3, file);
    std::fclose(file);
    return true;
}


// Scripted-pad observer: no direct production switch call is used. Unsafe and
// knockout setup is explicitly injected and cannot establish natural combat.
class CaptainSwitchApp final : public PlugPikiApp {
    int frames=0, tick=-1;
    Navi *a=nullptr,*b=nullptr;
    Vector3f movementStart;
    bool sawGather=false,sawHeld=false,sawFlying=false;
    std::vector<Piki*> squad;
    std::vector<Navi*> owners;
    bool screenshot=false;
    void pad(unsigned keys=0,int x=0,int y=0) { pc_p2_input_script_set(1,keys,x,y); }
    void active(int slot) {
        require(naviMgr->getActiveNavi()->mNaviID==slot,"selected captain");
        Navi* n=slot?b:a;
        require(cameraMgr->mController==n->mKontroller,"camera controller selection");
        require(cameraMgr->mCamera->mTargetCreature==n,"camera target selection");
        std::printf("P2_SWITCH_SELECTED tick=%d slot=%d camera=1\n",tick,slot);
    }
public:
    void draw(Graphics& gfx) override {
        PlugPikiApp::draw(gfx);
        if(tick>40 && !screenshot) screenshot=capture("captain-switch.ppm");
    }
    int idle() override {
        int result=PlugPikiApp::idle();
        require(++frames<1800,"fixture frame bound");
        if(gameflow.mMoviePlayer && gameflow.mMoviePlayer->mIsActive) {
            gameflow.mMoviePlayer->requestSkip(); return result;
        }
        if(!pc_p2_preview_ready() || !naviMgr || !naviMgr->getNavi()) return result;
        require(!GameStat::orimaDead,"unexpected game over");
        if(gameflow.mPauseAll || gameflow.mIsUIOverlayActive) return result;
        if(tick<0) {
            a=naviMgr->getNavi(0);
            if(a->getCurrState()->getID()!=NAVISTATE_Walk) return result;
            if(!sSingle) {b=naviMgr->getNavi(1); require(b!=nullptr,"second captain exists");}
            if(b && b->getCurrState()->getID()!=NAVISTATE_Walk) return result;
            Iterator it(pikiMgr); CI_LOOP(it) {
                auto* p=static_cast<Piki*>(*it);
                if(p && p->isAlive()){squad.push_back(p);owners.push_back(p->mNavi);}
            }
            require(squad.size()==20,"fresh live squad exactly20");
            std::printf("P2_SWITCH_ADOPTION live=20 active_gameplay=1 extinction=0 captains=%d\n",naviMgr->getNaviCount());
            tick=0;pad();return result;
        }
        ++tick;
        Navi* selected=naviMgr->getActiveNavi();
        require(selected && selected->mHealth>1 && !naviMgr->isNaviDead(selected),"unexpected selected captain down");
        if(sSingle || sCoop) {
            require(!pc_p2_captain::single_player_switch_enabled(),"single/co-op excluded");
            if(tick==5) pad(KBBTN_DPAD_UP);
            if(tick==20) {require(naviMgr->getActiveNavi()==a,"excluded mode no switch");pad();}
            if(tick==50) {
                std::printf("P2_SWITCH_EXCLUSION single=%d coop=%d observed=1\n",int(sSingle),int(sCoop));
                std::puts("PASS P2_CAPTAIN_SWITCH_RUNTIME");std::fflush(nullptr);std::_Exit(0);
            }
            return result;
        }
        if(tick<90) for(size_t i=0;i<squad.size();++i) require(squad[i]->mNavi==owners[i],"switch preserved squad ownership");
        if(tick>8 && tick<55) {
            require(a->mKontroller->mCurrentInput==0 && a->mKontroller->mMainStickX==0,"inactive neutral controls");
        }
        int state=selected->getCurrState()->getID();
        if(tick>=95 && tick<115 && state==NAVISTATE_Gather)sawGather=true;
        if(tick>=120 && tick<145 && (selected->isHolding()||state==NAVISTATE_ThrowWait))sawHeld=true;
        Iterator it(pikiMgr); CI_LOOP(it) {
            auto* p=static_cast<Piki*>(*it); if(p && p->isAlive() && p->getState()==PIKISTATE_Flying)sawFlying=true;
        }
        switch(tick) {
        case 5:pad(KBBTN_DPAD_UP);break;
        case 10:active(1);break;
        case 15:active(1);pad();break;
        case 25:movementStart=b->getPosition();pad(0,60,0);break;
        case 40: {
            Vector3f delta=b->getPosition()-movementStart;
            require(delta.length()>1,"selected captain walked");
            std::printf("P2_SWITCH_MOVE distance=%.3f inactive_input=0 ownership_preserved=1\n",delta.length());pad();break;
        }
        case 55:pad(KBBTN_DPAD_UP);break;
        case 60:active(0);break;
        case 65:pad();break;
        case 75:pad(KBBTN_DPAD_UP);break;
        case 80:active(1);break;
        case 85:pad();break;
        case 95:pad(KBBTN_B);break;
        case 105:pad();break;
        case 115:require(sawGather,"selected captain whistle state");break;
        case 120:pad(KBBTN_A);break;
        case 130:require(sawHeld,"selected captain held Pikmin");pad(KBBTN_A|KBBTN_DPAD_UP);break;
        case 138:active(1);std::puts("P2_SWITCH_UNSAFE held_rejected=1");break;
        case 140:pad();break;
        case 175:require(sawFlying,"selected captain threw Pikmin through live input");std::puts("P2_SWITCH_ACTIONS whistle=1 hold=1 throw_flying=1");break;
        case 180:require(pc_p2_captain::capture_captain(0,928),"inject target captivity");break;
        case 185:pad(KBBTN_DPAD_UP);break;
        case 190:active(1);std::puts("P2_SWITCH_UNSAFE injected_captive_rejected=1");break;
        case 195:pad();require(pc_p2_captain::release_captain(0,928),"release injected captivity");break;
        case 205:a->mHealth=0;break;
        case 210:pad(KBBTN_DPAD_UP);break;
        case 215:active(1);std::puts("P2_SWITCH_UNSAFE injected_zero_health_rejected=1");break;
        case 220:pad();a->mHealth=100;break;
        case 230: {
            InteractAttack hit(nullptr,nullptr,500.0f,false);hit.actNavi(b);
            std::puts("P2_SWITCH_SURVIVOR injected_attack_receiver=1");break;
        }
        case 240:active(0);require(naviMgr->isNaviDead(b),"down captain recorded");break;
        case 260:require(screenshot,"render capture");std::puts("PASS P2_CAPTAIN_SWITCH_RUNTIME");std::fflush(nullptr);std::_Exit(0);
        }
        if(tick%10==0) {std::printf("P2_SWITCH_TICK tick=%d active=%d states=%d,%d\n",tick,naviMgr->getActiveNavi()->mNaviID,a->getCurrState()->getID(),b->getCurrState()->getID());std::fflush(stdout);}
        return result;
    }
};
} // namespace
int main(int argc,char** argv) {
    for(int i=1;i<argc;++i){sSingle|=std::string(argv[i])=="--single";sCoop|=std::string(argv[i])=="--coop";}
    _putenv_s("PIKMIN_P2_SECOND_CAPTAIN",sSingle?"0":"1");
    _putenv_s("PIKMIN_RANDOMIZER_TEST_BACKGROUND","1");
    SDL_setenv("SDL_AUDIODRIVER","dummy",1);SDL_SetMainReady();pc_gpu_preference_apply();
    pc_bbft_init(argc,argv);require(pc_pikipelago_room_preview(),"experimental room required");
    if(!pc_window_init("P2 Captain switch acceptance",960,540))return 3;
    pc_settings_init();pc_window_set_display_mode(0);pc_window_set_window_size(960,540);pc_window_center();
    SDL_Window* window=SDL_GL_GetCurrentWindow();int w,h,x,y;SDL_GetWindowSize(window,&w,&h);SDL_GetWindowPosition(window,&x,&y);
    SDL_Rect bounds{};SDL_GetDisplayBounds(SDL_GetWindowDisplayIndex(window),&bounds);
    bool centered=std::abs(x-(bounds.x+(bounds.w-w)/2))<=2 && std::abs(y-(bounds.y+(bounds.h-h)/2))<=2;
    require(w==960 && h==540 && centered,"fresh fixture window baseline after settings");
    std::printf("P2_SWITCH_WINDOW size=%dx%d pos=%d,%d centered=%d after_settings=1\n",w,h,x,y,int(centered));std::fflush(stdout);
    pc_coop_set_pending(sCoop);pc_p2_input_script_set(1,0);pc_p2_input_script_set(2,0);
    gsys->Initialise();pc_settings_p2d_init();nodeMgr=new NodeMgr();gsys->run(new CaptainSwitchApp());return 0;
}

