#pragma once
// Netplay M4 lane A: versioned randomizer external-state snapshot (issue #885).
//
// Canonical 64-byte little-endian POD (v2, fix round 1). The host packs the
// sim-visible subset of state.txt into this struct, bumps `gen`, and streams
// it to both peers in frame-tagged input fragments (16 fragments of 4
// payload bytes). Both peers apply it through
// pc_randomizer_apply_net_state() at the start of the same tick.
//
// Layout (offsets are exact, all multi-byte fields little-endian):
//   0   ver         u8  =2
//   1   ready       u8  0/1
//   2   repairs     u8  0..25
//   3   unlocks     u8  bitmask
//   4   flarlic     u8  0..10
//   5   emperor     u8  0/1
//   6   deathLinks  u16 monotonic received count
//   8   checks[24]  u8  bitset, slot i in byte[i/8] bit (i%8), slots 0..191
//   32  stats[12]   u8  [3][4] tiers
//   44  benefits[9] u8  receipt counters
//   53  _rsv        u8  zero
//   54  _rsv2       u8  zero
//   55  _rsv3       u8  zero
//   56  gen         u32 host snapshot generation, monotonic
//   60  crc         u32 CRC32 (IEEE) of bytes 0..59
//
// v1 was 44 bytes with a 4-byte checksLo mask (slots 0..29); it could not
// carry real seeds (schema >= 2 has 55+ checks, schema 9 up to 158) and the
// host exited on slot >= 32. v2 widens checks to 24 bytes (192 slots, index
// 0..15 fragments) with room for catalog growth inside the 4-bit fragment
// index (16 fragments max). The decoder accepts any bit pattern in the
// bitset; slot-vs-catalog validation lives in apply_net_state, which shares
// kCheckBytes/kCheckSlots below.
//
// Engine-free (only <cstdint>/<cstddef>), so the host unit test links it
// without the game. Serialisation is explicit per-field, never a raw struct
// memcpy, so the wire format is stable across compilers.

#include <cstddef>
#include <cstdint>

namespace pc_randstate {

constexpr size_t kCheckBytes = 24; // check bitset width; 192 slots
constexpr size_t kCheckSlots = kCheckBytes * 8;
constexpr size_t kStateBytes = 64;
constexpr size_t kPayloadBytes = 60; // bytes covered by the CRC
constexpr uint8_t kVersion = 2;
constexpr size_t kFragCount = 16; // 64 bytes at 4 payload bytes per fragment
constexpr size_t kFragBytes = 4;
// M4 lane B1 (lane A recheck item 5): the first snapshot (nothing applied
// yet) applies at the tick start of max(kFirstApplyFrame, completion + 1),
// so the gen-1 apply frame no longer moves with the local delay (it was 18
// at delay 2 and 20 at delay 4). At every delay 1..8 the 16 fragments
// complete by frame 23, so gen 1 lands at 32 whatever the delay; a later
// completion is logged as a warning. Later generations keep completion + 1.
constexpr uint32_t kFirstApplyFrame = 32;

// Fragment sequence byte (input pad[11]): high nibble = stream id (0 for the
// randomizer snapshot stream), low nibble = fragment index 0..15.
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
	uint8_t checks[kCheckBytes] = {};
	uint8_t stats[12] = {};
	uint8_t benefits[9] = {};
	uint8_t rsv[3] = {};
	uint32_t gen = 0;
	uint32_t crc = 0;
};

// IEEE CRC32 over `len` bytes.
uint32_t crc32(const uint8_t* data, size_t len);

// Encodes exactly 64 bytes (computes and stores the CRC). Returns 64.
size_t encode(const PcRandState& st, uint8_t out[kStateBytes]);

// Decodes from the first 64 bytes. Validates ver == 2, reserved bytes zero,
// and the CRC. The check bitset is accepted verbatim; slot-vs-catalog range
// checks belong to apply_net_state (which knows checkCount). Returns false
// (leaving `out` untouched) when avail < 64 or any check fails.
bool decode(const uint8_t* data, size_t avail, PcRandState& out);

// Payload equality ignoring gen/crc: true when the sim-visible state is
// identical (the host uses this to publish only on content change).
bool payload_equal(const PcRandState& a, const PcRandState& b);

// Sim-side reassembly buffer. Both peers feed the host input's fragments in
// frame order, so both hold identical copies. Generations are carried
// consecutively: the sender never interrupts an in-flight generation (a
// newer snapshot waits in a one-deep queue and starts at the next fragment
// 0), so fragment 0 always marks a generation boundary. The receiver resets
// a partial transfer when fragment 0 arrives, which keys reassembly by
// (gen, idx) without spending wire bits on the generation: a mid-transfer
// publish can no longer mix two generations into one CRC-failing buffer.
// When all 16 slots are filled and the CRC validates, the snapshot
// completes at the current frame F and must be applied at the start of the
// tick for frame F+1. Duplicate fragments, stale generations
// (gen <= appliedGen) and incomplete transfers are no-ops that never
// disturb the sim. A CRC/validation failure is logged as
// `[netplay] randstate gen=<g> dropped: <reason>` (gen read from the wire
// bytes) so a dead stream is visible.
class Reassembler {
public:
	Reassembler();

	// Feed one fragment carried by the Advance for `frame`. Has no effect
	// unless hasChunk is set. `seq` is the pad[11] sequence byte, `payload`
	// the 4 pad[12..15] bytes, `last` the CHUNK_LAST flag (advisory only:
	// completion is mask-driven so a lost flag bit cannot stall the
	// stream; a last flag on a non-final index is ignored).
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
	// M4 lane B1 RESUME: drops a pending snapshot whose generation is
	// <= gen (the bulk RESUME snapshot supersedes it). Returns true when one
	// was dropped.
	bool discard_pending_upto(uint32_t gen);

	uint32_t applied_gen() const { return mAppliedGen; }
	void reset();

private:
	uint8_t mSlots[kStateBytes] = {};
	uint32_t mMask = 0; // bit i set when fragment i received
	uint32_t mAppliedGen = 0;
	bool mHasPending = false;
	uint32_t mPendingGen = 0;
	uint32_t mPendingFrame = 0;
	PcRandState mPending = {};
};

} // namespace pc_randstate
