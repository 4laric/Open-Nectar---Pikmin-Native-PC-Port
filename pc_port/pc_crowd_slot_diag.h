#pragma once

// One-shot, rate-limited diagnostics for the formation slot bookkeeping
// (netplay co-op issue #1033). ActCrowd saves the plate it joined (mPlateMgr)
// at init, but ActCrowd::cleanup used to decrement the plate count of the
// Pikmin's *current* captain (mPiki->mNavi). When mNavi changed under a live
// ActCrowd the two plates' counts went wrong, CPlate::refresh then shrank
// mUsedSlotCount below the real occupancy and the next ActCrowd::exec hit
// "invalid slotId!". These lines say which Pikmin, which captains, which slot
// and from where. They take plain values only, print at most kMaxLines lines
// per process and never touch simulation state, so they cannot desync a
// netplay session.

#include <cstdio>

namespace pc_crowd_slot_diag {

constexpr int kMaxLines = 24;

struct Info {
	const char* where;
	unsigned frame;
	const void* piki;
	int color, happa;
	float x, z;
	int naviId;      // the Pikmin's current captain, -1 for none
	int plateOwner;  // captain that owns the plate the ActCrowd joined, -1 for nobody
	int slot, used, total;
	unsigned count;  // that plate's mPlatePikiCount
	int pikiMode, pikiState, actMode, actState;
};

inline int& lineCount()
{
	static int sLines = 0;
	return sLines;
}

inline void ownerMismatch(const Info& i)
{
	static const void* sLastPiki = nullptr;
	static const char* sLastWhere = nullptr;
	if (i.piki == sLastPiki && i.where == sLastWhere) return; // one line per Pikmin and site until another logs
	if (lineCount() >= kMaxLines) return;
	++lineCount();
	sLastPiki = i.piki;
	sLastWhere = i.where;
	std::printf("[crowd-slot] MISMATCH at=%s frame=%u piki=%p color=%d happa=%d pos=(%.0f,%.0f) navi=%d plate_owner=%d slot=%d "
	            "used=%d total=%d count=%u piki_mode=%d state=%d act_mode=%d act_state=%d\n",
	            i.where, i.frame, i.piki, i.color, i.happa, double(i.x), double(i.z), i.naviId, i.plateOwner, i.slot, i.used, i.total,
	            i.count, i.pikiMode, i.pikiState, i.actMode, i.actState);
	std::fflush(stdout);
}

// Navi::refresh is about to hand its plate a pikmin count below the slots it
// has: the bookkeeping is already miswired (a legitimate leave lowers the
// count and mTotalSlotCount together).
inline void refreshShrink(unsigned frame, int naviId, int count, int total, int used)
{
	if (count >= total) return;
	if (lineCount() >= kMaxLines) return;
	++lineCount();
	std::printf("[crowd-slot] REFRESH_SHRINK frame=%u navi=%d count=%d total=%d used=%d\n", frame, naviId, count, total, used);
	std::fflush(stdout);
}

// ActCrowd::init ran on an action that still holds a plate (its previous
// init was never cleaned up): that plate's count and slot leak.
inline void reinit(unsigned frame, const void* piki, int oldOwner, int newNavi, int slot)
{
	if (lineCount() >= kMaxLines) return;
	++lineCount();
	std::printf("[crowd-slot] REINIT frame=%u piki=%p old_plate_owner=%d new_navi=%d old_slot=%d\n", frame, piki, oldOwner, newNavi, slot);
	std::fflush(stdout);
}

} // namespace pc_crowd_slot_diag
