// Netplay M4 lane A randomizer snapshot codec + reassembler (issue #885).
// See pc_netplay_randstate.h for the layout and the determinism contract.
// Engine-free: <cstdint>/<cstddef>/<cstring> only.

#include "netplay/pc_netplay_randstate.h"

#include <cstring>

namespace pc_randstate {
namespace {

uint32_t crc_table_entry(unsigned i)
{
	uint32_t c = i;
	for (int k = 0; k < 8; ++k) c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
	return c;
}

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
	out[8]  = (uint8_t)(st.checksLo & 0xFF);
	out[9]  = (uint8_t)((st.checksLo >> 8) & 0xFF);
	out[10] = (uint8_t)((st.checksLo >> 16) & 0xFF);
	out[11] = (uint8_t)((st.checksLo >> 24) & 0xFF);
	for (int i = 0; i < 12; ++i) out[12 + i] = st.stats[i];
	for (int i = 0; i < 9; ++i) out[24 + i] = st.benefits[i];
	out[33] = st.rsv[0];
	out[34] = st.rsv[1];
	out[35] = st.rsv[2];
	out[36] = (uint8_t)(st.gen & 0xFF);
	out[37] = (uint8_t)((st.gen >> 8) & 0xFF);
	out[38] = (uint8_t)((st.gen >> 16) & 0xFF);
	out[39] = (uint8_t)((st.gen >> 24) & 0xFF);
	const uint32_t crc = crc32(out, kPayloadBytes);
	out[40] = (uint8_t)(crc & 0xFF);
	out[41] = (uint8_t)((crc >> 8) & 0xFF);
	out[42] = (uint8_t)((crc >> 16) & 0xFF);
	out[43] = (uint8_t)((crc >> 24) & 0xFF);
	return kStateBytes;
}

bool decode(const uint8_t* data, size_t avail, PcRandState& out)
{
	if (data == nullptr || avail < kStateBytes) return false;
	if (data[0] != kVersion) return false;
	if (data[33] != 0 || data[34] != 0 || data[35] != 0) return false;
	const uint32_t checksLo =
	    (uint32_t)data[8] | ((uint32_t)data[9] << 8) | ((uint32_t)data[10] << 16)
	    | ((uint32_t)data[11] << 24);
	if (checksLo & 0xC0000000u) return false; // bits 30..31 must be zero
	const uint32_t want =
	    (uint32_t)data[40] | ((uint32_t)data[41] << 8) | ((uint32_t)data[42] << 16)
	    | ((uint32_t)data[43] << 24);
	if (crc32(data, kPayloadBytes) != want) return false;
	PcRandState st;
	st.ver = data[0];
	st.ready = data[1];
	st.repairs = data[2];
	st.unlocks = data[3];
	st.flarlic = data[4];
	st.emperor = data[5];
	st.deathLinks = (uint16_t)(data[6] | ((uint16_t)data[7] << 8));
	st.checksLo = checksLo;
	for (int i = 0; i < 12; ++i) st.stats[i] = data[12 + i];
	for (int i = 0; i < 9; ++i) st.benefits[i] = data[24 + i];
	st.rsv[0] = st.rsv[1] = st.rsv[2] = 0;
	st.gen = (uint32_t)data[36] | ((uint32_t)data[37] << 8) | ((uint32_t)data[38] << 16)
	    | ((uint32_t)data[39] << 24);
	st.crc = want;
	out = st;
	return true;
}

bool payload_equal(const PcRandState& a, const PcRandState& b)
{
	if (a.ver != b.ver || a.ready != b.ready || a.repairs != b.repairs
	    || a.unlocks != b.unlocks || a.flarlic != b.flarlic || a.emperor != b.emperor
	    || a.deathLinks != b.deathLinks || a.checksLo != b.checksLo)
		return false;
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
	mSeenGen = 0;
	mHaveGen = false;
	mAppliedGen = 0;
	mHasPending = false;
	mPendingGen = 0;
	mPendingFrame = 0;
	mPending = PcRandState();
}

void Reassembler::feed(bool hasChunk, uint8_t seq, const uint8_t payload[kFragBytes], bool last,
                       uint32_t frame)
{
	(void)last; // completion is mask-driven (all 11 present), not flag-driven
	if (!hasChunk || payload == nullptr) return;
	if (frag_seq_stream(seq) != kStreamId) return;
	const uint8_t idx = frag_seq_index(seq);
	if (idx >= kFragCount) return;
	if (mMask & (uint16_t)(1u << idx)) return; // duplicate fragment: no-op
	for (size_t i = 0; i < kFragBytes; ++i) mSlots[idx * kFragBytes + i] = payload[i];
	mMask |= (uint16_t)(1u << idx);
	if (idx == 9) {
		// Fragment 9 carries bytes 36..39: the generation.
		mSeenGen = (uint32_t)mSlots[36] | ((uint32_t)mSlots[37] << 8)
		    | ((uint32_t)mSlots[38] << 16) | ((uint32_t)mSlots[39] << 24);
		mHaveGen = true;
	}
	if (mMask != (uint16_t)((1u << kFragCount) - 1)) return; // incomplete: wait
	PcRandState st;
	if (!decode(mSlots, sizeof(mSlots), st)) {
		// Corrupt transfer (never happens on the reliable lockstep stream):
		// drop it identically on both peers rather than applying garbage.
		memset(mSlots, 0, sizeof(mSlots));
		mMask = 0;
		mHaveGen = false;
		return;
	}
	memset(mSlots, 0, sizeof(mSlots));
	mMask = 0;
	mHaveGen = false;
	if (st.gen <= mAppliedGen) return; // stale or replayed generation: no-op
	// A newer generation replaces an unconsumed pending one only when it
	// completes later; both peers see the same stream, so the pending slot
	// stays identical. (The sender emits each generation's 11 fragments
	// consecutively, so a completion always carries a single generation.)
	mPending = st;
	mPendingGen = st.gen;
	mPendingFrame = frame + 1; // apply at the start of the tick for frame F+1
	mHasPending = true;
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
