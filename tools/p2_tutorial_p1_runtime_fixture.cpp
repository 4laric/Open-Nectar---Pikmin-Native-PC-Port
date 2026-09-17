// Tutorial P1 native runtime fixture (lane p2-overworld-tutorial-p1-native-runtime).
//
// Replacement-main harness that boots the REAL game-linked engine over the
// P1 Valley of Repose (course tutorial) run layout, enforces captain safety
// #632 on every idle tick, and checks each surface-session boundary against
// actual engine capability. Boundaries the engine cannot perform (overworld
// course boot, day advance, save serializer, receipt ledger, exit/reentry) are
// reported UNSUPPORTED and the run FAILS CLOSED: no PASS marker is ever emitted
// without genuine observation. Diagnostic engine facts are labelled as such and
// prove nothing about the surface session.
//
// Replacement-main convention mirrors tools/p2_forest_p1_runtime_fixture.cpp,
// tools/p2_kurage_runtime.cpp and tools/p2_cave_guarded_boot_fixture.cpp
// (scenario main instead of pc_main.cpp; 960x540 centred window;
// --experimental-pikmin2-room boot). Promotion to a first-class CMake target is
// a shared-owner follow-up; the lane runner links this TU against the private
// pikmin_pc graph without editing shared build files.
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
#include "pc_p2_preview.h"
#include "pc_window.h"
#include "settings/pc_settings.h"
#include "settings/pc_settings_p2d.h"
#include "teki.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

// Captain-safety guard (#632), vendored verbatim from
// scripts/p2_fixture_captain_guard.h (sha256
// d2f678c9eda75e151eb534077dff9e30ad36ae4796881d971bbd09945f3c3474);
// observation-only, equivalent tested guard. The canonical header is consumed
// read-only; this vendored copy exists because a replacement-main TU cannot
// include a Python-tree script header at native build time.
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
bool sForceCaptainDown = false;

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
            std::printf("FAIL TUTORIAL_P1_RUNTIME selftest row=%d orima=%d dead=%d hp=%.3f got=%d want=%d\n",
                        int(i), int(rows[i].orima), int(rows[i].dead), rows[i].hp,
                        int(down), int(rows[i].expectDown));
            std::fflush(stdout);
            return 1;
        }
    }
    std::printf("P2_TUTORIAL_P1_SELFTEST_PASS rows=%d\n", int(sizeof(rows) / sizeof(rows[0])));
    std::fflush(stdout);
    return 0;
}

class TutorialP1RuntimeApp final : public PlugPikiApp {
    int frames = 0, observed = 0, waitMarks = 0;
    bool factsLogged = false;
    int alivePikis() {
        int count = 0;
        Iterator it(pikiMgr);
        CI_LOOP(it) { Creature* p = *it; if (p && p->isAlive()) ++count; }
        return count;
    }
    void unsupported(const char* boundary, const char* reason) {
        std::printf("P2_TUTORIAL_P1_UNSUPPORTED boundary=%s reason=%s\n", boundary, reason);
        std::fflush(stdout);
    }
public:
    int idle() override {
        int result = PlugPikiApp::idle();
        if (++frames > 30000) {
            std::printf("FAIL TUTORIAL_P1_RUNTIME timeout observed=%d\n", observed);
            std::fflush(stdout);
            std::_Exit(2);
        }
        if (!naviMgr || !tekiMgr || !pikiMgr) {
            if (frames % 600 == 0) {
                std::printf("P2_TUTORIAL_P1_WAIT frames=%d navi_mgr=%d teki_mgr=%d piki_mgr=%d\n",
                            frames, int(naviMgr != nullptr), int(tekiMgr != nullptr),
                            int(pikiMgr != nullptr));
                std::fflush(stdout);
            }
            return result;
        }
        Navi* n = naviMgr->getNavi();
        if (!n) {
            if (frames % 600 == 0) {
                std::printf("P2_TUTORIAL_P1_WAIT frames=%d navi=0\n", frames);
                std::fflush(stdout);
            }
            return result;
        }
        if (sForceCaptainDown)
            p2_fixture_require_captain(true, true, 0.0f, observed);
        else
            p2_fixture_require_captain(GameStat::orimaDead, !n->isAlive(), n->mHealth, observed);
        if (gameflow.mMoviePlayer && gameflow.mMoviePlayer->mIsActive) {
            gameflow.mMoviePlayer->requestSkip();
            return result;
        }
        if (gameflow.mPauseAll || gameflow.mIsUIOverlayActive) return result;
        ++observed;
        if (!factsLogged && observed >= 120) {
            factsLogged = true;
            std::printf("P2_TUTORIAL_P1_ENGINE_FACT observed=%d squad_alive=%d diagnostic_only=1\n",
                        observed, alivePikis());
            std::fflush(stdout);
            unsupported("boot_tutorial_surface", "no-overworld-course-boot-in-port");
            unsupported("day_transition", "no-day-advance-api-in-port");
            unsupported("save_reload", "no-save-serializer-in-port");
            unsupported("receipt_replay", "no-receipt-ledger-in-port");
            unsupported("exit_reentry", "no-exit-reentry-path-in-port");
            std::printf("FAIL TUTORIAL_P1_RUNTIME boundaries_unobservable=5 observed=%d\n", observed);
            std::fflush(stdout);
            std::_Exit(1);
        }
        if (observed % 600 == 0 && waitMarks < 48) {
            ++waitMarks;
            std::printf("P2_TUTORIAL_P1_WAIT observed=%d squad_alive=%d\n", observed, alivePikis());
            std::fflush(stdout);
        }
        return result;
    }
};
} // namespace

int main(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--guard-self-test") sGuardSelfTest = true;
        if (std::string(argv[i]) == "--guard-negative-test") sGuardNegativeTest = true;
    }
    if (sGuardSelfTest) return guardSelfTest();
    if (sGuardNegativeTest) {
        p2_fixture_require_captain(true, true, 0.0f, 0);
        std::printf("FAIL TUTORIAL_P1_RUNTIME negative test did not trip\n");
        std::fflush(stdout);
        return 1;
    }
    const char* force = std::getenv("P2_TUTORIAL_P1_FORCE_CAPTAIN_DOWN");
    if (force && force[0] == '1' && force[1] == '\0') sForceCaptainDown = true;
    SDL_setenv("SDL_AUDIODRIVER", "dummy", 1);
    SDL_SetMainReady();
    pc_gpu_preference_apply();
    _putenv_s("PIKMIN_RANDOMIZER_TEST_BACKGROUND", "1");
    pc_bbft_init(argc, argv);
    if (!pc_pikipelago_room_preview()) {
        std::printf("FAIL TUTORIAL_P1_RUNTIME requires --experimental-pikmin2-room\n");
        std::fflush(stdout);
        return 3;
    }
    if (!pc_window_init("P2 Tutorial P1 runtime fixture", 960, 540)) return 3;
    pc_window_center();
    {
        SDL_Window* window = SDL_GL_GetCurrentWindow();
        int width = 0, height = 0, x = 0, y = 0;
        SDL_GetWindowSize(window, &width, &height);
        SDL_GetWindowPosition(window, &x, &y);
        SDL_Rect bounds{0, 0, 0, 0};
        SDL_GetDisplayBounds(SDL_GetWindowDisplayIndex(window), &bounds);
        const bool centered = std::abs(x - (bounds.x + (bounds.w - width) / 2)) <= 2
            && std::abs(y - (bounds.y + (bounds.h - height) / 2)) <= 2;
        std::printf("P2_TUTORIAL_P1_WINDOW size=%dx%d pos=%d,%d display=%dx%d centered=%d\n",
                    width, height, x, y, bounds.w, bounds.h, int(centered));
        std::fflush(stdout);
    }
    pc_settings_init();
    gsys->Initialise();
    pc_settings_p2d_init();
    nodeMgr = new NodeMgr();
    gsys->run(new TutorialP1RuntimeApp());
    return 0;
}
