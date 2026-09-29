// Netplay M6b production snapshot (issue #896): the game-side hooks the
// snapshot needs but cannot reach from pc_snapshot.cpp (that TU includes
// <windows.h> and stays free of engine headers).
//
// Compiled only with the CMake option PIKMIN_NETPLAY_SNAPSHOT=ON. Every entry
// point is called from pc_snapshot.cpp.
//
//   * crowd bootstrap (the production equivalent of the M6a spike's):
//     PIKMIN_NETPLAY_SNAPSHOT_CROWD=N (or the spike's
//     PIKMIN_NETPLAY_SNAPSHOT_SPIKE_CROWD=N) tops the field up to N red Pikmin
//     once per day through the normal Onion exit path. It works with the
//     snapshot on or off, so a crowd run has its own M1 pair;
//   * a per-tick sample for the CSV: day-end phase and field Pikmin count;
//   * AyuHeap usage high-water per heap (sizes the arena zone, M6b item 3).

#include "BaseInf.h"
#include "GameStat.h"
#include "GoalItem.h"
#include "ItemMgr.h"
#include "MoviePlayer.h"
#include "NaviMgr.h"
#include "PlayerState.h"
#include "gameflow.h"
#include "system.h"
#include "pc_randomizer.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

int sCrowdTarget  = -1; // -1 = env not read yet, 0 = off
int sCrowdLastDay = -1;
int sCrowdArmTick = -1;
bool sCrowdDone   = false;

int fieldPikis()
{
	return int(GameStat::formationPikis) + int(GameStat::freePikis) + int(GameStat::workPikis);
}

struct HeapHw {
	uintptr_t lo;   // heap start when last seen (a re-init resets the marks)
	int64_t size;
	int64_t up;     // max bytes used growing up
	int64_t down;   // max bytes used growing down
	int64_t total;  // max mTotalSize
};
HeapHw sHeapHw[SYSHEAP_COUNT];
bool sHeapSeen = false;

} // namespace

void pc_snapshot_game_tick(uint64_t tick)
{
	if (sCrowdTarget < 0) {
		const char* v = std::getenv("PIKMIN_NETPLAY_SNAPSHOT_CROWD");
		if (!v || !*v) v = std::getenv("PIKMIN_NETPLAY_SNAPSHOT_SPIKE_CROWD");
		sCrowdTarget = (v && *v) ? std::atoi(v) : 0;
		if (sCrowdTarget > 0) {
			std::printf("[snapshot] crowd bootstrap: field target %d red Pikmin\n", sCrowdTarget);
			std::fflush(stdout);
		}
	}
	if (sCrowdTarget <= 0 || !naviMgr || !itemMgr || !playerState || !pc_randomizer_ready()) return;
	if (!gameflow.mMoviePlayer || gameflow.mMoviePlayer->mIsActive || gameflow.mPauseAll || gameflow.mIsUIOverlayActive) return;
	if (gameflow.mIsDayEndActive || gameflow.mIsDayEndTriggered) return;
	const int day = gameflow.mWorldClock.mCurrentDay;
	if (day != sCrowdLastDay) {
		// A new day (or the first live tick): let the start-of-day withdraw
		// finish first, then top up once.
		sCrowdLastDay = day;
		sCrowdArmTick = int(tick) + 600;
		sCrowdDone    = false;
	}
	if (sCrowdDone || int(tick) < sCrowdArmTick) return;
	GoalItem* onion = itemMgr->getContainer(Red);
	if (!onion || itemMgr->getContainerExitCount() > 0) return;
	const int field = fieldPikis();
	const int want  = sCrowdTarget - field;
	sCrowdDone      = true;
	if (want <= 0) return;
	const int stock = onion->getTotalStorePikis();
	const int add   = want > stock ? want - stock : 0;
	if (add > 0) {
		pikiInfMgr.mPikiCounts[Red][Leaf] += add;
		onion->mHeldPikis[Leaf] += add;
		GameStat::containerPikis.add(Red, add);
		playerState->mTotalBornPikiNum += add;
		playerState->mLivingPikiNum += add;
		GameStat::update();
	}
	onion->exitPikis(want);
	std::printf("[snapshot] crowd bootstrap: tick=%llu day=%d field=%d stocked=%d exit=%d queued=%d\n",
	    (unsigned long long)tick, day, field, add, want, itemMgr->getContainerExitCount());
	std::fflush(stdout);
}

void pc_snapshot_game_sample(int* phase, int* pikis)
{
	*pikis = naviMgr ? fieldPikis() : 0;
	if (!naviMgr) *phase = 2;
	else if (gameflow.mIsDayEndActive || gameflow.mIsDayEndTriggered) *phase = 1;
	else *phase = 0;
}

void pc_snapshot_game_heap_sample(void)
{
	if (!gsys) return;
	sHeapSeen = true;
	for (int i = 0; i < SYSHEAP_COUNT; ++i) {
		AyuHeap& h = gsys->mHeaps[i];
		if (!h.mIsActive || h.mSize <= 0) continue;
		HeapHw& w = sHeapHw[i];
		if (w.lo != uintptr_t(h.mInitialStackTop) || w.size != int64_t(h.mSize)) {
			w.lo   = uintptr_t(h.mInitialStackTop);
			w.size = int64_t(h.mSize);
		}
		const int64_t up   = int64_t(h.mStackTop - h.mInitialStackTop);
		const int64_t down = int64_t(h.mInitialStackLimit - h.mStackLimit);
		if (up > w.up) w.up = up;
		if (down > w.down) w.down = down;
		if (int64_t(h.mTotalSize) > w.total) w.total = int64_t(h.mTotalSize);
	}
}

void pc_snapshot_game_heap_report(FILE* out)
{
	if (!sHeapSeen || !gsys) return;
	for (int i = 0; i < SYSHEAP_COUNT; ++i) {
		const HeapHw& w = sHeapHw[i];
		if (!w.size) continue;
		const AyuHeap& h = gsys->mHeaps[i];
		std::fprintf(out, "[snapshot] heap %d %s lo=0x%llx size=%.2f MB max_up=%.2f MB max_down=%.2f MB max_total=%.2f MB\n", i,
		    h.mName ? h.mName : "?", (unsigned long long)w.lo, double(w.size) / 1048576.0, double(w.up) / 1048576.0,
		    double(w.down) / 1048576.0, double(w.total) / 1048576.0);
	}
}
