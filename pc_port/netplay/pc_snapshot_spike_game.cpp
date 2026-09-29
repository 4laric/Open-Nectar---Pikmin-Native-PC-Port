// Netplay M6a snapshot spike (issue #896), fix round 1: the game-side hooks
// the spike needs but cannot reach from pc_snapshot_spike.cpp (that TU
// includes <windows.h> and stays free of engine headers).
//
// Compiled only with the CMake option PIKMIN_NETPLAY_SNAPSHOT_SPIKE=ON. Every
// entry point is called from pc_snapshot_spike.cpp; nothing here runs in a
// build without the option.
//
//   * crowd bootstrap (MV-7): PIKMIN_NETPLAY_SNAPSHOT_SPIKE_CROWD=N tops the
//     field up to N red Pikmin once per day through the normal Onion exit
//     path (the same stock-and-exit calls as the scripted "capacity" test).
//     It works with the spike on or off, so a crowd run has its own M1 pair;
//   * a per-tick sample for the CSV: day-end phase and field Pikmin count;
//   * the audio-event hash the synctest compares (MV-4): SeSystem's event
//     table (in the region) and the Jac free-event count (audio facade, not
//     rolled back);
//   * the divergent-input perturbation for the synctest's first pass (F4c).

#include "netplay/pc_snapshot_spike.h"

#include "netplay/pc_netplay_pad.h"

#include "BaseInf.h"
#include "GameStat.h"
#include "GoalItem.h"
#include "ItemMgr.h"
#include "MoviePlayer.h"
#include "NaviMgr.h"
#include "PlayerState.h"
#include "SoundMgr.h"
#include "gameflow.h"
#include "Graphics.h"
#include "system.h"
#include "jaudio/pikiinter.h"
#include "pc_randomizer.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

// Read-only view of SeSystem's protected event table (measurement only).
struct SeSystemPeek : SeSystem {
	static int eventCount(SeSystem* s) { return static_cast<SeSystemPeek*>(s)->mCurrentEventCount; }
	static int maxEvents(SeSystem* s) { return static_cast<SeSystemPeek*>(s)->mMaxEventCount; }
	static int handle(SeSystem* s, int i) { return static_cast<SeSystemPeek*>(s)->mEvents[i].mHandle; }
};

int sCrowdTarget   = -1; // -1 = env not read yet, 0 = off
int sCrowdLastDay  = -1;
int sCrowdArmTick  = -1;
bool sCrowdDone    = false;

int fieldPikis()
{
	return int(GameStat::formationPikis) + int(GameStat::freePikis) + int(GameStat::workPikis);
}

} // namespace

void pc_snapshot_spike_game_tick(uint64_t tick)
{
	if (sCrowdTarget < 0) {
		const char* v = std::getenv("PIKMIN_NETPLAY_SNAPSHOT_SPIKE_CROWD");
		sCrowdTarget  = (v && *v) ? std::atoi(v) : 0;
		if (sCrowdTarget > 0) {
			std::printf("[m6a] crowd bootstrap: field target %d red Pikmin\n", sCrowdTarget);
			std::fflush(stdout);
		}
	}
	if (sCrowdTarget <= 0 || !naviMgr || !itemMgr || !playerState || !pc_randomizer_ready()) return;
	if (!gameflow.mMoviePlayer || gameflow.mMoviePlayer->mIsActive || gameflow.mPauseAll || gameflow.mIsUIOverlayActive) return;
	if (gameflow.mIsDayEndActive || gameflow.mIsDayEndTriggered) return;
	const int day = gameflow.mWorldClock.mCurrentDay;
	if (day != sCrowdLastDay) {
		// A new day (or the first live tick): let the start-of-day withdraw
		// finish first (the harness's field=20 readiness marker must keep its
		// meaning), then top up once.
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
	std::printf("[m6a] crowd bootstrap: tick=%llu day=%d field=%d stocked=%d exit=%d queued=%d\n",
	    (unsigned long long)tick, day, field, add, want, itemMgr->getContainerExitCount());
	std::fflush(stdout);
}

void pc_snapshot_spike_game_sample(int* phase, int* pikis)
{
	*pikis = naviMgr ? fieldPikis() : 0;
	if (!naviMgr) *phase = 2;
	else if (gameflow.mIsDayEndActive || gameflow.mIsDayEndTriggered) *phase = 1;
	else *phase = 0;
}

uint64_t pc_snapshot_spike_game_audio_hash(void)
{
	uint64_t h = 0xcbf29ce484222325ull;
	auto mix   = [&h](uint64_t v) { h = (h ^ v) * 0x100000001b3ull; };
	mix(uint64_t(uint32_t(Jac_CheckFreeEvents())));
	if (seSystem) {
		const int n = SeSystemPeek::eventCount(seSystem);
		mix(uint64_t(uint32_t(n)));
		const int m = SeSystemPeek::maxEvents(seSystem);
		for (int i = 0; i < m && i < 64; ++i) mix(uint64_t(uint32_t(SeSystemPeek::handle(seSystem, i))));
	}
	return h;
}

void pc_snapshot_spike_game_perturb_input(void)
{
	PADStatus* pads = pc_netplay_pad_status();
	pads[0].stickX = static_cast<decltype(pads[0].stickX)>(-pads[0].stickX + 40);
	pads[0].stickY = static_cast<decltype(pads[0].stickY)>(-pads[0].stickY - 30);
	pads[0].button ^= PAD_BUTTON_A;
}

// Offsets of the clock/retrace members that the synctest residual diffs point
// at (F12: sys+0x430, and the DGXGraphics block allocated in main), printed
// once so the report can name them.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
void pc_snapshot_spike_game_describe(void)
{
	std::printf("[m6a] layout System: size=0x%zx mRetraceCount=0x%zx mPrevTick=0x%zx mFpsSampleStart=0x%zx "
	            "mFrameTicks=0x%zx mDeltaTime=0x%zx mFPS=0x%zx mEngineFrames=0x%zx mFramesAtSampleStart=0x%zx "
	            "mTotalFrames=0x%zx mIsRendering=0x%zx\n",
	    sizeof(System), offsetof(System, mRetraceCount), offsetof(System, mPrevTick), offsetof(System, mFpsSampleStart),
	    offsetof(System, mFrameTicks), offsetof(System, mDeltaTime), offsetof(System, mFPS),
	    offsetof(System, mEngineFrames), offsetof(System, mFramesAtSampleStart), offsetof(System, mTotalFrames),
	    offsetof(System, mIsRendering));
	std::printf("[m6a] layout DGXGraphics: size=0x%zx mDisplayBuffer=0x%zx mPostRetraceWaitCount=0x%zx "
	            "mRetraceCount=0x%zx mSystemFrameRate=0x%zx mRetraceCallback=0x%zx\n",
	    sizeof(DGXGraphics), offsetof(DGXGraphics, mDisplayBuffer), offsetof(DGXGraphics, mPostRetraceWaitCount),
	    offsetof(DGXGraphics, mRetraceCount), offsetof(DGXGraphics, mSystemFrameRate),
	    offsetof(DGXGraphics, mRetraceCallback));
	std::fflush(stdout);
}
#pragma GCC diagnostic pop
