#pragma once
// Netplay M4 lane A: versioned randomizer external-state snapshot (issue #885).
//
// Canonical 44-byte little-endian POD from the M4 plan section 2a. The host
// packs the sim-visible subset of state.txt into this struct, bumps `gen`,
// and streams it to both peers in frame-tagged input fragments (11 fragments
// of 4 payload bytes). Both peers apply it through
// pc_randomizer_apply_net_state() at the start of the same tick.
//
// Layout (offsets are exact, all multi-byte fields little-endian):
//   0   ver         u8  =1
//   1   ready       u8  0/1
//   2   repairs     u8  0..25
//   3   unlocks     u8  bitmask
//   4   flarlic     u8  0..10
//   5   emperor     u8  0/1
//   6   deathLinks  u16 monotonic received count
//   8   checksLo    u32 slots 0..29 bitset (bits 30..31 zero)
//   12  stats[12]   u8  [3][4] tiers
//   24  benefits[9] u8  receipt counters
//   33  _rsv        u8  zero
//   34  _rsv2       u8  zero
//   35  _rsv3       u8  zero
//   36  gen         u32 host snapshot generation, monotonic
//   40  crc         u32 CRC32 (IEEE) of bytes 0..39
//
// Engine-free (only <cstdint>/<cstddef>), so the host unit test links it
// without the game. Serialisation is explicit per-field, never a raw struct
// memcpy, so the wire format is stable across compilers.

#include <cstddef>
#include <cstdint>

namespace pc_randstate {

constexpr size_t kStateBytes = 44;
constexpr size_t kPayloadBytes = 40; // bytes covered by the CRC
constexpr uint8_t kVersion = 1;
constexpr size_t kFragCount = 11;    // 44 bytes at 4 payload bytes per fragment
constexpr size_t kFragBytes = 4;

// Fragment sequence byte (input pad[11]): high nibble = stream id (0 for the
// randomizer snapshot stream), low nibble = fragment index 0..10.
constexpr uint8_t kStreamId = 0;

inline uint8_t frag_seq_make(uint8_t idx) { return (uint8_t)((kStreamId << 4) | (idx & 0x0F)); }
inline uint8_t frag_seq_stream(uint8_t seq) { return (uint8_t)((seq >> 4) & 0x0F); }
inline uint8_t frag_seq_index(uint8_t seq) { return (uint8_t)(seq & 0x0F); }

struct PcRandState {
	uint8_t ver = 0;
	uint8_t ready = 0;
	uint8_t repairs = 0;
	uint8_t unlocks = 0;
	uint8_t flarlic = 0;
	uint8_t emperor = 0;
	uint16_t deathLinks = 0;
	uint32_t checksLo = 0;
	uint8_t stats[12] = {};
	uint8_t benefits[9] = {};
	uint8_t rsv[3] = {};
	uint32_t gen = 0;
	uint32_t crc = 0;
};

// IEEE CRC32 over `len` bytes.
uint32_t crc32(const uint8_t* data, size_t len);

// Encodes exactly 44 bytes (computes and stores the CRC). Returns 44.
size_t encode(const PcRandState& st, uint8_t out[kStateBytes]);

// Decodes from the first 44 bytes. Validates ver == 1, reserved bytes zero,
// checksLo bits 30..31 zero, and the CRC. Returns false (leaving `out`
// untouched) when avail < 44 or any check fails.
bool decode(const uint8_t* data, size_t avail, PcRandState& out);

// Payload equality ignoring gen/crc: true when the sim-visible state is
// identical (the host uses this to publish only on content change).
bool payload_equal(const PcRandState& a, const PcRandState& b);

// Sim-side reassembly buffer. Both peers feed the host input's fragments in
// frame order, so both hold identical copies. Keyed implicitly by arrival
// order (the sender emits each generation's 11 fragments consecutively):
// slots fill by fragment index; the generation is learned when fragment 9
// (which carries `gen`) arrives. When all 11 slots are filled and the CRC
// validates, the snapshot completes at the current frame F and must be
// applied at the start of the tick for frame F+1. Duplicate fragments,
// stale generations (gen <= appliedGen) and incomplete transfers are no-ops
// that never disturb the sim.
class Reassembler {
public:
	Reassembler();

	// Feed one fragment carried by the Advance for `frame`. Has no effect
	// unless hasChunk is set. `seq` is the pad[11] sequence byte, `payload`
	// the 4 pad[12..15] bytes, `last` the CHUNK_LAST flag.
	void feed(bool hasChunk, uint8_t seq, const uint8_t payload[kFragBytes], bool last,
	          uint32_t frame);

	// True when a snapshot completed and is waiting for its apply frame.
	bool has_pending() const { return mHasPending; }
	uint32_t pending_gen() const { return mPendingGen; }
	// The frame whose tick-start must apply the pending snapshot (F+1 where
	// F is the frame whose Advance completed the transfer).
	uint32_t pending_frame() const { return mPendingFrame; }
	const PcRandState& pending() const { return mPending; }

	// Applies (consumes) the pending snapshot. The caller performs the
	// actual sim apply, then records the generation. Returns false when
	// nothing was pending.
	bool take_pending(PcRandState& out);
	void mark_applied(uint32_t gen);

	uint32_t applied_gen() const { return mAppliedGen; }
	void reset();

private:
	uint8_t mSlots[kStateBytes] = {};
	uint16_t mMask = 0; // bit i set when fragment i received
	uint32_t mSeenGen = 0;
	bool mHaveGen = false;
	uint32_t mAppliedGen = 0;
	bool mHasPending = false;
	uint32_t mPendingGen = 0;
	uint32_t mPendingFrame = 0;
	PcRandState mPending = {};
};

} // namespace pc_randstate
