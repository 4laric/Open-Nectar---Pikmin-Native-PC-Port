// #1148 admitted private fixture under development. No runtime acceptance yet.
// This wrapper retains the production entry point, including ICE bootstrap,
// deterministic initialization, session argv notification and settings adoption.
// The command file feeds only a real SDL virtual controller, never game state.
// Observations/readiness alone do not emit a gameplay PASS.
#include "App.h"
#include <SDL.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <fstream>
#include <sstream>
#include <cstdint>
#include <iterator>
#include <cmath>
#include "Controller.h"
#include "Kontroller.h"
#include "KeyConfig.h"
#include "pc_coop.h"
#include "pc_p2_fuefuki_teki.h"
#include "Navi.h"
#include "NaviMgr.h"
#include "NaviState.h"
#include "Piki.h"
#include "PikiMgr.h"
#include "Camera.h"
#include "GameStat.h"
#include "GoalItem.h"
#include "Collision.h"
#include "PlayerState.h"
#include "ItemMgr.h"
#include "CPlate.h"
#include "AIConstant.h"
// Unchanged legacy inline UI helpers omit unused enum cases. Suppress that
// header's existing switch warnings only; all fixture/global flags stay strict.
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch"
#endif
#include "zen/DrawContainer.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#include "pc_window.h"
#include "netplay/pc_netplay_launch.h"
#include "netplay/pc_input_log.h"
#include "pc_randomizer.h"
#include "pc_p2_preview.h"
#include "p2_fixture_captain_guard.h"
#include "p2_coop_fixture_input_snapshot.h"
#include "p2_coop_fixture_step.h"
#include "MoviePlayer.h"
#include "netplay/pc_netplay_pad.h"
#include "netplay/pc_netplay_session.h"
#include "netplay/pc_netplay_loadguard.h"
extern std::uint32_t pc_netplay_current_frame(void);

// These definitions satisfy only the private fixture's optional session observers.
// State is fixed capacity; no production pointer is retained or input changed.
static PcCoopQueueObserver gFixtureQueue;
void pc_coop_fixture_note_submit(std::uint64_t land, int role, const std::uint8_t* wire) {
    gFixtureQueue.submit(land, role, wire);
}
void pc_coop_fixture_note_advance(std::uint64_t frame, int role, std::uint64_t next,
    unsigned delay, unsigned maximum, bool adaptive, bool hold, bool speculative,
    bool rand_neutral, const std::uint8_t* wire) {
    gFixtureQueue.advance(frame, role, next, delay, maximum, adaptive, hold, speculative, rand_neutral, wire);
}

namespace {
void fixture_wire_hex(const unsigned char* wire, unsigned bytes) {
    for (unsigned i = 0; i < bytes; ++i) std::printf("%02x", unsigned(wire[i]));
}
// Read-only protected-member access via a correctly typed base member pointer.
// No layout casts, fake derived object, callback, or menu/world write is used.
struct PlateObserver : CPlate {
    static unsigned count(const CPlate& plate) { return plate.*(&PlateObserver::mPlatePikiCount); }
};
struct ContainerObserver : zen::DrawContainer {
    static int stored(const zen::DrawContainer& win) { return win.*(&ContainerObserver::mInitialContainerCount); }
    static int squad(const zen::DrawContainer& win) { return win.*(&ContainerObserver::mInitialSquadCount); }
    static int field(const zen::DrawContainer& win) { return win.*(&ContainerObserver::mSquadTotalCount); }
    static int limit(const zen::DrawContainer& win) { return win.*(&ContainerObserver::mSquadTotalLimit); }
    static int delta(const zen::DrawContainer& win) { return win.*(&ContainerObserver::mTransferDelta); }
    static int displayedDelta(const zen::DrawContainer& win) { return win.*(&ContainerObserver::mDeltaPikiNum); }
};
void fixture_require(bool ok, const char* why) {
    if (ok) return;
    std::printf("COOP_PROTOCOL_FIXTURE_BLOCKED reason=%s\n", why);
    std::fflush(nullptr);
    std::_Exit(1);
}


// Read one immutable whole-command snapshot. The writer uses ReplaceFileW for
// an existing Windows path; a retained reader sees the complete previous file.
bool command_snapshot(const std::string& path, std::string& text) {
    char bytes[256];
    unsigned count = 0;
    const PcCoopSnapshotResult result = pc_coop_fixture_input_snapshot(
        path.c_str(), bytes, sizeof(bytes), &count);
    if (result == PC_COOP_SNAPSHOT_MISSING) return false;
    fixture_require(result != PC_COOP_SNAPSHOT_IO_ERROR, "SDL input snapshot I/O");
    fixture_require(result == PC_COOP_SNAPSHOT_OK, "SDL input command length");
    text.assign(bytes, count);
    return true;
}

class CoopProtocolFixtureApp : public PlugPikiApp {
    SDL_Joystick* mPad = nullptr;
    bool mCaptainInitialized[2] = {false, false};
    bool mForcedDownDelivered = false;
    unsigned mFrames = 0;
    int mLocalRole = 0;
    Uint64 mStarted = 0;
    Uint64 mLastInput = 0;
    std::uint64_t mInputSequence = 0;
    unsigned mButtons = 0;
    int mAxes[4] = {0, 0, 0, 0};
    std::string mInputPath;
    std::string mInputDirectory;
    PcCoopGenerationState mGenerationState = {};
    std::string mStepDirectory, mStepEpoch;
    PcCoopStepState mStepState;
    bool mSawStartupMovie = false, mStepArmed = false;
    unsigned mGenerationDiagnosticFrame = 0;

    bool stepReady() const {
        if (!mSawStartupMovie || !gameflow.mMoviePlayer || gameflow.mMoviePlayer->mIsActive
            || gameflow.mPauseAll || gameflow.mIsUIOverlayActive || !naviMgr || !pikiMgr || !itemMgr) return false;
        for (int i = 0; i < 2; ++i) {
            Navi* n = naviMgr->getNavi(i);
            if (!n || !n->getCurrState() || n->getCurrState()->getID() != NAVISTATE_Walk
                || !std::isfinite(n->mHealth) || n->mHealth <= 0.f) return false;
        }
        GoalItem* red = itemMgr->pcGetContainer(Red, 0);
        if (!red || red->getTotalStorePikis() != 20 || red->mPikisToExit || itemMgr->getContainerExitCount()) return false;
        unsigned alive = 0; Iterator it(pikiMgr);
        CI_LOOP(it) {
            Piki* p = static_cast<Piki*>(*it);
            if (p && p->getCurrState() && p->isAlive()) {
                if (p->mColor != Red || p->isBuried()) return false;
                ++alive;
            }
        }
        return alive == 20;
    }

    void waitStep() {
        if (!mStepArmed) return;
        unsigned long long beganBoot = 0;
        fixture_require(pc_coop_fixture_input_clock(&beganBoot), "step boot wait clock");
        const Uint64 began = SDL_GetTicks64();
        const unsigned frame = pc_netplay_current_frame();
        std::printf("COOP_PROTOCOL_STEP_WAIT epoch=%s role=%d sim_frame=%u next_frame=%u next_permit=%llu input_sequence=%llu bounded_ms=500 wait_started_boot_ms=%llu\n",
            mStepEpoch.c_str(), mLocalRole, frame, frame + 1, mStepState.sequence + 1, mInputSequence, beganBoot);
        std::fflush(nullptr);
        for (;;) {
            const Uint64 tick = SDL_GetTicks64();
            fixture_require(pc_coop_step_elapsed_ok(tick, began, mStarted, mFrames), "step permit elapsed/overall deadline");
            // Refresh only the real virtual SDL device. No PADRead, engine idle,
            // ControllerMgr update or Gekko game-event consumption inside wait.
            input();
            char bytes[256]; unsigned count = 0; PcCoopGenerationInfo info = {};
            const PcCoopSnapshotResult result = pc_coop_fixture_generation_snapshot(
                mStepDirectory.c_str(), bytes, sizeof(bytes), &count, &info);
            fixture_require(result == PC_COOP_SNAPSHOT_MISSING || result == PC_COOP_SNAPSHOT_OK, "step immutable snapshot refusal");
            unsigned long long now = 0;
            fixture_require(pc_coop_fixture_input_clock(&now), "step clock unsupported");
            // now is Windows boot time, exclusively for packet freshness below.
            // Re-read SDL elapsed time after IO against its own SDL starts.
            fixture_require(pc_coop_step_elapsed_ok(SDL_GetTicks64(), began, mStarted, mFrames),
                "step post-read deadline");
            if (result == PC_COOP_SNAPSHOT_OK) {
                const PcCoopStepDecision decision = pc_coop_step_accept(mStepState, bytes, count, info.sequence,
                    now, mStepEpoch, mLocalRole, frame, mInputSequence);
                fixture_require(decision != PC_COOP_STEP_INVALID, "step canonical epoch role frame sequence command freshness refusal");
                if (decision == PC_COOP_STEP_RELEASE) {
                    fixture_require(pc_coop_step_elapsed_ok(SDL_GetTicks64(), began, mStarted, mFrames),
                        "step post-parser deadline");
                    std::printf("COOP_PROTOCOL_STEP_RELEASE epoch=%s role=%d sim_frame=%u next_frame=%llu permit=%llu input_sequence=%llu applied_generation_proven=0\n",
                        mStepEpoch.c_str(), mLocalRole, frame, mStepState.frame, mStepState.sequence, mInputSequence);
                    std::fflush(nullptr);
                    return;
                }
            }
            pc_netplay_load_keepalive(pc_netplay_loadguard::kSiteStall); // Network-only, existing enabled guard.
            SDL_Delay(1);
        }
    }

    // #1148 read-only post-idle facts, before parsing the next SDL command.
    // These snapshots are not the pre-callPikis state or an eligibility oracle.
    bool mPreviousGatherObservation = false;
    bool mRecruitSchemaEmitted = false;
    unsigned mRecruitBatch = 0;

    void observeRecruitment() {
        bool gather = false;
        for (int captain = 0; captain < 2; ++captain) {
            Navi* n = naviMgr ? naviMgr->getNavi(captain) : nullptr;
            gather = gather || (n && n->getCurrState() && n->getCurrState()->getID() == NAVISTATE_Gather);
        }
        const bool emit = mStepArmed || gather || mPreviousGatherObservation || mFrames % 15 == 0;
        mPreviousGatherObservation = gather;
        if (!emit) return;
        if (!mRecruitSchemaEmitted) {
            std::printf("COOP_PROTOCOL_RECRUIT_META schema=1 phase=post_idle_pre_reader cadence=15_gather_terminal max_entries=4096 eligibility_oracle=0 call_time_oracle=0\n");
            mRecruitSchemaEmitted = true;
        }
        fixture_require(mRecruitBatch < 5000, "recruitment batch ceiling");
        const unsigned batch = ++mRecruitBatch;
        const unsigned frame = pc_netplay_current_frame();
        std::printf("COOP_PROTOCOL_RECRUIT_BEGIN schema=1 batch=%u sim_frame=%u role=%d captains=2 piki_mgr_present=%d paused=%d vs=%d phase=post_idle_pre_reader\n",
            batch, frame, mLocalRole, int(pikiMgr != nullptr), int(gameflow.mPauseAll != 0), int(pc_vs_active()));
        for (int captain = 0; captain < 2; ++captain) {
            Navi* n = naviMgr ? naviMgr->getNavi(captain) : nullptr;
            const bool stateValid = n && n->getCurrState();
            const int state = stateValid ? n->getCurrState()->getID() : -1;
            const bool active = stateValid && state == NAVISTATE_Gather;
            const float radius = active ? static_cast<NaviGatherState*>(n->getCurrState())->mWhistleCallRadius : 0.f;
            const bool radiusValid = active && std::isfinite(radius) && radius > 0.f;
            float minimum = 0.f, maximum = 0.f, neutral = 0.f, cursorThreshold = 0.f;
            float clamp = 0.f, shake = 0.f, speed = 0.f, cursorMax = 0.f;
            bool paramsValid = n && n->mProps;
            if (paramsValid) {
                minimum = C_NAVI_PARM(n, mWhistleMinRadius);
                maximum = C_NAVI_PARM(n, mWhistleMaxRadius);
                neutral = C_NAVI_PARM(n, mNeutralStickThreshold);
                cursorThreshold = C_NAVI_PARM(n, mCursorMoveStickThreshold);
                clamp = C_NAVI_PARM(n, mClampStickToMaxThreshold);
                shake = C_NAVI_PARM(n, mShakePreventionAngle);
                speed = C_NAVI_PARM(n, mCursorMoveSpeed);
                cursorMax = C_NAVI_PARM(n, mCursorMaxRadius);
                paramsValid = std::isfinite(minimum) && std::isfinite(maximum)
                    && std::isfinite(neutral) && std::isfinite(cursorThreshold)
                    && std::isfinite(clamp) && std::isfinite(shake) && std::isfinite(speed) && std::isfinite(cursorMax);
            }
            const bool whistleValid = n && n->mKontroller && KeyConfig::_instance;
            const int whistleDown = whistleValid ? int(n->mKontroller->keyDown(KeyConfig::_instance->mSetCursorKey.mBind)) : -1;
            const int whistleUp = whistleValid ? int(n->mKontroller->keyUp(KeyConfig::_instance->mSetCursorKey.mBind)) : -1;
            const int tapRecall = active ? int(static_cast<NaviGatherState*>(n->getCurrState())->mTapState.recallWorkers) : -1;
            const float benefit = pc_randomizer_benefit_multiplier(PC_BENEFIT_WHISTLE);
            float yawSin = 0.f, yawCos = 1.f;
            const bool yawValid = n && pc_netplay_control_yaw(n->mNaviID, &yawSin, &yawCos);
            std::printf("COOP_PROTOCOL_RECRUIT_NAVI schema=1 batch=%u sim_frame=%u role=%d captain=%d present=%d state_valid=%d state=%d hp=%.3f followers=%d "
                "xyz=%.3f,%.3f,%.3f cursor=%.3f,%.3f,%.3f gather_active=%d radius_valid=%d radius=%.7f held_timer=%.7f "
                "params_valid=%d whistle_min=%.7f whistle_max=%.7f benefit_valid=%d benefit=%.7f neutral_threshold=%.7f cursor_threshold=%.7f clamp_threshold=%.7f shake_angle=%.7f cursor_speed=%.7f cursor_max=%.7f "
                "yaw_valid=%d yaw_sin=%.7f yaw_cos=%.7f reader_sequence_before_next_parse=%llu whistle_input_valid=%d whistle_down=%d whistle_up=%d tap_recall_valid=%d tap_recall_workers=%d\n",
                batch, frame, mLocalRole, captain, int(n != nullptr), int(stateValid), state,
                n ? n->mHealth : 0.f, n && n->mPlateMgr ? n->getPlatePikis() : -1,
                n ? n->mSRT.t.x : 0.f, n ? n->mSRT.t.y : 0.f, n ? n->mSRT.t.z : 0.f,
                n ? n->mCursorWorldPos.x : 0.f, n ? n->mCursorWorldPos.y : 0.f, n ? n->mCursorWorldPos.z : 0.f,
                int(active), int(radiusValid), radius, n ? n->mWhistleTimer : 0.f,
                int(paramsValid), minimum, maximum, int(std::isfinite(benefit)), benefit, neutral, cursorThreshold, clamp, shake, speed, cursorMax,
                int(yawValid), yawSin, yawCos, static_cast<unsigned long long>(mInputSequence), int(whistleValid), whistleDown, whistleUp, int(active), tapRecall);
        }
        unsigned count = 0, validAlive = 0, invalidState = 0;
        if (pikiMgr) {
            Iterator it(pikiMgr);
            CI_LOOP(it) {
                fixture_require(count < 4096, "recruitment iterator ceiling");
                Piki* p = static_cast<Piki*>(*it);
                const bool stateValid = p && p->getCurrState();
                const int state = stateValid ? p->getState() : -1;
                const int alive = stateValid ? int(p->isAlive()) : -1;
                if (!stateValid) ++invalidState;
                if (alive == 1) ++validAlive;
                int owner = -1;
                if (p && p->mNavi && naviMgr && p->mNavi == naviMgr->getNavi(0)) owner = 0;
                else if (p && p->mNavi && naviMgr && p->mNavi == naviMgr->getNavi(1)) owner = 1;
                std::printf("COOP_PROTOCOL_RECRUIT_PIKI schema=1 batch=%u sim_frame=%u role=%d index=%u present=%d state_valid=%d state=%d alive_valid=%d alive=%d "
                    "color=%d maturity=%d mode=%d owner=%d owner_null=%d player_id=%d xyz=%.3f,%.3f,%.3f is_callable=%d is_buried=%d is_kinoko=%d is_damaged=%d is_fired=%d beetle_block=%d whistle_pending=%d\n",
                    batch, frame, mLocalRole, count++, int(p != nullptr), int(stateValid), state, int(stateValid), alive,
                    p ? int(p->mColor) : -1, p ? p->mHappa : -1, p ? int(p->mMode) : -1, owner, p ? int(p->mNavi == nullptr) : -1,
                    p ? p->mPlayerId : -1, p ? p->mSRT.t.x : 0.f, p ? p->mSRT.t.y : 0.f, p ? p->mSRT.t.z : 0.f,
                    stateValid ? int(p->mIsCallable) : -1, stateValid ? int(p->isBuried()) : -1,
                    stateValid ? int(p->isKinoko()) : -1, stateValid ? int(p->isDamaged()) : -1,
                    stateValid ? int(p->isFired()) : -1, stateValid ? int(pc_p2_fuefuki_follower_blocks_recruit(p)) : -1,
                    stateValid ? int(p->mIsWhistlePending) : -1);
            }
        }
        std::printf("COOP_PROTOCOL_RECRUIT_END schema=1 batch=%u sim_frame=%u role=%d captain_count=2 iterator_count=%u emitted_count=%u valid_alive_count=%u invalid_state_count=%u\n",
            batch, frame, mLocalRole, count, count, validAlive, invalidState);
        std::fflush(nullptr);
    }

    void input() {
        const bool generations = !mInputDirectory.empty();
        if (generations) {
            char bytes[256];
            unsigned count = 0;
            PcCoopGenerationInfo info = {0, 0, 0};
            const PcCoopSnapshotResult snapshot = pc_coop_fixture_generation_snapshot(
                mInputDirectory.c_str(), bytes, sizeof(bytes), &count, &info);
            unsigned long long now = 0;
            fixture_require(pc_coop_fixture_input_clock(&now), "generation clock unsupported");
            if (snapshot != PC_COOP_SNAPSHOT_MISSING && snapshot != PC_COOP_SNAPSHOT_OK) {
                std::printf("COOP_PROTOCOL_GENERATION_READ_ERROR role=%d result=%d os_error=%u close_error=%u sequence=%llu\n",
                    mLocalRole, int(snapshot), info.os_error, info.close_error, info.sequence);
                fixture_require(false, "generation snapshot refusal");
            }
            if (snapshot == PC_COOP_SNAPSHOT_MISSING && info.os_error) {
                std::printf("COOP_PROTOCOL_GENERATION_MISSING role=%d os_error=%u sequence=%llu\n",
                    mLocalRole, info.os_error, info.sequence);
            }
            PcCoopGenerationDecision decision = PC_COOP_GENERATION_OLDER;
            if (snapshot == PC_COOP_SNAPSHOT_OK) {
                // SDK-isolated parser/freshness logic is standalone-testable.
                decision = pc_coop_fixture_generation_accept(&mGenerationState, bytes, count,
                    info.sequence, now, 1u << SDL_CONTROLLER_BUTTON_MAX);
                fixture_require(decision != PC_COOP_GENERATION_INVALID, "generation canonical/order/clock refusal");
            }
            int axes[4] = {};
            fixture_require(pc_coop_fixture_generation_effective(&mGenerationState, now, &mButtons, axes),
                "generation reader clock regressed");
            for (unsigned i = 0; i < 4; ++i) mAxes[i] = axes[i];
            mInputSequence = mGenerationState.sequence;
            const bool fresh = mGenerationState.sequence && now >= mGenerationState.published_ms
                && now - mGenerationState.published_ms <= 500;
            if (decision == PC_COOP_GENERATION_NEW_FRESH || decision == PC_COOP_GENERATION_NEW_EXPIRED
                || (mFrames % 15 == 0 && (!mStepArmed || mGenerationDiagnosticFrame != mFrames))) {
                mGenerationDiagnosticFrame = mFrames;
                std::printf("COOP_PROTOCOL_GENERATION_READ role=%d observed_sequence=%llu consumed_sequence=%llu decision=%d published_tick=%llu reader_tick=%llu expired=%d effective_buttons=%u attempted_input_only=1 applied_PAD_proven=0\n",
                    mLocalRole, info.sequence, static_cast<unsigned long long>(mInputSequence), int(decision),
                    mGenerationState.published_ms, now, int(!fresh), mButtons);
                std::fflush(nullptr);
            }
        }
        if (!mInputPath.empty()) {
            std::string text;
            if (command_snapshot(mInputPath, text)) {
                fixture_require(text.size() < 256, "SDL input command length");
                std::istringstream stream(text);
                std::string tag, end, extra;
                unsigned long long sequence = 0;
                unsigned buttons = 0;
                int x = 0, y = 0, cx = 0, cy = 0;
                fixture_require(bool(stream >> tag >> sequence >> buttons >> x >> y >> cx >> cy >> end)
                    && tag == "SDL1" && end == "END" && !(stream >> extra), "SDL input grammar");
                fixture_require(sequence > 0 && sequence >= mInputSequence
                    && buttons < (1u << SDL_CONTROLLER_BUTTON_MAX)
                    && x >= -32768 && x <= 32767 && y >= -32768 && y <= 32767
                    && cx >= -32768 && cx <= 32767 && cy >= -32768 && cy <= 32767,
                    "SDL input bounds/order");
                const std::string canonical = "SDL1 " + std::to_string(sequence) + " "
                    + std::to_string(buttons) + " " + std::to_string(x) + " " + std::to_string(y)
                    + " " + std::to_string(cx) + " " + std::to_string(cy) + " END\n";
                fixture_require(text == canonical, "SDL input canonical ASCII/LF");
                if (sequence > mInputSequence) {
                    mInputSequence = sequence; mButtons = buttons;
                    mAxes[0] = x; mAxes[1] = y; mAxes[2] = cx; mAxes[3] = cy;
                    mLastInput = SDL_GetTicks64();
                }
            }
        }
        // A stopped orchestrator cannot leave a held movement/throw command.
        if (!generations && (!mLastInput || SDL_GetTicks64() - mLastInput > 500)) {
            mButtons = 0;
            for (int& axis : mAxes) axis = 0;
        }
        pc_window_input_assign(mLocalRole, PC_INPUT_DEV_GAMEPAD, SDL_JoystickInstanceID(mPad));
        pc_window_input_assign(1 - mLocalRole, PC_INPUT_DEV_NONE, -1);
        for (int b = 0; b < SDL_CONTROLLER_BUTTON_MAX; ++b)
            SDL_JoystickSetVirtualButton(mPad, b, (mButtons >> b) & 1u);
        SDL_JoystickSetVirtualAxis(mPad, SDL_CONTROLLER_AXIS_LEFTX, Sint16(mAxes[0]));
        SDL_JoystickSetVirtualAxis(mPad, SDL_CONTROLLER_AXIS_LEFTY, Sint16(mAxes[1]));
        SDL_JoystickSetVirtualAxis(mPad, SDL_CONTROLLER_AXIS_RIGHTX, Sint16(mAxes[2]));
        SDL_JoystickSetVirtualAxis(mPad, SDL_CONTROLLER_AXIS_RIGHTY, Sint16(mAxes[3]));
        SDL_JoystickUpdate();
    }
public:
    CoopProtocolFixtureApp() : PlugPikiApp(), mStarted(SDL_GetTicks64()) {
        const PcNetplayLaunch& launch = pc_netplay_launch_setup();
        fixture_require(launch.active && launch.externalState, "requires actual ICE external-state launcher");
        pc_window_set_display_mode(PC_WINDOW_FULLSCREEN_WINDOWED);
        pc_window_set_window_size(960, 540);
        pc_window_center();
        SDL_Window* window = SDL_GL_GetCurrentWindow();
        fixture_require(window != nullptr, "actual SDL GL window");
        int width = 0, height = 0, x = 0, y = 0;
        SDL_GetWindowSize(window, &width, &height);
        SDL_GetWindowPosition(window, &x, &y);
        SDL_Rect bounds;
        const int display = SDL_GetWindowDisplayIndex(window);
        fixture_require(display >= 0 && SDL_GetDisplayBounds(display, &bounds) == 0, "actual display bounds");
        const bool centered = std::abs(x - (bounds.x + (bounds.w - width) / 2)) <= 2
                           && std::abs(y - (bounds.y + (bounds.h - height) / 2)) <= 2;
        const bool windowed = !(SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN);
        fixture_require(width == 960 && height == 540 && centered && windowed, "actual SDL window geometry");
        std::printf("COOP_PROTOCOL_FIXTURE_WINDOW measured=1 width=%d height=%d centered=%d windowed=%d x=%d y=%d display=%d bounds=%d,%d,%d,%d hidden=%d\n",
                    width, height, int(centered), int(windowed), x, y, display,
                    bounds.x, bounds.y, bounds.w, bounds.h, int(bool(SDL_GetWindowFlags(window) & SDL_WINDOW_HIDDEN)));
        mLocalRole = launch.isHost ? 0 : 1;
        if (const char* path = std::getenv("PIKMIN_COOP_FIXTURE_INPUT")) mInputPath = path;
        if (const char* path = std::getenv("PIKMIN_COOP_FIXTURE_INPUT_DIR")) mInputDirectory = path;
        if (const char* path = std::getenv("PIKMIN_COOP_FIXTURE_STEP_DIR")) mStepDirectory = path;
        if (const char* epoch = std::getenv("PIKMIN_COOP_FIXTURE_STEP_EPOCH")) mStepEpoch = epoch;
        fixture_require(mStepDirectory.empty() == mStepEpoch.empty(), "step directory and epoch paired");
        fixture_require(mStepDirectory.empty() || (!mInputDirectory.empty() && mInputPath.empty()
            && mStepDirectory != mInputDirectory && pc_coop_step_epoch(mStepEpoch)), "step generation transport and epoch required");
        gFixtureQueue.enabled = !mStepDirectory.empty();
        gFixtureQueue.role = mLocalRole;
        fixture_require(mInputPath.empty() || mInputDirectory.empty(), "fixture input transports mutually exclusive");
        if (!mInputDirectory.empty()) {
            unsigned long long tick = 0;
            fixture_require(pc_coop_fixture_input_clock(&tick), "Windows generation input clock required");
        }
        // Production post-settings/ICE setup has already completed. This device
        // feeds the local SDL polling path; no source-input script is used.
        const int device = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,
            SDL_CONTROLLER_AXIS_MAX, SDL_CONTROLLER_BUTTON_MAX, 0);
        fixture_require(device >= 0, "SDL virtual controller creation");
        char guid[64];
        SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(device), guid, sizeof(guid));
        const std::string mapping = std::string(guid) +
            ",Protocol acceptance local pad,a:b0,b:b1,x:b2,y:b3,back:b4,guide:b5,"
            "start:b6,leftstick:b7,rightstick:b8,leftshoulder:b9,rightshoulder:b10,"
            "dpup:b11,dpdown:b12,dpleft:b13,dpright:b14,leftx:a0,lefty:a1,"
            "rightx:a2,righty:a3,lefttrigger:a4,righttrigger:a5,";
        fixture_require(SDL_GameControllerAddMapping(mapping.c_str()) >= 0, "SDL mapping");
        mPad = SDL_JoystickOpen(device);
        fixture_require(mPad != nullptr, "SDL virtual controller open");
        pc_window_input_assign(mLocalRole, PC_INPUT_DEV_GAMEPAD, SDL_JoystickInstanceID(mPad));
        pc_window_input_assign(1 - mLocalRole, PC_INPUT_DEV_NONE, -1);
        std::printf("COOP_PROTOCOL_FIXTURE_INPUT role=%d actual_SDL=1 source_script=0 command_file=%s command_dir=%s input_protocol=%d\n",
                    mLocalRole, mInputPath.c_str(), mInputDirectory.c_str(), mInputDirectory.empty() ? 1 : 2);
    }

    int idle() override {
        // Native simulation always runs first. Guard precedes readiness,
        // pause/movie and manager early returns. Check both initialized captains.
        if (!mStepDirectory.empty() && gameflow.mMoviePlayer && gameflow.mMoviePlayer->mIsActive) mSawStartupMovie = true;
        PADStatus applied[2] = {pc_netplay_pad_status()[0], pc_netplay_pad_status()[1]};
        const unsigned appliedFrame = pc_netplay_current_frame();
        const unsigned yaw[2] = {pc_input_log_yaw_raw(0), pc_input_log_yaw_raw(1)};
        const unsigned flags[2] = {pc_input_log_yaw_flags(0), pc_input_log_yaw_flags(1)};
        const int yawValid[2] = {int(pc_input_log_yaw_valid(0)), int(pc_input_log_yaw_valid(1))};
        const int result = PlugPikiApp::idle();
        ++mFrames;
        for (int i = 0; i < 2; ++i) {
            Navi* n = naviMgr ? naviMgr->getNavi(i) : nullptr;
            if (n && n->getCurrState()) mCaptainInitialized[i] = true;
            if (!mCaptainInitialized[i]) continue;
            // Explicit guard-negative instrumentation only. Positive launch
            // scrubs this variable. The real initialized engine captain is
            // made down and immediately evaluated by the ordinary guard.
            const char* force = std::getenv("PIKMIN_COOP_FIXTURE_FORCE_DOWN_CAPTAIN");
            if (n && n->getCurrState() && force && !mForcedDownDelivered && force[0] == char('0' + i) && force[1] == '\0') {
                n->mHealth = 0.f;
                mForcedDownDelivered = true;
                std::printf("COOP_PROTOCOL_GUARD_NEGATIVE actual_initialized_captain=%d frame=%u\n", i, mFrames);
            }
            const bool missing = !naviMgr || !n || !n->getCurrState();
            const bool dead = missing || naviMgr->isNaviDead(n)
                || n->getCurrState()->getID() == NAVISTATE_Dead;
            p2_fixture_require_captain(GameStat::orimaDead != 0, dead,
                n ? n->mHealth : 0.f, int(mFrames));
        }
        fixture_require(SDL_GetTicks64() - mStarted < 60000, "60 second local ceiling");
        fixture_require(mFrames < 5000, "frame ceiling");
        if (!mStepDirectory.empty() && !mStepArmed && stepReady()) {
            mStepArmed = true;
            gFixtureQueue.armed = true;
            std::printf("COOP_PROTOCOL_STEP_ARM epoch=%s role=%d sim_frame=%u saw_startup_movie=1 post_movie=1 healthy_walk=1 live_red=20 stored=20\n",
                mStepEpoch.c_str(), mLocalRole, appliedFrame);
        }
        if (mStepArmed) {
            fixture_require(pc_netplay_current_frame() == appliedFrame, "one authoritative step inside idle");
            fixture_require(gFixtureQueue.complete(appliedFrame), "actual submit/applied/future queue coverage refusal");
            std::printf("COOP_PROTOCOL_STEP_QUEUE epoch=%s role=%d sim_frame=%u next_land=%llu submit_serial=%llu last_submit=%llu delay=%u maximum=%u adaptive=%d hold=%d speculative=%d rand_neutral=%d future_neutral=%d applied=",
                mStepEpoch.c_str(), mLocalRole, appliedFrame, gFixtureQueue.next_land, gFixtureQueue.submit_serial, gFixtureQueue.last_submit,
                gFixtureQueue.delay, gFixtureQueue.maximum, int(gFixtureQueue.adaptive), int(gFixtureQueue.hold),
                int(gFixtureQueue.speculative), int(gFixtureQueue.rand_neutral), int(gFixtureQueue.future_neutral()));
            fixture_wire_hex(gFixtureQueue.applied, 32);
            std::printf(" current_submission=");
            fixture_wire_hex(gFixtureQueue.slot(appliedFrame)->wire, 16);
            std::printf(" pending=");
            if (gFixtureQueue.next_land == appliedFrame + 1) std::printf("none");
            for (unsigned long long f = appliedFrame + 1; f < gFixtureQueue.next_land; ++f) {
                if (f != appliedFrame + 1) std::printf(",");
                std::printf("%llu:", f); fixture_wire_hex(gFixtureQueue.slot(f)->wire, 16);
            }
            std::printf(" applied_generation_proven=0\n");
            std::printf("COOP_PROTOCOL_STEP_APPLIED epoch=%s role=%d sim_frame=%u phase=synced_pre_idle "
                "p0=%u,%d,%d,%d,%d,%u,%u,%u,%u,%d p1=%u,%d,%d,%d,%d,%u,%u,%u,%u,%d yaw=%u,%u yaw_flags=%u,%u yaw_valid=%d,%d applied_generation_proven=0\n",
                mStepEpoch.c_str(), mLocalRole, appliedFrame,
                unsigned(applied[0].button), int(applied[0].stickX), int(applied[0].stickY), int(applied[0].substickX), int(applied[0].substickY),
                unsigned(applied[0].triggerLeft), unsigned(applied[0].triggerRight), unsigned(applied[0].analogA), unsigned(applied[0].analogB), int(applied[0].err),
                unsigned(applied[1].button), int(applied[1].stickX), int(applied[1].stickY), int(applied[1].substickX), int(applied[1].substickY),
                unsigned(applied[1].triggerLeft), unsigned(applied[1].triggerRight), unsigned(applied[1].analogA), unsigned(applied[1].analogB), int(applied[1].err),
                yaw[0], yaw[1], flags[0], flags[1], yawValid[0], yawValid[1]);
        }
        observeRecruitment();
        input();
        // UI and world counts are sampled on every authoritative engine frame,
        // including closing animation. Reading never refreshes their counters.
        for (int captain = 0; captain < 2; ++captain) {
            Navi* n = naviMgr ? naviMgr->getNavi(captain) : nullptr;
            zen::DrawContainer* menu = captain == 1 && containerWindow2 ? containerWindow2 : containerWindow;
            if (!n || !n->getCurrState() || !menu) continue;
            GoalItem* onion = n->mGoalItem;
            if (n->getCurrState()->getID() != NAVISTATE_Container && menu->getStatus() == zen::DrawContainer::STATE_Wait) continue;
            std::printf("COOP_PROTOCOL_ONION sim_frame=%u role=%d captain=%d state=%d menu=%d "
                "stored=%d available=%d pending_onion=%d pending_all=%d live=%d cap=%d "
                "display_stored=%d display_squad=%d menu_stored=%d menu_squad=%d menu_field=%d menu_limit=%d "
                "delta=%d displayed_delta=%d formation=%u onion_color=%d onion_xyz=%.3f,%.3f,%.3f\n",
                pc_netplay_current_frame(), mLocalRole, captain, n->getCurrState()->getID(), int(menu->getStatus()),
                onion ? onion->getTotalStorePikis() : -1,
                onion ? onion->getTotalStorePikis() - onion->mPikisToExit : -1,
                onion ? onion->mPikisToExit : -1, itemMgr ? itemMgr->getContainerExitCount() : -1,
                int(GameStat::mapPikis), int(AICONST.mMaxPikisOnField()),
                menu->getContainerPikiDisp(), menu->getMyPikiDisp(), ContainerObserver::stored(*menu),
                ContainerObserver::squad(*menu), ContainerObserver::field(*menu), ContainerObserver::limit(*menu),
                ContainerObserver::delta(*menu), ContainerObserver::displayedDelta(*menu),
                n->mPlateMgr ? PlateObserver::count(*n->mPlateMgr) : 0u,
                onion ? int(onion->mOnionColour) : -1,
                onion ? onion->mSRT.t.x : 0.f, onion ? onion->mSRT.t.y : 0.f, onion ? onion->mSRT.t.z : 0.f);
            std::fflush(nullptr);
        }
        if (mStepArmed || mFrames % 15 == 0) {
            // Read-only approach facts from the actual owner-filtered GoalItem,
            // including its real 'cont' sphere. No lookup assigns mGoalItem,
            // updates GameStat, enters UI, or changes collision/stock/world.
            for (int captain = 0; captain < 2; ++captain) {
                Navi* n = naviMgr ? naviMgr->getNavi(captain) : nullptr;
                if (!n || !n->getCurrState()) continue;
                const Vector3f naviCentre = n->getCentre();
                float yawSin = 0.f, yawCos = 1.f;
                const bool yawValid = pc_netplay_control_yaw(n->mNaviID, &yawSin, &yawCos);
                for (int color = 0; color < 3; ++color) {
                    GoalItem* goal = itemMgr ? itemMgr->pcGetContainer(color, n->mNaviID) : nullptr;
                    const bool pod = goal && pc_p2_preview_is_pod(goal);
                    CollPart* cont = goal && !pod && goal->mCollInfo && goal->mCollInfo->hasInfo()
                        ? goal->mCollInfo->getSphere('cont') : nullptr;
                    std::printf("COOP_PROTOCOL_GOAL sim_frame=%u role=%d captain=%d color=%d present=%d pod=%d cont_present=%d ordinary_candidate=%d "
                        "navi_center=%.3f,%.3f,%.3f yaw_valid=%d yaw_sin=%.7f yaw_cos=%.7f goal_xyz=%.3f,%.3f,%.3f cont_xyz=%.3f,%.3f,%.3f radius=%.3f navi_size=%.3f stored=%d "
                        "pending_onion=%d radar_unlocked=%d\n",
                        pc_netplay_current_frame(), mLocalRole, captain, color, int(goal != nullptr), int(pod), int(cont != nullptr), int(goal && !pod && cont),
                        naviCentre.x, naviCentre.y, naviCentre.z, int(yawValid), yawSin, yawCos,
                        goal ? goal->mSRT.t.x : 0.f, goal ? goal->mSRT.t.y : 0.f, goal ? goal->mSRT.t.z : 0.f,
                        cont ? cont->mCentre.x : 0.f, cont ? cont->mCentre.y : 0.f, cont ? cont->mCentre.z : 0.f,
                        cont ? cont->mRadius : 0.f, n->getSize(), goal ? goal->getTotalStorePikis() : -1,
                        goal ? goal->mPikisToExit : -1, playerState ? int(playerState->hasRadar()) : -1);
                }
            }
            int alive = 0;
            if (pikiMgr) { Iterator it(pikiMgr); CI_LOOP(it) { Piki* p = static_cast<Piki*>(*it); if (p && p->getCurrState() && p->isAlive()) ++alive; } }
            for (int captain = 0; captain < 2; ++captain) {
            Navi* p1 = naviMgr ? naviMgr->getNavi(captain) : nullptr;
            if (p1 && p1->getCurrState() && p1->controlCamera()) {
                const Vector3f& axis = p1->controlCamera()->mViewXAxis;
                std::printf("COOP_PROTOCOL_OBSERVE frame=%u sim_frame=%u role=%d captain=%d alive=%d state=%d hp=%.3f followers=%d "
                            "xyz=%.3f,%.3f,%.3f cursor=%.3f,%.3f,%.3f camera=%.5f,%.5f,%.5f input_sequence=%llu\n",
                            mFrames, pc_netplay_current_frame(), mLocalRole, captain, alive, p1->getCurrState()->getID(), p1->mHealth,
                            p1->getPlatePikis(), p1->mSRT.t.x, p1->mSRT.t.y, p1->mSRT.t.z,
                            p1->mCursorWorldPos.x, p1->mCursorWorldPos.y, p1->mCursorWorldPos.z,
                            axis.x, axis.y, axis.z, static_cast<unsigned long long>(mInputSequence));
                std::fflush(nullptr);
            }
            }
            pc_randstate::PcRandState inventory;
            if (pc_randomizer_get_net_state(&inventory)) {
                std::uint8_t wire[pc_randstate::kStateBytes];
                fixture_require(pc_randstate::encode(inventory, wire) == sizeof(wire), "read-only inventory encoding");
                std::printf("COOP_PROTOCOL_INVENTORY sim_frame=%u role=%d hash=%016llx wire=",
                    pc_netplay_current_frame(), mLocalRole,
                    static_cast<unsigned long long>(pc_randomizer_hash()));
                for (std::uint8_t byte : wire) std::printf("%02x", unsigned(byte));
                std::printf("\n");
            }
            if (pikiMgr) {
                int index = 0; Iterator it(pikiMgr);
                CI_LOOP(it) {
                    Piki* p = static_cast<Piki*>(*it);
                    int owner = -1;
                    if (p && naviMgr && p->mNavi == naviMgr->getNavi(0)) owner = 0;
                    else if (p && naviMgr && p->mNavi == naviMgr->getNavi(1)) owner = 1;
                    std::printf("COOP_PROTOCOL_PIKI sim_frame=%u role=%d index=%d alive=%d color=%u maturity=%d owner=%d xyz=%.3f,%.3f,%.3f\n",
                        pc_netplay_current_frame(), mLocalRole, index++, p && p->getCurrState() ? int(p->isAlive()) : -1,
                        p ? unsigned(p->mColor) : 0u, p ? p->mHappa : -1, owner, p ? p->mSRT.t.x : 0.f, p ? p->mSRT.t.y : 0.f, p ? p->mSRT.t.z : 0.f);
                }
            }
            std::fflush(nullptr);
        }
        waitStep();
        return result;
    }
};
}

// App.h was included above, so this substitution only changes the production
// new-app expression. There is one main and one copy of production GPU exports.
// The harness sets window/audio/background environment before process startup.
#define PlugPikiApp CoopProtocolFixtureApp
#include "../pc_port/pc_main.cpp"
#undef PlugPikiApp
