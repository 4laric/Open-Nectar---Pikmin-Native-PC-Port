// Netplay M6a snapshot spike (issue #896). See pc_snapshot_spike.h.
//
// Measurement code, not production rollback: it favours visibility over
// speed and tolerates crashes. Nothing here runs unless the CMake option
// PIKMIN_NETPLAY_SNAPSHOT_SPIKE is ON *and* PIKMIN_NETPLAY_SNAPSHOT_SPIKE=1
// (the crowd bootstrap in pc_snapshot_spike_game.cpp is the one exception:
// it follows its own env var so a crowd run has a spike-off M1 twin).
//
// Rules this TU keeps:
//   * it never calls operator new (it is called from inside operator new):
//     all of its own storage comes from VirtualAlloc or the C heap;
//   * all of its mutable state lives in one VirtualAlloc'd struct, outside
//     both the sim region and the exe's .data/.bss bracket, so the globals
//     measurement does not see the spike and a synctest restore cannot roll
//     the spike back.
//
// Routing rule (main-thread SIM domain): an allocation goes to the region
// when it is made on the thread that called pc_snapshot_spike_init (main)
// and none of these hold:
//   - an infrastructure scope is open (presentation pass, doneRender, the
//     os_stubs thread/mutex/queue maps, the audio facade);
//   - the System::run loop has started and we are outside app->idle()
//     (input polling, input log, state hash, window events).
// Everything else (other threads, static init before main, the scopes
// above, region exhaustion) stays on malloc and is counted per category and
// per call site (return address of operator new).
//
// Modes and switches (env):
//   PIKMIN_NETPLAY_SNAPSHOT_SPIKE_MODE=timing   production save path only:
//       GetWriteWatch + undo/shadow copies + globals compare/save. No
//       content-identical restore, no periodic full copy, no coverage checks.
//       Default is the measurement mode (everything below).
//   PIKMIN_NETPLAY_SNAPSHOT_SPIKE_WW=0          region reserved without
//       MEM_WRITE_WATCH; the save copies an emulated dirty set of the same
//       size (..._NOWW_PAGES, default 740) so both A/B arms move the same
//       bytes (MV-8).
//   PIKMIN_NETPLAY_SNAPSHOT_SPIKE_WW_SPLIT=1    GetWriteWatch runs over the
//       touched extents ("hot", what a compact region would scan) and over
//       the rest of the zones ("cold") separately, both with RESET (MV-2).
//   PIKMIN_NETPLAY_SNAPSHOT_SPIKE_AUDIT=N       every N ticks: per-page hash
//       of every committed region page, compared with the previous audit;
//       a page that changed without write watch reporting it is a missed
//       write (MV-3). Reading a never-touched demand-zero page is itself
//       reported as a write, so the first audit dirties every committed
//       page once: audit runs check coverage, not dirty counts or timing.
//   PIKMIN_NETPLAY_SNAPSHOT_SPIKE_PTRSCAN=1     with AUDIT: track off-region
//       blocks and scan region/globals/off-region blocks for cross pointers
//       at each audit (MV-4).
//   PIKMIN_NETPLAY_SNAPSHOT_SPIKE_MALLOC_AUDIT=1 hook the exe's malloc,
//       calloc, realloc and free imports and count them by category and call
//       site (MV-4).
//   PIKMIN_NETPLAY_SNAPSHOT_SYNCTEST=k|cycle    crude synctest, k fixed or
//       cycling 1..7; ..._RESTORE=dedup|naive; ..._DIVERGE=1 perturbs the
//       first pass's inputs; ..._ANY=1 allows anchors over non-live ticks.

#include "netplay/pc_snapshot_spike.h"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#undef ERROR
#undef near
#undef far
#undef small
#endif

#include "netplay/pc_netplay_det.h"
#include "netplay/pc_state_hash.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

class NaviMgr;
extern NaviMgr* naviMgr;

// Hooks in other TUs, defined there when they are compiled with the spike
// define (CMake adds it to exactly those TUs whenever this one is built).
// Strong references on purpose: with LTO, a definition reached only through
// a weak reference was dropped from the link.
void pc_state_hash_spike_suppress_log(bool on);
unsigned long long piki_pc_spike_unknown_frees(void);
void piki_pc_spike_register_preserve(void);
// os_stubs.cpp defines these inside its extern "C" block.
extern "C" bool pc_os_stubs_static_arena(void** lo, size_t* bytes);
extern "C" void pc_os_stubs_spike_register_preserve(void);
// pc_snapshot_spike_game.cpp (engine headers live there).
void pc_snapshot_spike_game_tick(uint64_t tick);
void pc_snapshot_spike_game_sample(int* phase, int* pikis);
uint64_t pc_snapshot_spike_game_audio_hash(void);
void pc_snapshot_spike_game_perturb_input(void);
void pc_snapshot_spike_game_describe(void);

extern "C" char __data_start__[];
extern "C" char __data_end__[];
extern "C" char __bss_start__[];
extern "C" char __bss_end__[];

#if defined(_WIN32)

namespace {

// ---------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------
constexpr size_t kPage          = 4096;
constexpr size_t kReserve       = size_t(2) << 30; // 2 GB
constexpr size_t kPages         = kReserve / kPage;
constexpr size_t kMetaOff       = 0;
constexpr size_t kMetaCommit    = 64 << 10;
constexpr size_t kArenaOff      = size_t(1) << 20;
constexpr size_t kArenaSize     = size_t(256) << 20;
constexpr size_t kSmallOff      = size_t(512) << 20;
constexpr size_t kSmallEnd      = size_t(1024) << 20;
constexpr size_t kLargeOff      = size_t(1024) << 20;
constexpr size_t kLargeEnd      = kReserve;
constexpr size_t kGranule       = size_t(1) << 20;
constexpr size_t kGranules      = kReserve / kGranule;
constexpr size_t kSmallMax      = 32768;
constexpr size_t kLargeSplitMin = 64 << 10;
constexpr uint32_t kMagicLive   = 0x53504B4Cu; // "SPKL"
constexpr uint32_t kMagicFreeS  = 0x53504B53u; // "SPKS"
constexpr uint32_t kMagicFreeL  = 0x53504B46u; // "SPKF"
constexpr uint32_t kLargeCls    = 0xFFFFu;
constexpr int kNumClasses       = 44;
constexpr int kUndoRing         = 16; // ticks of undo history kept
constexpr size_t kUndoPoolPages = 65536; // 256 MB reserved, committed lazily
constexpr int kFullEvery        = 30; // full-copy measurement period (ticks)
constexpr int kMaxK             = 8;
constexpr int kPreserveMax      = 16384;
constexpr size_t kHotGapPages   = 64; // touched extents closer than this merge
constexpr size_t kMaxRuns       = 65536;
constexpr size_t kSmallUnits    = (kSmallEnd - kSmallOff) / 16;
constexpr size_t kLargePages    = (kLargeEnd - kLargeOff) / kPage;
constexpr size_t kOffTableCap   = size_t(1) << 20;
constexpr size_t kPairCap       = size_t(1) << 16;

const uintptr_t kPreferredBases[] = {
	0x00000E0000000000ull,
	0x00000D0000000000ull,
	0x0000060000000000ull,
};

struct BlockHdr {
	uint32_t magic;
	uint32_t cls;
	uint64_t cap; // payload bytes (small, live large); whole span (free large)
};
static_assert(sizeof(BlockHdr) == 16, "block header must keep 16-byte payload alignment");

struct LargeFree {
	BlockHdr h;
	LargeFree* next;
	LargeFree* prev;
};

// Allocator metadata: lives at the region base, so it is snapshotted and
// restored together with the blocks it describes.
struct Meta {
	uint64_t magic;
	uint8_t* smallWild;
	uint8_t* smallCommitted;
	uint8_t* largeWild;
	void* smallFree[kNumClasses];
	LargeFree* largeHead; // address ordered
	uint64_t liveBytes;
	uint64_t liveBlocks;
	uint8_t* largeCommitHw; // large zone committed up to here; never decommitted (F8)
};

enum OffCategory {
	kOffPreInit = 0,     // before pc_snapshot_spike_init (static init)
	kOffOtherThread,     // not the main thread
	kOffPresent,         // presentation pass scope
	kOffDoneRender,      // doneRender scope
	kOffInfraOther,      // other infra scope (os_stubs maps, audio facade)
	kOffOutsideIdle,     // main loop, outside app->idle()
	kOffRegionFull,      // region exhausted
	kOffCount
};
const char* const kOffNames[kOffCount] = {
	"pre_init", "other_thread", "present", "done_render", "infra_other", "outside_idle", "region_full",
};

// malloc audit categories (MV-4)
enum MallocCategory {
	kMallocOtherThread = 0,
	kMallocMainInfra,      // main thread, infra scope open
	kMallocMainOutsideIdle,
	kMallocMainSim,        // main thread, inside idle, no infra scope: SIM domain
	kMallocMainPreLoop,    // main thread before System::run (boot)
	kMallocCount
};
const char* const kMallocNames[kMallocCount] = {
	"other_thread", "main_infra", "main_outside_idle", "main_sim", "main_boot",
};

const char* const kUnknownNames[4] = { "arena_zone", "image", "private_heap", "other" };

struct Site {
	uintptr_t ra;
	uint64_t count;
	uint64_t bytes;
	uint32_t cat;
	uint32_t pad;
};
constexpr size_t kSites = 16384;

struct Range {
	uint8_t* lo;
	uint8_t* hi;
};

struct PreserveSeg {
	uint32_t page; // global page index
	uint16_t off;
	uint16_t len;
};

struct UndoTick {
	uint64_t tick;
	uint64_t rStart; // absolute slot counter in the region undo pool
	uint32_t rCount;
	uint64_t gStart;
	uint32_t gCount;
	bool valid;
};

struct Run {
	uint8_t* lo;
	uint8_t* hi;
	uint8_t zone;
	uint8_t hot;
};

struct OffBlock {
	uintptr_t ptr; // 0 = empty slot
	uintptr_t size;
	uintptr_t ra;
};

struct Pair {
	uintptr_t src;
	uintptr_t dst;
	uint64_t count;
	uint32_t kind; // 0 empty, 1 region->off, 2 globals->off, 3 off->region
	uint32_t pad;
};

struct SyncResult {
	uint64_t anchor;
	int k;
	int firstBad;     // 0 = match; else 1-based offset of first mismatching tick
	unsigned badMask; // sub-hash columns that differed at firstBad
	int audioBad;     // 0 = audio hash matched every step; else first step
	uint32_t regionDiffPages;
	uint32_t regionUnreported; // differing pages write watch never reported
	uint32_t globalDiffPages;
	uint32_t globalPresDiffPages; // preserved bytes differing (not restored)
	uint32_t regionUnion;
	uint32_t globalUnion;
	uint32_t onlyFirst;  // pages dirtied in the first pass but not the resim
	uint32_t onlySecond; // pages dirtied in the resim but not the first pass
};

struct State {
	// switches
	bool active;
	bool loopStarted;
	bool inIdle;
	bool authMarked;
	bool timing;     // production save path only
	bool doRestore;  // content-identical real restore each tick (measure mode)
	bool writeWatch; // region reserved with MEM_WRITE_WATCH and measured (default on)
	bool splitAuth;  // extra GetWriteWatch at the end of the auth pass (default off)
	bool wwSplit;    // hot/cold GetWriteWatch split (MV-2)
	bool offTrack;   // track off-region blocks for the pointer scan
	bool mallocAudit;
	uint32_t nowwPages;
	int auditEvery;
	bool ptrScan;
	double wwZoneMs[3];
	double wwHotMs, wwColdMs;
	uint32_t wwHotCalls, wwColdCalls;
	uint32_t rdHot, rdCold;
	double hotMb, coldMb;
	uint32_t wwCalls;
	void* lastDeleteRa; // main thread: return address of the delete in flight
	uint64_t unknownByKind[4];
	int infraDepth;
	int infraCat;
	DWORD mainTid;
	SRWLOCK lock;
	SRWLOCK siteLock;
	SRWLOCK offLock;

	// region
	uint8_t* base;
	uint8_t* shadow;
	uint8_t* scratch; // full-copy target
	Meta* meta;
	uint32_t caps[kNumClasses];
	bool shadowCommitted[kGranules];
	bool scratchCommitted[kGranules];
	uintptr_t* smallSite; // per 16-byte unit of the small zone: alloc site at a block header
	bool smallSiteCommitted[(kSmallUnits * sizeof(uintptr_t)) / kGranule + 1];
	uintptr_t* largeSite; // per page of the large zone: alloc site at a live block header

	// write watch
	void** wwAddrs;
	ULONG_PTR wwCap;
	uint32_t* pageMark; // epoch per region page
	uint32_t* dirtyList;
	uint32_t dirtyCount;
	uint32_t epoch;
	uint64_t* touched; // bitmap: pages ever reported written
	bool runsStale;
	Run* runs;
	uint32_t runCount;
	uint32_t* carryList; // pages written after the tick's collect (ww_late)
	uint32_t* restoreTmp; // measurement-mode restore list
	uint8_t* runsSmallHi; // zone bounds the hot/cold runs were built for
	uint8_t* runsLargeHi;
	uint32_t carryCount;
	uint64_t* dirtySince; // audit: pages reported since the last audit
	uint32_t rdAuth;
	uint32_t rdPost;
	double wwMs;
	uint32_t nowwCursor;

	// undo pools
	uint8_t* rUndo;       // kUndoPoolPages pages
	uint32_t* rUndoIdx;   // page index per slot
	bool rUndoCommitted[(kUndoPoolPages * kPage) / kGranule];
	uint64_t rUndoCursor; // absolute
	uint8_t* gUndo;
	uint32_t* gUndoIdx;
	uint64_t gUndoPages;
	uint64_t gUndoCursor;
	UndoTick ring[kUndoRing];
	bool barrier; // set by oversize ticks; cleared each tick end

	// globals
	Range glob[4];
	int nGlob;
	uint64_t globPages;
	uint64_t globBytes;
	uint8_t* globShadow;
	uint8_t** globPagePtr; // page index -> address
	uint32_t* globDirty;
	uint32_t globDirtyCount;
	PreserveSeg* preserveSegs; // sorted by page after a rebuild
	uint32_t preserveCount;
	uint32_t preserveCap;
	uint32_t* presFirst; // per global page: first seg index
	uint16_t* presCount; // per global page: seg count (0 = not preserved)
	Range* preserveRaw; // kPreserveMax entries
	int preserveRawCount;
	bool preserveBatch; // loading a preserve file: rebuild the segments once at the end

	// timing
	double msPerCount;
	int64_t tFrame;
	int64_t tIdle;
	int64_t tAuth;
	int64_t tPresentEnd;
	int64_t tDoneBegin;
	int64_t tDoneEnd;
	int64_t tParseEnd;
	int64_t tIdleEnd;
	double instrInIdleMs;
	// machine load (MV-6)
	uint64_t loadIdle, loadKernel, loadUser;
	double sysBusy;

	// counters (this tick)
	uint64_t allocs, allocBytes, frees, freeBytes;
	uint64_t offAllocs, offBytes;
	uint64_t regionFreesOffMain;
	uint64_t badFrees;
	uint32_t wwLate, concRegion, concGlobal;
	int64_t missed; // audit: -1 = no audit this tick
	// totals
	volatile LONG64 offCount[kOffCount];
	volatile LONG64 offBytesTotal[kOffCount];
	volatile LONG64 mallocCount[kMallocCount];
	volatile LONG64 mallocBytes[kMallocCount];
	volatile LONG64 freeCalls;
	uint64_t totalAllocs, totalAllocBytes, totalFrees, totalFreeBytes;
	uint64_t totalRegionFreesOffMain;
	uint64_t totalWwLate, totalConcRegion, totalConcGlobal;
	uint64_t audits, auditMissed, auditPages;
	uint64_t smallHwBytes;

	Site* regionSites;
	Site* offSites;
	Site* unknownSites;
	Site* mallocSites;
	uint64_t siteOverflow;

	// off-region block table (pointer scan)
	OffBlock* offTable;
	uint64_t offLive;
	uint64_t offOverflow;
	Pair* pairs;
	OffBlock* offSorted;

	// audit hashes
	uint64_t* pageHash;
	uint64_t* auditHave; // bitmap: pageHash valid
	uint64_t zeroHash;
	FILE* auditLog;
	FILE* gdirtyLog;

	FILE* csv;
	FILE* syncCsv;
	uint64_t ticksLogged;

	// synctest
	int syncKArg; // 0 off, 1..kMaxK fixed, -1 cycle 1..7
	int syncK;    // this test's k
	int syncPeriod;
	uint64_t syncStart;
	uint64_t syncEnd; // no new anchor from here (the run exits soon)
	bool syncDedup;
	bool syncDiverge;
	bool syncAny;
	bool syncFullHash;
	int syncPhase; // 0 idle, 1 first pass, 2 resim
	uint64_t syncAnchor;
	int syncStep;
	uint64_t syncHash[kMaxK + 1][8];
	uint64_t syncAudio[kMaxK + 1];
	uint8_t* syncFull;       // first-pass end copy of every touched region page
	bool syncFullCommitted[kGranules];
	uint64_t* syncTouched;   // touched bitmap at first-pass end
	uint64_t* syncPageHash;  // full-hash mode: first-pass end hash of every committed page
	uint8_t* syncGFull;      // first-pass end copy of every global page
	uint32_t* syncMark;      // per region page: 1 first pass, 2 resim, 3 both
	uint8_t* syncGMark;
	uint32_t* rStamp;        // dedup restore: per region page
	uint32_t* gStamp;        // dedup restore: per global page
	uint32_t stamp;
	uint32_t* restoredList;
	uint64_t syncTests, syncMatches, syncAborted, syncAudioBadTests, syncTestIndex;
	uint64_t syncTestsK[kMaxK + 1], syncMatchesK[kMaxK + 1];
	int syncFirstBad;
	unsigned syncBadMask;
	int syncAudioBad;
	uint64_t syncNextAt;
	double rsTotalMs, rsRegionMs, rsShadowMs, rsGlobMs, rsWwMs;
	uint32_t rsRegionPages, rsGlobPages, rsEntries;
};

State* S = nullptr; // set once by init; the pointee lives outside every bracket

long long sPreInitCount = 0;
long long sPreInitBytes = 0;

inline int64_t now()
{
	LARGE_INTEGER t;
	QueryPerformanceCounter(&t);
	return t.QuadPart;
}

inline double msSince(int64_t t0) { return double(now() - t0) * S->msPerCount; }
inline double msBetween(int64_t a, int64_t b) { return double(b - a) * S->msPerCount; }

inline bool onMain() { return GetCurrentThreadId() == S->mainTid; }

inline size_t alignUp(size_t v, size_t a) { return (v + a - 1) & ~(a - 1); }

inline bool bitGet(const uint64_t* b, size_t i) { return (b[i >> 6] >> (i & 63)) & 1; }
inline void bitSet(uint64_t* b, size_t i) { b[i >> 6] |= 1ull << (i & 63); }

void* osReserve(size_t bytes)
{
	return VirtualAlloc(nullptr, bytes, MEM_RESERVE, PAGE_READWRITE);
}

bool osCommit(void* p, size_t bytes)
{
	return VirtualAlloc(p, bytes, MEM_COMMIT, PAGE_READWRITE) != nullptr;
}

void* osAllocZero(size_t bytes)
{
	return VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
}

void ensureGranules(uint8_t* poolBase, bool* committed, size_t granules, size_t offset, size_t bytes)
{
	size_t g0 = offset / kGranule;
	size_t g1 = (offset + bytes - 1) / kGranule;
	for (size_t g = g0; g <= g1 && g < granules; ++g) {
		if (!committed[g]) {
			if (!osCommit(poolBase + g * kGranule, kGranule)) {
				std::fprintf(stderr, "[m6a] commit failed at pool %p granule %zu\n", (void*)poolBase, g);
				std::abort();
			}
			committed[g] = true;
		}
	}
}

uintptr_t exeBase() { return reinterpret_cast<uintptr_t>(GetModuleHandleA(nullptr)); }

void moduleOf(uintptr_t addr, char* name, size_t cap, uintptr_t* modBase)
{
	HMODULE mod = nullptr;
	std::strncpy(name, "?", cap);
	GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
	    reinterpret_cast<LPCSTR>(addr), &mod);
	*modBase = reinterpret_cast<uintptr_t>(mod);
	if (mod) {
		GetModuleFileNameA(mod, name, DWORD(cap));
		const char* slash = std::strrchr(name, '\\');
		if (slash) std::memmove(name, slash + 1, std::strlen(slash + 1) + 1);
	}
}

// ---------------------------------------------------------------------------
// Call-site tables
// ---------------------------------------------------------------------------
void noteSite(Site* table, uintptr_t ra, size_t bytes, uint32_t cat)
{
	AcquireSRWLockExclusive(&S->siteLock);
	uintptr_t h = (ra >> 3) * 0x9E3779B97F4A7C15ull;
	size_t i    = (h >> 40) & (kSites - 1);
	for (size_t probe = 0; probe < kSites; ++probe) {
		Site& s = table[(i + probe) & (kSites - 1)];
		if (s.ra == ra && s.cat == cat) {
			s.count++;
			s.bytes += bytes;
			ReleaseSRWLockExclusive(&S->siteLock);
			return;
		}
		if (s.ra == 0) {
			s.ra    = ra;
			s.cat   = cat;
			s.count = 1;
			s.bytes = bytes;
			ReleaseSRWLockExclusive(&S->siteLock);
			return;
		}
	}
	S->siteOverflow++;
	ReleaseSRWLockExclusive(&S->siteLock);
}

// ---------------------------------------------------------------------------
// Off-region block table (linear probing, backward-shift delete)
// ---------------------------------------------------------------------------
inline size_t offSlot(uintptr_t p) { return size_t(((p >> 4) * 0x9E3779B97F4A7C15ull) >> 44) & (kOffTableCap - 1); }

void offInsert(void* ptr, size_t size, uintptr_t ra)
{
	if (!ptr) return;
	const uintptr_t p = reinterpret_cast<uintptr_t>(ptr);
	AcquireSRWLockExclusive(&S->offLock);
	if (S->offLive >= kOffTableCap - kOffTableCap / 8) {
		S->offOverflow++;
		ReleaseSRWLockExclusive(&S->offLock);
		return;
	}
	size_t i = offSlot(p);
	while (S->offTable[i].ptr && S->offTable[i].ptr != p) i = (i + 1) & (kOffTableCap - 1);
	if (!S->offTable[i].ptr) S->offLive++;
	S->offTable[i] = { p, size, ra };
	ReleaseSRWLockExclusive(&S->offLock);
}

void offRemove(void* ptr)
{
	if (!ptr) return;
	const uintptr_t p = reinterpret_cast<uintptr_t>(ptr);
	AcquireSRWLockExclusive(&S->offLock);
	size_t i = offSlot(p);
	while (S->offTable[i].ptr && S->offTable[i].ptr != p) i = (i + 1) & (kOffTableCap - 1);
	if (S->offTable[i].ptr == p) {
		S->offLive--;
		size_t hole = i;
		size_t j    = i;
		for (;;) {
			j = (j + 1) & (kOffTableCap - 1);
			if (!S->offTable[j].ptr) break;
			const size_t home = offSlot(S->offTable[j].ptr);
			// move j into the hole when its home is not in (hole, j]
			const bool inRange = hole <= j ? (home > hole && home <= j) : (home > hole || home <= j);
			if (!inRange) {
				S->offTable[hole] = S->offTable[j];
				hole              = j;
			}
		}
		S->offTable[hole] = { 0, 0, 0 };
	}
	ReleaseSRWLockExclusive(&S->offLock);
}

// ---------------------------------------------------------------------------
// malloc import hooks (MV-4)
// ---------------------------------------------------------------------------
typedef void* (*MallocFn)(size_t);
typedef void* (*CallocFn)(size_t, size_t);
typedef void* (*ReallocFn)(void*, size_t);
typedef void (*FreeFn)(void*);
MallocFn gRealMalloc   = nullptr;
CallocFn gRealCalloc   = nullptr;
ReallocFn gRealRealloc = nullptr;
FreeFn gRealFree       = nullptr;

void mallocNote(void* p, size_t n, void* ra)
{
	State* s = S;
	if (!s || !s->active || !p) return;
	int cat;
	if (!onMain()) cat = kMallocOtherThread;
	else if (s->infraDepth > 0) cat = kMallocMainInfra;
	else if (!s->loopStarted) cat = kMallocMainPreLoop;
	else if (!s->inIdle) cat = kMallocMainOutsideIdle;
	else cat = kMallocMainSim;
	InterlockedIncrement64(&s->mallocCount[cat]);
	InterlockedAdd64(&s->mallocBytes[cat], LONG64(n));
	noteSite(s->mallocSites, reinterpret_cast<uintptr_t>(ra), n, uint32_t(cat));
	if (s->offTrack) offInsert(p, n, reinterpret_cast<uintptr_t>(ra));
}

void* hookMalloc(size_t n)
{
	void* p = gRealMalloc(n);
	mallocNote(p, n, __builtin_return_address(0));
	return p;
}

void* hookCalloc(size_t a, size_t b)
{
	void* p = gRealCalloc(a, b);
	mallocNote(p, a * b, __builtin_return_address(0));
	return p;
}

void* hookRealloc(void* old, size_t n)
{
	if (old && S && S->offTrack) offRemove(old);
	void* p = gRealRealloc(old, n);
	mallocNote(p, n, __builtin_return_address(0));
	return p;
}

void hookFree(void* p)
{
	if (p && S) {
		InterlockedIncrement64(&S->freeCalls);
		if (S->offTrack) offRemove(p);
	}
	gRealFree(p);
}

int patchImports()
{
	uint8_t* mod    = reinterpret_cast<uint8_t*>(GetModuleHandleA(nullptr));
	auto* dos       = reinterpret_cast<IMAGE_DOS_HEADER*>(mod);
	auto* nt        = reinterpret_cast<IMAGE_NT_HEADERS*>(mod + dos->e_lfanew);
	const auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
	if (!dir.VirtualAddress) return 0;
	int patched = 0;
	for (auto* imp = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(mod + dir.VirtualAddress); imp->Name; ++imp) {
		auto* orig  = reinterpret_cast<IMAGE_THUNK_DATA*>(mod + (imp->OriginalFirstThunk ? imp->OriginalFirstThunk : imp->FirstThunk));
		auto* thunk = reinterpret_cast<IMAGE_THUNK_DATA*>(mod + imp->FirstThunk);
		for (; orig->u1.AddressOfData; ++orig, ++thunk) {
			if (IMAGE_SNAP_BY_ORDINAL(orig->u1.Ordinal)) continue;
			const char* name = reinterpret_cast<const char*>(reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(mod + orig->u1.AddressOfData)->Name);
			void* repl       = nullptr;
			void** real      = nullptr;
			if (!std::strcmp(name, "malloc")) {
				repl = reinterpret_cast<void*>(&hookMalloc);
				real = reinterpret_cast<void**>(&gRealMalloc);
			} else if (!std::strcmp(name, "calloc")) {
				repl = reinterpret_cast<void*>(&hookCalloc);
				real = reinterpret_cast<void**>(&gRealCalloc);
			} else if (!std::strcmp(name, "realloc")) {
				repl = reinterpret_cast<void*>(&hookRealloc);
				real = reinterpret_cast<void**>(&gRealRealloc);
			} else if (!std::strcmp(name, "free")) {
				repl = reinterpret_cast<void*>(&hookFree);
				real = reinterpret_cast<void**>(&gRealFree);
			}
			if (!repl || *real) continue;
			DWORD old = 0;
			if (!VirtualProtect(&thunk->u1.Function, sizeof(thunk->u1.Function), PAGE_READWRITE, &old)) continue;
			*real               = reinterpret_cast<void*>(thunk->u1.Function);
			thunk->u1.Function  = reinterpret_cast<ULONG_PTR>(repl);
			VirtualProtect(&thunk->u1.Function, sizeof(thunk->u1.Function), old, &old);
			++patched;
		}
	}
	return patched;
}

// ---------------------------------------------------------------------------
// Region allocator
// ---------------------------------------------------------------------------
void buildClasses()
{
	int n = 0;
	for (uint32_t c = 16; c <= 256; c += 16) S->caps[n++] = c;
	for (uint32_t b = 256; b < kSmallMax; b *= 2) {
		for (uint32_t q = 1; q <= 4; ++q) S->caps[n++] = b + b * q / 4;
	}
	if (n != kNumClasses) {
		std::fprintf(stderr, "[m6a] class table size %d != %d\n", n, kNumClasses);
		std::abort();
	}
}

uint32_t classOf(size_t n)
{
	if (n <= 256) return uint32_t((n + 15) / 16) - 1;
	uint32_t lo = 16, hi = kNumClasses - 1;
	while (lo < hi) {
		uint32_t mid = (lo + hi) / 2;
		if (S->caps[mid] >= n) hi = mid;
		else lo = mid + 1;
	}
	return lo;
}

void noteSmallSite(uint8_t* hdr, uintptr_t ra)
{
	const size_t unit = size_t(hdr - (S->base + kSmallOff)) / 16;
	const size_t off  = unit * sizeof(uintptr_t);
	ensureGranules(reinterpret_cast<uint8_t*>(S->smallSite), S->smallSiteCommitted,
	    sizeof(S->smallSiteCommitted), off, sizeof(uintptr_t));
	S->smallSite[unit] = ra ? ra : 1;
}

// Zero [p, end) (end page aligned, p may be mid-page) without writing pages
// that were never written: a page write watch never reported and did not
// report since the last reset is still the zero page it was committed as.
// Such pages are not even read: on Windows a first read of a demand-zero
// page in a MEM_WRITE_WATCH region is reported as a write, so reading every
// untouched page would dirty (and physically commit) all of them.
void zeroSpan(uint8_t* p, size_t bytes)
{
	uint8_t* end = p + bytes;
	uint8_t* lo  = reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(p) & ~uintptr_t(kPage - 1));
	ULONG_PTR count = 0;
	bool known      = false;
	if (S->writeWatch && end > lo) {
		count      = S->wwCap;
		DWORD gran = 0;
		known      = GetWriteWatch(0, lo, size_t(end - lo), S->wwAddrs, &count, &gran) == 0;
	}
	ULONG_PTR k = 0;
	while (p < end) {
		uint8_t* pageLo = reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(p) & ~uintptr_t(kPage - 1));
		uint8_t* pageHi = pageLo + kPage < end ? pageLo + kPage : end;
		if (p != pageLo || pageHi != pageLo + kPage) {
			std::memset(p, 0, size_t(pageHi - p));
		} else {
			bool written = !known || bitGet(S->touched, size_t(pageLo - S->base) / kPage);
			if (!written) {
				while (k < count && static_cast<uint8_t*>(S->wwAddrs[k]) < pageLo) ++k;
				written = k < count && static_cast<uint8_t*>(S->wwAddrs[k]) == pageLo;
			}
			if (written) std::memset(pageLo, 0, kPage);
		}
		p = pageHi;
	}
}

void largeUnlink(LargeFree* f)
{
	if (f->prev) f->prev->next = f->next;
	else S->meta->largeHead = f->next;
	if (f->next) f->next->prev = f->prev;
}

// Large spans stay committed once committed (F8): a free no longer
// decommits, so it no longer makes the tick non-rollbackable. A reused span
// is zero-filled instead (the sim relies on zeroed blocks, as before).
void* largeAlloc(size_t n, uintptr_t ra)
{
	Meta* m           = S->meta;
	const size_t need = alignUp(sizeof(BlockHdr) + n, kPage);
	LargeFree* best   = nullptr;
	for (LargeFree* f = m->largeHead; f; f = f->next) {
		if (f->h.cap >= need && (!best || f->h.cap < best->h.cap)) best = f;
	}
	uint8_t* blk = nullptr;
	size_t span  = 0;
	if (best) {
		blk          = reinterpret_cast<uint8_t*>(best);
		span         = best->h.cap;
		LargeFree* p = best->prev;
		LargeFree* x = best->next;
		largeUnlink(best);
		if (span - need >= kLargeSplitMin) {
			uint8_t* rem = blk + need;
			LargeFree* r = reinterpret_cast<LargeFree*>(rem);
			r->h.magic   = kMagicFreeL;
			r->h.cls     = kLargeCls;
			r->h.cap     = span - need;
			r->prev      = p;
			r->next      = x;
			if (p) p->next = r;
			else m->largeHead = r;
			if (x) x->prev = r;
			span = need;
		}
		zeroSpan(blk + sizeof(BlockHdr), span - sizeof(BlockHdr));
	} else {
		blk = m->largeWild;
		if (blk + need > S->base + kLargeEnd) return nullptr;
		span           = need;
		uint8_t* oldHw = m->largeCommitHw;
		if (blk + need > oldHw) {
			uint8_t* from = blk > oldHw ? blk : oldHw;
			if (!osCommit(from, size_t(blk + need - from))) return nullptr;
			m->largeCommitHw = blk + need;
		}
		// Below the old commit high-water mark the pages may hold a freed
		// block's bytes (a tail given back to the wilderness): zero them.
		// Pages committed just now are zero already.
		uint8_t* dirtyEnd = oldHw < blk + need ? oldHw : blk + need;
		if (dirtyEnd > blk) zeroSpan(blk, size_t(dirtyEnd - blk));
		m->largeWild += need;
	}
	BlockHdr* h = reinterpret_cast<BlockHdr*>(blk);
	h->magic    = kMagicLive;
	h->cls      = kLargeCls;
	h->cap      = span - sizeof(BlockHdr);
	m->liveBytes += h->cap;
	m->liveBlocks++;
	S->largeSite[size_t(blk - (S->base + kLargeOff)) / kPage] = ra ? ra : 1;
	return h + 1;
}

void largeFree(BlockHdr* h)
{
	Meta* m      = S->meta;
	uint8_t* blk = reinterpret_cast<uint8_t*>(h);
	size_t span  = h->cap + sizeof(BlockHdr);
	m->liveBytes -= h->cap;
	m->liveBlocks--;
	S->largeSite[size_t(blk - (S->base + kLargeOff)) / kPage] = 0;
	LargeFree* f = reinterpret_cast<LargeFree*>(blk);
	f->h.magic   = kMagicFreeL;
	f->h.cls     = kLargeCls;
	f->h.cap     = span;
	// address-ordered insert
	LargeFree* prev = nullptr;
	LargeFree* next = m->largeHead;
	while (next && reinterpret_cast<uint8_t*>(next) < blk) {
		prev = next;
		next = next->next;
	}
	f->prev = prev;
	f->next = next;
	if (prev) prev->next = f;
	else m->largeHead = f;
	if (next) next->prev = f;
	// coalesce forward
	if (next && blk + f->h.cap == reinterpret_cast<uint8_t*>(next)) {
		f->h.cap += next->h.cap;
		largeUnlink(next);
	}
	// coalesce backward
	if (prev && reinterpret_cast<uint8_t*>(prev) + prev->h.cap == blk) {
		prev->h.cap += f->h.cap;
		largeUnlink(f);
		f = prev;
	}
	// give the tail back to the wilderness (it stays committed)
	if (reinterpret_cast<uint8_t*>(f) + f->h.cap == m->largeWild) {
		largeUnlink(f);
		m->largeWild = reinterpret_cast<uint8_t*>(f);
	}
}

void* regionAlloc(size_t n, uintptr_t ra)
{
	if (n == 0) n = 1;
	if (n > kSmallMax) return largeAlloc(n, ra);
	Meta* m          = S->meta;
	const uint32_t c = classOf(n);
	const size_t cap = S->caps[c];
	void* p          = m->smallFree[c];
	if (p) {
		m->smallFree[c] = *static_cast<void**>(p);
		BlockHdr* h     = static_cast<BlockHdr*>(p) - 1;
		h->magic        = kMagicLive;
		std::memset(p, 0, cap);
		noteSmallSite(reinterpret_cast<uint8_t*>(h), ra);
	} else {
		const size_t need = sizeof(BlockHdr) + cap;
		uint8_t* at       = m->smallWild;
		if (at + need > S->base + kSmallEnd) return nullptr;
		if (at + need > m->smallCommitted) {
			uint8_t* target = S->base + alignUp(size_t(at + need - S->base), kGranule);
			if (!osCommit(m->smallCommitted, size_t(target - m->smallCommitted))) return nullptr;
			m->smallCommitted = target;
		}
		BlockHdr* h = reinterpret_cast<BlockHdr*>(at);
		h->magic    = kMagicLive;
		h->cls      = c;
		h->cap      = cap;
		m->smallWild = at + need;
		p            = h + 1; // fresh committed memory is already zero
		noteSmallSite(at, ra);
	}
	m->liveBytes += cap;
	m->liveBlocks++;
	return p;
}

// Returns freed payload bytes. A bad pointer inside the region aborts (F7).
size_t regionFree(void* p, void* ra)
{
	BlockHdr* h = static_cast<BlockHdr*>(p) - 1;
	if (h->magic != kMagicLive) {
		S->badFrees++;
		std::printf("[m6a] FATAL bad region free %p magic=0x%08x caller rva=0x%llx tick=%llu\n", p, h->magic,
		    (unsigned long long)(reinterpret_cast<uintptr_t>(ra) - exeBase()), (unsigned long long)pc_state_hash_tick());
		std::fflush(stdout);
		std::abort();
	}
	const size_t cap = h->cap;
	if (h->cls == kLargeCls) {
		largeFree(h);
		return cap;
	}
	Meta* m          = S->meta;
	h->magic         = kMagicFreeS;
	*static_cast<void**>(p) = m->smallFree[h->cls];
	m->smallFree[h->cls]    = p;
	m->liveBytes -= cap;
	m->liveBlocks--;
	return cap;
}

// Allocation site of the region block that holds region offset off; 1 for
// the arena zone (AyuHeap sys heap, not attributed); 0 when not in a block.
uintptr_t regionSiteOf(size_t off, size_t* blockOff)
{
	*blockOff = 0;
	if (off < kSmallOff) return 1;
	if (off < kSmallEnd) {
		uint8_t* p = S->base + off;
		if (p >= S->meta->smallWild) return 0;
		size_t unit = (off - kSmallOff) / 16;
		for (size_t back = 0; back <= kSmallMax / 16 + 1 && unit - back < kSmallUnits; ++back) {
			const size_t u = unit - back;
			const size_t g = (u * sizeof(uintptr_t)) / kGranule;
			if (!S->smallSiteCommitted[g]) return 0;
			if (S->smallSite[u]) {
				*blockOff = off - (kSmallOff + u * 16);
				return S->smallSite[u];
			}
			if (u == 0) break;
		}
		return 0;
	}
	size_t pg = (off - kLargeOff) / kPage;
	for (size_t back = 0; back <= pg; ++back) {
		const size_t q = pg - back;
		if (S->largeSite[q]) {
			const BlockHdr* h = reinterpret_cast<const BlockHdr*>(S->base + kLargeOff + q * kPage);
			const size_t hdrOff = kLargeOff + q * kPage;
			if (off < hdrOff + sizeof(BlockHdr) + h->cap) {
				*blockOff = off - hdrOff;
				return S->largeSite[q];
			}
			return 0;
		}
	}
	return 0;
}

// ---------------------------------------------------------------------------
// Globals bracket
// ---------------------------------------------------------------------------
void addGlobRange(uint8_t* lo, uint8_t* hi)
{
	lo = reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(lo) & ~uintptr_t(kPage - 1));
	hi = reinterpret_cast<uint8_t*>(alignUp(reinterpret_cast<uintptr_t>(hi), kPage));
	if (hi > lo && S->nGlob < 4) S->glob[S->nGlob++] = { lo, hi };
}

void setupGlobals()
{
	uint8_t* dLo = reinterpret_cast<uint8_t*>(__data_start__);
	uint8_t* dHi = reinterpret_cast<uint8_t*>(__data_end__);
	uint8_t* bLo = reinterpret_cast<uint8_t*>(__bss_start__);
	uint8_t* bHi = reinterpret_cast<uint8_t*>(__bss_end__);
	addGlobRange(dLo, dHi);
	void* aLo     = nullptr;
	size_t aBytes = 0;
	if (pc_os_stubs_static_arena(&aLo, &aBytes) && aLo >= (void*)bLo && aLo < (void*)bHi) {
		// The static 256 MB arena is replaced by the region's arena zone and
		// never touched: leave its whole pages out of the bracket.
		uint8_t* a0 = static_cast<uint8_t*>(aLo);
		uint8_t* a1 = a0 + aBytes;
		uint8_t* cut0 = reinterpret_cast<uint8_t*>(alignUp(reinterpret_cast<uintptr_t>(a0), kPage));
		uint8_t* cut1 = reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(a1) & ~uintptr_t(kPage - 1));
		addGlobRange(bLo, cut0);
		addGlobRange(cut1, bHi);
	} else {
		addGlobRange(bLo, bHi);
	}
	S->globPages = 0;
	for (int i = 0; i < S->nGlob; ++i) S->globPages += size_t(S->glob[i].hi - S->glob[i].lo) / kPage;
	S->globBytes   = S->globPages * kPage;
	S->globShadow  = static_cast<uint8_t*>(osAllocZero(S->globBytes));
	S->globPagePtr = static_cast<uint8_t**>(std::calloc(S->globPages, sizeof(uint8_t*)));
	S->globDirty   = static_cast<uint32_t*>(std::calloc(S->globPages, sizeof(uint32_t)));
	S->presFirst   = static_cast<uint32_t*>(std::calloc(S->globPages, sizeof(uint32_t)));
	S->presCount   = static_cast<uint16_t*>(std::calloc(S->globPages, sizeof(uint16_t)));
	S->syncGMark   = static_cast<uint8_t*>(std::calloc(S->globPages, 1));
	S->gStamp      = static_cast<uint32_t*>(std::calloc(S->globPages, sizeof(uint32_t)));
	uint64_t gp = 0;
	for (int i = 0; i < S->nGlob; ++i) {
		for (uint8_t* p = S->glob[i].lo; p < S->glob[i].hi; p += kPage) S->globPagePtr[gp++] = p;
	}
	// Undo pool for globals: enough for every page on every ring tick.
	S->gUndoPages = S->globPages * 4;
	S->gUndo      = static_cast<uint8_t*>(osAllocZero(S->gUndoPages * kPage));
	S->gUndoIdx   = static_cast<uint32_t*>(std::calloc(S->gUndoPages, sizeof(uint32_t)));
	// Initial shadow: the globals as they are right now.
	for (uint64_t i = 0; i < S->globPages; ++i) std::memcpy(S->globShadow + i * kPage, S->globPagePtr[i], kPage);
}

int cmpSeg(const void* a, const void* b)
{
	const PreserveSeg* x = static_cast<const PreserveSeg*>(a);
	const PreserveSeg* y = static_cast<const PreserveSeg*>(b);
	if (x->page != y->page) return x->page < y->page ? -1 : 1;
	return x->off < y->off ? -1 : x->off > y->off ? 1 : 0;
}

// Map a global byte range to per-page preserve segments, indexed by page
// (F11: the restore no longer scans every segment per preserved page).
void buildPreserveSegs()
{
	for (int r = 0; r < S->preserveRawCount; ++r) {
		uint8_t* lo = S->preserveRaw[r].lo;
		uint8_t* hi = S->preserveRaw[r].hi;
		for (uint64_t i = 0; i < S->globPages; ++i) {
			uint8_t* p0 = S->globPagePtr[i];
			uint8_t* p1 = p0 + kPage;
			uint8_t* a  = lo > p0 ? lo : p0;
			uint8_t* b  = hi < p1 ? hi : p1;
			if (a >= b) continue;
			if (S->preserveCount == S->preserveCap) {
				S->preserveCap = S->preserveCap ? S->preserveCap * 2 : 256;
				S->preserveSegs = static_cast<PreserveSeg*>(std::realloc(S->preserveSegs, S->preserveCap * sizeof(PreserveSeg)));
			}
			S->preserveSegs[S->preserveCount++] = { uint32_t(i), uint16_t(a - p0), uint16_t(b - a) };
		}
	}
	if (S->preserveCount) std::qsort(S->preserveSegs, S->preserveCount, sizeof(PreserveSeg), cmpSeg);
	std::memset(S->presCount, 0, S->globPages * sizeof(uint16_t));
	for (uint32_t s = 0; s < S->preserveCount; ++s) {
		const uint32_t pg = S->preserveSegs[s].page;
		if (S->presCount[pg] == 0) S->presFirst[pg] = s;
		S->presCount[pg]++;
	}
}

// Restore one global page from src, keeping preserved bytes as they are now.
void restoreGlobalPage(uint64_t gi, const uint8_t* src)
{
	uint8_t* dst = S->globPagePtr[gi];
	const uint32_t n = S->presCount[gi];
	if (n == 0) {
		std::memcpy(dst, src, kPage);
		return;
	}
	// copy the gaps between the (sorted) preserved segments only
	const PreserveSeg* seg = S->preserveSegs + S->presFirst[gi];
	size_t at              = 0;
	for (uint32_t k = 0; k < n; ++k) {
		if (seg[k].off > at) std::memcpy(dst + at, src + at, seg[k].off - at);
		const size_t end = size_t(seg[k].off) + seg[k].len;
		if (end > at) at = end;
	}
	if (at < kPage) std::memcpy(dst + at, src + at, kPage - at);
}

// Build the preserved-byte mask of global page gi.
void presMask(uint64_t gi, uint8_t* mask)
{
	std::memset(mask, 0, kPage);
	const PreserveSeg* seg = S->preserveSegs + S->presFirst[gi];
	for (uint32_t k = 0; k < S->presCount[gi]; ++k) std::memset(mask + seg[k].off, 1, seg[k].len);
}

// ---------------------------------------------------------------------------
// Write watch
// ---------------------------------------------------------------------------
// Write watch is queried per used zone (meta + arena, the committed small
// zone, the large zone below its commit high-water mark) rather than over
// the whole 2 GB reservation; the per-zone cost is logged so the scan cost
// can be related to the committed size. With the hot/cold split each zone is
// cut into the touched extents and the rest.
struct Zone {
	uint8_t* lo;
	uint8_t* hi;
};

int usedZones(Zone* z)
{
	z[0] = { S->base, S->base + kArenaOff + kArenaSize };
	z[1] = { S->base + kSmallOff, S->meta->smallCommitted };
	uint8_t* largeHi = S->meta->largeCommitHw > S->meta->largeWild ? S->meta->largeCommitHw : S->meta->largeWild;
	z[2] = { S->base + kLargeOff, largeHi };
	return 3;
}

void addRun(uint8_t* lo, uint8_t* hi, int zone, bool hot)
{
	if (hi <= lo || S->runCount >= kMaxRuns) return;
	S->runs[S->runCount++] = { lo, hi, uint8_t(zone), uint8_t(hot ? 1 : 0) };
}

void buildRuns()
{
	S->runCount = 0;
	Zone z[3];
	usedZones(z);
	addRun(z[1].lo, z[1].hi, 1, true); // the small zone is all live data
	for (int zi : { 0, 2 }) {
		const size_t p0 = size_t(z[zi].lo - S->base) / kPage;
		const size_t p1 = size_t(z[zi].hi - S->base) / kPage;
		size_t cold     = p0; // start of the pending cold stretch
		size_t p        = p0;
		while (p < p1) {
			if (!bitGet(S->touched, p)) {
				++p;
				continue;
			}
			size_t e = p;
			// extend over touched pages and gaps shorter than kHotGapPages
			for (;;) {
				while (e < p1 && bitGet(S->touched, e)) ++e;
				size_t g = e;
				while (g < p1 && g - e < kHotGapPages && !bitGet(S->touched, g)) ++g;
				if (g < p1 && g - e < kHotGapPages && bitGet(S->touched, g)) {
					e = g;
					continue;
				}
				break;
			}
			addRun(S->base + cold * kPage, S->base + p * kPage, zi, false);
			addRun(S->base + p * kPage, S->base + e * kPage, zi, true);
			cold = e;
			p    = e;
		}
		addRun(S->base + cold * kPage, S->base + p1 * kPage, zi, false);
	}
	S->runsStale = false;
}

// One GetWriteWatch call over [lo, hi): records the dirty pages; returns the
// number reported.
uint32_t watchRange(uint8_t* lo, uint8_t* hi, int zone, DWORD flags, bool record, double* ms)
{
	const int64_t t0 = now();
	ULONG_PTR count  = S->wwCap;
	DWORD gran       = 0;
	const UINT rc    = GetWriteWatch(flags, lo, size_t(hi - lo), S->wwAddrs, &count, &gran);
	const double dt  = msSince(t0);
	*ms += dt;
	if (flags) {
		S->wwZoneMs[zone] += dt;
		S->wwCalls++;
	}
	if (rc != 0) {
		std::fprintf(stderr, "[m6a] GetWriteWatch failed (%lu)\n", GetLastError());
		return 0;
	}
	if (!record) return uint32_t(count);
	for (ULONG_PTR i = 0; i < count; ++i) {
		const size_t idx = size_t(static_cast<uint8_t*>(S->wwAddrs[i]) - S->base) / kPage;
		if (idx >= kPages) continue;
		if (!bitGet(S->touched, idx)) {
			bitSet(S->touched, idx);
			S->runsStale = true;
		}
		if (S->dirtySince) bitSet(S->dirtySince, idx);
		if (S->pageMark[idx] != S->epoch) {
			S->pageMark[idx]              = S->epoch;
			S->dirtyList[S->dirtyCount++] = uint32_t(idx);
		}
	}
	return uint32_t(count);
}

uint32_t collectWriteWatch()
{
	if (!S->writeWatch) return 0;
	// pages another thread wrote after the previous tick's collect (ww_late)
	for (uint32_t i = 0; i < S->carryCount; ++i) {
		const uint32_t idx = S->carryList[i];
		if (S->pageMark[idx] != S->epoch) {
			S->pageMark[idx]              = S->epoch;
			S->dirtyList[S->dirtyCount++] = idx;
		}
	}
	S->carryCount  = 0;
	uint32_t total = 0;
	if (S->wwSplit) {
		if (S->runsStale) buildRuns();
		// the zones' bounds move with the wilderness; rebuild when they do
		Zone z[3];
		usedZones(z);
		if (z[1].hi != S->runsSmallHi || z[2].hi != S->runsLargeHi) {
			buildRuns();
			S->runsSmallHi = z[1].hi;
			S->runsLargeHi = z[2].hi;
		}
		for (uint32_t r = 0; r < S->runCount; ++r) {
			const Run& run = S->runs[r];
			double* ms     = run.hot ? &S->wwHotMs : &S->wwColdMs;
			const uint32_t n = watchRange(run.lo, run.hi, run.zone, WRITE_WATCH_FLAG_RESET, true, ms);
			const double mb  = double(run.hi - run.lo) / (1024.0 * 1024.0);
			if (run.hot) {
				S->wwHotCalls++;
				S->rdHot += n;
				S->hotMb += mb;
			} else {
				S->wwColdCalls++;
				S->rdCold += n;
				S->coldMb += mb;
			}
			total += n;
		}
		return total;
	}
	Zone z[3];
	const int n = usedZones(z);
	for (int k = 0; k < n; ++k) {
		if (z[k].hi <= z[k].lo) continue;
		double ms = 0.0;
		total += watchRange(z[k].lo, z[k].hi, k, WRITE_WATCH_FLAG_RESET, true, &ms);
	}
	return total;
}

void resetWriteWatch()
{
	if (!S->writeWatch) return;
	Zone z[3];
	const int n = usedZones(z);
	for (int k = 0; k < n; ++k) {
		if (z[k].hi > z[k].lo) ResetWriteWatch(z[k].lo, size_t(z[k].hi - z[k].lo));
	}
}

// Measurement mode, before the post-restore reset: pages reported written
// since this tick's collect that are not this tick's dirty pages were
// written by someone else (another thread) after the collect. They are
// carried into the next tick's dirty set instead of being lost to the reset.
uint32_t collectLate()
{
	Zone z[3];
	const int n    = usedZones(z);
	uint32_t late  = 0;
	for (int k = 0; k < n; ++k) {
		if (z[k].hi <= z[k].lo) continue;
		ULONG_PTR count = S->wwCap;
		DWORD gran      = 0;
		if (GetWriteWatch(0, z[k].lo, size_t(z[k].hi - z[k].lo), S->wwAddrs, &count, &gran) != 0) continue;
		for (ULONG_PTR i = 0; i < count; ++i) {
			const size_t idx = size_t(static_cast<uint8_t*>(S->wwAddrs[i]) - S->base) / kPage;
			if (idx >= kPages || S->pageMark[idx] == S->epoch) continue;
			late++;
			if (S->carryCount < kPages) S->carryList[S->carryCount++] = uint32_t(idx);
			if (S->dirtySince) bitSet(S->dirtySince, idx);
		}
	}
	return late;
}

// No write watch (A/B arm): an emulated dirty set with the in-game zone mix
// (~66% small zone, ~29% large zone, ~5% arena), rotating every tick.
void emulateDirty()
{
	Zone z[3];
	usedZones(z);
	const uint32_t n     = S->nowwPages;
	const uint32_t nArena = n * 5 / 100;
	const uint32_t nLarge = n * 29 / 100;
	const uint32_t nSmall = n - nArena - nLarge;
	const uint32_t parts[3] = { nArena, nSmall, nLarge };
	S->nowwCursor++;
	for (int k = 0; k < 3; ++k) {
		const size_t p0 = size_t(z[k].lo - S->base) / kPage + (k == 0 ? kArenaOff / kPage : 0);
		const size_t p1 = size_t(z[k].hi - S->base) / kPage;
		if (p1 <= p0 || parts[k] == 0) continue;
		const size_t span   = p1 - p0;
		const size_t stride = span / parts[k] ? span / parts[k] : 1;
		for (uint32_t i = 0; i < parts[k]; ++i) {
			const size_t idx = p0 + (size_t(i) * stride + S->nowwCursor) % span;
			if (S->pageMark[idx] != S->epoch) {
				S->pageMark[idx]              = S->epoch;
				S->dirtyList[S->dirtyCount++] = uint32_t(idx);
			}
		}
	}
}

// ---------------------------------------------------------------------------
// Machine load (MV-6): system busy % over the last sample interval
// ---------------------------------------------------------------------------
inline uint64_t ft64(const FILETIME& f) { return (uint64_t(f.dwHighDateTime) << 32) | f.dwLowDateTime; }

void sampleLoad()
{
	FILETIME idle, kernel, user;
	if (!GetSystemTimes(&idle, &kernel, &user)) return;
	const uint64_t i = ft64(idle), k = ft64(kernel), u = ft64(user);
	if (S->loadKernel) {
		const uint64_t di = i - S->loadIdle, dk = k - S->loadKernel, du = u - S->loadUser;
		const uint64_t tot = dk + du; // kernel time includes idle time
		if (tot) S->sysBusy = 100.0 * double(tot - di) / double(tot);
	}
	S->loadIdle   = i;
	S->loadKernel = k;
	S->loadUser   = u;
}

// ---------------------------------------------------------------------------
// CSV
// ---------------------------------------------------------------------------
void openCsv()
{
	const char* path = std::getenv("PIKMIN_NETPLAY_SNAPSHOT_SPIKE_CSV");
	if (!path || !*path) path = "snapshot_spike.csv";
	S->csv = std::fopen(path, "w");
	if (!S->csv) {
		std::fprintf(stderr, "[m6a] cannot open %s\n", path);
		return;
	}
	std::setvbuf(S->csv, nullptr, _IOFBF, 1 << 20);
	std::fprintf(S->csv,
	    "tick,loop,live,resim,auth_ms,idle_ms,frame_ms,"
	    "rd_auth,rd_post,rd_total,rd_meta,rd_arena,rd_small,rd_large,gd,"
	    "ww_ms,save_ms,restore_ms,restore_ww_ms,gcmp_ms,gsave_ms,full_ms,full_mb,gfull_ms,"
	    "allocs,alloc_bytes,frees,free_bytes,live_blocks,live_mb,small_hw_mb,large_hw_mb,touched_mb,"
	    "off_allocs,off_bytes,region_frees_off_main,unknown_frees,undo_ok,barrier,"
	    "ww_z0_ms,ww_z1_ms,ww_z2_ms,ww_calls,committed_mb,"
	    "phase,pikis,sys_busy,present_ms,done_ms,parse_ms,retrace_ms,"
	    "ww_hot_ms,ww_cold_ms,ww_hot_calls,ww_cold_calls,rd_hot,rd_cold,hot_mb,cold_mb,"
	    "grestore_ms,ww_late,conc_r,conc_g,missed,sync_phase\n");
}

double touchedMb()
{
	uint64_t n = 0;
	for (size_t w = 0; w < kPages / 64; ++w) n += __builtin_popcountll(S->touched[w]);
	return double(n) * kPage / (1024.0 * 1024.0);
}

// Copy every touched run of the region into dst (a lazily committed 2 GB
// reservation); returns ms and the MB copied.
double copyTouched(uint8_t* dst, bool* committed, double* mbOut)
{
	const int64_t t0 = now();
	uint64_t pages   = 0;
	size_t p         = 0;
	while (p < kPages) {
		const uint64_t word = S->touched[p >> 6];
		if (word == 0 && (p & 63) == 0) {
			p += 64;
			continue;
		}
		if (!(word & (1ull << (p & 63)))) {
			++p;
			continue;
		}
		size_t e = p;
		while (e < kPages && bitGet(S->touched, e)) ++e;
		const size_t off   = p * kPage;
		const size_t bytes = (e - p) * kPage;
		ensureGranules(dst, committed, kGranules, off, bytes);
		std::memcpy(dst + off, S->base + off, bytes);
		pages += e - p;
		p = e;
	}
	*mbOut = double(pages) * kPage / (1024.0 * 1024.0);
	return msSince(t0);
}

// ---------------------------------------------------------------------------
// Audit (MV-3): per-page hashes of every committed region page
// ---------------------------------------------------------------------------
uint64_t hashPage(const uint8_t* p)
{
	const uint64_t* w = reinterpret_cast<const uint64_t*>(p);
	uint64_t a = 0x9E3779B97F4A7C15ull, b = 0xC2B2AE3D27D4EB4Full;
	for (size_t i = 0; i < kPage / 8; i += 2) {
		a = (a ^ w[i]) * 0xFF51AFD7ED558CCDull;
		b = (b ^ w[i + 1]) * 0xC4CEB9FE1A85EC53ull;
	}
	return a ^ (b >> 1) ^ (b << 63);
}

template <typename Fn> void forEachCommittedPage(Fn fn)
{
	Zone z[3];
	usedZones(z);
	// meta (committed part only), arena, small, large up to the commit hw
	for (size_t p = 0; p < kMetaCommit / kPage; ++p) fn(p);
	for (size_t p = kArenaOff / kPage; p < (kArenaOff + kArenaSize) / kPage; ++p) fn(p);
	for (size_t p = kSmallOff / kPage; p < size_t(z[1].hi - S->base) / kPage; ++p) fn(p);
	for (size_t p = kLargeOff / kPage; p < size_t(S->meta->largeCommitHw - S->base) / kPage; ++p) fn(p);
}

void runAudit(uint64_t tick)
{
	uint64_t missed = 0, pages = 0, changed = 0;
	forEachCommittedPage([&](size_t p) {
		const uint64_t h    = hashPage(S->base + p * kPage);
		const uint64_t prev = bitGet(S->auditHave, p) ? S->pageHash[p] : S->zeroHash;
		if (h != prev) {
			changed++;
			if (!bitGet(S->dirtySince, p)) {
				missed++;
				if (S->auditLog && missed <= 32) {
					size_t bo            = 0;
					const uintptr_t site = regionSiteOf(p * kPage, &bo);
					std::fprintf(S->auditLog, "#missed tick=%llu page_off=0x%llx site_rva=0x%llx blk_off=0x%llx\n",
					    (unsigned long long)tick, (unsigned long long)(p * kPage),
					    (unsigned long long)(site > 1 ? site - exeBase() : site), (unsigned long long)bo);
				}
			}
		}
		S->pageHash[p] = h;
		bitSet(S->auditHave, p);
		pages++;
	});
	std::memset(S->dirtySince, 0, kPages / 8);
	S->audits++;
	S->auditMissed += missed;
	S->auditPages += pages;
	S->missed = int64_t(missed);
	if (S->auditLog) {
		std::fprintf(S->auditLog, "audit tick=%llu pages=%llu changed=%llu missed=%llu\n", (unsigned long long)tick,
		    (unsigned long long)pages, (unsigned long long)changed, (unsigned long long)missed);
		std::fflush(S->auditLog);
	}
}

// ---------------------------------------------------------------------------
// Pointer scan (MV-4)
// ---------------------------------------------------------------------------
void pairAdd(uint32_t kind, uintptr_t src, uintptr_t dst)
{
	uintptr_t h = ((src * 0x9E3779B97F4A7C15ull) ^ (dst * 0xC2B2AE3D27D4EB4Full) ^ kind) >> 20;
	for (size_t probe = 0; probe < kPairCap; ++probe) {
		Pair& p = S->pairs[(h + probe) & (kPairCap - 1)];
		if (p.kind == kind && p.src == src && p.dst == dst) {
			p.count++;
			return;
		}
		if (p.kind == 0) {
			p = { src, dst, 1, kind, 0 };
			return;
		}
	}
}

int cmpOff(const void* a, const void* b)
{
	const OffBlock* x = static_cast<const OffBlock*>(a);
	const OffBlock* y = static_cast<const OffBlock*>(b);
	return x->ptr < y->ptr ? -1 : x->ptr > y->ptr ? 1 : 0;
}

int cmpPair(const void* a, const void* b)
{
	const Pair* x = static_cast<const Pair*>(a);
	const Pair* y = static_cast<const Pair*>(b);
	return x->count < y->count ? 1 : x->count > y->count ? -1 : 0;
}

const OffBlock* findOff(const OffBlock* sorted, size_t n, uintptr_t v)
{
	size_t lo = 0, hi = n;
	while (lo < hi) {
		const size_t mid = (lo + hi) / 2;
		if (sorted[mid].ptr <= v) lo = mid + 1;
		else hi = mid;
	}
	if (lo == 0) return nullptr;
	const OffBlock* b = &sorted[lo - 1];
	return v < b->ptr + b->size ? b : nullptr;
}

void runPtrScan(uint64_t tick)
{
	AcquireSRWLockExclusive(&S->offLock); // blocks off-region frees for the scan
	size_t n = 0;
	for (size_t i = 0; i < kOffTableCap; ++i)
		if (S->offTable[i].ptr) S->offSorted[n++] = S->offTable[i];
	std::qsort(S->offSorted, n, sizeof(OffBlock), cmpOff);
	std::memset(S->pairs, 0, sizeof(Pair) * kPairCap);
	const uintptr_t lo = n ? S->offSorted[0].ptr : 0;
	const uintptr_t hi = n ? S->offSorted[n - 1].ptr + S->offSorted[n - 1].size : 0;
	uint64_t rToOff = 0, gToOff = 0, offToR = 0, gToR = 0;
	// 1. region (touched pages) -> off-region blocks
	for (size_t p = 0; p < kPages; ++p) {
		if (!bitGet(S->touched, p)) continue;
		const uint64_t* w = reinterpret_cast<const uint64_t*>(S->base + p * kPage);
		for (size_t i = 0; i < kPage / 8; ++i) {
			const uintptr_t v = w[i];
			if (v < lo || v >= hi) continue;
			const OffBlock* b = findOff(S->offSorted, n, v);
			if (!b) continue;
			size_t bo = 0;
			pairAdd(1, regionSiteOf(p * kPage + i * 8, &bo), b->ra);
			rToOff++;
		}
	}
	// 2. globals -> off-region blocks; globals -> region (count only)
	const uintptr_t rLo = reinterpret_cast<uintptr_t>(S->base);
	const uintptr_t rHi = rLo + kReserve;
	for (uint64_t g = 0; g < S->globPages; ++g) {
		const uint64_t* w = reinterpret_cast<const uint64_t*>(S->globPagePtr[g]);
		for (size_t i = 0; i < kPage / 8; ++i) {
			const uintptr_t v = w[i];
			if (v >= rLo && v < rHi) {
				gToR++;
				continue;
			}
			if (v < lo || v >= hi) continue;
			const OffBlock* b = findOff(S->offSorted, n, v);
			if (!b) continue;
			pairAdd(2, reinterpret_cast<uintptr_t>(w + i), b->ra);
			gToOff++;
		}
	}
	// 3. off-region blocks -> region
	for (size_t k = 0; k < n; ++k) {
		const OffBlock& b = S->offSorted[k];
		const uint64_t* w = reinterpret_cast<const uint64_t*>(b.ptr);
		const size_t words = b.size / 8;
		if (b.ptr & 7) continue;
		for (size_t i = 0; i < words; ++i) {
			const uintptr_t v = w[i];
			if (v < rLo || v >= rHi) continue;
			size_t bo = 0;
			pairAdd(3, b.ra, regionSiteOf(size_t(v - rLo), &bo));
			offToR++;
		}
	}
	ReleaseSRWLockExclusive(&S->offLock);
	// report: top pairs per kind (module-relative rvas; globals as exe rvas)
	const char* path = std::getenv("PIKMIN_NETPLAY_SNAPSHOT_SPIKE_PTRSCAN_LOG");
	if (!path || !*path) path = "snapshot_spike_ptrscan.txt";
	FILE* out = std::fopen(path, "a");
	if (!out) return;
	std::qsort(S->pairs, kPairCap, sizeof(Pair), cmpPair);
	const uintptr_t exe = exeBase();
	std::fprintf(out, "scan tick=%llu off_blocks=%zu region_to_off=%llu globals_to_off=%llu off_to_region=%llu globals_to_region=%llu\n",
	    (unsigned long long)tick, n, (unsigned long long)rToOff, (unsigned long long)gToOff,
	    (unsigned long long)offToR, (unsigned long long)gToR);
	const char* kinds[4] = { "", "region_to_off", "globals_to_off", "off_to_region" };
	int shown[4]         = { 0, 0, 0, 0 };
	for (size_t i = 0; i < kPairCap && S->pairs[i].kind; ++i) {
		const Pair& p = S->pairs[i];
		if (shown[p.kind]++ >= 80) continue;
		auto rel = [exe](uintptr_t a) -> unsigned long long { return a > 1 && a >= exe && a < exe + 0x20000000ull ? (unsigned long long)(a - exe) : (unsigned long long)a; };
		std::fprintf(out, "pair tick=%llu kind=%s src=0x%llx dst=0x%llx count=%llu\n", (unsigned long long)tick, kinds[p.kind],
		    rel(p.src), rel(p.dst), (unsigned long long)p.count);
	}
	std::fclose(out);
}

// ---------------------------------------------------------------------------
// Synctest
// ---------------------------------------------------------------------------
void syncLogOpen()
{
	const char* path = std::getenv("PIKMIN_NETPLAY_SNAPSHOT_SYNCTEST_CSV");
	if (!path || !*path) path = "snapshot_synctest.csv";
	S->syncCsv = std::fopen(path, "w");
	if (S->syncCsv) {
		std::fprintf(S->syncCsv,
		    "anchor,k,first_bad,bad_mask,region_diff_pages,global_diff_pages,only_first,only_second,restore_ms,restored_pages,"
		    "audio_bad,region_unreported,global_pres_diff,region_union,global_union,restore_region_ms,restore_shadow_ms,"
		    "restore_glob_ms,restore_ww_ms,restored_region,restored_glob,undo_entries,dedup,diverge,sys_busy\n");
	}
}

bool ringHas(uint64_t tick)
{
	const UndoTick& u = S->ring[tick % kUndoRing];
	if (!u.valid || u.tick != tick) return false;
	if (S->rUndoCursor - u.rStart > kUndoPoolPages) return false;
	if (S->gUndoCursor - u.gStart > S->gUndoPages) return false;
	return true;
}

void syncCaptureHash(int step)
{
	uint64_t total = 0, subs[7] = {}, tick = 0;
	pc_state_hash_current(&total, subs, &tick);
	S->syncHash[step][0] = total;
	for (int i = 0; i < 7; ++i) S->syncHash[step][i + 1] = subs[i];
	S->syncAudio[step] = pc_snapshot_spike_game_audio_hash();
}

// Roll region and globals back from the end of tick `from` to the end of
// the anchor tick.
//   dedup (default): oldest undo entry first, each page restored once (its
//     pre-image from the first tick that dirtied it is its anchor state);
//     region copies, shadow copies, globals and the write-watch reset are
//     timed separately.
//   naive: every undo entry newest first, region + shadow copy per entry.
void syncRollback(uint64_t from, uint64_t anchor)
{
	const int64_t t0 = now();
	uint32_t rp = 0, gp = 0, entries = 0;
	double regionMs = 0, shadowMs = 0, globMs = 0;
	if (S->syncDedup) {
		if (++S->stamp == 0) {
			std::memset(S->rStamp, 0, kPages * sizeof(uint32_t));
			std::memset(S->gStamp, 0, S->globPages * sizeof(uint32_t));
			S->stamp = 1;
		}
		int64_t a = now();
		for (uint64_t t = anchor + 1; t <= from; ++t) {
			const UndoTick& u = S->ring[t % kUndoRing];
			for (uint32_t i = 0; i < u.rCount; ++i) {
				const uint64_t slot = (u.rStart + i) % kUndoPoolPages;
				const uint32_t idx  = S->rUndoIdx[slot];
				++entries;
				if (S->rStamp[idx] == S->stamp) continue;
				S->rStamp[idx] = S->stamp;
				std::memcpy(S->base + size_t(idx) * kPage, S->rUndo + slot * kPage, kPage);
				S->restoredList[rp++] = idx;
			}
		}
		regionMs = msSince(a);
		a        = now();
		for (uint32_t i = 0; i < rp; ++i) {
			const size_t off = size_t(S->restoredList[i]) * kPage;
			std::memcpy(S->shadow + off, S->base + off, kPage);
		}
		shadowMs = msSince(a);
		a        = now();
		for (uint64_t t = anchor + 1; t <= from; ++t) {
			const UndoTick& u = S->ring[t % kUndoRing];
			for (uint32_t i = 0; i < u.gCount; ++i) {
				const uint64_t slot = (u.gStart + i) % S->gUndoPages;
				const uint32_t gi   = S->gUndoIdx[slot];
				++entries;
				if (S->gStamp[gi] == S->stamp) continue;
				S->gStamp[gi] = S->stamp;
				const uint8_t* src = S->gUndo + slot * kPage;
				restoreGlobalPage(gi, src);
				std::memcpy(S->globShadow + size_t(gi) * kPage, src, kPage);
				++gp;
			}
		}
		globMs = msSince(a);
	} else {
		int64_t a = now();
		for (uint64_t t = from; t > anchor; --t) {
			const UndoTick& u = S->ring[t % kUndoRing];
			for (uint32_t i = 0; i < u.rCount; ++i) {
				const uint64_t slot = (u.rStart + i) % kUndoPoolPages;
				const uint32_t idx  = S->rUndoIdx[slot];
				const uint8_t* src  = S->rUndo + slot * kPage;
				std::memcpy(S->base + size_t(idx) * kPage, src, kPage);
				std::memcpy(S->shadow + size_t(idx) * kPage, src, kPage);
				++rp;
				++entries;
			}
		}
		regionMs = msSince(a);
		a        = now();
		for (uint64_t t = from; t > anchor; --t) {
			const UndoTick& u = S->ring[t % kUndoRing];
			for (uint32_t i = 0; i < u.gCount; ++i) {
				const uint64_t slot = (u.gStart + i) % S->gUndoPages;
				const uint32_t gi   = S->gUndoIdx[slot];
				const uint8_t* src  = S->gUndo + slot * kPage;
				restoreGlobalPage(gi, src);
				std::memcpy(S->globShadow + size_t(gi) * kPage, src, kPage);
				++gp;
				++entries;
			}
		}
		globMs = msSince(a);
	}
	// The restore's own writes are not sim changes.
	const int64_t w = now();
	resetWriteWatch();
	S->rsWwMs        = msSince(w);
	S->rsTotalMs     = msSince(t0);
	S->rsRegionMs    = regionMs;
	S->rsShadowMs    = shadowMs;
	S->rsGlobMs      = globMs;
	S->rsRegionPages = rp;
	S->rsGlobPages   = gp;
	S->rsEntries     = entries;
	if (S->dirtySince) {
		for (uint64_t t = anchor + 1; t <= from; ++t) {
			const UndoTick& u = S->ring[t % kUndoRing];
			for (uint32_t i = 0; i < u.rCount; ++i) bitSet(S->dirtySince, S->rUndoIdx[(u.rStart + i) % kUndoPoolPages]);
		}
	}
}

// Offset of the first non-preserved byte of global page gi that differs
// from `was`; -1 if none. *presDiff reports a differing preserved byte.
int firstGlobalDiff(uint64_t gi, const uint8_t* was, bool* presDiff)
{
	const uint8_t* cur = S->globPagePtr[gi];
	*presDiff          = false;
	if (std::memcmp(was, cur, kPage) == 0) return -1;
	if (S->presCount[gi] == 0) {
		int at = 0;
		while (at < int(kPage) && was[at] == cur[at]) ++at;
		return at;
	}
	uint8_t mask[kPage];
	presMask(gi, mask);
	int first = -1;
	for (int at = 0; at < int(kPage); ++at) {
		if (was[at] == cur[at]) continue;
		if (mask[at]) *presDiff = true;
		else if (first < 0) first = at;
	}
	return first;
}

void syncMarkPass(uint64_t anchor, uint64_t tick, uint32_t bit)
{
	for (uint64_t t = anchor + 1; t <= tick; ++t) {
		const UndoTick& u = S->ring[t % kUndoRing];
		if (!u.valid || u.tick != t) continue;
		for (uint32_t i = 0; i < u.rCount; ++i) S->syncMark[S->rUndoIdx[(u.rStart + i) % kUndoPoolPages]] |= bit;
		for (uint32_t i = 0; i < u.gCount; ++i) S->syncGMark[S->gUndoIdx[(u.gStart + i) % S->gUndoPages]] |= uint8_t(bit);
	}
}

void syncAbort(uint64_t tick)
{
	S->syncAborted++;
	S->syncPhase  = 0;
	S->syncNextAt = tick + S->syncPeriod;
	pc_state_hash_spike_suppress_log(false);
}

void syncOnTickEnd(uint64_t tick, bool live, bool undoOk)
{
	if (S->syncKArg == 0) return;
	const bool liveOk = live || S->syncAny;
	if (S->syncPhase == 0) {
		if (!liveOk || !undoOk || tick < S->syncStart || tick < S->syncNextAt || tick >= S->syncEnd) return;
		S->syncK      = S->syncKArg > 0 ? S->syncKArg : 1 + int(S->syncTestIndex % 7);
		S->syncTestIndex++;
		S->syncPhase  = 1;
		S->syncAnchor = tick;
		S->syncStep   = 0;
		syncCaptureHash(0);
		std::memset(S->syncMark, 0, kPages * sizeof(uint32_t));
		std::memset(S->syncGMark, 0, S->globPages);
		// divergent mode: the perturbed first pass must not reach hashes.txt
		if (S->syncDiverge) pc_state_hash_spike_suppress_log(true);
		return;
	}
	if (S->syncPhase == 1) {
		S->syncStep++;
		syncCaptureHash(S->syncStep);
		if (!undoOk || !liveOk) {
			syncAbort(tick);
			return;
		}
		if (S->syncStep < S->syncK) return;
		for (uint64_t t = S->syncAnchor + 1; t <= tick; ++t) {
			if (!ringHas(t)) {
				syncAbort(tick);
				return;
			}
		}
		// First pass done: remember what it dirtied and (identical-input
		// mode) the whole touched region and all globals, then roll back.
		syncMarkPass(S->syncAnchor, tick, 1);
		if (!S->syncDiverge) {
			double mb = 0;
			copyTouched(S->syncFull, S->syncFullCommitted, &mb);
			std::memcpy(S->syncTouched, S->touched, kPages / 8);
			for (uint64_t g = 0; g < S->globPages; ++g) std::memcpy(S->syncGFull + g * kPage, S->globPagePtr[g], kPage);
			if (S->syncFullHash) forEachCommittedPage([](size_t p) { S->syncPageHash[p] = hashPage(S->base + p * kPage); });
		}
		syncRollback(tick, S->syncAnchor);
		S->syncPhase    = 2;
		S->syncStep     = 0;
		S->syncFirstBad = 0;
		S->syncBadMask  = 0;
		S->syncAudioBad = 0;
		pc_state_hash_spike_suppress_log(!S->syncDiverge);
		return;
	}
	// syncPhase == 2: resimulating
	S->syncStep++;
	if (!S->syncDiverge) {
		uint64_t total = 0, subs[7] = {}, t = 0;
		pc_state_hash_current(&total, subs, &t);
		const uint64_t* want = S->syncHash[S->syncStep];
		if (!S->syncFirstBad) {
			unsigned mask = 0;
			if (total != want[0]) mask |= 1u;
			for (int i = 0; i < 7; ++i)
				if (subs[i] != want[i + 1]) mask |= 2u << i;
			if (mask) {
				S->syncFirstBad = S->syncStep;
				S->syncBadMask  = mask;
			}
		}
		if (!S->syncAudioBad && pc_snapshot_spike_game_audio_hash() != S->syncAudio[S->syncStep]) S->syncAudioBad = S->syncStep;
	}
	if (S->syncStep < S->syncK) return;
	syncMarkPass(S->syncAnchor, tick, 2);
	SyncResult r = {};
	r.anchor     = S->syncAnchor;
	r.k          = S->syncK;
	r.firstBad   = S->syncFirstBad;
	r.badMask    = S->syncBadMask;
	r.audioBad   = S->syncAudioBad;
	for (size_t w = 0; w < kPages; ++w) {
		const uint32_t m = S->syncMark[w];
		if (!m) continue;
		r.regionUnion++;
		if (m == 1) r.onlyFirst++;
		else if (m == 2) r.onlySecond++;
	}
	for (uint64_t g = 0; g < S->globPages; ++g) {
		const uint8_t m = S->syncGMark[g];
		if (!m) continue;
		r.globalUnion++;
		if (m == 1) r.onlyFirst++;
		else if (m == 2) r.onlySecond++;
	}
	if (!S->syncDiverge) {
		// Every page touched at either end (not only what write watch
		// reported this test) against its first-pass end state.
		static const uint8_t zero[kPage] = {};
		int logged = 0;
		for (size_t p = 0; p < kPages; ++p) {
			const bool then = bitGet(S->syncTouched, p);
			const bool nowT = bitGet(S->touched, p);
			if (!then && !nowT) continue;
			const uint8_t* was = then ? S->syncFull + p * kPage : zero;
			const uint8_t* cur = S->base + p * kPage;
			if (std::memcmp(was, cur, kPage) == 0) continue;
			r.regionDiffPages++;
			const bool reported = S->syncMark[p] != 0;
			if (!reported) r.regionUnreported++;
			if (logged < 16 && S->syncCsv) {
				int at = 0;
				while (at < int(kPage) && was[at] == cur[at]) ++at;
				size_t bo            = 0;
				const uintptr_t site = regionSiteOf(p * kPage + at, &bo);
				std::fprintf(S->syncCsv, "#rdiff anchor=%llu off=0x%llx site_rva=0x%llx blk_off=0x%llx reported=%d\n",
				    (unsigned long long)r.anchor, (unsigned long long)(p * kPage + at),
				    (unsigned long long)(site > 1 ? site - exeBase() : site), (unsigned long long)bo, reported ? 1 : 0);
				logged++;
			}
		}
		if (S->syncFullHash) {
			uint32_t hashDiff = 0;
			forEachCommittedPage([&hashDiff](size_t p) {
				if (hashPage(S->base + p * kPage) != S->syncPageHash[p]) hashDiff++;
			});
			if (S->syncCsv) std::fprintf(S->syncCsv, "#fullhash anchor=%llu committed_pages_differing=%u\n", (unsigned long long)r.anchor, hashDiff);
		}
		const uintptr_t exe = exeBase();
		int glogged = 0, plogged = 0;
		for (uint64_t g = 0; g < S->globPages; ++g) {
			bool pres    = false;
			const int at = firstGlobalDiff(g, S->syncGFull + g * kPage, &pres);
			if (at >= 0) {
				r.globalDiffPages++;
				if (glogged++ < 16 && S->syncCsv) {
					std::fprintf(S->syncCsv, "#gdiff anchor=%llu rva=0x%llx reported=%d\n", (unsigned long long)r.anchor,
					    (unsigned long long)(uintptr_t(S->globPagePtr[g]) + at - exe), S->syncGMark[g] ? 1 : 0);
				}
			}
			if (pres) {
				r.globalPresDiffPages++;
				if (plogged < 24 && S->syncCsv && S->syncTests < 40) {
					uint8_t mask[kPage];
					presMask(g, mask);
					const uint8_t* was = S->syncGFull + g * kPage;
					const uint8_t* cur = S->globPagePtr[g];
					for (int b = 0; b < int(kPage) && plogged < 24; ++b) {
						if (mask[b] && was[b] != cur[b]) {
							std::fprintf(S->syncCsv, "#pdiff anchor=%llu rva=0x%llx\n", (unsigned long long)r.anchor,
							    (unsigned long long)(uintptr_t(cur) + b - exe));
							plogged++;
							while (b + 1 < int(kPage) && mask[b + 1]) ++b; // one line per segment
						}
					}
				}
			}
		}
	}
	S->syncTests++;
	S->syncTestsK[r.k]++;
	if (!r.firstBad) {
		S->syncMatches++;
		S->syncMatchesK[r.k]++;
	}
	if (r.audioBad) S->syncAudioBadTests++;
	if (S->syncCsv) {
		std::fprintf(S->syncCsv,
		    "%llu,%d,%d,0x%x,%u,%u,%u,%u,%.4f,%u,%d,%u,%u,%u,%u,%.4f,%.4f,%.4f,%.4f,%u,%u,%u,%d,%d,%.1f\n",
		    (unsigned long long)r.anchor, r.k, r.firstBad, r.badMask, r.regionDiffPages, r.globalDiffPages, r.onlyFirst,
		    r.onlySecond, S->rsTotalMs, S->rsRegionPages + S->rsGlobPages, r.audioBad, r.regionUnreported,
		    r.globalPresDiffPages, r.regionUnion, r.globalUnion, S->rsRegionMs, S->rsShadowMs, S->rsGlobMs, S->rsWwMs,
		    S->rsRegionPages, S->rsGlobPages, S->rsEntries, S->syncDedup ? 1 : 0, S->syncDiverge ? 1 : 0, S->sysBusy);
		std::fflush(S->syncCsv);
	}
	pc_state_hash_spike_suppress_log(false);
	S->syncPhase  = 0;
	S->syncNextAt = tick + S->syncPeriod;
}

// ---------------------------------------------------------------------------
// Exit report
// ---------------------------------------------------------------------------
int cmpSiteCount(const void* a, const void* b)
{
	const Site* x = static_cast<const Site*>(a);
	const Site* y = static_cast<const Site*>(b);
	if (x->count != y->count) return x->count < y->count ? 1 : -1;
	return 0;
}

void dumpSites(FILE* out, const char* label, Site* table)
{
	Site* copy = static_cast<Site*>(osAllocZero(sizeof(Site) * kSites));
	if (!copy) return;
	size_t n = 0;
	for (size_t i = 0; i < kSites; ++i)
		if (table[i].ra) copy[n++] = table[i];
	std::qsort(copy, n, sizeof(Site), cmpSiteCount);
	std::fprintf(out, "[m6a] %s call sites: %zu distinct\n", label, n);
	for (size_t i = 0; i < n && i < 400; ++i) {
		char name[MAX_PATH];
		uintptr_t mod = 0;
		moduleOf(copy[i].ra, name, sizeof(name), &mod);
		const char* cat = table == S->offSites       ? kOffNames[copy[i].cat]
		                : table == S->unknownSites   ? kUnknownNames[copy[i].cat & 3]
		                : table == S->mallocSites    ? kMallocNames[copy[i].cat % kMallocCount]
		                                             : "region";
		std::fprintf(out, "[m6a] site %s cat=%s module=%s rva=0x%llx count=%llu bytes=%llu\n", label, cat, name,
		    (unsigned long long)(copy[i].ra - mod), (unsigned long long)copy[i].count, (unsigned long long)copy[i].bytes);
	}
	VirtualFree(copy, 0, MEM_RELEASE);
}

void atExitReport()
{
	if (!S) return;
	if (S->csv) std::fflush(S->csv);
	if (S->syncCsv) std::fflush(S->syncCsv);
	const char* path = std::getenv("PIKMIN_NETPLAY_SNAPSHOT_SPIKE_REPORT");
	if (!path || !*path) path = "snapshot_spike_sites.txt";
	FILE* out = std::fopen(path, "w");
	if (!out) out = stdout;
	std::fprintf(out, "[m6a] exe_base=0x%llx region_base=0x%llx mode=%s ww=%d split=%d\n",
	    (unsigned long long)exeBase(), (unsigned long long)reinterpret_cast<uintptr_t>(S->base),
	    S->timing ? "timing" : "measure", S->writeWatch ? 1 : 0, S->wwSplit ? 1 : 0);
	std::fprintf(out, "[m6a] region allocs=%llu bytes=%llu frees=%llu bytes=%llu live_blocks=%llu live_bytes=%llu frees_off_main=%llu bad_frees=%llu\n",
	    (unsigned long long)S->totalAllocs, (unsigned long long)S->totalAllocBytes,
	    (unsigned long long)S->totalFrees, (unsigned long long)S->totalFreeBytes,
	    (unsigned long long)S->meta->liveBlocks, (unsigned long long)S->meta->liveBytes,
	    (unsigned long long)S->totalRegionFreesOffMain, (unsigned long long)S->badFrees);
	std::fprintf(out, "[m6a] off-region pre_init(count only, before the spike state) count=%lld bytes=%lld\n",
	    sPreInitCount, sPreInitBytes);
	for (int c = 0; c < kOffCount; ++c) {
		std::fprintf(out, "[m6a] off-region %s count=%lld bytes=%lld\n", kOffNames[c],
		    (long long)S->offCount[c], (long long)S->offBytesTotal[c]);
	}
	std::fprintf(out, "[m6a] unknown_frees=%llu site_overflow=%llu globals_pages=%llu preserve_segs=%u\n",
	    piki_pc_spike_unknown_frees(),
	    (unsigned long long)S->siteOverflow, (unsigned long long)S->globPages, S->preserveCount);
	std::fprintf(out, "[m6a] coverage: ww_late=%llu conc_region=%llu conc_globals=%llu audits=%llu audit_pages=%llu audit_missed=%llu\n",
	    (unsigned long long)S->totalWwLate, (unsigned long long)S->totalConcRegion,
	    (unsigned long long)S->totalConcGlobal, (unsigned long long)S->audits, (unsigned long long)S->auditPages,
	    (unsigned long long)S->auditMissed);
	if (S->mallocAudit) {
		for (int c = 0; c < kMallocCount; ++c) {
			std::fprintf(out, "[m6a] malloc %s count=%lld bytes=%lld\n", kMallocNames[c], (long long)S->mallocCount[c],
			    (long long)S->mallocBytes[c]);
		}
		std::fprintf(out, "[m6a] malloc free_calls=%lld hooks=%d\n", (long long)S->freeCalls, gRealMalloc ? 1 : 0);
	}
	if (S->offTrack) {
		std::fprintf(out, "[m6a] off-table live=%llu overflow=%llu\n", (unsigned long long)S->offLive,
		    (unsigned long long)S->offOverflow);
	}
	if (S->syncKArg != 0) {
		std::fprintf(out, "[m6a] synctest k=%d tests=%llu matches=%llu aborted=%llu audio_bad_tests=%llu dedup=%d diverge=%d\n",
		    S->syncKArg, (unsigned long long)S->syncTests, (unsigned long long)S->syncMatches,
		    (unsigned long long)S->syncAborted, (unsigned long long)S->syncAudioBadTests, S->syncDedup ? 1 : 0,
		    S->syncDiverge ? 1 : 0);
		for (int k = 1; k <= kMaxK; ++k) {
			if (S->syncTestsK[k]) {
				std::fprintf(out, "[m6a] synctest k=%d tests=%llu matches=%llu\n", k,
				    (unsigned long long)S->syncTestsK[k], (unsigned long long)S->syncMatchesK[k]);
			}
		}
	}
	std::fprintf(out, "[m6a] unknown frees by kind: arena_zone=%llu image=%llu private_heap=%llu other=%llu\n",
	    (unsigned long long)S->unknownByKind[0], (unsigned long long)S->unknownByKind[1],
	    (unsigned long long)S->unknownByKind[2], (unsigned long long)S->unknownByKind[3]);
	dumpSites(out, "off", S->offSites);
	dumpSites(out, "unknown", S->unknownSites);
	dumpSites(out, "region", S->regionSites);
	if (S->mallocAudit) dumpSites(out, "malloc", S->mallocSites);
	if (out != stdout) std::fclose(out);
	std::printf("[m6a] exit report written (%s); region live_blocks=%llu live_bytes=%llu synctests=%llu matches=%llu\n",
	    path, (unsigned long long)S->meta->liveBlocks, (unsigned long long)S->meta->liveBytes,
	    (unsigned long long)S->syncTests, (unsigned long long)S->syncMatches);
	std::fflush(stdout);
}

LONG WINAPI crashFilter(EXCEPTION_POINTERS* info)
{
	if (S && info && info->ExceptionRecord) {
		const EXCEPTION_RECORD* e = info->ExceptionRecord;
		char name[MAX_PATH];
		uintptr_t mod = 0;
		moduleOf(reinterpret_cast<uintptr_t>(e->ExceptionAddress), name, sizeof(name), &mod);
		const unsigned long long data = e->NumberParameters >= 2 ? (unsigned long long)e->ExceptionInformation[1] : 0ull;
		std::printf("[m6a] CRASH code=0x%08lx module=%s rva=0x%llx data=0x%llx tid_main=%d tick=%llu sync_phase=%d sync_anchor=%llu sync_step=%d\n",
		    (unsigned long)e->ExceptionCode, name,
		    (unsigned long long)(reinterpret_cast<uintptr_t>(e->ExceptionAddress) - mod), data,
		    GetCurrentThreadId() == S->mainTid ? 1 : 0, (unsigned long long)pc_state_hash_tick(), S->syncPhase,
		    (unsigned long long)S->syncAnchor, S->syncStep);
		// Crude stack scan: every qword near RSP that points into the exe
		// image, as an rva (the first ones are the most recent callers).
		const uintptr_t exe = exeBase();
		const uint64_t* sp  = reinterpret_cast<const uint64_t*>(info->ContextRecord->Rsp);
		int shown           = 0;
		std::printf("[m6a] CRASH stack exe rvas:");
		for (int i = 0; i < 512 && shown < 24; ++i) {
			const uint64_t v = sp[i];
			if (v > exe && v < exe + 0x20000000ull) {
				std::printf(" 0x%llx", (unsigned long long)(v - exe));
				++shown;
			}
		}
		std::printf("\n");
		std::fflush(stdout);
		if (S->csv) std::fflush(S->csv);
		if (S->syncCsv) std::fflush(S->syncCsv);
	}
	return EXCEPTION_CONTINUE_SEARCH;
}

void loadPreserveFile()
{
	const char* path = std::getenv("PIKMIN_NETPLAY_SNAPSHOT_PRESERVE");
	if (!path || !*path) return;
	FILE* in = std::fopen(path, "r");
	if (!in) {
		std::fprintf(stderr, "[m6a] cannot open preserve file %s\n", path);
		return;
	}
	const uintptr_t exe = exeBase();
	unsigned long long lo = 0, hi = 0;
	char line[1024];
	int n = 0;
	S->preserveBatch = true;
	while (std::fgets(line, sizeof(line), in)) {
		if (line[0] == '#') continue;
		if (std::sscanf(line, "%llx %llx", &lo, &hi) == 2 && hi > lo) {
			pc_snapshot_spike_preserve(reinterpret_cast<void*>(exe + lo), size_t(hi - lo));
			++n;
		}
	}
	std::fclose(in);
	S->preserveBatch = false;
	pc_snapshot_spike_preserve(nullptr, 0);
	std::printf("[m6a] preserve file %s: %d ranges\n", path, n);
}

bool envIs(const char* name, const char* value)
{
	const char* v = std::getenv(name);
	return v && !std::strcmp(v, value);
}

int envInt(const char* name, int fallback)
{
	const char* v = std::getenv(name);
	return (v && *v) ? std::atoi(v) : fallback;
}

// Globals changed-byte ranges for a sample tick (MV-11), before the save
// copies the live page into the shadow.
void logGlobalsDirty(uint64_t tick)
{
	if (!S->gdirtyLog) return;
	const uintptr_t exe = exeBase();
	int runs            = 0;
	for (uint32_t j = 0; j < S->globDirtyCount && runs < 1024; ++j) {
		const uint64_t gi  = S->globDirty[j];
		const uint8_t* was = S->globShadow + gi * kPage;
		const uint8_t* cur = S->globPagePtr[gi];
		int b              = 0;
		while (b < int(kPage) && runs < 1024) {
			if (was[b] == cur[b]) {
				++b;
				continue;
			}
			int e = b;
			while (e < int(kPage) && (was[e] != cur[e] || (e + 8 < int(kPage) && std::memcmp(was + e, cur + e, 8) != 0))) ++e;
			std::fprintf(S->gdirtyLog, "%llu %llx %llx\n", (unsigned long long)tick,
			    (unsigned long long)(uintptr_t(cur) + b - exe), (unsigned long long)(uintptr_t(cur) + e - exe));
			runs++;
			b = e;
		}
	}
}

} // namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
void pc_snapshot_spike_init(void)
{
	const char* on = std::getenv("PIKMIN_NETPLAY_SNAPSHOT_SPIKE");
	if (!on || on[0] != '1' || on[1] != '\0' || S) return;

	State* s = static_cast<State*>(osAllocZero(sizeof(State)));
	if (!s) return;
	InitializeSRWLock(&s->lock);
	InitializeSRWLock(&s->siteLock);
	InitializeSRWLock(&s->offLock);
	s->mainTid = GetCurrentThreadId();
	LARGE_INTEGER f;
	QueryPerformanceFrequency(&f);
	s->msPerCount = 1000.0 / double(f.QuadPart);

	const char* wwEnv = std::getenv("PIKMIN_NETPLAY_SNAPSHOT_SPIKE_WW");
	s->writeWatch     = !(wwEnv && wwEnv[0] == '0');
	s->splitAuth      = envIs("PIKMIN_NETPLAY_SNAPSHOT_SPIKE_SPLIT", "1");
	s->wwSplit        = envIs("PIKMIN_NETPLAY_SNAPSHOT_SPIKE_WW_SPLIT", "1");
	s->timing         = envIs("PIKMIN_NETPLAY_SNAPSHOT_SPIKE_MODE", "timing");
	s->nowwPages      = uint32_t(envInt("PIKMIN_NETPLAY_SNAPSHOT_SPIKE_NOWW_PAGES", 740));
	s->auditEvery     = s->writeWatch ? envInt("PIKMIN_NETPLAY_SNAPSHOT_SPIKE_AUDIT", 0) : 0;
	s->ptrScan        = s->auditEvery > 0 && envIs("PIKMIN_NETPLAY_SNAPSHOT_SPIKE_PTRSCAN", "1");
	s->offTrack       = s->ptrScan;
	s->mallocAudit    = envIs("PIKMIN_NETPLAY_SNAPSHOT_SPIKE_MALLOC_AUDIT", "1");
	uint8_t* base = nullptr;
	for (uintptr_t want : kPreferredBases) {
		base = static_cast<uint8_t*>(VirtualAlloc(reinterpret_cast<void*>(want), kReserve,
		    s->writeWatch ? (MEM_RESERVE | MEM_WRITE_WATCH) : MEM_RESERVE, PAGE_READWRITE));
		if (base) break;
	}
	if (!base) {
		std::fprintf(stderr, "[m6a] fixed-VA reservation failed (%lu); spike off\n", GetLastError());
		return;
	}
	s->base = base;
	S       = s; // from here the helpers may use S

	buildClasses();
	osCommit(base + kMetaOff, kMetaCommit);
	osCommit(base + kArenaOff, kArenaSize);
	Meta* m           = reinterpret_cast<Meta*>(base + kMetaOff);
	m->magic          = 0x4D36414D45544131ull;
	m->smallWild      = base + kSmallOff;
	m->smallCommitted = base + kSmallOff;
	m->largeWild      = base + kLargeOff;
	m->largeCommitHw  = base + kLargeOff;
	s->meta           = m;

	s->shadow     = static_cast<uint8_t*>(osReserve(kReserve));
	s->scratch    = static_cast<uint8_t*>(osReserve(kReserve));
	s->smallSite  = static_cast<uintptr_t*>(osReserve(kSmallUnits * sizeof(uintptr_t)));
	s->largeSite  = static_cast<uintptr_t*>(osAllocZero(kLargePages * sizeof(uintptr_t)));
	s->wwCap      = kPages;
	s->wwAddrs    = static_cast<void**>(osAllocZero(kPages * sizeof(void*)));
	s->pageMark   = static_cast<uint32_t*>(osAllocZero(kPages * sizeof(uint32_t)));
	s->dirtyList  = static_cast<uint32_t*>(osAllocZero(kPages * sizeof(uint32_t)));
	s->carryList  = static_cast<uint32_t*>(osAllocZero(kPages * sizeof(uint32_t)));
	s->restoreTmp = static_cast<uint32_t*>(osAllocZero(kPages * sizeof(uint32_t)));
	s->touched    = static_cast<uint64_t*>(osAllocZero(kPages / 8));
	s->runs       = static_cast<Run*>(osAllocZero(kMaxRuns * sizeof(Run)));
	s->runsStale  = true;
	s->rUndo      = static_cast<uint8_t*>(osReserve(kUndoPoolPages * kPage));
	s->rUndoIdx   = static_cast<uint32_t*>(osAllocZero(kUndoPoolPages * sizeof(uint32_t)));
	s->regionSites  = static_cast<Site*>(osAllocZero(sizeof(Site) * kSites));
	s->offSites     = static_cast<Site*>(osAllocZero(sizeof(Site) * kSites));
	s->unknownSites = static_cast<Site*>(osAllocZero(sizeof(Site) * kSites));
	s->mallocSites  = static_cast<Site*>(osAllocZero(sizeof(Site) * kSites));
	s->epoch       = 1;
	s->missed      = -1;
	s->sysBusy     = -1.0;
	s->preserveRaw = static_cast<Range*>(std::calloc(kPreserveMax, sizeof(Range)));
	s->doRestore   = !s->timing && !envIs("PIKMIN_NETPLAY_SNAPSHOT_SPIKE_RESTORE", "0");
	if (s->auditEvery > 0) {
		s->dirtySince = static_cast<uint64_t*>(osAllocZero(kPages / 8));
		s->auditHave  = static_cast<uint64_t*>(osAllocZero(kPages / 8));
		s->pageHash   = static_cast<uint64_t*>(osAllocZero(kPages * sizeof(uint64_t)));
		static const uint8_t zero[kPage] = {};
		s->zeroHash   = hashPage(zero);
		const char* ap = std::getenv("PIKMIN_NETPLAY_SNAPSHOT_SPIKE_AUDIT_LOG");
		s->auditLog    = std::fopen((ap && *ap) ? ap : "snapshot_spike_audit.txt", "w");
	}
	if (s->offTrack) {
		s->offTable  = static_cast<OffBlock*>(osAllocZero(kOffTableCap * sizeof(OffBlock)));
		s->offSorted = static_cast<OffBlock*>(osAllocZero(kOffTableCap * sizeof(OffBlock)));
		s->pairs     = static_cast<Pair*>(osAllocZero(kPairCap * sizeof(Pair)));
		if (!s->offTable || !s->offSorted || !s->pairs) s->offTrack = s->ptrScan = false;
	}
	if (!s->timing) {
		const char* gp = std::getenv("PIKMIN_NETPLAY_SNAPSHOT_SPIKE_GDIRTY_LOG");
		s->gdirtyLog   = std::fopen((gp && *gp) ? gp : "snapshot_spike_gdirty.txt", "w");
	}

	setupGlobals();
	piki_pc_spike_register_preserve();
	pc_os_stubs_spike_register_preserve();
	loadPreserveFile();

	const char* k = std::getenv("PIKMIN_NETPLAY_SNAPSHOT_SYNCTEST");
	if (k && *k) {
		if (!std::strcmp(k, "cycle") || !std::strcmp(k, "1-7")) s->syncKArg = -1;
		else s->syncKArg = std::atoi(k);
		if (s->syncKArg > kMaxK) s->syncKArg = kMaxK;
		if (s->syncKArg < -1) s->syncKArg = 0;
		s->syncPeriod     = envInt("PIKMIN_NETPLAY_SNAPSHOT_SYNCTEST_PERIOD", 30);
		if (s->syncPeriod < 1) s->syncPeriod = 1;
		const char* start = std::getenv("PIKMIN_NETPLAY_SNAPSHOT_SYNCTEST_START");
		s->syncStart      = (start && *start) ? std::strtoull(start, nullptr, 10) : 600;
		// A test still open when the harness exits would leave its ticks
		// unlogged (divergent mode suppresses the first pass): stop early.
		const char* exitAfter = std::getenv("PIKMIN_NETPLAY_EXIT_AFTER_TICKS");
		const uint64_t endAt  = (exitAfter && *exitAfter) ? std::strtoull(exitAfter, nullptr, 10) : 0;
		s->syncEnd            = endAt > uint64_t(2 * kMaxK + 2) ? endAt - uint64_t(2 * kMaxK + 2) : ~0ull;
		s->syncDedup      = !envIs("PIKMIN_NETPLAY_SNAPSHOT_SYNCTEST_RESTORE", "naive");
		s->syncDiverge    = envIs("PIKMIN_NETPLAY_SNAPSHOT_SYNCTEST_DIVERGE", "1");
		s->syncAny        = envIs("PIKMIN_NETPLAY_SNAPSHOT_SYNCTEST_ANY", "1");
		s->syncFullHash   = envIs("PIKMIN_NETPLAY_SNAPSHOT_SYNCTEST_FULLHASH", "1");
		s->syncFull       = static_cast<uint8_t*>(osReserve(kReserve));
		s->syncTouched    = static_cast<uint64_t*>(osAllocZero(kPages / 8));
		s->syncGFull      = static_cast<uint8_t*>(osAllocZero(s->globBytes));
		s->syncMark       = static_cast<uint32_t*>(osAllocZero(kPages * sizeof(uint32_t)));
		s->rStamp         = static_cast<uint32_t*>(osAllocZero(kPages * sizeof(uint32_t)));
		s->restoredList   = static_cast<uint32_t*>(osAllocZero(kPages * sizeof(uint32_t)));
		if (s->syncFullHash) s->syncPageHash = static_cast<uint64_t*>(osAllocZero(kPages * sizeof(uint64_t)));
		syncLogOpen();
	}

	openCsv();
	std::atexit(atExitReport);
	SetUnhandledExceptionFilter(crashFilter);
	sampleLoad();
	s->active = true;
	int hooks = 0;
	if (s->mallocAudit) hooks = patchImports();
	pc_snapshot_spike_game_describe();
	std::printf("[m6a] snapshot spike ON: region=%p reserve=%zu MB globals=%llu pages (%llu KB) mode=%s ww=%d split=%d "
	            "audit=%d ptrscan=%d malloc_hooks=%d synctest_k=%d period=%d dedup=%d diverge=%d preserve=%u\n",
	    (void*)base, kReserve >> 20, (unsigned long long)s->globPages, (unsigned long long)(s->globBytes >> 10),
	    s->timing ? "timing" : "measure", s->writeWatch ? 1 : 0, s->wwSplit ? 1 : 0, s->auditEvery, s->ptrScan ? 1 : 0,
	    hooks, s->syncKArg, s->syncPeriod, s->syncDedup ? 1 : 0, s->syncDiverge ? 1 : 0, s->preserveCount);
	std::fflush(stdout);
}

bool pc_snapshot_spike_active(void) { return S != nullptr && S->active; }

void* pc_snapshot_spike_new(size_t size, void* ra)
{
	State* s = S;
	if (!s || !s->active) {
		// Before init (static construction): counted, never routed.
		sPreInitCount++;
		sPreInitBytes += (long long)size;
		return nullptr;
	}
	int cat;
	if (!onMain()) cat = kOffOtherThread;
	else if (s->infraDepth > 0) cat = s->infraCat;
	else if (s->loopStarted && !s->inIdle) cat = kOffOutsideIdle;
	else {
		AcquireSRWLockExclusive(&s->lock);
		void* p = regionAlloc(size, reinterpret_cast<uintptr_t>(ra));
		if (p) {
			const size_t cap = static_cast<BlockHdr*>(p)[-1].cap;
			s->allocs++;
			s->allocBytes += cap;
			s->totalAllocs++;
			s->totalAllocBytes += cap;
		}
		ReleaseSRWLockExclusive(&s->lock);
		if (p) {
			noteSite(s->regionSites, reinterpret_cast<uintptr_t>(ra), size, 0);
			if (size >= (size_t(1) << 20)) {
				std::printf("[m6a] big region block %zu KB at %p (caller rva 0x%llx) tick=%llu\n", size >> 10, p,
				    (unsigned long long)(reinterpret_cast<uintptr_t>(ra) - exeBase()),
				    (unsigned long long)pc_state_hash_tick());
			}
			return p;
		}
		cat = kOffRegionFull;
	}
	InterlockedIncrement64(&s->offCount[cat]);
	InterlockedAdd64(&s->offBytesTotal[cat], (LONG64)size);
	if (onMain()) {
		s->offAllocs++;
		s->offBytes += size;
	}
	noteSite(s->offSites, reinterpret_cast<uintptr_t>(ra), size, uint32_t(cat));
	return nullptr;
}

void* pc_snapshot_spike_note_off(void* p, size_t size, void* ra)
{
	State* s = S;
	if (s && s->active && s->offTrack && p) offInsert(p, size, reinterpret_cast<uintptr_t>(ra));
	return p;
}

bool pc_snapshot_spike_delete(void* ptr, void* ra)
{
	State* s = S;
	if (!s || !ptr) return false;
	uint8_t* p = static_cast<uint8_t*>(ptr);
	if (p < s->base + kSmallOff || p >= s->base + kReserve) {
		if (onMain()) s->lastDeleteRa = ra;
		if (s->offTrack) offRemove(ptr);
		return false;
	}
	if (!onMain()) {
		// F5: a region free from another thread would race the tick-end
		// save and make region addresses depend on thread timing. None was
		// ever seen; refuse it loudly instead of tolerating it.
		s->regionFreesOffMain++;
		s->totalRegionFreesOffMain++;
		std::printf("[m6a] FATAL region free %p from a non-main thread (caller rva 0x%llx) tick=%llu\n", ptr,
		    (unsigned long long)(reinterpret_cast<uintptr_t>(ra) - exeBase()), (unsigned long long)pc_state_hash_tick());
		std::fflush(stdout);
		std::abort();
	}
	AcquireSRWLockExclusive(&s->lock);
	const size_t cap = regionFree(ptr, ra);
	s->frees++;
	s->freeBytes += cap;
	s->totalFrees++;
	s->totalFreeBytes += cap;
	ReleaseSRWLockExclusive(&s->lock);
	return true;
}

void pc_snapshot_spike_unknown_free(void* ptr)
{
	State* s = S;
	if (!s || !s->active) return;
	// kind: 0 = region arena zone (an AyuHeap block of the sys heap),
	// 1 = exe/DLL image (a static object), 2 = other private memory (the C
	// heap: blocks some other allocator made), 3 = anything else.
	uint32_t kind = 3;
	uint8_t* p    = static_cast<uint8_t*>(ptr);
	if (p >= s->base && p < s->base + kSmallOff) {
		kind = 0;
	} else {
		MEMORY_BASIC_INFORMATION mbi;
		if (VirtualQuery(ptr, &mbi, sizeof(mbi)) == sizeof(mbi)) {
			if (mbi.Type == MEM_IMAGE) kind = 1;
			else if (mbi.Type == MEM_PRIVATE) kind = 2;
		}
	}
	InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(&s->unknownByKind[kind]));
	const uintptr_t ra = onMain() ? reinterpret_cast<uintptr_t>(s->lastDeleteRa) : 1;
	noteSite(s->unknownSites, ra, 0, kind);
}

bool pc_snapshot_spike_arena(void** lo, size_t* bytes)
{
	if (!S || !S->active) return false;
	*lo    = S->base + kArenaOff;
	*bytes = kArenaSize;
	return true;
}

void pc_snapshot_spike_frame_begin(void)
{
	if (!S || !S->active) return;
	S->loopStarted = true;
	S->tFrame      = now();
}

void pc_snapshot_spike_idle_begin(void)
{
	if (!S || !S->active) return;
	// divergent synctest: perturb the first pass's inputs (the pads are
	// final here: pc_input_log_tick ran; the controllers read them in idle)
	if (S->syncPhase == 1 && S->syncDiverge) pc_snapshot_spike_game_perturb_input();
	S->inIdle        = true;
	S->authMarked    = false;
	S->instrInIdleMs = 0.0;
	S->tPresentEnd = S->tDoneBegin = S->tDoneEnd = S->tParseEnd = 0;
	S->tIdle         = now();
}

void pc_snapshot_spike_auth_end(void)
{
	if (!S || !S->active || !S->inIdle) return;
	S->tAuth      = now();
	S->authMarked = true;
	const int64_t t0 = now();
	if (S->splitAuth) S->rdAuth = collectWriteWatch();
	const double ww = msSince(t0);
	S->wwMs += ww;
	S->instrInIdleMs += ww;
}

void pc_snapshot_spike_mark(int which)
{
	if (!S || !S->active || !S->inIdle) return;
	const int64_t t = now();
	switch (which) {
	case kPcSpikeMarkPresentEnd: S->tPresentEnd = t; break;
	case kPcSpikeMarkDoneBegin: S->tDoneBegin = t; break;
	case kPcSpikeMarkDoneEnd: S->tDoneEnd = t; break;
	case kPcSpikeMarkParseEnd: S->tParseEnd = t; break;
	default: break;
	}
}

void pc_snapshot_spike_idle_end(void)
{
	if (!S || !S->active) return;
	S->inIdle   = false;
	S->tIdleEnd = now();
}

void pc_snapshot_spike_infra_push(int category)
{
	if (!S || !S->active || !onMain()) return;
	if (S->infraDepth++ == 0) {
		S->infraCat = category == kPcSpikeInfraPresent      ? kOffPresent
		            : category == kPcSpikeInfraDoneRender ? kOffDoneRender
		                                                  : kOffInfraOther;
	}
}

void pc_snapshot_spike_infra_pop(void)
{
	if (!S || !S->active || !onMain()) return;
	if (S->infraDepth > 0) S->infraDepth--;
}

void pc_snapshot_spike_preserve(const void* p, size_t bytes)
{
	if (!S) return;
	if (bytes != 0 && S->preserveRawCount < kPreserveMax) {
		uint8_t* lo = static_cast<uint8_t*>(const_cast<void*>(p));
		S->preserveRaw[S->preserveRawCount++] = { lo, lo + bytes };
	}
	if (S->preserveBatch) return;
	// (re)build the per-page segments from scratch
	S->preserveCount = 0;
	buildPreserveSegs();
}

bool pc_snapshot_spike_resimulating(void) { return S && S->active && S->syncPhase == 2; }

void pc_snapshot_spike_tick_end(void)
{
	// Crowd bootstrap (MV-7): runs with the spike on or off, before this
	// tick's collect so its writes are in this tick's dirty set.
	pc_snapshot_spike_game_tick(pc_state_hash_tick());
	State* s = S;
	if (!s || !s->active) return;
	const int64_t tEnd   = now();
	const double frameMs = double(tEnd - s->tFrame) * s->msPerCount;
	const double idleMs  = double(s->tIdleEnd - s->tIdle) * s->msPerCount - s->instrInIdleMs;
	const double authMs  = s->authMarked ? double(s->tAuth - s->tIdle) * s->msPerCount : idleMs;
	const double presentMs = (s->authMarked && s->tPresentEnd) ? msBetween(s->tAuth, s->tPresentEnd) : 0.0;
	const double doneMs    = (s->tDoneBegin && s->tDoneEnd) ? msBetween(s->tDoneBegin, s->tDoneEnd) : 0.0;
	const double parseMs   = (s->tDoneEnd && s->tParseEnd) ? msBetween(s->tDoneEnd, s->tParseEnd) : 0.0;
	const double retraceMs = s->tParseEnd ? msBetween(s->tParseEnd, s->tIdleEnd) : 0.0;
	const uint64_t tick  = pc_state_hash_tick();
	const bool live      = naviMgr != nullptr;
	int phase = 0, pikis = 0;
	pc_snapshot_spike_game_sample(&phase, &pikis);
	if (tick % 30 == 0) sampleLoad();

	// 1. dirty pages since the auth-end collection (or since the last tick)
	int64_t t0 = now();
	if (s->writeWatch) {
		s->rdPost = collectWriteWatch();
	} else {
		emulateDirty();
		s->rdPost = s->dirtyCount;
	}
	s->wwMs += msSince(t0);
	if (!s->authMarked || !s->splitAuth) {
		s->rdAuth = s->rdPost;
		s->rdPost = 0;
	}
	uint32_t rdMeta = 0, rdArena = 0, rdSmall = 0, rdLarge = 0;
	for (uint32_t i = 0; i < s->dirtyCount; ++i) {
		const size_t off = size_t(s->dirtyList[i]) * kPage;
		if (off < kArenaOff) rdMeta++;
		else if (off < kArenaOff + kArenaSize) rdArena++;
		else if (off >= kSmallOff && off < kSmallEnd) rdSmall++;
		else rdLarge++;
	}

	// 2. save: old shadow -> undo pool, region -> shadow
	t0 = now();
	UndoTick& u     = s->ring[tick % kUndoRing];
	u.tick          = tick;
	u.valid         = false;
	const bool fits = s->dirtyCount <= kUndoPoolPages / 2 && !s->barrier;
	u.rStart        = s->rUndoCursor;
	u.rCount        = 0;
	for (uint32_t i = 0; i < s->dirtyCount; ++i) {
		const size_t idx = s->dirtyList[i];
		const size_t off = idx * kPage;
		ensureGranules(s->shadow, s->shadowCommitted, kGranules, off, kPage);
		if (fits) {
			const uint64_t slot = s->rUndoCursor % kUndoPoolPages;
			ensureGranules(s->rUndo, s->rUndoCommitted, (kUndoPoolPages * kPage) / kGranule, slot * kPage, kPage);
			std::memcpy(s->rUndo + slot * kPage, s->shadow + off, kPage);
			s->rUndoIdx[slot] = uint32_t(idx);
			s->rUndoCursor++;
			u.rCount++;
		}
		std::memcpy(s->shadow + off, s->base + off, kPage);
	}
	const double saveMs = msSince(t0);

	// 3. measurement mode: content-identical restore shadow -> region for the
	// same pages. Pages that changed since the save copy (a concurrent
	// writer) are counted and left alone, not clobbered (MV-3). Then pages
	// written after the collect by anyone else are carried into the next
	// tick instead of being lost to the reset.
	double restoreMs = 0.0, restoreWwMs = 0.0;
	s->wwLate = s->concRegion = s->concGlobal = 0;
	if (s->doRestore && s->writeWatch) {
		uint32_t n = 0;
		for (uint32_t i = 0; i < s->dirtyCount; ++i) {
			const size_t off = size_t(s->dirtyList[i]) * kPage;
			if (std::memcmp(s->base + off, s->shadow + off, kPage) != 0) s->concRegion++;
			else s->restoreTmp[n++] = s->dirtyList[i];
		}
		t0 = now();
		for (uint32_t i = 0; i < n; ++i) {
			const size_t off = size_t(s->restoreTmp[i]) * kPage;
			std::memcpy(s->base + off, s->shadow + off, kPage);
		}
		restoreMs = msSince(t0);
		s->wwLate = collectLate();
		t0        = now();
		resetWriteWatch();
		restoreWwMs = msSince(t0);
		s->totalWwLate += s->wwLate;
		s->totalConcRegion += s->concRegion;
	}

	// 4. globals: compare against the shadow; changed pages are saved
	t0 = now();
	s->globDirtyCount = 0;
	for (uint64_t i = 0; i < s->globPages; ++i) {
		if (std::memcmp(s->globPagePtr[i], s->globShadow + i * kPage, kPage) != 0) {
			s->globDirty[s->globDirtyCount++] = uint32_t(i);
		}
	}
	const double gcmpMs = msSince(t0);
	if (!s->timing && live && tick % 1000 == 0) logGlobalsDirty(tick);
	t0 = now();
	u.gStart = s->gUndoCursor;
	u.gCount = 0;
	for (uint32_t j = 0; j < s->globDirtyCount; ++j) {
		const uint64_t i    = s->globDirty[j];
		const uint64_t slot = s->gUndoCursor % s->gUndoPages;
		std::memcpy(s->gUndo + slot * kPage, s->globShadow + i * kPage, kPage);
		s->gUndoIdx[slot] = uint32_t(i);
		s->gUndoCursor++;
		u.gCount++;
		std::memcpy(s->globShadow + i * kPage, s->globPagePtr[i], kPage);
	}
	const double gsaveMs = msSince(t0);
	u.valid = fits;

	// 4b. measurement mode: the real globals restore of the pages just saved
	// (content-identical, preserve list applied), timed (MV-11). A page that
	// changed since the save copy is counted and skipped.
	double grestoreMs = 0.0;
	if (s->doRestore) {
		uint32_t n = 0;
		for (uint32_t j = 0; j < s->globDirtyCount; ++j) {
			const uint64_t i = s->globDirty[j];
			if (std::memcmp(s->globPagePtr[i], s->globShadow + i * kPage, kPage) != 0) s->concGlobal++;
			else s->globDirty[n++] = uint32_t(i);
		}
		t0 = now();
		for (uint32_t j = 0; j < n; ++j) restoreGlobalPage(s->globDirty[j], s->globShadow + size_t(s->globDirty[j]) * kPage);
		grestoreMs = msSince(t0);
		s->totalConcGlobal += s->concGlobal;
	}

	// 5. periodic full copies (measurement mode)
	double fullMs = -1.0, fullMb = -1.0, gfullMs = -1.0;
	if (!s->timing && s->writeWatch && tick % kFullEvery == 0) {
		fullMs = copyTouched(s->scratch, s->scratchCommitted, &fullMb);
		t0     = now();
		ensureGranules(s->scratch, s->scratchCommitted, kGranules, 0, s->globBytes);
		for (uint64_t i = 0; i < s->globPages; ++i) std::memcpy(s->scratch + i * kPage, s->globPagePtr[i], kPage);
		gfullMs = msSince(t0);
	}

	// 6. coverage audit and pointer scan
	s->missed = -1;
	if (s->auditEvery > 0 && tick % uint64_t(s->auditEvery) == 0) {
		runAudit(tick);
		if (s->ptrScan && live && (s->audits % 10) == 1) runPtrScan(tick);
	}

	const unsigned long long unknownFrees = piki_pc_spike_unknown_frees();
	if (s->csv) {
		std::fprintf(s->csv,
		    "%llu,%u,%d,%d,%.4f,%.4f,%.4f,"
		    "%u,%u,%u,%u,%u,%u,%u,%u,"
		    "%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.2f,%.4f,"
		    "%llu,%llu,%llu,%llu,%llu,%.3f,%.3f,%.3f,%.2f,"
		    "%llu,%llu,%llu,%llu,%d,%d,"
		    "%.4f,%.4f,%.4f,%u,%.2f,"
		    "%d,%d,%.1f,%.4f,%.4f,%.4f,%.4f,"
		    "%.4f,%.4f,%u,%u,%u,%u,%.2f,%.2f,"
		    "%.4f,%u,%u,%u,%lld,%d\n",
		    (unsigned long long)tick, pc_netplay_tick(), live ? 1 : 0, s->syncPhase == 2 ? 1 : 0, authMs, idleMs,
		    frameMs, s->rdAuth, s->rdPost, s->dirtyCount, rdMeta, rdArena, rdSmall, rdLarge, s->globDirtyCount,
		    s->wwMs, saveMs, restoreMs, restoreWwMs, gcmpMs, gsaveMs, fullMs, fullMb, gfullMs,
		    (unsigned long long)s->allocs, (unsigned long long)s->allocBytes, (unsigned long long)s->frees,
		    (unsigned long long)s->freeBytes, (unsigned long long)s->meta->liveBlocks,
		    double(s->meta->liveBytes) / (1024.0 * 1024.0),
		    double(s->meta->smallWild - (s->base + kSmallOff)) / (1024.0 * 1024.0),
		    double(s->meta->largeWild - (s->base + kLargeOff)) / (1024.0 * 1024.0),
		    (!s->timing && tick % kFullEvery == 0) ? touchedMb() : -1.0, (unsigned long long)s->offAllocs,
		    (unsigned long long)s->offBytes, (unsigned long long)s->regionFreesOffMain, unknownFrees,
		    u.valid ? 1 : 0, s->barrier ? 1 : 0, s->wwZoneMs[0], s->wwZoneMs[1], s->wwZoneMs[2], s->wwCalls,
		    double((s->meta->smallCommitted - (s->base + kSmallOff)) + (s->meta->largeCommitHw - (s->base + kLargeOff))
		        + kArenaSize + kMetaCommit) / (1024.0 * 1024.0),
		    phase, pikis, s->sysBusy, presentMs, doneMs, parseMs, retraceMs,
		    s->wwHotMs, s->wwColdMs, s->wwHotCalls, s->wwColdCalls, s->rdHot, s->rdCold, s->hotMb, s->coldMb,
		    grestoreMs, s->wwLate, s->concRegion, s->concGlobal, (long long)s->missed, s->syncPhase);
		if (tick % 300 == 0) std::fflush(s->csv);
	}
	s->ticksLogged++;

	// 7. synctest state machine (may roll region + globals back)
	syncOnTickEnd(tick, live, u.valid);

	// reset per-tick state
	s->epoch++;
	if (s->epoch == 0) {
		std::memset(s->pageMark, 0, kPages * sizeof(uint32_t));
		s->epoch = 1;
	}
	s->dirtyCount = 0;
	s->rdAuth = s->rdPost = 0;
	s->wwMs     = 0.0;
	s->wwZoneMs[0] = s->wwZoneMs[1] = s->wwZoneMs[2] = 0.0;
	s->wwHotMs = s->wwColdMs = 0.0;
	s->wwHotCalls = s->wwColdCalls = s->rdHot = s->rdCold = 0;
	s->hotMb = s->coldMb = 0.0;
	s->wwCalls  = 0;
	s->allocs = s->allocBytes = s->frees = s->freeBytes = 0;
	s->offAllocs = s->offBytes = 0;
	s->regionFreesOffMain = 0;
	s->barrier = false;
	s->authMarked = false;
}

#else // !_WIN32: the spike is Windows-only (write watch); a build with the
      // option on elsewhere is a configuration error, not a silent no-op.
#error "PIKMIN_NETPLAY_SNAPSHOT_SPIKE is Windows-only (MEM_WRITE_WATCH); CMake adds this TU only under WIN32"
#endif
