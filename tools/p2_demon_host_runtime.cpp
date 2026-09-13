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
#include <string>
#include "Collision.h"

static const char* mode = "drop";
static void require(bool ok, const char* what) { if (!ok) { std::printf("FAIL DEMON_HOST %s\n", what); std::fflush(stdout); std::_Exit(1); } }

class DemonHostApp final : public PlugPikiApp {
    P2DemonHost host;
    p2retail::Motion catchFlyMotion, fallMeckMotion;
    std::string catchProfilePath, fallProfilePath;
    int ticks = 0;
    int phase = 0;
    int recoveryTicks = 0;
    int attackTicks = 0;
    int pursuitTicks = 0;
    bool sawDash = false, sawInterrupt = false;
    bool released = false, moveRequested = false;
    float startingHealth = 0;
    bool ready = false;
public:
    int idle() override {
        int result = PlugPikiApp::idle();
        require(++ticks < 600, "timeout");
        if (gameflow.mMoviePlayer && gameflow.mMoviePlayer->mIsActive) { gameflow.mMoviePlayer->requestSkip(); return result; }
        if (!pc_p2_preview_ready() || !naviMgr || !naviMgr->getNavi()) return result;
        Navi* n = naviMgr->getNavi();
        if (ready) host.update();
        if (!ready) {
            float x0, y0, z0, x1, y1, z1;
            std::ifstream input("demon-mouths.txt");
            input >> x0 >> y0 >> z0 >> x1 >> y1 >> z1;
            require(bool(input), "mouth profile");
            host.setPosition(Vector3f(0, 100, 100));
            require(host.load("courses/pikmin2room/demon0.mod", Vector3f(x0,y0,z0), Vector3f(x1,y1,z1)), "demon model");
            require(host.loadPoseMeshes("demon-attack-poses.txt"), "mesh and mouth pose bank");
            const char* catchProfile = std::getenv("DEMON_CATCHFLY_POSES");
            const char* fallProfile = std::getenv("DEMON_FALLMECK_POSES");
            catchProfilePath = catchProfile ? catchProfile : "demon-waitact2-poses.txt";
            fallProfilePath = fallProfile ? fallProfile : "demon-waitact1-poses.txt";
            require(host.preloadPoseMeshes(catchProfilePath.c_str()), "CatchFly pose bank");
            require(host.preloadPoseMeshes(fallProfilePath.c_str()), "FallMeck pose bank");
            require(host.applyPoseFrame(0) && host.renderedPoseFrame()==0, "initial synchronized pose");
            n->mStateMachine->transit(n, NAVISTATE_Walk);
            n->resetPosition(host.mouthCentre(0));
            ready = true;
            return result;
        }
        if (phase == 0) {
            if (!std::strcmp(mode, "timed") || !std::strcmp(mode, "timed_timeout")
                || !std::strcmp(mode, "timed_height")) {
                std::ifstream input("demon-retail-events.txt");
                auto table = p2retail::read(input);
                bool started = false;
                for (const auto& motion : table.motions) {
                    if (motion.name == "attack1.bca") started = host.beginTimedAttack(motion);
                    else if (motion.name == "waitact2.bca") catchFlyMotion = motion;
                    else if (motion.name == "waitact1.bca") fallMeckMotion = motion;
                }
                require(!catchFlyMotion.name.empty() && !fallMeckMotion.name.empty(), "retail transition motions");
                require(started, "retail attack motion");
                startingHealth = n->mHealth;
                phase = 4;
                return result;
            }
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
        } else if (phase == 4) {
            require(++attackTicks <= 60, "timed attack timeout");
            // Deterministic target tracking isolates animation/event behavior.
            // Natural approach and target movement remain separate acceptance.
            if (!host.occupied()) n->resetPosition(host.mouthCentre(0));
            const auto decision = host.tickTimedAttack(n, 1.0f, false);
            require(decision.valid, "timed attack update");
            if (decision.dash) { require(attackTicks==11, "retail dash boundary"); sawDash=true; }
            if (decision.clearNoInterrupt) { require(attackTicks==14, "retail interrupt boundary"); sawInterrupt=true; }
            if (decision.next != P2DemonAttackNext::None) {
                require(decision.next==P2DemonAttackNext::CatchFly && sawDash && sawInterrupt,
                    "timed attack reaches occupied CatchFly");
                require(host.switchPoseMeshes(catchProfilePath.c_str()), "switch CatchFly pose bank");
                require(host.beginCatchFly(catchFlyMotion), "begin CatchFly clock");
                phase=5;
            }
        } else if (phase == 5) {
            require(host.occupied(), "CatchFly keeps capture ownership");
            // Bounded world inputs make pursuit and source height ordering
            // observable without replacing the native movement implementation.
            const bool timeout = std::strcmp(mode, "timed_timeout") == 0;
            const bool height = std::strcmp(mode, "timed_height") == 0;
            p2demon::CatchFlyInput input{0, timeout ? 20.0f : 10.0f, 0,
                timeout ? 10000.0f : 10.0f, timeout ? 100.0f : 10.0f, 0,
                0, 0, 20, height ? 5.0f : 25.0f, 1, 0, 10, 0, 0, 0.4f, 10.0f,
                height ? p2demon::HeightNext::Fall : p2demon::HeightNext::None, true};
            const auto decision = host.tickCatchFly(n, 1.0f, input);
            require(decision.valid, "advance CatchFly clock");
            if (timeout) {
                const float drive2 = host.mTargetVelocity.x * host.mTargetVelocity.x
                    + host.mTargetVelocity.z * host.mTargetVelocity.z;
                if (drive2 > 0.0f) {
                require(std::fabs(drive2 - 100.0f) < 0.01f,
                    "CatchFly applies bounded grab-speed pursuit drive");
                require(host.mTargetVelocity.x > 0.0f && host.mTargetVelocity.z < 0.0f,
                    "CatchFly pursuit drive follows non-axis target heading");
                ++pursuitTicks;
                if (pursuitTicks > 1)
                    require(host.mSRT.t.x > 0.0f && host.mSRT.t.z < 100.0f,
                        "CatchFly host moves along pursuit heading over ordinary game frames");
                }
            }
            if (height) {
                require(decision.heightNext == P2DemonHeightNext::Fall
                    && decision.next == P2DemonAttackNext::None, "semantic CatchFly height transition");
                std::puts("PASS DEMON_HOST CatchFly height -> Fall");
                std::fflush(stdout); std::_Exit(0);
            }
            if (decision.next != P2DemonAttackNext::None) {
                require(decision.next == P2DemonAttackNext::FallMeck, "CatchFly END transition");
                if (timeout) require(pursuitTicks >= 301, "CatchFly pursues through strict ten-second boundary");
                require(host.switchPoseMeshes(fallProfilePath.c_str()), "switch FallMeck pose bank");
                require(host.beginFallMeck(fallMeckMotion), "start FallMeck motion");
                released = false;
                moveRequested = false;
                phase = 6;
            }
        } else if (phase == 6) {
            const auto decision = host.tickFallMeck(n, 1.0f, 10.0f, 200.0f);
            require(decision.valid, "advance FallMeck clock");
            if (n->getCurrState()->getID() == NAVISTATE_DemonDrop) released = true;
            if (decision.next != P2DemonAttackNext::None) {
                require(decision.next == P2DemonAttackNext::Move && released, "FallMeck END transition");
                moveRequested = true;
                std::puts("DEMON_HOST FallMeck END -> Move");
                phase = 3;
            }
        } else if (phase == 3) {
            require(++recoveryTicks < 240, "drop recovery timeout");
            if (n->getCurrState()->getID() == NAVISTATE_Walk) {
                if (!std::strcmp(mode, "timed") || !std::strcmp(mode, "timed_timeout")) require(moveRequested, "Move after FallMeck");
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
