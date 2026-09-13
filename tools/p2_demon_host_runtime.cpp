#include <SDL2/SDL.h>
#include "App.h"
#include "Navi.h"
#include "NaviMgr.h"
#include "NaviState.h"
#include "MoviePlayer.h"
#include "system.h"
#include "pc_bbft.h"
#include "pc_gpu_preference.h"
#include "pc_window.h"
#include "pc_p2_preview.h"
#include "settings/pc_settings.h"
#include "settings/pc_settings_p2d.h"
#include "pc_p2_demon_host.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <cmath>
#include "Collision.h"

static const char* mode = "drop";
static void require(bool ok, const char* what) { if (!ok) { std::printf("FAIL DEMON_HOST %s\n", what); std::fflush(stdout); std::_Exit(1); } }

class DemonHostApp final : public PlugPikiApp {
    P2DemonHost host;
    int ticks = 0;
    int phase = 0;
    int recoveryTicks = 0;
    float startingHealth = 0;
    bool ready = false;
public:
    int idle() override {
        int result = PlugPikiApp::idle();
        require(++ticks < 600, "timeout");
        if (gameflow.mMoviePlayer && gameflow.mMoviePlayer->mIsActive) { gameflow.mMoviePlayer->requestSkip(); return result; }
        if (!pc_p2_preview_ready() || !naviMgr || !naviMgr->getNavi()) return result;
        Navi* n = naviMgr->getNavi();
        if (!ready) {
            float x0, y0, z0, x1, y1, z1;
            std::ifstream input("demon-mouths.txt");
            input >> x0 >> y0 >> z0 >> x1 >> y1 >> z1;
            require(bool(input), "mouth profile");
            host.setPosition(Vector3f(0, 100, 100));
            require(host.load("courses/pikmin2room/demon0.mod", Vector3f(x0,y0,z0), Vector3f(x1,y1,z1)), "demon model");
            require(host.loadPoseMeshes("demon-attack-poses.txt"), "mesh and mouth pose bank");
            require(host.applyPoseFrame(0) && host.renderedPoseFrame()==0, "initial synchronized pose");
            n->mStateMachine->transit(n, NAVISTATE_Walk);
            n->resetPosition(host.mouthCentre(0));
            ready = true;
            return result;
        }
        if (phase == 0) {
            require(host.beginAttack(), "attack begin");
            phase = 1;
        } else if (phase == 1) {
            host.updateAttack(n, 17.0f, false);
            require(host.occupied(), "automatic source-window capture");
            if (!std::strcmp(mode, "pose")) {
                P2DemonPoseBank expectedBank;
                require(expectedBank.load("demon-attack-mouths.txt"), "expected pose bank");
                const auto* sample = expectedBank.exact(17);
                require(sample != nullptr, "expected frame17");
                host.mSRT.r.set(0, 0.7f, 0);
                host.mSRT.s.set(1.2f, 0.8f, 1.1f);
                host.setPosition(Vector3f(25, 100, 100));
                require(host.applyPoseFrame(17) && host.renderedPoseFrame()==17, "frame17 synchronized pose");
                require(!host.applyPoseFrame(18) && host.renderedPoseFrame()==17, "missing frame retains mesh");
                Matrix4f local, world, expected;
                local.makeIdentity();
                for (int r=0; r<3; ++r) for (int c=0; c<4; ++c)
                    local.mMtx[r][c] = sample->values[r*4+c];
                world.makeSRT(host.mSRT.s, host.mSRT.r, host.mSRT.t);
                world.multiplyTo(local, expected);
                CollPart* mouth = n->getStickPart();
                require(mouth != nullptr, "live mouth link");
                for (int r=0; r<3; ++r) for (int c=0; c<4; ++c)
                    require(std::fabs(mouth->mJointMatrix.mMtx[r][c]-expected.mMtx[r][c])<0.001f, "full joint basis");
                n->update();
                const Vector3f centre = host.mouthCentre(0);
                require((n->mSRT.t-centre).squaredLength()<0.01f, "captain follows full posed mouth");
                host.sceneExit();
                require(!n->isStickTo(), "pose teardown");
                std::puts("PASS DEMON_HOST full_mouth_pose_follow");
                std::fflush(stdout); std::_Exit(0);
            }
            require(host.endAttack(n), "CatchFly end");
            phase = 2;
        } else if (phase == 2) {
            if (!std::strcmp(mode, "teardown")) {
                host.sceneExit();
                require(!n->isStickTo(), "host owner teardown");
                std::puts("PASS DEMON_HOST teardown");
            } else {
                startingHealth = n->mHealth;
                require(host.forceDrop(n, 10.0f, 200.0f), "damaging forced drop");
                require(n->getCurrState()->getID() == NAVISTATE_DemonDrop && n->mVelocity.y == -400.0f, "drop state admission");
                phase = 3;
                return result;
            }
            std::fflush(stdout); std::_Exit(0);
        } else if (phase == 3) {
            require(++recoveryTicks < 240, "drop recovery timeout");
            if (n->getCurrState()->getID() == NAVISTATE_Walk) {
                require(n->mHealth == startingHealth - 10.0f, "one damaging drop completion");
                std::puts("PASS DEMON_HOST injected_capture_catchfly_drop_recovery");
                std::fflush(stdout); std::_Exit(0);
            }
        }
        return result;
    }
    void draw(Graphics& gfx) override { PlugPikiApp::draw(gfx); if (ready) host.refresh(gfx); }
};

int main(int argc, char** argv) {
    mode = std::getenv("DEMON_HOST_MODE"); if (!mode) mode = "drop";
    SDL_setenv("SDL_AUDIODRIVER", "dummy", 1); SDL_SetMainReady(); pc_gpu_preference_apply();
    _putenv_s("PIKMIN_RANDOMIZER_TEST_BACKGROUND", "1"); pc_bbft_init(argc, argv);
    require(pc_pikipelago_room_preview(), "room"); require(pc_window_init("Demon host fixture", 960, 720), "window");
    pc_settings_init(); gsys->Initialise(); pc_settings_p2d_init(); nodeMgr = new NodeMgr();
    gsys->run(new DemonHostApp());
    return 0;
}