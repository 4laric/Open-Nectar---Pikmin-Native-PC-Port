// Curated per-tick simulation hash (issue #878). See pc_state_hash.h for the
// exact format and field list.

#include "netplay/pc_state_hash.h"

#include "netplay/pc_input_log.h"

#include "Boss.h"
#include "GoalItem.h"
#include "ItemMgr.h"
#include "NaviMgr.h"
#include "Pellet.h"
#include "PikiMgr.h"
#include "PlayerState.h"
#include "gameflow.h"
#include "teki.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

// The m1-det lane owns these (pc_port/netplay/pc_netplay_det.*,
// pc_port/netplay/pc_sim_rng.*). Declared weak here so this TU links without
// that lane; each use is guarded by a null check.
#if defined(__GNUC__)
__attribute__((weak)) bool pc_netplay_deterministic(void);
__attribute__((weak)) unsigned pc_sim_rng_state(void);
__attribute__((weak)) int pc_sim_rand(void);
#else
bool pc_netplay_deterministic(void);
unsigned pc_sim_rng_state(void);
int pc_sim_rand(void);
#endif

namespace {
constexpr uint64_t kFnvOffset = 14695981039346656037ULL;
constexpr uint64_t kFnvPrime  = 1099511628211ULL;

uint64_t fnvMix(uint64_t h, const void* data, size_t len)
{
	const uint8_t* p = (const uint8_t*)data;
	for (size_t i = 0; i < len; ++i) {
		h ^= (uint64_t)p[i];
		h *= kFnvPrime;
	}
	return h;
}

void mixU64(uint64_t& h, uint64_t v)
{
	uint8_t b[8];
	for (int i = 0; i < 8; ++i) b[i] = (uint8_t)((v >> (i * 8)) & 0xFF);
	h = fnvMix(h, b, 8);
}

void mixU32(uint64_t& h, uint32_t v)
{
	uint8_t b[4];
	for (int i = 0; i < 4; ++i) b[i] = (uint8_t)((v >> (i * 8)) & 0xFF);
	h = fnvMix(h, b, 4);
}

void mixS32(uint64_t& h, int32_t v) { mixU32(h, (uint32_t)v); }

void mixF32(uint64_t& h, float v)
{
	uint32_t bits = 0;
	std::memcpy(&bits, &v, sizeof(bits));
	mixU32(h, bits);
}

void mixVec(uint64_t& h, const Vector3f& v)
{
	mixF32(h, v.x);
	mixF32(h, v.y);
	mixF32(h, v.z);
}

uint64_t sTick          = 0;
bool sInitialised       = false;
bool sLogActive         = false;
FILE* sLogFile          = nullptr;
uint64_t sExitAfter     = 0;
bool sExitAfterSet      = false;
bool sPrerollDone       = false;
const char* sArgvLog    = nullptr;
const char* sArgvExit   = nullptr;

uint64_t parseU64(const char* text, bool& ok)
{
	ok = false;
	if (text == nullptr || *text == '\0') return 0;
	char* end = nullptr;
	unsigned long long v = std::strtoull(text, &end, 10);
	if (end == text || *end != '\0') return 0;
	ok = true;
	return (uint64_t)v;
}

const char* argvValue(int argc, char** argv, const char* flag)
{
	for (int i = 1; i + 1 < argc; ++i) {
		if (std::strcmp(argv[i], flag) == 0) return argv[i + 1];
	}
	return nullptr;
}

uint64_t hashNavi(void)
{
	if (naviMgr == nullptr) return 0;
	uint64_t h = kFnvOffset;
	Iterator it(naviMgr);
	CI_LOOP(it)
	{
		Navi* n = static_cast<Navi*>(*it);
		if (n == nullptr) continue;
		mixS32(h, (int32_t)n->mNaviID);
		mixVec(h, n->mSRT.t);
		mixVec(h, n->mSRT.r);
		mixVec(h, n->mVelocity);
		mixF32(h, n->mHealth);
		AState<Navi>* st = n->getCurrState();
		mixS32(h, st != nullptr ? (int32_t)st->getID() : (int32_t)-1);
	}
	return h;
}

uint64_t hashPiki(void)
{
	if (pikiMgr == nullptr) return 0;
	uint64_t h = kFnvOffset;
	Iterator it(pikiMgr);
	CI_LOOP(it)
	{
		Piki* p = static_cast<Piki*>(*it);
		if (p == nullptr) continue;
		mixU32(h, (uint32_t)p->mColor);
		mixS32(h, (int32_t)p->mHappa);
		uint32_t species = (p->mP2Purple ? 1u : 0u) | (p->mP2White ? 2u : 0u) | (p->mP2Bulbmin ? 4u : 0u);
		mixU32(h, species);
		mixVec(h, p->mSRT.t);
		mixVec(h, p->mSRT.r);
		mixVec(h, p->mVelocity);
		mixF32(h, p->mHealth);
		AState<Piki>* st = p->getCurrState();
		mixS32(h, st != nullptr ? (int32_t)st->getID() : (int32_t)-1);
	}
	return h;
}

uint64_t hashTeki(void)
{
	if (tekiMgr == nullptr) return 0;
	uint64_t h = kFnvOffset;
	Iterator it(tekiMgr);
	CI_LOOP(it)
	{
		Teki* t = static_cast<Teki*>(*it);
		if (t == nullptr) continue;
		mixS32(h, (int32_t)t->mTekiType);
		mixVec(h, t->mSRT.t);
		mixVec(h, t->mSRT.r);
		mixVec(h, t->mVelocity);
		mixF32(h, t->mHealth);
		mixS32(h, (int32_t)t->mStateID);
	}
	return h;
}

uint64_t hashItem(void)
{
	uint64_t h = kFnvOffset;
	if (itemMgr != nullptr) {
		Iterator it(itemMgr);
		CI_LOOP(it)
		{
			Creature* c = *it;
			if (c == nullptr) continue;
			mixS32(h, (int32_t)c->mObjType);
			mixVec(h, c->mSRT.t);
			if (GoalItem* goal = dynamic_cast<GoalItem*>(c)) {
				mixU32(h, (uint32_t)goal->mOnionColour);
				mixU32(h, goal->mHeldPikis[0]);
				mixU32(h, goal->mHeldPikis[1]);
				mixU32(h, goal->mHeldPikis[2]);
			}
		}
		// Onion/container counts per colour, via the colour-indexed lookup
		// (0 when that colour has no Onion).
		for (int color = PikiMinColor; color <= PikiMaxColor; ++color) {
			GoalItem* onion = itemMgr->getContainer(color);
			if (onion == nullptr) {
				mixU32(h, 0);
				mixU32(h, 0);
				mixU32(h, 0);
			} else {
				mixU32(h, onion->mHeldPikis[0]);
				mixU32(h, onion->mHeldPikis[1]);
				mixU32(h, onion->mHeldPikis[2]);
			}
		}
	}
	if (pelletMgr != nullptr) {
		Iterator it(pelletMgr);
		CI_LOOP(it)
		{
			Pellet* p = static_cast<Pellet*>(*it);
			if (p == nullptr) continue;
			if (p->mConfig != nullptr) {
				mixU32(h, p->mConfig->mPelletId.mId);
				mixS32(h, (int32_t)p->mConfig->mPelletType());
			} else {
				mixU32(h, 0);
				mixS32(h, 0);
			}
			mixVec(h, p->mSRT.t);
			mixS32(h, (int32_t)p->getState());
			mixU32(h, (uint32_t)p->mCarrierCount);
		}
	}
	if (bossMgr != nullptr) {
		Iterator it(bossMgr);
		CI_LOOP(it)
		{
			Boss* b = static_cast<Boss*>(*it);
			if (b == nullptr) continue;
			mixS32(h, (int32_t)b->getCurrentState());
			mixS32(h, (int32_t)b->getNextState());
			mixF32(h, b->getCurrentLife());
			mixVec(h, b->mSRT.t);
		}
	}
	return h;
}

uint64_t hashWorld(void)
{
	uint64_t h = kFnvOffset;
	mixF32(h, gameflow.mWorldClock.mTimeOfDay);
	mixS32(h, (int32_t)gameflow.mWorldClock.mCurrentDay);
	if (playerState != nullptr) {
		mixS32(h, (int32_t)playerState->mLivingPikiNum);
		mixS32(h, (int32_t)playerState->mTotalBornPikiNum);
		mixS32(h, (int32_t)playerState->mTotalDeadPikiNum);
		mixS32(h, (int32_t)playerState->mTotalPluckedPikiCount);
	} else {
		mixS32(h, 0);
		mixS32(h, 0);
		mixS32(h, 0);
		mixS32(h, 0);
	}
	return h;
}

uint64_t hashRng(void)
{
#if defined(__GNUC__)
	if (pc_sim_rng_state != nullptr) return (uint64_t)pc_sim_rng_state();
#else
	(void)0;
#endif
	return 0;
}

void initOnce(void)
{
	sInitialised = true;
	const char* logPath = sArgvLog;
	const char* exitText = sArgvExit;
	if (logPath == nullptr) logPath = std::getenv("PIKMIN_STATE_HASH_LOG");
	if (exitText == nullptr) exitText = std::getenv("PIKMIN_NETPLAY_EXIT_AFTER_TICKS");
	if (logPath != nullptr && *logPath != '\0') {
		sLogFile = std::fopen(logPath, "w");
		if (sLogFile != nullptr) {
			std::setvbuf(sLogFile, nullptr, _IOFBF, 64 * 1024);
			sLogActive = true;
			std::printf("[netplay] state hash log: %s\n", logPath);
			std::fflush(stdout);
		} else {
			std::printf("[netplay] state hash log: cannot open %s\n", logPath);
			std::fflush(stdout);
		}
	}
	if (exitText != nullptr && *exitText != '\0') {
		bool ok = false;
		uint64_t n = parseU64(exitText, ok);
		if (ok && n > 0) {
			sExitAfter    = n;
			sExitAfterSet = true;
		}
	}
}
} // namespace

void pc_state_hash_notify_argv(int argc, char** argv)
{
	if (argv == nullptr) return;
	sArgvLog  = argvValue(argc, argv, "--state-hash-log");
	sArgvExit = argvValue(argc, argv, "--netplay-exit-after-ticks");
}

void pc_state_hash_before_first_tick(void)
{
	if (sPrerollDone) return;
	sPrerollDone = true;
	const char* text = std::getenv("PIKMIN_NETPLAY_TEST_PREROLL_RAND");
	if (text == nullptr || *text == '\0') return;
	bool ok     = false;
	uint64_t n  = parseU64(text, ok);
	if (!ok) return;
	for (uint64_t i = 0; i < n; ++i) {
		(void)std::rand();
#if defined(__GNUC__)
		if (pc_sim_rand != nullptr) (void)pc_sim_rand();
#endif
	}
}

uint64_t pc_state_hash_tick(void) { return sTick; }

void pc_state_hash_flush(void)
{
	if (sLogFile != nullptr) std::fflush(sLogFile);
}

void pc_state_hash_tick_end(void)
{
	if (!sInitialised) initOnce();
	++sTick;

	uint64_t navi  = hashNavi();
	uint64_t piki  = hashPiki();
	uint64_t teki  = hashTeki();
	uint64_t item  = hashItem();
	uint64_t world = hashWorld();
	uint64_t rng   = hashRng();
	uint64_t total = kFnvOffset;
	mixU64(total, navi);
	mixU64(total, piki);
	mixU64(total, teki);
	mixU64(total, item);
	mixU64(total, world);
	mixU64(total, rng);

	if (sLogActive) {
		std::fprintf(sLogFile, "%llu %016llx %016llx %016llx %016llx %016llx %016llx %016llx\n",
		             (unsigned long long)sTick, (unsigned long long)total, (unsigned long long)navi,
		             (unsigned long long)piki, (unsigned long long)teki, (unsigned long long)item,
		             (unsigned long long)world, (unsigned long long)rng);
		if (sTick % 300 == 0) std::fflush(sLogFile);
	}

	if (sExitAfterSet && sTick >= sExitAfter) {
		pc_state_hash_flush();
		pc_input_log_flush();
		std::fflush(stdout);
		// The main loop has no external quit-request API (it only breaks on
		// pc_window_should_close), so exit directly after flushing.
		std::exit(0);
	}
}
