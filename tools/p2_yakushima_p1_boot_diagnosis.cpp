// Yakushima P1 headed-boot stall diagnosis (#717).
//
// Replacement-main diagnosis harness for the #150 stall: the headed boot
// completes window + engine/shader/jaudio init and then goes silent with no
// squad/gameplay (no abort, no captain-down). This TU does NOT fix anything
// and does NOT edit shared engine files; it instruments the fixture's own
// boot phases and, when a phase never completes, captures the MAIN THREAD's
// instruction pointer so the exact blocking function can be named from the
// binary's symbols.
//
// Phase markers: P2_YAKUSHIMA_P1_DIAG_PHASE phase=<name> progress=<n>.
// On timeout: P2_YAKUSHIMA_P1_DIAG_TIMEOUT phase=<name> progress=<n>
//   rip=0x... -> exit 2 (fail closed, never a PASS).
// On reaching the idle-running state: P2_YAKUSHIMA_P1_DIAG_IDLE_RUNNING.
//
// Captain safety (#632), vendored verbatim from
// scripts/p2_fixture_captain_guard.h (sha256
// d2f678c9eda75e151eb534077dff9e30ad36ae4796881d971bbd09945f3c3474): checked
// immediately after engine idle, before any observation. No blanket
// invincibility; parked captain; a trip exits BLOCKED (86).
#include <SDL2/SDL.h>
#include <GL/gl.h>
#include "App.h"
#include "Node.h"
#include "Graphics.h"
#include "GameCoreSection.h"
#include "Generator.h"
#include "Section.h"
#include "NaviMgr.h"
#include "Navi.h"
#include "Piki.h"
#include "PikiMgr.h"
#include "PikiState.h"
#include "Collision.h"
#include "Creature.h"
#include "MoviePlayer.h"
#include "GameStat.h"
#include "PlayerState.h"
#include "gameflow.h"
#include "system.h"
#include "pc_bbft.h"
#include "pc_gpu_preference.h"
#include "pc_gfx.h"
#include "pc_window.h"
#include "settings/pc_settings.h"
#include "settings/pc_settings_p2d.h"
#include "teki.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>

// NOTE: <windows.h> cannot be included here - it collides with the engine's
// `typedef u32 HWND` in AtxStream.h (documented in pc_p2_breadbug_contest_host.h).
// The watchdog therefore declares the four kernel32 entry points it needs and
// uses a raw CONTEXT buffer whose Rip/ContextFlags offsets were measured by a
// standalone probe (sizeof(CONTEXT)=1232, ContextFlags at 48, Rip at 248,
// CONTEXT_CONTROL=0x100001). No engine type and no <windows.h> leaks in.
extern "C" {
__declspec(dllimport) unsigned long __stdcall GetCurrentThreadId(void);
__declspec(dllimport) void* __stdcall OpenThread(unsigned long, int, unsigned long);
__declspec(dllimport) int __stdcall CloseHandle(void*);
__declspec(dllimport) unsigned long __stdcall SuspendThread(void*);
__declspec(dllimport) unsigned long __stdcall ResumeThread(void*);
__declspec(dllimport) int __stdcall GetThreadContext(void*, void*);
}
namespace {
constexpr int kContextSize = 1232;
constexpr int kContextFlagsOffset = 48;
constexpr int kRipOffset = 248;
constexpr unsigned long kContextControl = 0x100001UL;
struct RawContext { unsigned char bytes[kContextSize]; };
}

// ---- captain guard (#632), vendored verbatim -------------------------------
inline bool p2_fixture_captain_down(bool orimaDead, bool deadState, float hp) {
    return orimaDead || deadState || !std::isfinite(hp) || hp <= 1.0f;
}
inline void p2_fixture_require_captain(bool orimaDead, bool deadState, float hp, int tick) {
    if (!p2_fixture_captain_down(orimaDead, deadState, hp)) return;
    std::printf("P2_FIXTURE_CAPTAIN_DOWN tick=%d hp=%.3f orima_dead=%d dead_state=%d outcome=BLOCKED\n",
                tick, hp, int(orimaDead), int(deadState));
    std::fflush(nullptr);
    std::_Exit(86);
}

namespace {
bool sGuardSelfTest = false;
bool sGuardNegativeTest = false;
int sWatchdogSeconds = 90;
int sIdleTicks = 0;
int sTargetIdle = 300;

std::atomic<int> sProgress{0};
std::atomic<const char*> sPhase{"pre-main"};
std::atomic<unsigned long> sMainThreadId{0};

int guardSelfTest() {
    struct Row { bool orima; bool dead; float hp; bool expectDown; };
    const Row rows[] = {
        {false, false, 100.0f, false},
        {false, false, 1.5f, false},
        {false, false, 1.0f, true},
        {false, false, 0.0f, true},
        {false, true, 100.0f, true},
        {true, false, 100.0f, true},
        {true, true, 0.0f, true},
    };
    for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); ++i) {
        const bool down = p2_fixture_captain_down(rows[i].orima, rows[i].dead, rows[i].hp);
        if (down != rows[i].expectDown) {
            std::printf("FAIL YAKUSHIMA_P1_BOOT_DIAG selftest row=%d\n", int(i));
            std::fflush(stdout);
            return 1;
        }
    }
    std::printf("P2_YAKUSHIMA_P1_DIAG_SELFTEST_PASS rows=%d\n", int(sizeof(rows) / sizeof(rows[0])));
    std::fflush(stdout);
    return 0;
}

void mark(const char* phase) {
    sPhase.store(phase);
    const int n = sProgress.fetch_add(1) + 1;
    std::printf("P2_YAKUSHIMA_P1_DIAG_PHASE phase=%s progress=%d\n", phase, n);
    std::fflush(stdout);
}

// Map a return address to the nearest preceding exported symbol in the exe.
// The python helper does the offline nm-based mapping; here we only record it.
void watchdog() {
    const int seconds = sWatchdogSeconds;
    for (int i = 0; i < seconds * 10; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (sProgress.load() >= 1000000) return; // retired by main
    }
    RawContext ctx{};
    *reinterpret_cast<unsigned long*>(ctx.bytes + kContextFlagsOffset) = kContextControl;
    const char* phase = sPhase.load();
    const int progress = sProgress.load();
    // THREAD_SUSPEND_RESUME (0x0002) | THREAD_GET_CONTEXT (0x0008) on the REAL
    // main-thread handle opened by id (GetCurrentThread's pseudo-handle is only
    // valid in the owning thread, so it would suspend this watchdog instead).
    void* mainThread = OpenThread(0x0002 | 0x0008, 0, sMainThreadId.load());
    if (mainThread != nullptr && SuspendThread(mainThread) != (unsigned long)-1) {
        if (GetThreadContext(mainThread, ctx.bytes)) {
            const unsigned long long rip = *reinterpret_cast<const unsigned long long*>(
                ctx.bytes + kRipOffset);
            std::printf("P2_YAKUSHIMA_P1_DIAG_TIMEOUT phase=%s progress=%d rip=0x%llx\n",
                        phase, progress, rip);
        } else {
            std::printf("P2_YAKUSHIMA_P1_DIAG_TIMEOUT phase=%s progress=%d rip=unavailable\n",
                        phase, progress);
        }
        ResumeThread(mainThread);
        CloseHandle(mainThread);
    } else {
        std::printf("P2_YAKUSHIMA_P1_DIAG_TIMEOUT phase=%s progress=%d rip=no-handle\n",
                    phase, progress);
    }
    std::printf("FAIL YAKUSHIMA_P1_BOOT_DIAG stall phase=%s progress=%d\n", phase, progress);
    std::fflush(nullptr);
    std::_Exit(2); // fail closed; never a PASS
}

class BootDiagnosisApp final : public PlugPikiApp {
    bool sFirstIdle = false;
public:
    int idle() override {
        int result = PlugPikiApp::idle();
        if (!sFirstIdle) {
            sFirstIdle = true;
            mark("first-idle");
        }
        if (!naviMgr || !tekiMgr || !pikiMgr) {
            if ((sIdleTicks % 120) == 0) {
                std::printf("P2_YAKUSHIMA_P1_DIAG_WAIT phase=managers ticks=%d navi=%d teki=%d piki=%d\n",
                            sIdleTicks, int(naviMgr != nullptr), int(tekiMgr != nullptr),
                            int(pikiMgr != nullptr));
                std::fflush(stdout);
            }
            ++sIdleTicks;
            return result;
        }
        Navi* n = naviMgr->getNavi();
        if (!n) {
            ++sIdleTicks;
            return result;
        }
        // Guard FIRST: immediately after engine idle, before any observation.
        p2_fixture_require_captain(GameStat::orimaDead, !n->isAlive(), n->mHealth, sIdleTicks);
        if (gameflow.mMoviePlayer && gameflow.mMoviePlayer->mIsActive) {
            gameflow.mMoviePlayer->requestSkip();
            return result;
        }
        if (gameflow.mPauseAll || gameflow.mIsUIOverlayActive) return result;
        ++sIdleTicks;
        if (sIdleTicks == 1) mark("idle-squad-live");
        if (sIdleTicks >= sTargetIdle) {
            std::printf("P2_YAKUSHIMA_P1_DIAG_IDLE_RUNNING idle_ticks=%d\n", sIdleTicks);
            std::fflush(stdout);
            std::_Exit(0);
        }
        return result;
    }
};
} // namespace

int main(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--guard-self-test") sGuardSelfTest = true;
        else if (std::string(argv[i]) == "--guard-negative-test") sGuardNegativeTest = true;
        else if (std::string(argv[i]) == "--watchdog-seconds" && i + 1 < argc)
            sWatchdogSeconds = std::atoi(argv[++i]);
        else if (std::string(argv[i]) == "--target-idle" && i + 1 < argc)
            sTargetIdle = std::atoi(argv[++i]);
    }
    if (sGuardSelfTest) return guardSelfTest();
    if (sGuardNegativeTest) {
        p2_fixture_require_captain(true, true, 0.0f, 0);
        std::printf("FAIL YAKUSHIMA_P1_BOOT_DIAG negative test did not trip\n");
        std::fflush(stdout);
        return 1;
    }
    sMainThreadId.store(GetCurrentThreadId());
    std::thread(watchdog).detach();

    mark("main-entry");
    std::printf("P2_YAKUSHIMA_P1_DIAG_START watchdog_seconds=%d target_idle=%d\n",
                sWatchdogSeconds, sTargetIdle);
    std::fflush(stdout);

    SDL_setenv("SDL_AUDIODRIVER", "dummy", 1);
    SDL_SetMainReady();
    pc_gpu_preference_apply();
    _putenv_s("PIKMIN_RANDOMIZER_TEST_BACKGROUND", "1");

    mark("before-bbft-init");
    pc_bbft_init(argc, argv);
    mark("after-bbft-init");
    if (!pc_pikipelago_room_preview()) {
        std::printf("FAIL YAKUSHIMA_P1_BOOT_DIAG requires --experimental-pikmin2-room\n");
        std::fflush(stdout);
        return 3;
    }
    mark("before-window-init");
    if (!pc_window_init("P2 Yakushima P1 boot diagnosis", 960, 540)) return 3;
    pc_window_center();
    mark("after-window-init");
    {
        SDL_Window* window = SDL_GL_GetCurrentWindow();
        int width = 0, height = 0, x = 0, y = 0;
        SDL_GetWindowSize(window, &width, &height);
        SDL_GetWindowPosition(window, &x, &y);
        SDL_Rect bounds{0, 0, 0, 0};
        SDL_GetDisplayBounds(SDL_GetWindowDisplayIndex(window), &bounds);
        const bool centered = std::abs(x - (bounds.x + (bounds.w - width) / 2)) <= 2
            && std::abs(y - (bounds.y + (bounds.h - height) / 2)) <= 2;
        std::printf("P2_YAKUSHIMA_P1_DIAG_WINDOW size=%dx%d pos=%d,%d display=%dx%d centered=%d\n",
                    width, height, x, y, bounds.w, bounds.h, int(centered));
        std::fflush(stdout);
    }
    mark("before-settings-init");
    pc_settings_init();
    mark("before-gsys-initialise");
    gsys->Initialise();
    mark("after-gsys-initialise");
    pc_settings_p2d_init();
    mark("after-settings-p2d-init");
    nodeMgr = new NodeMgr();
    mark("before-gsys-run");
    gsys->run(new BootDiagnosisApp());
    mark("after-gsys-run");
    std::printf("P2_YAKUSHIMA_P1_DIAG_EXIT_RUN\n");
    std::fflush(stdout);
    return 0;
}
