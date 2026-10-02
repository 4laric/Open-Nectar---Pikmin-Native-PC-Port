// Standalone helper/clock/parser regression, no engine/game/SDL runtime.
// Invoke after owner admission in an exclusive private build environment.
#include "p2_coop_fixture_input_snapshot.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#endif

static void require(bool ok, const char* reason) {
    if (!ok) { std::fprintf(stderr, "FAIL %s\n", reason); std::exit(1); }
}
static PcCoopGenerationDecision offer(PcCoopGenerationState& state,
    const char* text, unsigned long long seq, unsigned long long now) {
    return pc_coop_fixture_generation_accept(&state, text, unsigned(std::strlen(text)), seq, now, 1u << 21);
}
static void effective(PcCoopGenerationState& state, unsigned long long now, unsigned expected) {
    unsigned buttons = 99; int axes[4] = {};
    require(pc_coop_fixture_generation_effective(&state, now, &buttons, axes), "effective clock");
    require(buttons == expected, "effective buttons");
    if (!expected) for (int axis : axes) require(!axis, "expired/neutral axes");
}

int main(int argc, char** argv) {
    PcCoopGenerationState state = {};
    require(offer(state, "SDL2 1 100 2 1100 0 0 0 END\n", 1, 100) == PC_COOP_GENERATION_NEW_FRESH, "first fresh");
    effective(state, 600, 2);
    require(offer(state, "SDL2 1 100 2 1100 0 0 0 END\n", 1, 601) == PC_COOP_GENERATION_DUPLICATE, "duplicate");
    effective(state, 601, 0);
    require(state.published_ms == 100, "duplicate no refresh");
    require(offer(state, "SDL2 1 100 0 0 0 0 0 END\n", 1, 602) == PC_COOP_GENERATION_INVALID, "mutated duplicate refused");
    require(offer(state, "SDL2 2 100 2 0 0 0 0 END\n", 2, 700) == PC_COOP_GENERATION_NEW_EXPIRED, "stale unread consumed");
    effective(state, 700, 0);
    require(offer(state, "SDL2 1 100 2 1100 0 0 0 END\n", 1, 701) == PC_COOP_GENERATION_OLDER, "older ignored");
    effective(state, 701, 0);
    require(offer(state, "SDL2 3 702 2 1 2 3 4 END\n", 3, 702) == PC_COOP_GENERATION_NEW_FRESH, "fresh held heartbeat");
    effective(state, 702, 2);
    require(offer(state, "SDL2 4 703 0 0 0 0 0 END\n", 4, 703) == PC_COOP_GENERATION_NEW_FRESH, "fresh neutral");
    effective(state, 703, 0);
    require(offer(state, "SDL2 5 704 0 0 0 0 0 END\n", 6, 704) == PC_COOP_GENERATION_INVALID, "name mismatch");
    require(offer(state, "SDL2 5 705 0 0 0 0 0 END\n", 5, 704) == PC_COOP_GENERATION_INVALID, "future tick");
    require(offer(state, "SDL2 5 702 0 0 0 0 0 END\n", 5, 704) == PC_COOP_GENERATION_INVALID, "publication regression");
    require(offer(state, "SDL2 5 704 0 32768 0 0 0 END\n", 5, 704) == PC_COOP_GENERATION_INVALID, "axis bounds");
    require(offer(state, "SDL2 5 704 2097152 0 0 0 0 END\n", 5, 704) == PC_COOP_GENERATION_INVALID, "button bounds");
    require(offer(state, "SDL2 5 704 0 0 0 0 0", 5, 704) == PC_COOP_GENERATION_INVALID, "partial input");
    require(offer(state, "SDL2 05 704 0 0 0 0 0 END\n", 5, 704) == PC_COOP_GENERATION_INVALID, "noncanonical input");
    require(offer(state, "SDL2 5 704 0 0 0 0 0 END\r\n", 5, 704) == PC_COOP_GENERATION_INVALID, "CRLF rejected");
    char oversized[256] = {};
    require(pc_coop_fixture_generation_accept(&state, oversized, 256, 5, 704, 1u<<21) == PC_COOP_GENERATION_INVALID, "capacity refusal");
    unsigned buttons = 0; int axes[4] = {};
    require(!pc_coop_fixture_generation_effective(&state, 702, &buttons, axes), "reader clock regression refused");
#if defined(_WIN32)
    require(argc == 2, "fresh owned output directory argument");
    const std::string directory(argv[1]);
    require(directory.find("output") != std::string::npos, "private output only");
    require(CreateDirectoryA(directory.c_str(), nullptr), "exclusive fresh test directory");
    unsigned long long now = 0;
    require(pc_coop_fixture_input_clock(&now), "Windows boot clock");
    char bytes[256]; unsigned count = 99; PcCoopGenerationInfo info = {};
    require(pc_coop_fixture_generation_snapshot(directory.c_str(), bytes, 256, &count, &info) == PC_COOP_SNAPSHOT_MISSING, "empty queue missing");
    require(count == 0 && !info.sequence, "missing queue does not fabricate generation");
    const std::string pending = directory + "/g00000000000000000001.pending";
    HANDLE file = CreateFileA(pending.c_str(), GENERIC_WRITE, 7, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    require(file != INVALID_HANDLE_VALUE, "exclusive partial pending");
    DWORD written = 0;
    require(WriteFile(file, "SDL2 1", 6, &written, nullptr) && written == 6, "partial pending bytes");
    require(CloseHandle(file), "pending creator close");
    require(pc_coop_fixture_generation_snapshot(directory.c_str(), bytes, 256, &count, &info) == PC_COOP_SNAPSHOT_MISSING, "partial pending invisible");
    const std::string ready = directory + "/g00000000000000000002.sdl";
    const std::string command = "SDL2 2 " + std::to_string(now) + " 0 0 0 0 0 END\n";
    file = CreateFileA(ready.c_str(), GENERIC_WRITE, 7, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    require(file != INVALID_HANDLE_VALUE, "exclusive owned ready test fixture");
    require(WriteFile(file, command.data(), DWORD(command.size()), &written, nullptr) && written == command.size(), "complete synthetic ready bytes");
    require(CloseHandle(file), "ready creator close");
    require(pc_coop_fixture_generation_snapshot(directory.c_str(), bytes, 256, &count, &info) == PC_COOP_SNAPSHOT_OK, "ready snapshot");
    require(info.sequence == 2 && count == command.size() && !std::memcmp(bytes, command.data(), count), "snapshot raw identity");
    const std::string wrong = directory + "/unknown.sdl";
    file = CreateFileA(wrong.c_str(), GENERIC_WRITE, 7, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    require(file != INVALID_HANDLE_VALUE && CloseHandle(file), "unknown owned entry");
    require(pc_coop_fixture_generation_snapshot(directory.c_str(), bytes, 256, &count, &info) == PC_COOP_SNAPSHOT_INVALID_ENTRY, "unknown entry refusal");
#else
    (void)argc; (void)argv;
    unsigned long long now = 99;
    require(!pc_coop_fixture_input_clock(&now), "non-Windows queue explicitly unsupported");
#endif
    std::puts("PASS standalone generation helper/parser only; no native gameplay acceptance");
    return 0;
}
