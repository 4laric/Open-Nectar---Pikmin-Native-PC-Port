#ifndef P2_COOP_FIXTURE_INPUT_SNAPSHOT_H
#define P2_COOP_FIXTURE_INPUT_SNAPSHOT_H
// Engine-free byte boundary: no SDK handles or engine aliases cross this API.
enum PcCoopSnapshotResult { PC_COOP_SNAPSHOT_MISSING, PC_COOP_SNAPSHOT_OK,
    PC_COOP_SNAPSHOT_IO_ERROR, PC_COOP_SNAPSHOT_TOO_LONG,
    PC_COOP_SNAPSHOT_INVALID_ENTRY, PC_COOP_SNAPSHOT_GENERATION_LIMIT };
PcCoopSnapshotResult pc_coop_fixture_input_snapshot(const char* path,
    char* bytes, unsigned capacity, unsigned* count);

// Optional Windows diagnostic queue only. No Windows/engine aliases cross here.
struct PcCoopGenerationInfo {
    unsigned long long sequence;
    unsigned os_error;
    unsigned close_error;
};
bool pc_coop_fixture_input_clock(unsigned long long* milliseconds);
PcCoopSnapshotResult pc_coop_fixture_generation_snapshot(const char* directory,
    char* bytes, unsigned capacity, unsigned* count, PcCoopGenerationInfo* info);

enum PcCoopGenerationDecision {
    PC_COOP_GENERATION_INVALID, PC_COOP_GENERATION_NEW_FRESH,
    PC_COOP_GENERATION_NEW_EXPIRED, PC_COOP_GENERATION_DUPLICATE,
    PC_COOP_GENERATION_OLDER
};
struct PcCoopGenerationState {
    unsigned long long sequence;
    unsigned long long published_ms;
    unsigned long long last_reader_ms;
    bool clock_seen;
    unsigned buttons;
    int axes[4];
    unsigned canonical_size;
    char canonical[256];
};
PcCoopGenerationDecision pc_coop_fixture_generation_accept(PcCoopGenerationState* state,
    const char* bytes, unsigned count, unsigned long long expected_sequence,
    unsigned long long now_ms, unsigned button_limit);
bool pc_coop_fixture_generation_effective(PcCoopGenerationState* state,
    unsigned long long now_ms, unsigned* buttons, int* axes);
#endif
