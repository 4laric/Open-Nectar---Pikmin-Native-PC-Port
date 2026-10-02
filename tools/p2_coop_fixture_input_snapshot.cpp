#include "p2_coop_fixture_input_snapshot.h"
#include <cstring>
#include <string>
#include <sstream>
#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#else
#include <cstdio>
#endif

PcCoopSnapshotResult pc_coop_fixture_input_snapshot(const char* path,
    char* bytes, unsigned capacity, unsigned* count) {
    if (!path || !bytes || !count || !capacity) return PC_COOP_SNAPSHOT_IO_ERROR;
    *count = 0;
#if defined(_WIN32)
    HANDLE file = CreateFileA(path, GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return PC_COOP_SNAPSHOT_MISSING;
    DWORD size = 0;
    const BOOL read = ReadFile(file, bytes, capacity, &size, nullptr);
    const BOOL closed = CloseHandle(file);
    if (!read || !closed) return PC_COOP_SNAPSHOT_IO_ERROR;
    *count = size;
#else
    FILE* file = std::fopen(path, "rb");
    if (!file) return PC_COOP_SNAPSHOT_MISSING;
    const unsigned size = static_cast<unsigned>(std::fread(bytes, 1, capacity, file));
    const bool error = std::ferror(file) != 0;
    const bool closed = std::fclose(file) == 0;
    if (error || !closed) return PC_COOP_SNAPSHOT_IO_ERROR;
    *count = size;
#endif
    return *count < capacity ? PC_COOP_SNAPSHOT_OK : PC_COOP_SNAPSHOT_TOO_LONG;
}

bool pc_coop_fixture_input_clock(unsigned long long* milliseconds) {
    if (!milliseconds) return false;
#if defined(_WIN32)
    *milliseconds = GetTickCount64();
    return true;
#else
    *milliseconds = 0;
    return false; // No silent SDL/Python clock-origin substitution.
#endif
}

PcCoopSnapshotResult pc_coop_fixture_generation_snapshot(const char* directory,
    char* bytes, unsigned capacity, unsigned* count, PcCoopGenerationInfo* info) {
    if (!directory || !*directory || !bytes || !capacity || !count || !info)
        return PC_COOP_SNAPSHOT_IO_ERROR;
    *count = 0;
    *info = {0, 0, 0};
#if defined(_WIN32)
    const std::string prefix = std::string(directory) + "/";
    WIN32_FIND_DATAA entry;
    HANDLE search = FindFirstFileA((prefix + "*").c_str(), &entry);
    if (search == INVALID_HANDLE_VALUE) {
        info->os_error = GetLastError();
        return info->os_error == ERROR_FILE_NOT_FOUND || info->os_error == ERROR_PATH_NOT_FOUND
            ? PC_COOP_SNAPSHOT_MISSING : PC_COOP_SNAPSHOT_IO_ERROR;
    }
    unsigned entries = 0;
    std::string latest;
    PcCoopSnapshotResult status = PC_COOP_SNAPSHOT_MISSING;
    for (;;) {
        const std::string name(entry.cFileName);
        if (name != "." && name != "..") {
            if (++entries > 4097) { status = PC_COOP_SNAPSHOT_GENERATION_LIMIT; break; }
            if (entry.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) {
                status = PC_COOP_SNAPSHOT_INVALID_ENTRY; break;
            }
            const bool pending = name.size() >= 8 && name.compare(name.size() - 8, 8, ".pending") == 0;
            if (!pending) {
                bool valid = name.size() == 25 && name[0] == 'g' && name.compare(21, 4, ".sdl") == 0;
                unsigned long long sequence = 0;
                for (unsigned i = 1; valid && i <= 20; ++i) {
                    if (name[i] < '0' || name[i] > '9') { valid = false; break; }
                    sequence = sequence * 10 + unsigned(name[i] - '0');
                    if (sequence > 4096) valid = false;
                }
                if (!valid || !sequence) { status = PC_COOP_SNAPSHOT_INVALID_ENTRY; break; }
                if (sequence > info->sequence) { info->sequence = sequence; latest = name; status = PC_COOP_SNAPSHOT_OK; }
            }
        }
        if (!FindNextFileA(search, &entry)) {
            const DWORD error = GetLastError();
            if (error != ERROR_NO_MORE_FILES) { info->os_error = error; status = PC_COOP_SNAPSHOT_IO_ERROR; }
            break;
        }
    }
    if (!FindClose(search)) {
        info->close_error = GetLastError();
        return status == PC_COOP_SNAPSHOT_OK || status == PC_COOP_SNAPSHOT_MISSING
            ? PC_COOP_SNAPSHOT_IO_ERROR : status;
    }
    if (status != PC_COOP_SNAPSHOT_OK) return status;
    HANDLE file = CreateFileA((prefix + latest).c_str(), GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        info->os_error = GetLastError();
        // New queue mode records non-absence failures as fatal I/O. The
        // legacy single-file API above preserves its existing classification.
        return info->os_error == ERROR_FILE_NOT_FOUND || info->os_error == ERROR_PATH_NOT_FOUND
            ? PC_COOP_SNAPSHOT_MISSING : PC_COOP_SNAPSHOT_IO_ERROR;
    }
    DWORD size = 0;
    const BOOL read = ReadFile(file, bytes, capacity, &size, nullptr);
    if (!read) info->os_error = GetLastError();
    const BOOL closed = CloseHandle(file);
    if (!closed) info->close_error = GetLastError();
    if (!read || !closed) return PC_COOP_SNAPSHOT_IO_ERROR;
    *count = size;
    return size < capacity ? PC_COOP_SNAPSHOT_OK : PC_COOP_SNAPSHOT_TOO_LONG;
#else
    return PC_COOP_SNAPSHOT_IO_ERROR; // Queue clock/transport not supported here.
#endif
}

PcCoopGenerationDecision pc_coop_fixture_generation_accept(PcCoopGenerationState* state,
    const char* bytes, unsigned count, unsigned long long expected_sequence,
    unsigned long long now_ms, unsigned button_limit) {
    if (!state || !bytes || !count || count >= 256 || !button_limit
        || (state->clock_seen && now_ms < state->last_reader_ms)) return PC_COOP_GENERATION_INVALID;
    const std::string text(bytes, count);
    std::istringstream input(text);
    std::string tag, end, extra;
    unsigned long long sequence = 0, published = 0;
    unsigned buttons = 0;
    int axes[4] = {};
    if (!(input >> tag >> sequence >> published >> buttons >> axes[0] >> axes[1] >> axes[2] >> axes[3] >> end)
        || tag != "SDL2" || end != "END" || (input >> extra)
        || !sequence || sequence > 4096 || sequence != expected_sequence
        || published > now_ms || buttons >= button_limit) return PC_COOP_GENERATION_INVALID;
    for (int value : axes) if (value < -32768 || value > 32767) return PC_COOP_GENERATION_INVALID;
    const std::string canonical = "SDL2 " + std::to_string(sequence) + " " + std::to_string(published)
        + " " + std::to_string(buttons) + " " + std::to_string(axes[0]) + " " + std::to_string(axes[1])
        + " " + std::to_string(axes[2]) + " " + std::to_string(axes[3]) + " END\n";
    if (text != canonical) return PC_COOP_GENERATION_INVALID;
    if (sequence < state->sequence) return PC_COOP_GENERATION_OLDER;
    if (sequence == state->sequence) {
        if (count != state->canonical_size || std::memcmp(bytes, state->canonical, count)) return PC_COOP_GENERATION_INVALID;
        return PC_COOP_GENERATION_DUPLICATE;
    }
    if (state->sequence && published < state->published_ms) return PC_COOP_GENERATION_INVALID;
    state->sequence = sequence; state->published_ms = published; state->buttons = buttons;
    for (unsigned i = 0; i < 4; ++i) state->axes[i] = axes[i];
    state->canonical_size = count; std::memcpy(state->canonical, bytes, count);
    return now_ms - published > 500 ? PC_COOP_GENERATION_NEW_EXPIRED : PC_COOP_GENERATION_NEW_FRESH;
}

bool pc_coop_fixture_generation_effective(PcCoopGenerationState* state,
    unsigned long long now_ms, unsigned* buttons, int* axes) {
    if (!state || !buttons || !axes || (state->clock_seen && now_ms < state->last_reader_ms)) return false;
    state->clock_seen = true; state->last_reader_ms = now_ms;
    const bool expired = !state->sequence || now_ms < state->published_ms || now_ms - state->published_ms > 500;
    *buttons = expired ? 0 : state->buttons;
    for (unsigned i = 0; i < 4; ++i) axes[i] = expired ? 0 : state->axes[i];
    return true;
}
