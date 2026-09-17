 // Stage-select boot fixture for P2 ch_NARI_01kusachi (lane
 // challenge-stage-boot-native-hook, #675). Replacement-main TU built by
 // scripts/build_pikmin2_fixture.py against the private pikmin_pc graph;
 // no shared build files touched.
 //
 // What it proves (and nothing more): the engine parses
 // --experimental-challenge-stage <cave_id> (new flag in pc_bbft.cpp),
 // the run dir carries the #669 P2_CHALLENGE_STAGE_SELECT_1 record, the
 // embedded decode table resolves the flag key to the kusachi row, and a
 // 960x540 centred window boots. It never loads room geometry, actors,
 // saves, or seeds, and all six runtime gates stay UNTESTED.
 //
 // Markers: P2_CHALLENGE_STAGE_FLAG (engine flag parse),
 // P2_CHALLENGE_STAGE_SIDECAR / _TABLE / _RESOLVED (selection chain),
 // P2_CHALLENGE_STAGE_WINDOW (boot environment). Success ends with
 // "PASS CHALLENGE_STAGE_BOOT" and exit 0. Refusals exit 1 with reason;
 // guard trips exit 86. No PASS is ever emitted without observed evidence.
#include <SDL2/SDL.h>
#include <GL/gl.h>
#include "pc_bbft.h"
#include "pc_gpu_preference.h"
#include "pc_window.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>

// Defined in pc_port/pc_bbft.cpp (lane-owned edit); declared here so no
// shared header is touched.
extern const char* pc_p2_challenge_stage();

// Captain-safety guard (#632), vendored verbatim from
// scripts/p2_fixture_captain_guard.h; observation-only, equivalent tested
// guard. Exercised by --guard-self-test / --guard-negative-test without
// any engine boot.
inline bool p2_fixture_captain_down(bool orimaDead, bool deadState, float hp) {
    return orimaDead || deadState || !std::isfinite(hp) || hp <= 1.0f;
}
inline void p2_fixture_require_captain(bool orimaDead, bool deadState, float hp, int tick) {
    if (!p2_fixture_captain_down(orimaDead, deadState, hp)) return;
    std::printf("P2_FIXTURE_CAPTAIN_DOWN tick=%d hp=%.3f orima_dead=%d dead_state=%d outcome=BLOCKED\n",
                tick, hp, int(orimaDead), int(deadState));
    std::fflush(nullptr);
    std::_Exit(86); // interrupted observation, never a successful fixture exit
}

namespace {
// Decode table row transcribed from the #669 selector record, itself pinned
// to docs/PIKMIN_CONTENT_IMPORT_LANES.json + docs/PIKMIN2_CONTENT_INVENTORY.json
// (ch_NARI_01kusachi: ui_index 3, 1 floor, 180 s, 50 blue leaf, sprays 1/2,
// source sha b8d232f4...34bb8d85). Only this stage resolves here; anything
// else is refused, never defaulted.
struct StageRow {
    const char* caveId;
    int uiIndex;
    int floors;
    float floorSeconds;
    int rosterBlueLeaf;
    int bitterSprays;
    int spicySprays;
    const char* sourceSha;
};
const StageRow kTable[] = {
    {"ch_NARI_01kusachi", 3, 1, 180.0f, 50, 1, 2,
     "b8d232f417ce3fd4b2903571a1c53234e63dec49e127d5ef5b8ef3cc34bb8d85"},
};

const StageRow* selectRow(const char* caveId) {
    if (!caveId) return nullptr;
    for (size_t i = 0; i < sizeof(kTable) / sizeof(kTable[0]); ++i)
        if (!std::strcmp(kTable[i].caveId, caveId)) return &kTable[i];
    return nullptr;
}

void fail(const char* reason) {
    std::printf("P2_CHALLENGE_STAGE_BOOT_REFUSED reason=%s\n", reason);
    std::fflush(stdout);
    std::_Exit(1);
}

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
            std::printf("FAIL CHALLENGE_STAGE_BOOT selftest row=%d\n", int(i));
            std::fflush(stdout);
            return 1;
        }
    }
    std::printf("P2_CHALLENGE_STAGE_BOOT_SELFTEST_PASS rows=%d\n",
                int(sizeof(rows) / sizeof(rows[0])));
    std::fflush(stdout);
    return 0;
}

// Minimal P2_CHALLENGE_STAGE_SELECT_1 reader: magic + key/value shape only;
// semantic agreement is checked field-by-field below against the table.
bool readSidecar(const char* path, std::string& cave, int& ui, std::string& sha,
                 int& floors, float& seconds, int& blueLeaf, int& bitter, int& spicy) {
    std::ifstream in(path);
    std::string magic;
    if (!(in >> magic) || magic != "P2_CHALLENGE_STAGE_SELECT_1") return false;
    std::string word;
    if (!(in >> word) || word != "cave") return false;
    if (!(in >> cave >> word) || word != "ui_index") return false;
    if (!(in >> ui >> word) || word != "table_order") return false;
    int tableOrder = 0;
    if (!(in >> tableOrder >> word) || word != "floors") return false;
    if (!(in >> floors)) return false;
    (void)tableOrder;
    if (!(in >> word) || word != "source") return false;
    std::string sourcePath;
    if (!(in >> sourcePath >> sha)) return false;
    if (!(in >> word) || word != "timers") return false;
    if (!(in >> seconds)) return false;
    if (!(in >> word)) return false; // legacy
    float legacy = 0;
    if (!(in >> legacy)) return false;
    if (!(in >> word) || word != "sprays") return false;
    if (!(in >> word)) return false; // bitter
    if (!(in >> bitter)) return false;
    if (!(in >> word)) return false; // spicy
    if (!(in >> spicy)) return false;
    if (!(in >> word)) return false; // treasure_field
    int treasureField = 0;
    if (!(in >> treasureField)) return false;
    (void)treasureField;
    int total = 0;
    std::string line;
    std::getline(in, line);
    while (in >> word) {
        if (word != "roster") return false;
        int a = 0, b = 0, c = 0;
        if (!(in >> a >> b >> c)) return false;
        total += a + b + c;
    }
    blueLeaf = total;
    return true;
}
} // namespace

int main(int argc, char** argv) {
    bool selfTest = false, negativeTest = false;
    const char* flagStage = nullptr;
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--guard-self-test")) selfTest = true;
        if (!std::strcmp(argv[i], "--guard-negative-test") || !std::strcmp(argv[i], "--guard-negative")) {
            p2_fixture_require_captain(true, true, 0.0f, 0);
            std::printf("FAIL CHALLENGE_STAGE_BOOT negative test did not trip\n");
            std::fflush(stdout);
            return 1;
        }
        if (!std::strcmp(argv[i], "--experimental-challenge-stage") && i + 1 < argc) flagStage = argv[++i];
    }
    if (selfTest) return guardSelfTest();
    SDL_setenv("SDL_AUDIODRIVER", "dummy", 1);
    SDL_SetMainReady();
    pc_gpu_preference_apply();
    _putenv_s("PIKMIN_RANDOMIZER_TEST_BACKGROUND", "1");
    // Engine parses the new flag itself (pc_bbft.cpp); the recorded marker
    // below must agree with the argv copy this fixture also reads.
    pc_bbft_init(argc, argv);
    const char* engineStage = pc_p2_challenge_stage();
    if (!flagStage) fail("no-flag");
    if (!engineStage || std::strcmp(engineStage, flagStage) != 0) fail("flag-not-recorded");
    std::printf("P2_CHALLENGE_STAGE_ARGV cave=%s\n", flagStage);
    std::fflush(stdout);
    std::string cave;
    int ui = -1, floors = 0, blueLeaf = 0, bitter = -1, spicy = -1;
    float seconds = 0;
    std::string sha;
    if (!readSidecar("p2-challenge-stage-select.txt", cave, ui, sha, floors, seconds, blueLeaf, bitter, spicy))
        fail("bad-sidecar");
    std::printf("P2_CHALLENGE_STAGE_SIDECAR cave=%s ui_index=%d\n", cave.c_str(), ui);
    std::fflush(stdout);
    if (cave != flagStage) fail("flag-sidecar-mismatch");
    const StageRow* row = selectRow(cave.c_str());
    if (!row) fail("unknown-stage");
    std::printf("P2_CHALLENGE_STAGE_TABLE cave=%s ui_index=%d floors=%d\n",
                row->caveId, row->uiIndex, row->floors);
    std::fflush(stdout);
    if (ui != row->uiIndex || floors != row->floors || sha != row->sourceSha ||
        seconds != row->floorSeconds || blueLeaf != row->rosterBlueLeaf ||
        bitter != row->bitterSprays || spicy != row->spicySprays) fail("pin-mismatch");
    if (!pc_window_init("P2 Challenge stage boot fixture", 960, 540)) fail("window");
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
        std::printf("P2_CHALLENGE_STAGE_WINDOW size=%dx%d pos=%d,%d display=%dx%d centered=%d\n",
                    width, height, x, y, bounds.w, bounds.h, int(centered));
        std::fflush(stdout);
        if (width != 960 || height != 540 || !centered) fail("window-geometry");
    }
    // No per-tick guard call: this boot has no game world (no Navi to
    // observe); the vendored guard is proven by the self-test and negative
    // modes above, and documented for the future game-world consumer.
    std::printf("P2_CHALLENGE_STAGE_RESOLVED cave=%s ui_index=%d floors=%d\n",
                row->caveId, row->uiIndex, row->floors);
    std::fflush(stdout);
    std::puts("PASS CHALLENGE_STAGE_BOOT");
    std::fflush(stdout);
    return 0;
}