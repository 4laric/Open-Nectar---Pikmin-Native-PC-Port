// Isolated integration fixture: real movement and transport AI, no production input changes.
#include <SDL2/SDL.h>
#include <GL/gl.h>
#include "gl/pc_opengl.h"
#include "system.h"
#include "App.h"
#include "Node.h"
#include "Section.h"
#include "FlowController.h"
#include "MoviePlayer.h"
#include "Navi.h"
#include "NaviMgr.h"
#include "NaviState.h"
#include "Kontroller.h"
#include "Piki.h"
#include "PikiMgr.h"
#include "PikiAI.h"
#include "Pellet.h"
#include "MapMgr.h"
#include "PlayerState.h"
#include "Demo.h"
#include "teki.h"
#include "pc_p2_preview.h"
#include "pc_window.h"
#include "pc_bbft.h"
#include "pc_gpu_preference.h"
#include "settings/pc_settings.h"
#include "settings/pc_settings_p2d.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>
static int phase=0,ticks=0;
static Vector3f origin;
static void require(bool value,const char* message) { if(!value) { std::printf("FAIL p2 room: %s\n",message);std::fflush(stdout);std::_Exit(1); } }
// Navi::update polls its controller after GameCoreSection::updateAI starts.
// Override that virtual poll in the standalone fixture, never production input.
class FixtureController : public Kontroller {
public:
    FixtureController() : Kontroller(1) {}
    void update() override {
        updateCont(phase==1?KBBTN_MSTICK_RIGHT:0);
        mMainStickX=phase==1?74:0; mMainStickY=0;
        mSubStickX=0; mSubStickY=0;
    }
};
static void capture() {
    auto bind=reinterpret_cast<PFNGLBINDFRAMEBUFFERPROC>(SDL_GL_GetProcAddress("glBindFramebuffer"));
    GLint previous=0;glGetIntegerv(GL_FRAMEBUFFER_BINDING,&previous);bind(GL_FRAMEBUFFER,0);
    int w=0,h=0;SDL_GL_GetDrawableSize(SDL_GL_GetCurrentWindow(),&w,&h);
    std::vector<unsigned char> pixels(size_t(w)*h*3);glPixelStorei(GL_PACK_ALIGNMENT,1);glReadBuffer(GL_BACK);
    glReadPixels(0,0,w,h,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());bind(GL_FRAMEBUFFER,previous);
    require(glGetError()==GL_NO_ERROR,"capture GL error");
    FILE* f=std::fopen("p2-room.ppm","wb");require(f!=nullptr,"capture file");std::fprintf(f,"P6\n%d %d\n255\n",w,h);
    for(int y=h-1;y>=0;--y)std::fwrite(pixels.data()+size_t(y)*w*3,1,size_t(w)*3,f);std::fclose(f);
}
class RoomApp : public PlugPikiApp {
    int frames=0,repairs=0;
    Teki* enemy=nullptr;
public:
    int idle() override {
        int result=PlugPikiApp::idle();require(++frames<10000,"timeout");
        if(gameflow.mMoviePlayer && gameflow.mMoviePlayer->mIsActive){gameflow.mMoviePlayer->requestSkip();return result;}
        if(!pc_p2_preview_ready() || !naviMgr || !pikiMgr || !tekiMgr)return result;
        Navi* n=naviMgr->getNavi();if(!n || !n->getCurrState() || (phase<=1 && n->getCurrState()->getID()!=NAVISTATE_Walk) || gameflow.mPauseAll || gameflow.mIsUIOverlayActive)return result;
        if(phase==0) {
            if(++ticks<60)return result;
            for(int f=0;f<DEMOFLAG_COUNT;++f)playerState->mDemoFlags.setFlagOnly(f);
            int reds=0,dwarfs=0;Iterator p(pikiMgr);CI_LOOP(p){Piki* v=static_cast<Piki*>(*p);if(v->isAlive() && v->mColor==Red)++reds;}
            Iterator e(tekiMgr);CI_LOOP(e){Teki* v=static_cast<Teki*>(*e);if(v->isAlive() && v->mTekiType==TEKI_Chappy){++dwarfs;enemy=v;}}
            capture();
            std::printf("P2_FIXTURE_COUNTS reds=%d dwarfs=%d\n",reds,dwarfs);
            require(reds==20,"expected twenty field reds");require(dwarfs==1,"expected one dwarf bulborb");
            const float points[][2]={{-85,0},{-175,-100},{185,-180},{-220,-180}};
            for(auto& p:points)require(std::fabs(mapMgr->getMinY(p[0],p[1],true))<0.05f,"native ground differs from decoded zero plane");
            n->mKontroller=new FixtureController();
            repairs=playerState->getCurrParts();origin=n->mSRT.t;phase=1;ticks=0;
            std::puts("P2_FIXTURE_ACTORS_GROUND_PASS");
        } else if(phase==1 && ++ticks>=60) {
            float dx=n->mSRT.t.x-origin.x,dz=n->mSRT.t.z-origin.z;
            std::printf("P2_MOVE_DEBUG origin=%.2f,%.2f,%.2f now=%.2f,%.2f,%.2f stick=%d,%d main=%.2f,%.2f vel=%.2f,%.2f target=%.2f,%.2f flags=%u state=%d\n",origin.x,origin.y,origin.z,n->mSRT.t.x,n->mSRT.t.y,n->mSRT.t.z,int(n->mKontroller->mMainStickX),int(n->mKontroller->mMainStickY),n->mMainStick.x,n->mMainStick.z,n->mVelocity.x,n->mVelocity.z,n->mTargetVelocity.x,n->mTargetVelocity.z,n->mKontroller->mCurrentInput,n->getCurrState()->getID());
            require(dx*dx+dz*dz>4,"controller movement failed");require(std::fabs(n->mSRT.t.y)<5,"captain left floor");
            std::printf("P2_FIXTURE_MOVEMENT_PASS distance=%.2f\n",std::sqrt(dx*dx+dz*dz));
            Pellet* target=pc_p2_preview_treasure();int count=0;Iterator p(pikiMgr);CI_LOOP(p){
                Piki* v=static_cast<Piki*>(*p);if(!v->isAlive())continue;
                v->mActiveAction->abandon(nullptr);v->mActiveAction->mCurrActionIdx=PikiAction::Transport;
                v->mActiveAction->mChildActions[PikiAction::Transport].initialise(target);v->mMode=PikiMode::TransportMode;++count;
            }
            require(count>=5,"insufficient carriers");phase=2;ticks=0;
            std::puts("P2_FIXTURE_TRANSPORT_ASSIGNED real AI, positions unchanged");
        } else if(phase==2) {
            if(FILE* f=std::fopen("treasure-receipt.txt","r")) {
                std::fclose(f);require(playerState->getCurrParts()==repairs,"treasure changed repairs");
                std::puts("P2_FIXTURE_DELIVERY_PASS"); phase=3; ticks=0;
            }
            require(++ticks<5000,"native transport did not deliver");
        } else if(phase==3 && ++ticks>=90) {
            Iterator p(pikiMgr);int attackers=0;CI_LOOP(p){
                Piki* v=static_cast<Piki*>(*p);if(!v->isAlive())continue;
                v->mActiveAction->abandon(nullptr);v->mActiveAction->mCurrActionIdx=PikiAction::Attack;
                v->mActiveAction->mChildActions[PikiAction::Attack].initialise(enemy);v->mMode=PikiMode::AttackMode;++attackers;
            }
            require(attackers>0,"no attackers after delivery");phase=4;ticks=0;
            std::printf("P2_FIXTURE_ATTACK_ASSIGNED count=%d\n",attackers);
        } else if(phase==4) {
            if(!enemy->isAlive()) {
                capture();std::puts("PASS p2 room: actors, ground, controller movement, native carry delivery, unchanged repairs, native combat kill");std::fflush(stdout);std::_Exit(0);
            }
            if(++ticks%300==0)std::printf("P2_COMBAT_PROGRESS health=%.2f\n",enemy->mHealth);
            require(ticks<3000,"native combat did not kill dwarf");
        }
        std::fflush(stdout);return result;
    }
};
int main(int argc,char** argv) {
    SDL_SetMainReady();pc_gpu_preference_apply();_putenv_s("PIKMIN_RANDOMIZER_TEST_BACKGROUND","1");pc_bbft_init(argc,argv);
    require(pc_pikipelago_room_preview(),"requires --experimental-pikmin2-room");
    if(!pc_window_init("P2 room integration fixture",960,720))return 3;
    pc_settings_init();gsys->Initialise();pc_settings_p2d_init();nodeMgr=new NodeMgr();gsys->run(new RoomApp());return 0;
}
