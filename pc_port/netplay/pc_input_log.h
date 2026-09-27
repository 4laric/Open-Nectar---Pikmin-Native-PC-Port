#pragma once
// Input record/replay for the netplay determinism harness (issue #878).
//
// File format (little-endian binary, "PKNI" v1):
//   offset  size  field
//   0       4     magic: 'P' 'K' 'N' 'I' (0x49, 0x4B, 0x4E, 0x49)
//   4       2     version: u16, currently 1
//   6       2     pad count: u16, currently 4
//   8       2     record size: u16, bytes per tick (currently 44)
//   10      44*N  tick records, one per tick, tick index = file order
//
// Each tick record holds the 4 pads in channel order. Each pad is 11 bytes,
// every field serialised explicitly (never a raw struct memcpy, so the file
// is stable across compilers and struct layouts):
//   offset  size  field
//   0       2     button: u16 LE (PAD_BUTTON_* / PAD_TRIGGER_* bits)
//   2       1     stickX: s8
//   3       1     stickY: s8
//   4       1     substickX (C-stick X): s8
//   5       1     substickY (C-stick Y): s8
//   6       1     triggerLeft: u8
//   7       1     triggerRight: u8
//   8       1     analogA: u8
//   9       1     analogB: u8
//   10      1     err: s8 (0 = connected, -1 = no controller, ...)
//
// Runtime behaviour:
//   PIKMIN_INPUT_RECORD=<file> (or --input-record <file>): on each tick,
//     append the 4 PADStatus records.
//   PIKMIN_INPUT_REPLAY=<file> (or --input-replay <file>): on each tick,
//     overwrite the 4 pads with the recorded values for that tick index.
//     This happens after PADRead, so replay bypasses local window-focus
//     gating on purpose. Past the end of the file, neutral pads are fed
//     (all zeros; err kept connected (0) for pad 0, no-controller (-1) for
//     pads 1-3).
//   With neither switch set, pc_input_log_tick() is a no-op: no files are
//     touched, no log lines are printed, the RNG sequence is unchanged.

#include <cstddef>
#include <cstdint>
#include <vector>

struct PcInputPad {
	uint16_t button;
	int8_t stickX;
	int8_t stickY;
	int8_t substickX;
	int8_t substickY;
	uint8_t triggerLeft;
	uint8_t triggerRight;
	uint8_t analogA;
	uint8_t analogB;
	int8_t err;
};

namespace pc_input_log {
constexpr uint8_t kMagic[4]     = { 'P', 'K', 'N', 'I' };
constexpr uint16_t kVersion     = 1;
constexpr uint16_t kPadCount    = 4;
constexpr size_t kPadBytes      = 11;
constexpr size_t kRecordBytes   = kPadCount * kPadBytes; // 44
constexpr size_t kHeaderBytes   = 10;
constexpr int8_t kErrConnected  = 0;
constexpr int8_t kErrNoController = -1;
} // namespace pc_input_log

// Pure encode/decode API (no globals, no files; host-testable).
// Encodes 4 pads into exactly 44 bytes. Returns bytes written (44).
size_t pc_input_log_encode_tick(const PcInputPad pads[4], uint8_t out[44]);

// Decodes 4 pads from the first 44 bytes. Returns false when avail < 44.
bool pc_input_log_decode_tick(const uint8_t* data, size_t avail, PcInputPad pads[4]);

// Appends the 10-byte header to out.
void pc_input_log_write_header(std::vector<uint8_t>& out);

enum PcInputHeaderResult {
	PC_INPUT_HEADER_OK          = 0,
	PC_INPUT_HEADER_TRUNCATED   = 1,
	PC_INPUT_HEADER_BAD_MAGIC   = 2,
	PC_INPUT_HEADER_UNSUPPORTED = 3, // version, pad count or record size mismatch
};

// Validates the 10-byte header at data. version/padCount/recordSize are set
// only on PC_INPUT_HEADER_OK.
PcInputHeaderResult pc_input_log_read_header(const uint8_t* data, size_t len, uint16_t& version,
                                             uint16_t& padCount, uint16_t& recordSize);

// Runtime API (engine). argv capture must happen before the first tick
// (pc_main calls it at startup); env vars are read lazily on the first tick.
void pc_input_log_notify_argv(int argc, char** argv);

// Per-tick hook: call after PADRead (after the hold check), before the sim.
void pc_input_log_tick(void);

// Flush the record file, if any. Called every 300 ticks and on exit.
void pc_input_log_flush(void);
