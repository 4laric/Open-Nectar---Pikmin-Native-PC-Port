// Netplay M4 lane A randomizer snapshot codec + reassembler (issue #885).
// See pc_netplay_randstate.h for the layout and the determinism contract.
// Engine-free: <cstdint>/<cstddef>/<cstring>/<cstdio> only.

#include "netplay/pc_netplay_randstate.h"

#include <cstdio>
#include <cstring>

namespace pc_randstate {
namespace {

uint32_t crc_table_entry(unsigned i)
{
	uint32_t c = i;
	for (int k = 0; k < 8; ++k) c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
	return c;
}

constexpr size_t kGenOff = 56; // gen u32 offset in the v2 wire layout
constexpr size_t kCrcOff = 60; // crc u32 offset in the v2 wire layout

} // namespace

uint32_t crc32(const uint8_t* data, size_t len)
{
	uint32_t crc = 0xFFFFFFFFu;
	for (size_t i = 0; i < len; ++i) {
		const uint8_t b = data != nullptr ? data[i] : 0;
		crc = crc_table_entry((crc ^ b) & 0xFF) ^ (crc >> 8);
	}
	return crc ^ 0xFFFFFFFFu;
}

size_t encode(const PcRandState& st, uint8_t out[kStateBytes])
{
	if (out == nullptr) return 0;
	out[0] = st.ver;
	out[1] = st.ready;
	out[2] = st.repairs;
	out[3] = st.unlocks;
	out[4] = st.flarlic;
	out[5] = st.emperor;
	out[6] = (uint8_t)(st.deathLinks & 0xFF);
	out[7] = (uint8_t)((st.deathLinks >> 8) & 0xFF);
	for (size_t i = 0; i < kCheckBytes; ++i) out[8 + i] = st.checks[i];
	for (int i = 0; i < 12; ++i) out[32 + i] = st.stats[i];
	for (int i = 0; i < 9; ++i) out[44 + i] = st.benefits[i];
	out[53] = st.maturity;
	out[54] = st.dayLength;
	out[55] = st.whistlePluck;
	out[kGenOff] = (uint8_t)(st.gen & 0xFF);
	out[kGenOff + 1] = (uint8_t)((st.gen >> 8) & 0xFF);
	out[kGenOff + 2] = (uint8_t)((st.gen >> 16) & 0xFF);
	out[kGenOff + 3] = (uint8_t)((st.gen >> 24) & 0xFF);
	const uint32_t crc = crc32(out, kPayloadBytes);
	out[kCrcOff] = (uint8_t)(crc & 0xFF);
	out[kCrcOff + 1] = (uint8_t)((crc >> 8) & 0xFF);
	out[kCrcOff + 2] = (uint8_t)((crc >> 16) & 0xFF);
	out[kCrcOff + 3] = (uint8_t)((crc >> 24) & 0xFF);
	return kStateBytes;
}

bool decode(const uint8_t* data, size_t avail, PcRandState& out)
{
	if (data == nullptr || avail < kStateBytes) return false;
	if (data[0] != kVersion) return false;
	// v3 (#982): maturity is 3 x 2-bit tiers 0..2 with bits 6-7 zero; dayLength
	// is 0..10; whistlePluck is 0/1. Anything else is a malformed snapshot.
	if ((data[53] & 0xC0) != 0 || (data[53] & 0x03) == 3 || ((data[53] >> 2) & 0x03) == 3
	    || ((data[53] >> 4) & 0x03) == 3 || data[54] > 10 || data[55] > 1)
		return false;
	const uint32_t want =
	    (uint32_t)data[kCrcOff] | ((uint32_t)data[kCrcOff + 1] << 8)
	    | ((uint32_t)data[kCrcOff + 2] << 16) | ((uint32_t)data[kCrcOff + 3] << 24);
	if (crc32(data, kPayloadBytes) != want) return false;
	PcRandState st;
	st.ver = data[0];
	st.ready = data[1];
	st.repairs = data[2];
	st.unlocks = data[3];
	st.flarlic = data[4];
	st.emperor = data[5];
	st.deathLinks = (uint16_t)(data[6] | ((uint16_t)data[7] << 8));
	for (size_t i = 0; i < kCheckBytes; ++i) st.checks[i] = data[8 + i];
	for (int i = 0; i < 12; ++i) st.stats[i] = data[32 + i];
	for (int i = 0; i < 9; ++i) st.benefits[i] = data[44 + i];
	st.maturity = data[53];
	st.dayLength = data[54];
	st.whistlePluck = data[55];
	st.gen = (uint32_t)data[kGenOff] | ((uint32_t)data[kGenOff + 1] << 8)
	    | ((uint32_t)data[kGenOff + 2] << 16) | ((uint32_t)data[kGenOff + 3] << 24);
	st.crc = want;
	out = st;
	return true;
}

bool payload_equal(const PcRandState& a, const PcRandState& b)
{
	if (a.ver != b.ver || a.ready != b.ready || a.repairs != b.repairs
	    || a.unlocks != b.unlocks || a.flarlic != b.flarlic || a.emperor != b.emperor
	    || a.deathLinks != b.deathLinks || a.maturity != b.maturity || a.dayLength != b.dayLength
	    || a.whistlePluck != b.whistlePluck)
		return false;
	for (size_t i = 0; i < kCheckBytes; ++i) {
		if (a.checks[i] != b.checks[i]) return false;
	}
	for (int i = 0; i < 12; ++i) {
		if (a.stats[i] != b.stats[i]) return false;
	}
	for (int i = 0; i < 9; ++i) {
		if (a.benefits[i] != b.benefits[i]) return false;
	}
	return true;
}

Reassembler::Reassembler() { reset(); }

void Reassembler::reset()
{
	memset(mSlots, 0, sizeof(mSlots));
	mMask = 0;
	mAppliedGen = 0;
	mHasPending = false;
	mPendingGen = 0;
	mPendingFrame = 0;
	mPending = PcRandState();
}

void Reassembler::feed(bool hasChunk, uint8_t seq, const uint8_t payload[kFragBytes], bool last,
                       uint32_t frame)
{
	// `last` is advisory: completion is mask-driven (all 16 present), so a
	// lost CHUNK_LAST bit cannot stall the stream. A last flag on a
	// non-final index is ignored rather than acted on.
	(void)last;
	if (!hasChunk || payload == nullptr) return;
	if (frag_seq_stream(seq) != kStreamId) return;
	const uint8_t idx = frag_seq_index(seq);
	if (idx >= kFragCount) return;
	if (idx == 0 && mMask != 0 && mMask != 0x01u) {
		// Generation boundary (M1 fix): the sender emits each
		// generation's fragments consecutively starting at 0 and never
		// interrupts an in-flight generation, so a fragment 0 arriving
		// mid-transfer opens a new generation. Drop the partial buffer
		// identically on both peers and start the new one. (A bare
		// duplicate of frag 0 with nothing else buffered is still a
		// no-op below.)
		memset(mSlots, 0, sizeof(mSlots));
		mMask = 0;
	}
	if (mMask & (1u << idx)) return; // duplicate fragment: no-op
	for (size_t i = 0; i < kFragBytes; ++i) mSlots[idx * kFragBytes + i] = payload[i];
	mMask |= (1u << idx);
	if (mMask != (kFragCount >= 32 ? 0xFFFFFFFFu : ((1u << kFragCount) - 1))) return; // incomplete: wait
	PcRandState st;
	if (!decode(mSlots, sizeof(mSlots), st)) {
		// Corrupt transfer: drop it identically on both peers rather than
		// applying garbage, and say so. The generation is read straight
		// off the wire so the operator can see which update died.
		const uint32_t wireGen = (uint32_t)mSlots[kGenOff]
		    | ((uint32_t)mSlots[kGenOff + 1] << 8) | ((uint32_t)mSlots[kGenOff + 2] << 16)
		    | ((uint32_t)mSlots[kGenOff + 3] << 24);
		std::printf("[netplay] randstate gen=%u dropped: decode failed\n", wireGen);
		std::fflush(stdout);
		memset(mSlots, 0, sizeof(mSlots));
		mMask = 0;
		return;
	}
	memset(mSlots, 0, sizeof(mSlots));
	mMask = 0;
	if (st.gen <= mAppliedGen) return; // stale or replayed generation: no-op
	// A newer generation replaces an unconsumed pending one only when it
	// completes later; both peers see the same stream, so the pending slot
	// stays identical.
	mPending = st;
	mPendingGen = st.gen;
	mPendingFrame = frame + 1; // apply at the start of the tick for frame F+1
	if (mAppliedGen == 0) {
		// B1 first-apply rule: delay-independent first snapshot frame.
		if (mPendingFrame > kFirstApplyFrame) {
			std::printf("[netplay] randstate gen=%u first apply late: frame=%u > %u\n", st.gen,
			            mPendingFrame, kFirstApplyFrame);
			std::fflush(stdout);
		} else {
			mPendingFrame = kFirstApplyFrame;
		}
	}
	mHasPending = true;
}

bool Reassembler::discard_pending_upto(uint32_t gen)
{
	if (!mHasPending || mPendingGen > gen) return false;
	mHasPending = false;
	return true;
}

bool Reassembler::take_pending(PcRandState& out)
{
	if (!mHasPending) return false;
	out = mPending;
	mHasPending = false;
	return true;
}

void Reassembler::mark_applied(uint32_t gen)
{
	if (gen > mAppliedGen) mAppliedGen = gen;
}

} // namespace pc_randstate
