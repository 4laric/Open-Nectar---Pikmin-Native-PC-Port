#pragma once
// Private fixture pacing only. Never an input, PAD, actor, or simulation write.
#include <cstring>
#include <sstream>
#include <string>

// Elapsed values are SDL timer-origin times ONLY. Boot-clock publication freshness
// is checked independently by pc_coop_step_accept; never subtract clock domains.
inline bool pc_coop_step_elapsed_ok(unsigned long long sdl_now, unsigned long long sdl_wait_started,
    unsigned long long sdl_fixture_started, unsigned frames) {
    return sdl_now >= sdl_wait_started && sdl_now - sdl_wait_started < 500
        && sdl_now >= sdl_fixture_started && sdl_now - sdl_fixture_started < 60000 && frames < 5000;
}

enum PcCoopStepDecision { PC_COOP_STEP_INVALID, PC_COOP_STEP_WAIT, PC_COOP_STEP_RELEASE };
struct PcCoopStepState {
    unsigned long long sequence = 0, published_ms = 0, frame = 0, last_reader_ms = 0;
    bool clock_seen = false;
    unsigned canonical_size = 0;
    char canonical[256] = {};
};
inline bool pc_coop_step_epoch(const std::string& epoch) {
    if (epoch.size() != 32) return false;
    for (char c : epoch) if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    return true;
}
inline PcCoopStepDecision pc_coop_step_accept(PcCoopStepState& state,
    const char* bytes, unsigned size, unsigned long long filename_sequence,
    unsigned long long now, const std::string& expected_epoch, int expected_role,
    unsigned long long current_frame, unsigned long long input_sequence) {
    if (!bytes || !size || size >= sizeof(state.canonical) || !pc_coop_step_epoch(expected_epoch)
        || expected_role < 0 || expected_role > 1 || current_frame >= 5000 || !input_sequence
        || (state.clock_seen && now < state.last_reader_ms)) return PC_COOP_STEP_INVALID;
    std::string tag, epoch, end, extra;
    unsigned long long sequence = 0, tick = 0, frame = 0, command = 0;
    int role = -1;
    std::istringstream stream(std::string(bytes, size));
    if (!(stream >> tag >> sequence >> tick >> epoch >> role >> frame >> command >> end)
        || stream >> extra || tag != "STEP1" || end != "END" || epoch != expected_epoch
        || role != expected_role || !sequence || sequence > 4096 || sequence != filename_sequence
        || !command || command > 4096 || frame >= 5000 || now < tick) return PC_COOP_STEP_INVALID;
    const std::string canonical = "STEP1 " + std::to_string(sequence) + " " + std::to_string(tick)
        + " " + epoch + " " + std::to_string(role) + " " + std::to_string(frame)
        + " " + std::to_string(command) + " END\n";
    if (canonical.size() != size || std::memcmp(bytes, canonical.data(), size)) return PC_COOP_STEP_INVALID;
    if (state.sequence) {
        if (current_frame != state.frame) return PC_COOP_STEP_INVALID;
        if (sequence == state.sequence) {
            if (size != state.canonical_size || std::memcmp(bytes, state.canonical, size)) return PC_COOP_STEP_INVALID;
            state.last_reader_ms = now; state.clock_seen = true;
            return PC_COOP_STEP_WAIT; // Already spent. Never refresh a grant or its waiting deadline.
        }
    }
    if (sequence != state.sequence + 1 || frame != current_frame + 1
        || command != input_sequence || now - tick > 500) return PC_COOP_STEP_INVALID;
    state.sequence = sequence; state.published_ms = tick; state.frame = frame;
    state.last_reader_ms = now; state.clock_seen = true; state.canonical_size = size;
    std::memcpy(state.canonical, bytes, size);
    return PC_COOP_STEP_RELEASE;
}

// Fixture-owned copies of actual submissions. Callbacks never allocate, throw,
// modify input or invoke the engine. Coverage is evidence, not a game-state write.
struct PcCoopQueueSlot { unsigned long long frame = 0; bool seen = false; unsigned char wire[16] = {}; };
struct PcCoopQueueObserver {
    bool enabled = false, armed = false, invalid = false, submit_seen = false, advance_seen = false;
    int role = -1;
    unsigned long long last_submit = 0, submit_serial = 0, frame = 0, next_land = 0, previous_frame = 0;
    unsigned delay = 0, maximum = 0;
    bool adaptive = false, hold = false, speculative = false, rand_neutral = false;
    unsigned char applied[32] = {};
    PcCoopQueueSlot slots[16] = {};
    void submit(unsigned long long land, int source_role, const unsigned char* wire) noexcept {
        if (!enabled) return;
        if (!wire || source_role != role || land >= 5000 || (submit_seen && land != last_submit + 1)) {
            invalid = true; return;
        }
        PcCoopQueueSlot& slot = slots[land % 16];
        slot.frame = land; slot.seen = true; std::memcpy(slot.wire, wire, 16);
        submit_seen = true; last_submit = land; ++submit_serial;
    }
    void advance(unsigned long long current, int source_role, unsigned long long next,
        unsigned local_delay, unsigned max_delay, bool changing, bool holding,
        bool predicting, bool gated, const unsigned char* wire) noexcept {
        if (!enabled) return;
        if (!wire || source_role != role || current >= 5000 || (advance_seen && current != previous_frame + 1)) {
            invalid = true; return;
        }
        frame = current; previous_frame = current; advance_seen = true; next_land = next;
        delay = local_delay; maximum = max_delay; adaptive = changing; hold = holding;
        speculative = predicting; rand_neutral = gated; std::memcpy(applied, wire, 32);
    }
    const PcCoopQueueSlot* slot(unsigned long long requested) const noexcept {
        const PcCoopQueueSlot& found = slots[requested % 16];
        return found.seen && found.frame == requested ? &found : nullptr;
    }
    static bool controls_neutral(const unsigned char* wire) noexcept {
        if (!wire) return false;
        // Yaw and randomizer fragment bytes are observed exactly but are not
        // controller actions. HOLD/unknown flags are refused independently.
        for (unsigned i = 0; i < 8; ++i) if (wire[i]) return false;
        return true;
    }
    bool complete(unsigned long long expected_frame) const noexcept {
        if (invalid || !advance_seen || frame != expected_frame || role < 0 || role > 1
            || maximum != 8 || delay > maximum || adaptive || hold || speculative || rand_neutral
            || !submit_seen || next_land != last_submit + 1
            || next_land <= frame || next_land - frame - 1 > maximum) return false;
        const PcCoopQueueSlot* current = slot(frame);
        if (!current || std::memcmp(current->wire, applied + 16 * role, 16)) return false;
        for (unsigned peer = 0; peer < 2; ++peer) if (applied[16 * peer + 10] & ~3u) return false;
        for (unsigned long long f = frame + 1; f < next_land; ++f) {
            const PcCoopQueueSlot* pending = slot(f);
            if (!pending || (pending->wire[10] & ~3u)) return false;
        }
        return true;
    }
    bool future_neutral() const noexcept {
        if (!complete(frame)) return false;
        for (unsigned long long f = frame + 1; f < next_land; ++f)
            if (!controls_neutral(slot(f)->wire)) return false;
        return true;
    }
};
