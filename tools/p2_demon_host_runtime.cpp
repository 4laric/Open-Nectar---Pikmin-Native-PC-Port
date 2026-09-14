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
#include "pc_p2_demon_bridge.h"
#include "teki.h"
#include "Generator.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
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
    int bindingFrames = 0;
    int attackTicks = 0;
    int pursuitTicks = 0;
    bool sawDash = false, sawInterrupt = false;
    bool released = false, moveRequested = false;
    float startingHealth = 0;
    int naturalTicks = 0;
    bool naturalSawAttack = false, naturalSawCapture = false, naturalSawDrop = false;
    float naturalStartZ = 0;
    bool ready = false;
    BTeki* bindingActor = nullptr;
    unsigned bindingGenerator = 0;
    int bindingType = 0;
public:
    int idle() override {
        int result = PlugPikiApp::idle();
        const bool naturalMode = !std::strcmp(mode, "natural");
        require(++ticks < (naturalMode ? 8000 : 600), "timeout");
        if (gameflow.mMoviePlayer && gameflow.mMoviePlayer->mIsActive) { gameflow.mMoviePlayer->requestSkip(); return result; }
        if (!pc_p2_preview_ready() || !naviMgr || !naviMgr->getNavi()) return result;
        Navi* n = naviMgr->getNavi();
        // The P1 Demon bridge admits capture only from Walk (NaviState 0). With no
        // player input the captain idles, so hold the real captain in Walk until
        // the natural capture succeeds; this is a bridge-contract accommodation,
        // not an injected capture, frame or target.
        if (ready && naturalMode && !host.occupied() && !naturalSawCapture && !naturalSawDrop
            && n->getCurrState()->getID() != NAVISTATE_Walk) {
            n->mStateMachine->transit(n, NAVISTATE_Walk);
        }
        if (ready) host.update();
        if (!ready) {
            if (!std::strcmp(mode, "binding_discover")) {
                Iterator actors(tekiMgr); CI_LOOP(actors) {
                    BTeki* actor = static_cast<BTeki*>(*actors);
                    if (actor && actor->mGenerator) {
                        std::printf("DEMON_BINDING_CANDIDATE generator=%u type=%d\n", actor->mGenerator->_70, actor->mTekiType);
                        std::fflush(stdout); std::_Exit(0);
                    }
                }
                require(false, "generated binding candidate");
            }
            if (!std::strcmp(mode, "binding_auto")) {
                const char* gen = std::getenv("DEMON_BIND_GENERATOR");
                const char* type = std::getenv("DEMON_BIND_TYPE");
                require(gen && type, "automatic binding identity inputs");
                const unsigned wantedGenerator = unsigned(std::strtoul(gen, nullptr, 10));
                const int wantedType = std::atoi(type);
                BTeki* actor = nullptr;
                Iterator actors(tekiMgr); CI_LOOP(actors) {
                    BTeki* candidate = static_cast<BTeki*>(*actors);
                    if (candidate && candidate->mGenerator && candidate->mGenerator->_70 == wantedGenerator && candidate->mTekiType == wantedType) {
                        actor = candidate; break;
                    }
                }
                require(actor && pc_p2_demon_manager_binding_count() == 1, "automatic manager binding");
                require(pc_p2_demon_manager_is_bound(actor), "automatic identity validation");
                if (++bindingFrames > 30) {
                    require(pc_p2_demon_manager_render_count() > 0, "automatic host rendered geometry");
                    std::puts("PASS DEMON_HOST automatic_binding_final_setup"); std::fflush(stdout); std::_Exit(0);
                }
                return result;
            }
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
            if (!std::strcmp(mode, "binding")) {
                const char* gen = std::getenv("DEMON_BIND_GENERATOR");
                const char* type = std::getenv("DEMON_BIND_TYPE");
                const bool explicitIdentity = gen && type;
                if (explicitIdentity) {
                    bindingGenerator = unsigned(std::strtoul(gen, nullptr, 10));
                    bindingType = std::atoi(type);
                }
                Iterator actors(tekiMgr);
                BTeki* otherActor = nullptr;
                CI_LOOP(actors) {
                    BTeki* candidate = static_cast<BTeki*>(*actors);
                    if (candidate && candidate->mGenerator && (!explicitIdentity
                        || (candidate->mGenerator->_70 == bindingGenerator && candidate->mTekiType == bindingType))) {
                        if (!bindingActor) bindingActor = candidate;
                        else if (!otherActor) otherActor = candidate;
                    }
                    if (candidate && candidate != bindingActor) otherActor = candidate;
                }
                require(bindingActor && bindingActor->mGenerator, "binding actor available");
                bindingGenerator = bindingActor->mGenerator->_70;
                bindingType = bindingActor->mTekiType;
                require(!host.bindNativeActor(nullptr, bindingGenerator, bindingType), "null actor rejected");
                require(!host.bindNativeActor(bindingActor, bindingGenerator + 1, bindingType), "generator mismatch rejected");
                require(!host.bindNativeActor(bindingActor, bindingGenerator, bindingType + 1), "type mismatch rejected");
                require(bindingActor && host.bindNativeActor(bindingActor, bindingGenerator, bindingType), "bind matching actor");
                require(pc_p2_demon_manager_bind(&host, bindingActor, bindingGenerator, bindingType), "manager bind matching actor");
                if (otherActor) require(!host.bindNativeActor(otherActor, bindingGenerator, bindingType), "second actor rejected");
                host.unbindNativeActor(nullptr);
                require(host.boundNativeActor() == bindingActor, "null unbind preserves actor");
                host.unbindNativeActor(otherActor);
                require(host.boundNativeActor() == bindingActor, "other actor unbind preserves actor");
                const unsigned original = bindingActor->mGenerator->_70;
                bindingActor->mGenerator->_70 = original + 1;
                pc_p2_demon_manager_update();
                require(!host.revalidateNativeActor(bindingActor, original, bindingType), "identity reuse revocation");
                bindingActor->mGenerator->_70 = original;
                host.sceneExit();
                require(host.boundNativeActor() == nullptr, "scene exit unbind");
                std::puts("PASS DEMON_HOST binding_lifecycle"); std::fflush(stdout); std::_Exit(0);
            }
            if (!std::strcmp(mode, "natural")) {
                std::ifstream events("demon-retail-events.txt");
                require(bool(events), "natural retail event table");
                const auto table = p2retail::read(events);
                p2retail::Motion attack, catchFly, fallMeck;
                for (const auto& motion : table.motions) {
                    if (motion.name == "attack1.bca") attack = motion;
                    else if (motion.name == "waitact2.bca") catchFly = motion;
                    else if (motion.name == "waitact1.bca") fallMeck = motion;
                }
                require(!attack.name.empty() && !catchFly.name.empty() && !fallMeck.name.empty(), "natural retail motions");
                host.setNaturalMotions(attack, catchFly, fallMeck);
                host.setNaturalPoseProfiles(catchProfilePath.c_str(), fallProfilePath.c_str());
                host.mSRT.r.set(0, 0, 0);
                host.setPosition(Vector3f(0, 100, 100));
                n->mStateMachine->transit(n, NAVISTATE_Walk);
                n->resetPosition(Vector3f(0, 100, 160));
                startingHealth = n->mHealth;
                naturalStartZ = host.mSRT.t.z;
                host.enableNatural(30.0f, 3.0f, 20.0f, 12.0f, 200.0f, 60.0f, 300.0f, Vector3f(0, 100, 100));
                require(host.naturalEnabled(), "natural captor enabled");
                std::printf("DEMON_NATURAL_BEGIN host=(%.2f,%.2f,%.2f) captain=(%.2f,%.2f,%.2f)\n",
                    host.mSRT.t.x, host.mSRT.t.y, host.mSRT.t.z, n->mSRT.t.x, n->mSRT.t.y, n->mSRT.t.z);
                std::fflush(stdout);
            }
            return result;
        }
        if (ready && !std::strcmp(mode, "natural")) {
            // Ordinary captor front end: no fixture-injected frame, target, END,
            // capture or drop. host.update() performs source target acquisition,
            // approach, Attack/CatchFly/FallMeck and pc_demon_capture delivery.
            require(++naturalTicks < 1500, "natural captor timeout");
            if (host.naturalPhase() >= 2) naturalSawAttack = true;
            if (host.occupied()) naturalSawCapture = true;
            if (n->getCurrState()->getID() == NAVISTATE_DemonDrop) naturalSawDrop = true;
            if (naturalTicks % 30 == 0) {
                const Vector3f mouth = host.mouthCentre(0);
                const float md = (n->mSRT.t - mouth).length();
                std::printf("DEMON_NATURAL tick=%d phase=%d host=(%.2f,%.2f,%.2f) cap=(%.2f,%.2f,%.2f) hp=%.1f state=%d stuck=%d mouthdist=%.3f occupied=%d\n",
                    naturalTicks, host.naturalPhase(), host.mSRT.t.x, host.mSRT.t.y, host.mSRT.t.z,
                    n->mSRT.t.x, n->mSRT.t.y, n->mSRT.t.z, n->mHealth, n->getCurrState()->getID(),
                    int(n->isStickTo()), md, int(host.occupied()));
                std::fflush(stdout);
            }
            if (naturalSawDrop && n->getCurrState()->getID() == NAVISTATE_Walk) {
                require(naturalSawAttack, "natural attack reached");
                require(naturalSawCapture, "natural mouth capture admitted");
                require(host.mSRT.t.z > naturalStartZ + 1.0f, "host approached the live captain");
                require(n->mHealth == startingHealth - 10.0f, "one natural damaging drop completed");
                std::printf("PASS DEMON_HOST natural_captor_acquire_attack_capture_drop (ticks=%d)\n", naturalTicks);
                std::fflush(stdout); std::_Exit(0);
            }
            return result;
        }
        if (!std::strcmp(mode, "livecapture")) {
            // FIXTURE-DRIVEN owner/collision capture. The real converted Demon
            // host supplies its own live mouth CollPart; the fixture stages the
            // real P1 captain at that part and calls pc_demon_capture directly,
            // then releases while captain, owner and part are all still alive.
            // No natural approach, enemy AI, FSM, pose parity or manager
            // registration is claimed.
            CollPart* const mouth = host.mouthPart(0);
            CollPart* const other = host.mouthPart(1);
            require(mouth && other && mouth != other, "host owns two distinct live mouth parts");
            require(mouth->isBouncySphereType() && std::isfinite(mouth->mRadius) && mouth->mRadius > 0.0f,
                "host mouth is a live bound sphere");
            const std::uint64_t token = host.ownerToken();
            require(token != 0, "host generation token");
            std::printf("DEMON_HOST_LIVE mouth=%p other=%p radius=%.3f token=%llu owner=%p\n",
                (void*)mouth, (void*)other, double(mouth->mRadius),
                (unsigned long long)token, (void*)&host);
            std::fflush(stdout);

            n->mStateMachine->transit(n, NAVISTATE_Walk);
            n->resetPosition(host.mouthCentre(0));
            require(pc_demon_capture(n, &host, mouth, token, 0), "live owner-mouth capture admission");
            require(n->isStickToMouth(), "captain marked stuck to mouth");
            require(n->getStickObject() == static_cast<Creature*>(&host), "stick owner is exact host");
            require(n->getStickPart() == mouth, "stick part is exact host mouth");
            require(pc_demon_bound(n), "bridge binding live");
            require(pc_demon_owned_by(n, &host), "bridge owner is exact host");
            std::puts("DEMON_HOST_LIVE link owner=exact part=exact");
            std::fflush(stdout);

            // Move the real host mouth through a loaded pose frame; the native
            // Creature stick update must carry the real captain to that centre.
            host.mSRT.r.set(0.0f, 0.7f, 0.0f);
            host.mSRT.s.set(1.2f, 0.8f, 1.1f);
            host.setPosition(Vector3f(25, 100, 100));
            require(host.applyPoseFrame(17) && host.renderedPoseFrame() == 17, "loaded frame17 mouth pose");
            const Vector3f moved = host.mouthCentre(0);
            require(n->getStickPart() == mouth && (mouth->mCentre - moved).squaredLength() < 0.0001f,
                "exact live part moved with host pose");
            n->update();
            require((n->mSRT.t - moved).squaredLength() < 0.01f, "native stick update follows real mouth");
            std::puts("DEMON_HOST_LIVE follow native_stick_update=1");
            std::fflush(stdout);

            // Release while the captain and the owner/part are all still live.
            require(n->isAlive() && host.mouthPart(0) == mouth, "captain and owner live before release");
            pc_demon_release(n);
            require(!n->isStickToMouth() && !n->isStickTo(), "captain detached from mouth");
            require(n->getStickObject() == nullptr && n->getStickPart() == nullptr, "no captain stick pointers");
            require(!pc_demon_bound(n) && !pc_demon_owned_by(n, &host), "bridge authority revoked");
            Matrix4f releasedPose;
            require(!pc_demon_capture_matrix(n, releasedPose), "released binding reads no part");
            std::puts("DEMON_HOST_LIVE release detached=1 pointers=null");
            std::fflush(stdout);

            // Owner-side revocation after release must be inert, and teardown
            // must stay safe while the host is still alive.
            pc_demon_owner_lost(token);
            require(!n->isStickTo() && host.mouthPart(0) == mouth, "stale owner token is inert");
            host.sceneExit();
            require(!n->isStickTo() && !pc_demon_bound(n), "owner teardown stays detached");
            require(host.mouthPart(0) == mouth && n->isAlive(), "owner and captain alive after teardown");
            pc_demon_scene_exit();
            require(!n->isStickTo(), "scene exit stays detached");
            std::puts("PASS DEMON_HOST live_owner_mouth_capture_release");
            std::fflush(stdout); std::_Exit(0);
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
                const bool pursuitTarget = !std::strcmp(mode, "timed_pursuit");
                const bool timeoutTarget = !std::strcmp(mode, "timed_timeout");
                const float targetRadius = timeoutTarget ? 10000.0f : (pursuitTarget ? 40.0f : 0.0f);
                require(host.selectCatchFlyTarget(Vector3f(0, 10, 0), targetRadius, 1.5707963f), "select CatchFly target");
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
                0, 0, 20, height ? 5.0f : 25.0f, 1, 0, 10, 0, 0.4f, 10.0f, 0,
                height ? p2demon::HeightNext::Fall : p2demon::HeightNext::None, true};
            const auto decision = host.tickCatchFly(n, 1.0f, input);
            require(decision.valid, "advance CatchFly clock");
            if (timeout) {
                const float drive2 = host.mTargetVelocity.x * host.mTargetVelocity.x
                    + host.mTargetVelocity.z * host.mTargetVelocity.z;
                if (drive2 > 0.0f) {
                require(std::fabs(drive2 - 100.0f) < 0.01f,
                    "CatchFly applies bounded grab-speed pursuit drive");
                require(host.mTargetVelocity.x > 0.0f,
                    "CatchFly pursuit turns toward non-axis target");
                ++pursuitTicks;
                if (pursuitTicks == 1)
                    require(host.mTargetVelocity.z > 0.0f,
                        "CatchFly first pursuit tick obeys capped turn");
                if (pursuitTicks > 20)
                    require(host.mTargetVelocity.z < 0.0f,
                        "CatchFly pursuit converges past target quadrant boundary");
                if (pursuitTicks > 1)
                    require(host.mSRT.t.x > 0.0f,
                        "CatchFly host moves over ordinary game frames");
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
    require(pc_pikipelago_room_preview(), "room"); require(pc_window_init("Demon host fixture", 960, 540), "window");
    pc_window_center();
    {
        SDL_Window* window = SDL_GL_GetCurrentWindow();
        int width = 0, height = 0, x = 0, y = 0; SDL_GetWindowSize(window, &width, &height);
        SDL_GetWindowPosition(window, &x, &y);
        SDL_Rect bounds{0, 0, 0, 0}; SDL_GetDisplayBounds(SDL_GetWindowDisplayIndex(window), &bounds);
        const bool centered = std::abs(x - (bounds.x + (bounds.w - width) / 2)) <= 2
            && std::abs(y - (bounds.y + (bounds.h - height) / 2)) <= 2;
        std::printf("P2_DEMON_HOST_WINDOW size=%dx%d pos=%d,%d display=%dx%d centered=%d\n",
            width, height, x, y, bounds.w, bounds.h, int(centered));
        std::fflush(stdout);
    }
    pc_settings_init(); gsys->Initialise(); pc_settings_p2d_init(); nodeMgr = new NodeMgr();
    gsys->run(new DemonHostApp());
    return 0;
}
