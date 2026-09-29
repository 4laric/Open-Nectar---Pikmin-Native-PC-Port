// Netplay M6b production snapshot/restore (issue #896). See pc_snapshot.h.
//
// Rules this TU keeps:
//   * it never calls operator new (it runs inside it): its own storage comes
//     from VirtualAlloc or the C heap;
//   * all of its mutable state lives in one VirtualAlloc'd struct, outside
//     the region and outside the exe's .data/.bss bracket, so a restore can
//     never roll it back;
//   * nothing runs unless PIKMIN_NETPLAY_SNAPSHOT=1 (the crowd bootstrap in
//     pc_snapshot_game.cpp follows its own switch, so a crowd run has an M1
//     twin with the snapshot off).
//
// Routing rule (opt-in SIM, M6b item 2 / F10): main-thread operator new goes
// to the region only while a SIM scope is open and no infra scope is:
//   - the authoritative pass: pc_snapshot_idle_begin .. pc_snapshot_auth_end
//     (a soft-reset idle -- stage loads, section changes -- never reaches the
//     auth end, so the whole idle runs as SIM);
//   - parseMessages;
//   - the PlugPikiApp construction (AyuHeaps, gameflow hard reset).
// Every other main-thread operator new stays on malloc and is counted by
// category and by call site in the exit report (the visible misses).
//
// Per tick (pc_snapshot_tick_end): one GetWriteWatch(RESET) over the region
// up to its commit high-water mark, a single copy of every dirty page into the
// ring, and the globals compare-and-undo inherited from the spike (M6b item 5
// replaces it with a linker-split bracket). Barriers re-baseline the ring.
//
// Env switches (all need PIKMIN_NETPLAY_SNAPSHOT=1):
//   PIKMIN_NETPLAY_SNAPSHOT_ARENA_MB=N      arena zone size (default 32; the
//       sys, ovl and app AyuHeaps nest in it: a FoH day uses ~16 MB of it)
//   PIKMIN_NETPLAY_SNAPSHOT_DEPTH=N         ring depth in frames (default 12)
//   PIKMIN_NETPLAY_SNAPSHOT_SLOTS=N         ring slot pool, pages (default 131072)
//   PIKMIN_NETPLAY_SNAPSHOT_GUARD=0|count|full  no between-tick region write
//       guard / count tail writes instead of aborting / also count the
//       pre-tick main-loop writes (a second GetWriteWatch call per tick)
//   PIKMIN_NETPLAY_SNAPSHOT_PRESERVE=file   globals preserve list (rva ranges)
//   PIKMIN_NETPLAY_SNAPSHOT_MEASURE=1       measurement protocol + CSV
//   PIKMIN_NETPLAY_SNAPSHOT_PIN=pcore       pin the main thread to P-cores
//   PIKMIN_NETPLAY_SNAPSHOT_CSV=file        per-tick CSV (default snapshot.csv)
//   PIKMIN_NETPLAY_SNAPSHOT_SYNCTEST=k|cycle in-process synctest through the
//       production save/restore (..._PERIOD, ..._START)
//   PIKMIN_NETPLAY_SNAPSHOT_REPORT=file     exit report (default snapshot_report.txt)

#include "netplay/pc_snapshot.h"

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
#else
#error "PIKMIN_NETPLAY_SNAPSHOT is Windows-only (MEM_WRITE_WATCH); CMake adds this TU only under WIN32"
#endif

#include "netplay/pc_netplay_det.h"
#include "netplay/pc_snapshot_region.h"
#include "netplay/pc_snapshot_ring.h"
#include "netplay/pc_state_hash.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>

class NaviMgr;
extern NaviMgr* naviMgr;

// Hooks in other TUs, defined there when they are compiled with the
// snapshot define (CMake adds it to exactly those TUs whenever this one is
// built). Strong references on purpose: with LTO, a definition reached only
// through a weak reference was dropped from the link (M6a).
void pc_state_hash_snapshot_suppress_log(bool on);
unsigned long long piki_pc_snapshot_unknown_frees(void);
void piki_pc_snapshot_register_preserve(void);
extern "C" bool pc_os_stubs_snapshot_static_arena(void** lo, size_t* bytes);
extern "C" void pc_os_stubs_snapshot_register_preserve(void);
// pc_snapshot_game.cpp (engine headers live there).
void pc_snapshot_game_tick(uint64_t tick);
void pc_snapshot_game_sample(int* phase, int* pikis);
void pc_snapshot_game_heap_sample(void);
void pc_snapshot_game_heap_report(FILE* out);

extern "C" char __data_start__[];
extern "C" char __data_end__[];
extern "C" char __bss_start__[];
extern "C" char __bss_end__[];

namespace {

using pcsnap::kPage;

constexpr int kMaxK            = 12;
constexpr int kGRing           = 32;     // globals undo records kept (ticks)
constexpr int kPreserveMax     = 16384;
constexpr size_t kSites        = 16384;
constexpr uint32_t kForcedBarrierPages = 12000; // a save this large re-baselines instead

enum OffCategory {
	kOffOtherThread = 0, // not the main thread
	kOffBoot,            // main thread before System::run, outside SIM
	kOffOutsideIdle,     // main loop, outside app->idle()
	kOffIdleNonSim,      // inside idle, SIM closed (presentation, doneRender, ...)
	kOffInfra,           // an infra scope inside SIM
	kOffCount
};
const char* const kOffNames[kOffCount] = { "other_thread", "boot", "outside_idle", "idle_nonsim", "infra" };

struct Site {
	uintptr_t ra;
	uint64_t count;
	uint64_t bytes;
	uint32_t cat;
	uint32_t pad;
};

struct Range {
	uint8_t* lo;
	uint8_t* hi;
};

struct PreserveSeg {
	uint32_t page;
	uint16_t off;
	uint16_t len;
};

struct GRec {
	uint64_t tick;
	uint64_t start; // absolute cursor in the globals undo pool
	uint32_t count;
	bool valid;
};

struct CpuMap {
	uint16_t group;
	uint8_t lp;
	uint8_t cls;
};

struct State {
	bool active;
	bool loopStarted;
	bool inIdle;
	bool authOpen;
	bool parseOpen;
	int simDepth;
	int infraDepth;
	DWORD mainTid;
	double msPerCount;

	pcsnap::Region region;
	pcsnap::PageRing ring;
	uint32_t* touchedList;
	uint32_t* pendingList;

	// globals (spike compare-and-undo)
	Range glob[4];
	int nGlob;
	uint64_t globPages;
	uint64_t globBytes;
	uint8_t* globShadow;
	uint8_t** globPagePtr;
	uint32_t* globDirty;
	uint32_t globDirtyCount;
	uint8_t* gUndo;
	uint32_t* gUndoIdx;
	uint64_t gUndoPages;
	uint64_t gUndoCursor;
	GRec gring[kGRing];
	uint32_t* gStamp;
	uint32_t gStampVal;
	PreserveSeg* preserveSegs;
	uint32_t preserveCount;
	uint32_t preserveCap;
	uint32_t* presFirst;
	uint16_t* presCount;
	Range* preserveRaw;
	int preserveRawCount;
	bool preserveBatch;

	// barriers
	bool barrierPending;
	const char* barrierReason;
	uint64_t lastBarrier;
	uint64_t barriers;
	uint64_t savedTick; // newest frame in the ring / globals log
	bool started;

	// guard (between-tick region writes). tail: from the tick-end save to
	// the top of the next loop iteration (other threads; the main thread only
	// runs pacing there): fatal unless GUARD=count. pre: from the loop top to
	// the tick start (pc_bbft_update, Jac_Gsync, CARDProbe, the pad poll):
	// counted and logged (main-thread sim mutations outside the tick).
	bool guard;
	bool guardAbort;
	bool guardFull;
	bool tickDone;
	bool topChecked;
	uint32_t guardTail;
	uint32_t guardPre;
	uint64_t guardTailPages, guardTailTicks;
	uint64_t guardPrePages, guardPreTicks;
	uint32_t guardPreFirst;

	// timing (this tick)
	int64_t tFrame, tIdle, tAuth, tIdleEnd;
	double wwMs, copyMs, gcmpMs, gsaveMs, rebaseMs, guardMs, calibUs;
	bool barrierThisTick;
	const char* barrierThisReason;

	// routing counters
	uint64_t allocs, frees; // this tick (main thread region)
	uint64_t totalAllocs, totalAllocBytes, totalFrees, totalFreeBytes;
	volatile LONG64 offCount[kOffCount];
	volatile LONG64 offBytes[kOffCount];
	Site* regionSites;
	Site* offSites;
	uint64_t siteOverflow;
	uint64_t maxScanned;
	uint64_t maxRingBytes;
	uint64_t maxRingSlots;
	uint64_t maxHeapUsed;

	// measurement protocol (item 0)
	bool measure;
	bool pinned;
	int maxClass;
	CpuMap* cpus;
	int nCpus;
	uint64_t ldIdle, ldKernel, ldUser, ldOwn;
	int64_t ldAt;
	double busyAll, busyOther;
	int cpuNow, clsNow;
	FILE* csv;

	// synctest (production path)
	int syncKArg; // 0 off, 1..kMaxK fixed, -1 cycle 1..7
	int syncK;
	int syncPeriod;
	uint64_t syncStart;
	uint64_t syncEnd;
	int syncPhase; // 0 idle, 1 first pass, 2 resim
	uint64_t syncAnchor;
	int syncStep;
	uint64_t syncHash[kMaxK + 1][8];
	uint64_t syncTests, syncMatches, syncSkipped, syncTestIndex;
	uint64_t syncTestsK[kMaxK + 1], syncMatchesK[kMaxK + 1];
	int syncFirstBad;
	unsigned syncBadMask;
	uint64_t syncNextAt;
	FILE* syncCsv;
	// last restore
	double rsCollectMs, rsCopyMs, rsGlobMs, rsResetMs, rsTotalMs;
	uint32_t rsUnion, rsZero, rsEntries, rsGlobPages, rsPending;
};

State* S = nullptr; // set once by init; the pointee lives outside every bracket
std::atomic<long long> sPreInitCount{ 0 };
std::atomic<long long> sPreInitBytes{ 0 };

inline int64_t now()
{
	LARGE_INTEGER t;
	QueryPerformanceCounter(&t);
	return t.QuadPart;
}
inline double msSince(int64_t t0) { return double(now() - t0) * S->msPerCount; }
inline bool onMain() { return GetCurrentThreadId() == S->mainTid; }
inline size_t alignUp(size_t v, size_t a) { return (v + a - 1) & ~(a - 1); }
uintptr_t exeBase() { return reinterpret_cast<uintptr_t>(GetModuleHandleA(nullptr)); }

bool envIs(const char* name, const char* value)
{
	const char* v = std::getenv(name);
	return v && !std::strcmp(v, value);
}

long envLong(const char* name, long fallback)
{
	const char* v = std::getenv(name);
	return (v && *v) ? std::strtol(v, nullptr, 10) : fallback;
}

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
// Call-site tables (main thread only, no lock)
// ---------------------------------------------------------------------------
void noteSite(Site* table, uintptr_t ra, size_t bytes, uint32_t cat)
{
	uintptr_t h = (ra >> 3) * 0x9E3779B97F4A7C15ull;
	size_t i    = (h >> 40) & (kSites - 1);
	for (size_t probe = 0; probe < kSites; ++probe) {
		Site& s = table[(i + probe) & (kSites - 1)];
		if (s.ra == ra && s.cat == cat) {
			s.count++;
			s.bytes += bytes;
			return;
		}
		if (s.ra == 0) {
			s = { ra, 1, bytes, cat, 0 };
			return;
		}
	}
	S->siteOverflow++;
}

// ---------------------------------------------------------------------------
// free/realloc import hooks: a region pointer handed to the C heap is fatal
// (C2-6). The exe links libstdc++ statically, so its CRT calls go through
// these imports too.
// ---------------------------------------------------------------------------
typedef void* (*ReallocFn)(void*, size_t);
typedef void (*FreeFn)(void*);
ReallocFn gRealRealloc = nullptr;
FreeFn gRealFree       = nullptr;
FreeFn gRealAlignedFree = nullptr;

void hookFree(void* p)
{
	if (p && S && S->region.contains(p)) pcsnap::fatal("free() of region pointer %p (caller rva 0x%llx)", p,
	    (unsigned long long)(reinterpret_cast<uintptr_t>(__builtin_return_address(0)) - exeBase()));
	gRealFree(p);
}

void hookAlignedFree(void* p)
{
	if (p && S && S->region.contains(p)) pcsnap::fatal("_aligned_free() of region pointer %p (caller rva 0x%llx)", p,
	    (unsigned long long)(reinterpret_cast<uintptr_t>(__builtin_return_address(0)) - exeBase()));
	gRealAlignedFree(p);
}

void* hookRealloc(void* p, size_t n)
{
	if (p && S && S->region.contains(p)) pcsnap::fatal("realloc() of region pointer %p (caller rva 0x%llx)", p,
	    (unsigned long long)(reinterpret_cast<uintptr_t>(__builtin_return_address(0)) - exeBase()));
	return gRealRealloc(p, n);
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
			void* repl  = nullptr;
			void** real = nullptr;
			if (!std::strcmp(name, "free")) {
				repl = reinterpret_cast<void*>(&hookFree);
				real = reinterpret_cast<void**>(&gRealFree);
			} else if (!std::strcmp(name, "realloc")) {
				repl = reinterpret_cast<void*>(&hookRealloc);
				real = reinterpret_cast<void**>(&gRealRealloc);
			} else if (!std::strcmp(name, "_aligned_free")) {
				repl = reinterpret_cast<void*>(&hookAlignedFree);
				real = reinterpret_cast<void**>(&gRealAlignedFree);
			}
			if (!repl || *real) continue;
			DWORD old = 0;
			if (!VirtualProtect(&thunk->u1.Function, sizeof(thunk->u1.Function), PAGE_READWRITE, &old)) continue;
			*real              = reinterpret_cast<void*>(thunk->u1.Function);
			thunk->u1.Function = reinterpret_cast<ULONG_PTR>(repl);
			VirtualProtect(&thunk->u1.Function, sizeof(thunk->u1.Function), old, &old);
			++patched;
		}
	}
	return patched;
}

// ---------------------------------------------------------------------------
// Globals: compare-and-undo against a shadow (the spike's method, kept until
// the linker-split bracket of M6b item 5 lands). The static 256 MB arena is
// cut out: the region's arena zone replaces it.
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
	if (pc_os_stubs_snapshot_static_arena(&aLo, &aBytes) && aLo >= (void*)bLo && aLo < (void*)bHi) {
		uint8_t* a0   = static_cast<uint8_t*>(aLo);
		uint8_t* a1   = a0 + aBytes;
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
	S->globShadow  = static_cast<uint8_t*>(pcsnap::osAllocZero(S->globBytes));
	S->globPagePtr = static_cast<uint8_t**>(pcsnap::osAllocZero(S->globPages * sizeof(uint8_t*)));
	S->globDirty   = static_cast<uint32_t*>(pcsnap::osAllocZero(S->globPages * sizeof(uint32_t)));
	S->presFirst   = static_cast<uint32_t*>(pcsnap::osAllocZero(S->globPages * sizeof(uint32_t)));
	S->presCount   = static_cast<uint16_t*>(pcsnap::osAllocZero(S->globPages * sizeof(uint16_t)));
	S->gStamp      = static_cast<uint32_t*>(pcsnap::osAllocZero(S->globPages * sizeof(uint32_t)));
	uint64_t gp = 0;
	for (int i = 0; i < S->nGlob; ++i) {
		for (uint8_t* p = S->glob[i].lo; p < S->glob[i].hi; p += kPage) S->globPagePtr[gp++] = p;
	}
	S->gUndoPages = S->globPages * 8;
	S->gUndo      = static_cast<uint8_t*>(pcsnap::osAllocZero(S->gUndoPages * kPage));
	S->gUndoIdx   = static_cast<uint32_t*>(pcsnap::osAllocZero(S->gUndoPages * sizeof(uint32_t)));
	if (!S->globShadow || !S->gUndo) pcsnap::fatal("cannot allocate the globals shadow/undo (%llu pages)", (unsigned long long)S->globPages);
	for (uint64_t i = 0; i < S->globPages; ++i) std::memcpy(S->globShadow + i * kPage, S->globPagePtr[i], kPage);
}

int cmpSeg(const void* a, const void* b)
{
	const PreserveSeg* x = static_cast<const PreserveSeg*>(a);
	const PreserveSeg* y = static_cast<const PreserveSeg*>(b);
	if (x->page != y->page) return x->page < y->page ? -1 : 1;
	return x->off < y->off ? -1 : x->off > y->off ? 1 : 0;
}

void buildPreserveSegs()
{
	S->preserveCount = 0;
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
				S->preserveCap  = S->preserveCap ? S->preserveCap * 2 : 256;
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
	uint8_t* dst     = S->globPagePtr[gi];
	const uint32_t n = S->presCount[gi];
	if (n == 0) {
		std::memcpy(dst, src, kPage);
		return;
	}
	const PreserveSeg* seg = S->preserveSegs + S->presFirst[gi];
	size_t at              = 0;
	for (uint32_t k = 0; k < n; ++k) {
		if (seg[k].off > at) std::memcpy(dst + at, src + at, seg[k].off - at);
		const size_t end = size_t(seg[k].off) + seg[k].len;
		if (end > at) at = end;
	}
	if (at < kPage) std::memcpy(dst + at, src + at, kPage - at);
}

void loadPreserveFile()
{
	const char* path = std::getenv("PIKMIN_NETPLAY_SNAPSHOT_PRESERVE");
	if (!path || !*path) return;
	FILE* in = std::fopen(path, "r");
	if (!in) pcsnap::fatal("cannot open preserve file %s", path);
	const uintptr_t exe = exeBase();
	unsigned long long lo = 0, hi = 0;
	char line[1024];
	int n = 0;
	S->preserveBatch = true;
	while (std::fgets(line, sizeof(line), in)) {
		if (line[0] == '#') continue;
		if (std::sscanf(line, "%llx %llx", &lo, &hi) == 2 && hi > lo) {
			pc_snapshot_preserve(reinterpret_cast<void*>(exe + lo), size_t(hi - lo));
			++n;
		}
	}
	std::fclose(in);
	S->preserveBatch = false;
	pc_snapshot_preserve(nullptr, 0);
	std::printf("[snapshot] preserve file %s: %d ranges, %u segments\n", path, n, S->preserveCount);
}

// Compare every global page with its shadow; changed pages get their
// pre-image logged for `tick` and the shadow updated.
void globalsSave(uint64_t tick)
{
	int64_t t0 = now();
	S->globDirtyCount = 0;
	for (uint64_t i = 0; i < S->globPages; ++i) {
		if (std::memcmp(S->globPagePtr[i], S->globShadow + i * kPage, kPage) != 0) S->globDirty[S->globDirtyCount++] = uint32_t(i);
	}
	S->gcmpMs = msSince(t0);
	t0        = now();
	GRec& r   = S->gring[tick % kGRing];
	r.tick    = tick;
	r.start   = S->gUndoCursor;
	r.count   = 0;
	r.valid   = S->globDirtyCount <= S->gUndoPages / 4;
	for (uint32_t j = 0; j < S->globDirtyCount; ++j) {
		const uint64_t i = S->globDirty[j];
		if (r.valid) {
			const uint64_t slot = S->gUndoCursor % S->gUndoPages;
			std::memcpy(S->gUndo + slot * kPage, S->globShadow + i * kPage, kPage);
			S->gUndoIdx[slot] = uint32_t(i);
			S->gUndoCursor++;
			r.count++;
		}
		std::memcpy(S->globShadow + i * kPage, S->globPagePtr[i], kPage);
	}
	S->gsaveMs = msSince(t0);
}

// Globals rebaseline: shadow := current; no undo record is older than now.
void globalsRebaseline(uint64_t tick)
{
	for (uint64_t i = 0; i < S->globPages; ++i) std::memcpy(S->globShadow + i * kPage, S->globPagePtr[i], kPage);
	for (int i = 0; i < kGRing; ++i) S->gring[i].valid = false;
	GRec& r = S->gring[tick % kGRing];
	r       = { tick, S->gUndoCursor, 0, true };
}

bool globalsHave(uint64_t from, uint64_t to)
{
	for (uint64_t t = from; t <= to; ++t) {
		const GRec& r = S->gring[t % kGRing];
		if (!r.valid || r.tick != t) return false;
		if (S->gUndoCursor - r.start > S->gUndoPages) return false;
	}
	return true;
}

// Globals back to the end of `anchor` from the end of `newest`: the oldest
// pre-image of each page after the anchor is its anchor state.
uint32_t globalsRestore(uint64_t anchor, uint64_t newest)
{
	if (++S->gStampVal == 0) {
		std::memset(S->gStamp, 0, S->globPages * sizeof(uint32_t));
		S->gStampVal = 1;
	}
	uint32_t n = 0;
	for (uint64_t t = anchor + 1; t <= newest; ++t) {
		GRec& r = S->gring[t % kGRing];
		for (uint32_t i = 0; i < r.count; ++i) {
			const uint64_t slot = (r.start + i) % S->gUndoPages;
			const uint32_t gi   = S->gUndoIdx[slot];
			if (S->gStamp[gi] == S->gStampVal) continue;
			S->gStamp[gi]      = S->gStampVal;
			const uint8_t* src = S->gUndo + slot * kPage;
			restoreGlobalPage(gi, src);
			std::memcpy(S->globShadow + size_t(gi) * kPage, src, kPage);
			++n;
		}
	}
	// the dropped ticks' pre-images are free again
	const GRec& first = S->gring[(anchor + 1) % kGRing];
	if (newest > anchor && first.valid && first.tick == anchor + 1) S->gUndoCursor = first.start;
	for (uint64_t t = anchor + 1; t <= newest; ++t) S->gring[t % kGRing].valid = false;
	return n;
}

// ---------------------------------------------------------------------------
// Measurement protocol (M6b item 0; decision section 1b)
// ---------------------------------------------------------------------------
#ifndef ProcessPowerThrottling
#define PC_SNAP_PROCESS_POWER_THROTTLING 4
#else
#define PC_SNAP_PROCESS_POWER_THROTTLING ProcessPowerThrottling
#endif
struct PcPowerThrottlingState {
	ULONG Version;
	ULONG ControlMask;
	ULONG StateMask;
};

typedef BOOL(WINAPI* SetProcessInformationFn)(HANDLE, int, LPVOID, DWORD);
typedef BOOL(WINAPI* GetSystemCpuSetInformationFn)(PSYSTEM_CPU_SET_INFORMATION, ULONG, PULONG, HANDLE, ULONG);

inline uint64_t ft64(const FILETIME& f) { return (uint64_t(f.dwHighDateTime) << 32) | f.dwLowDateTime; }

void setupMeasure()
{
	HMODULE k32 = GetModuleHandleA("kernel32.dll");
	// 1. no execution-speed throttling (EcoQoS) for this process only
	auto setInfo = reinterpret_cast<SetProcessInformationFn>(reinterpret_cast<void*>(GetProcAddress(k32, "SetProcessInformation")));
	int throttleOff = 0;
	if (setInfo) {
		PcPowerThrottlingState st = { 1, 0x1 /* EXECUTION_SPEED */, 0 };
		throttleOff = setInfo(GetCurrentProcess(), PC_SNAP_PROCESS_POWER_THROTTLING, &st, sizeof(st)) ? 1 : 0;
	}
	// 2. CPU sets: efficiency class per logical processor
	auto getSets = reinterpret_cast<GetSystemCpuSetInformationFn>(reinterpret_cast<void*>(GetProcAddress(k32, "GetSystemCpuSetInformation")));
	if (getSets) {
		ULONG len = 0;
		getSets(nullptr, 0, &len, GetCurrentProcess(), 0);
		uint8_t* buf = static_cast<uint8_t*>(std::malloc(len));
		if (buf && getSets(reinterpret_cast<PSYSTEM_CPU_SET_INFORMATION>(buf), len, &len, GetCurrentProcess(), 0)) {
			S->cpus = static_cast<CpuMap*>(std::calloc(256, sizeof(CpuMap)));
			for (ULONG off = 0; off < len && S->nCpus < 256;) {
				auto* e = reinterpret_cast<SYSTEM_CPU_SET_INFORMATION*>(buf + off);
				if (e->Type == CpuSetInformation) {
					S->cpus[S->nCpus++] = { e->CpuSet.Group, e->CpuSet.LogicalProcessorIndex, e->CpuSet.EfficiencyClass };
					if (e->CpuSet.EfficiencyClass > S->maxClass) S->maxClass = e->CpuSet.EfficiencyClass;
				}
				off += e->Size;
			}
		}
		std::free(buf);
	}
	// 3. optional: pin the main thread to the highest efficiency class (P-cores)
	if (envIs("PIKMIN_NETPLAY_SNAPSHOT_PIN", "pcore") && S->nCpus) {
		DWORD_PTR mask = 0;
		for (int i = 0; i < S->nCpus; ++i) {
			if (S->cpus[i].group == 0 && S->cpus[i].cls == S->maxClass && S->cpus[i].lp < 64) mask |= DWORD_PTR(1) << S->cpus[i].lp;
		}
		if (mask) S->pinned = SetThreadAffinityMask(GetCurrentThread(), mask) != 0;
	}
	// 4. AC / battery, once
	SYSTEM_POWER_STATUS ps = {};
	GetSystemPowerStatus(&ps);
	int pcores = 0, ecores = 0;
	for (int i = 0; i < S->nCpus; ++i) (S->cpus[i].cls == S->maxClass ? pcores : ecores)++;
	std::printf("[snapshot] measure: throttling_off=%d pinned=%d cpus=%d pclass=%d p_lps=%d e_lps=%d ac=%d battery_flag=%d battery_pct=%d\n",
	    throttleOff, S->pinned ? 1 : 0, S->nCpus, S->maxClass, pcores, ecores, int(ps.ACLineStatus), int(ps.BatteryFlag),
	    int(ps.BatteryLifePercent));
	std::fflush(stdout);
}

int classOfCpu(int group, int lp)
{
	for (int i = 0; i < S->nCpus; ++i) {
		if (S->cpus[i].group == group && S->cpus[i].lp == lp) return S->cpus[i].cls;
	}
	return -1;
}

// System busy (all processes) and busy minus this process's own CPU time,
// over the last >= 100 ms window (GetSystemTimes is too coarse per tick).
void sampleLoad()
{
	const int64_t t = now();
	if (S->ldAt && double(t - S->ldAt) * S->msPerCount < 100.0) return;
	FILETIME idle, kernel, user, pc, pe, pk, pu;
	if (!GetSystemTimes(&idle, &kernel, &user)) return;
	if (!GetProcessTimes(GetCurrentProcess(), &pc, &pe, &pk, &pu)) return;
	const uint64_t i = ft64(idle), k = ft64(kernel), u = ft64(user), own = ft64(pk) + ft64(pu);
	if (S->ldAt) {
		const uint64_t di = i - S->ldIdle, tot = (k - S->ldKernel) + (u - S->ldUser), dOwn = own - S->ldOwn;
		if (tot) {
			const double busy = double(tot - di);
			S->busyAll        = 100.0 * busy / double(tot);
			S->busyOther      = 100.0 * (busy > double(dOwn) ? busy - double(dOwn) : 0.0) / double(tot);
		}
	}
	S->ldIdle   = i;
	S->ldKernel = k;
	S->ldUser   = u;
	S->ldOwn    = own;
	S->ldAt     = t;
}

// A fixed amount of integer work timed with QPC: a per-tick reading of the
// main thread's effective speed (core class, clock, contention).
volatile uint64_t gCalibSink = 0;
double calibrate()
{
	const int64_t t0 = now();
	uint64_t x       = 0x9E3779B97F4A7C15ull;
	for (int i = 0; i < 20000; ++i) {
		x ^= x << 13;
		x ^= x >> 7;
		x ^= x << 17;
	}
	gCalibSink = x;
	return msSince(t0) * 1000.0;
}

void sampleCpu()
{
	PROCESSOR_NUMBER pn = {};
	GetCurrentProcessorNumberEx(&pn);
	S->cpuNow = int(pn.Group) * 64 + int(pn.Number);
	S->clsNow = classOfCpu(pn.Group, pn.Number);
}

void openCsv()
{
	const char* path = std::getenv("PIKMIN_NETPLAY_SNAPSHOT_CSV");
	if (!path || !*path) path = "snapshot.csv";
	S->csv = std::fopen(path, "w");
	if (!S->csv) return;
	std::setvbuf(S->csv, nullptr, _IOFBF, 1 << 20);
	std::fprintf(S->csv,
	    "tick,loop,live,resim,phase,pikis,auth_ms,frame_ms,dirty,ww_ms,copy_ms,gcmp_ms,gsave_ms,save_ms,"
	    "gd,barrier,rebase_ms,scan_mb,touched_mb,ring_mb,ring_slots,ring_frames,live_blocks,heap_mb,"
	    "allocs,frees,guard_ms,guard_tail,guard_pre,cpu,eff_class,busy_all,busy_other,calib_us,sync_phase\n");
}

// ---------------------------------------------------------------------------
// Synctest (production path): save -> advance k -> restore -> re-advance k,
// through pc_snapshot_restore, comparing the curated hash per step.
// ---------------------------------------------------------------------------
void syncOpen()
{
	const char* path = std::getenv("PIKMIN_NETPLAY_SNAPSHOT_SYNCTEST_CSV");
	if (!path || !*path) path = "snapshot_synctest.csv";
	S->syncCsv = std::fopen(path, "w");
	if (S->syncCsv) {
		std::fprintf(S->syncCsv,
		    "anchor,k,first_bad,bad_mask,restore_ms,collect_ms,copy_ms,glob_ms,reset_ms,union_pages,zero_pages,"
		    "entries,glob_pages,pending_pages,cpu,eff_class,busy_all,busy_other,calib_us\n");
	}
}

void captureHash(int step)
{
	uint64_t total = 0, subs[7] = {}, tick = 0;
	pc_state_hash_current(&total, subs, &tick);
	S->syncHash[step][0] = total;
	for (int i = 0; i < 7; ++i) S->syncHash[step][i + 1] = subs[i];
}

void syncOnTickEnd(uint64_t tick, bool live)
{
	if (S->syncKArg == 0) return;
	if (S->syncPhase == 0) {
		if (!live || S->barrierThisTick || tick < S->syncStart || tick < S->syncNextAt || tick >= S->syncEnd) return;
		S->syncK = S->syncKArg > 0 ? S->syncKArg : 1 + int(S->syncTestIndex % 7);
		S->syncTestIndex++;
		S->syncPhase  = 1;
		S->syncAnchor = tick;
		S->syncStep   = 0;
		captureHash(0);
		return;
	}
	if (S->syncPhase == 1) {
		S->syncStep++;
		captureHash(S->syncStep);
		if (S->lastBarrier > S->syncAnchor || !live) {
			// the window crosses a barrier (or leaves live play): skip it
			S->syncSkipped++;
			S->syncPhase  = 0;
			S->syncNextAt = tick + S->syncPeriod;
			return;
		}
		if (S->syncStep < S->syncK) return;
		pc_snapshot_restore(S->syncAnchor);
		S->syncPhase    = 2;
		S->syncStep     = 0;
		S->syncFirstBad = 0;
		S->syncBadMask  = 0;
		pc_state_hash_snapshot_suppress_log(true);
		return;
	}
	// resimulating
	S->syncStep++;
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
	if (S->syncStep < S->syncK) return;
	S->syncTests++;
	S->syncTestsK[S->syncK]++;
	if (!S->syncFirstBad) {
		S->syncMatches++;
		S->syncMatchesK[S->syncK]++;
	}
	if (S->syncCsv) {
		std::fprintf(S->syncCsv, "%llu,%d,%d,0x%x,%.4f,%.4f,%.4f,%.4f,%.4f,%u,%u,%u,%u,%u,%d,%d,%.1f,%.1f,%.2f\n",
		    (unsigned long long)S->syncAnchor, S->syncK, S->syncFirstBad, S->syncBadMask, S->rsTotalMs, S->rsCollectMs,
		    S->rsCopyMs, S->rsGlobMs, S->rsResetMs, S->rsUnion, S->rsZero, S->rsEntries, S->rsGlobPages, S->rsPending,
		    S->cpuNow, S->clsNow, S->busyAll, S->busyOther, S->calibUs);
		std::fflush(S->syncCsv);
	}
	pc_state_hash_snapshot_suppress_log(false);
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

void dumpSites(FILE* out, const char* label, Site* table, bool off)
{
	Site* copy = static_cast<Site*>(pcsnap::osAllocZero(sizeof(Site) * kSites));
	if (!copy) return;
	size_t n = 0;
	for (size_t i = 0; i < kSites; ++i)
		if (table[i].ra) copy[n++] = table[i];
	std::qsort(copy, n, sizeof(Site), cmpSiteCount);
	std::fprintf(out, "[snapshot] %s call sites: %zu distinct\n", label, n);
	for (size_t i = 0; i < n && i < 400; ++i) {
		char name[MAX_PATH];
		uintptr_t mod = 0;
		moduleOf(copy[i].ra, name, sizeof(name), &mod);
		std::fprintf(out, "[snapshot] site %s cat=%s module=%s rva=0x%llx count=%llu bytes=%llu\n", label,
		    off ? kOffNames[copy[i].cat % kOffCount] : "region", name, (unsigned long long)(copy[i].ra - mod),
		    (unsigned long long)copy[i].count, (unsigned long long)copy[i].bytes);
	}
	pcsnap::osRelease(copy);
}

void atExitReport()
{
	if (!S) return;
	if (S->csv) std::fflush(S->csv);
	if (S->syncCsv) std::fflush(S->syncCsv);
	const char* path = std::getenv("PIKMIN_NETPLAY_SNAPSHOT_REPORT");
	if (!path || !*path) path = "snapshot_report.txt";
	FILE* out = std::fopen(path, "w");
	if (!out) out = stdout;
	const pcsnap::RegionStats rs = S->region.stats();
	const pcsnap::RingStats gs   = S->ring.stats();
	std::fprintf(out, "[snapshot] exe_base=0x%llx region_base=0x%llx arena_mb=%zu heap_lo_off=0x%llx\n",
	    (unsigned long long)exeBase(), (unsigned long long)reinterpret_cast<uintptr_t>(S->region.base()),
	    S->region.arenaBytes() >> 20, (unsigned long long)(S->region.heapLo() - S->region.base()));
	std::fprintf(out, "[snapshot] region allocs=%llu bytes=%llu frees=%llu bytes=%llu live_blocks=%llu live_bytes=%llu slabs=%llu\n",
	    (unsigned long long)S->totalAllocs, (unsigned long long)S->totalAllocBytes, (unsigned long long)S->totalFrees,
	    (unsigned long long)S->totalFreeBytes, (unsigned long long)rs.liveBlocks, (unsigned long long)rs.liveBytes,
	    (unsigned long long)rs.slabs);
	// Every abort condition below is fatal when it happens; a report that
	// reaches this line has seen none of them.
	std::fprintf(out, "[snapshot] allocator: unknown_frees=%llu bad_frees=0 region_full=0 off_main_region_frees=0 "
	                  "pre_init_allocs=%lld\n",
	    piki_pc_snapshot_unknown_frees(), (long long)sPreInitCount.load());
	std::fprintf(out, "[snapshot] guard=%d: between-tick region writes tail=%llu pages in %llu ticks (not the tick); "
	                  "pre-tick main-loop writes=%llu pages in %llu ticks (first at offset 0x%llx)\n",
	    S->guard ? 1 : 0, (unsigned long long)S->guardTailPages, (unsigned long long)S->guardTailTicks,
	    (unsigned long long)S->guardPrePages, (unsigned long long)S->guardPreTicks,
	    (unsigned long long)(uint64_t(S->guardPreFirst) * kPage));
	std::fprintf(out, "[snapshot] scan high-water %.1f MB, heap used %.1f MB (max %.1f), touched %.1f MB\n",
	    double(S->maxScanned) / 1048576.0, double(rs.heapUsedBytes) / 1048576.0, double(S->maxHeapUsed) / 1048576.0,
	    double(S->region.touchedPages()) * kPage / 1048576.0);
	std::fprintf(out, "[snapshot] ring depth=%d max_slots_live=%llu max_committed_mb=%.1f barriers=%llu last_barrier=%llu frames=%u\n",
	    int(envLong("PIKMIN_NETPLAY_SNAPSHOT_DEPTH", 12)), (unsigned long long)S->maxRingSlots,
	    double(S->maxRingBytes) / 1048576.0, (unsigned long long)S->barriers, (unsigned long long)S->lastBarrier, gs.frames);
	std::fprintf(out, "[snapshot] globals pages=%llu preserve_segs=%u\n", (unsigned long long)S->globPages, S->preserveCount);
	for (int c = 0; c < kOffCount; ++c) {
		std::fprintf(out, "[snapshot] outside-SIM operator new %s count=%lld bytes=%lld\n", kOffNames[c],
		    (long long)S->offCount[c], (long long)S->offBytes[c]);
	}
	if (S->syncKArg != 0) {
		std::fprintf(out, "[snapshot] synctest k=%d tests=%llu matches=%llu skipped_barrier=%llu\n", S->syncKArg,
		    (unsigned long long)S->syncTests, (unsigned long long)S->syncMatches, (unsigned long long)S->syncSkipped);
		for (int k = 1; k <= kMaxK; ++k) {
			if (S->syncTestsK[k]) {
				std::fprintf(out, "[snapshot] synctest k=%d tests=%llu matches=%llu\n", k, (unsigned long long)S->syncTestsK[k],
				    (unsigned long long)S->syncMatchesK[k]);
			}
		}
	}
	pc_snapshot_game_heap_report(out);
	dumpSites(out, "outside_sim", S->offSites, true);
	dumpSites(out, "region", S->regionSites, false);
	if (out != stdout) std::fclose(out);
	std::printf("[snapshot] exit report written (%s); live_blocks=%llu synctests=%llu matches=%llu skipped=%llu\n", path,
	    (unsigned long long)rs.liveBlocks, (unsigned long long)S->syncTests, (unsigned long long)S->syncMatches,
	    (unsigned long long)S->syncSkipped);
	std::fflush(stdout);
}

void flushOnFatal()
{
	if (!S) return;
	if (S->csv) std::fflush(S->csv);
	if (S->syncCsv) std::fflush(S->syncCsv);
}

LONG WINAPI crashFilter(EXCEPTION_POINTERS* info)
{
	if (S && info && info->ExceptionRecord) {
		const EXCEPTION_RECORD* e = info->ExceptionRecord;
		char name[MAX_PATH];
		uintptr_t mod = 0;
		moduleOf(reinterpret_cast<uintptr_t>(e->ExceptionAddress), name, sizeof(name), &mod);
		const unsigned long long data = e->NumberParameters >= 2 ? (unsigned long long)e->ExceptionInformation[1] : 0ull;
		std::printf("[snapshot] CRASH code=0x%08lx module=%s rva=0x%llx data=0x%llx main=%d tick=%llu sync_phase=%d sync_anchor=%llu\n",
		    (unsigned long)e->ExceptionCode, name, (unsigned long long)(reinterpret_cast<uintptr_t>(e->ExceptionAddress) - mod),
		    data, GetCurrentThreadId() == S->mainTid ? 1 : 0, (unsigned long long)pc_state_hash_tick(), S->syncPhase,
		    (unsigned long long)S->syncAnchor);
		const uintptr_t exe = exeBase();
		const uint64_t* sp  = reinterpret_cast<const uint64_t*>(info->ContextRecord->Rsp);
		int shown           = 0;
		std::printf("[snapshot] CRASH stack exe rvas:");
		for (int i = 0; i < 512 && shown < 24; ++i) {
			const uint64_t v = sp[i];
			if (v > exe && v < exe + 0x20000000ull) {
				std::printf(" 0x%llx", (unsigned long long)(v - exe));
				++shown;
			}
		}
		std::printf("\n");
		std::fflush(stdout);
		flushOnFatal();
	}
	return EXCEPTION_CONTINUE_SEARCH;
}

// Ring re-baseline at a barrier: the base becomes a full copy of the touched
// set; the globals log restarts from here.
void rebaseline(uint64_t tick, const char* reason)
{
	const int64_t t0 = now();
	const uint32_t n = S->region.listTouched(S->touchedList);
	if (!S->ring.rebaseline(tick, S->region.base(), S->touchedList, n)) {
		pcsnap::fatal("ring cannot hold the %u-page touched set at tick %llu (raise PIKMIN_NETPLAY_SNAPSHOT_SLOTS)", n,
		    (unsigned long long)tick);
	}
	globalsRebaseline(tick);
	S->rebaseMs          = msSince(t0);
	S->lastBarrier       = tick;
	S->barriers++;
	S->barrierThisTick   = true;
	S->barrierThisReason = reason;
	std::printf("[snapshot] barrier %s tick=%llu rebaseline %u pages (%.1f MB) %.1f ms\n", reason, (unsigned long long)tick, n,
	    double(n) * kPage / 1048576.0, S->rebaseMs);
	std::fflush(stdout);
}

} // namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
void pc_snapshot_init(void)
{
	if (!envIs("PIKMIN_NETPLAY_SNAPSHOT", "1") || S) return;
	State* s = static_cast<State*>(pcsnap::osAllocZero(sizeof(State)));
	if (!s) return;
	s->mainTid = GetCurrentThreadId();
	LARGE_INTEGER f;
	QueryPerformanceFrequency(&f);
	s->msPerCount = 1000.0 / double(f.QuadPart);
	S             = s; // helpers below use S; routing stays off until active

	long arenaMb = envLong("PIKMIN_NETPLAY_SNAPSHOT_ARENA_MB", 32);
	if (arenaMb < 24) arenaMb = 24; // System::Initialise keeps its symbol-map branch at >= 24 MB
	if (!s->region.init(pcsnap::kFixedBase, size_t(arenaMb) << 20, true)) {
		pcsnap::fatal("fixed region base 0x%llx unavailable (%lu); no fallback base", (unsigned long long)pcsnap::kFixedBase,
		    (unsigned long)GetLastError());
	}
	pcsnap::gFatalHook = flushOnFatal;
	const long depth   = envLong("PIKMIN_NETPLAY_SNAPSHOT_DEPTH", 12);
	const long slots   = envLong("PIKMIN_NETPLAY_SNAPSHOT_SLOTS", 131072);
	if (depth < 1 || depth > 30 || !s->ring.init(uint32_t(slots), int(depth), pcsnap::kPages)) {
		pcsnap::fatal("cannot set up the snapshot ring (depth %ld, %ld slots)", depth, slots);
	}
	s->touchedList = static_cast<uint32_t*>(pcsnap::osAllocZero(pcsnap::kPages * sizeof(uint32_t)));
	s->regionSites = static_cast<Site*>(pcsnap::osAllocZero(sizeof(Site) * kSites));
	s->offSites    = static_cast<Site*>(pcsnap::osAllocZero(sizeof(Site) * kSites));
	s->preserveRaw = static_cast<Range*>(std::calloc(kPreserveMax, sizeof(Range)));
	s->guard       = !envIs("PIKMIN_NETPLAY_SNAPSHOT_GUARD", "0");
	s->guardAbort  = !envIs("PIKMIN_NETPLAY_SNAPSHOT_GUARD", "count");
	s->guardFull   = envIs("PIKMIN_NETPLAY_SNAPSHOT_GUARD", "full");
	s->maxClass    = -1;
	s->busyAll = s->busyOther = -1.0;
	s->cpuNow = s->clsNow = -1;

	setupGlobals();
	piki_pc_snapshot_register_preserve();
	pc_os_stubs_snapshot_register_preserve();
	loadPreserveFile();

	s->measure = envIs("PIKMIN_NETPLAY_SNAPSHOT_MEASURE", "1");
	if (s->measure) {
		setupMeasure();
		openCsv();
	}

	const char* k = std::getenv("PIKMIN_NETPLAY_SNAPSHOT_SYNCTEST");
	if (k && *k) {
		if (!std::strcmp(k, "cycle") || !std::strcmp(k, "1-7")) s->syncKArg = -1;
		else s->syncKArg = std::atoi(k);
		if (s->syncKArg > kMaxK || s->syncKArg > int(depth)) s->syncKArg = int(depth < kMaxK ? depth : kMaxK);
		if (s->syncKArg < -1) s->syncKArg = 0;
		s->syncPeriod = int(envLong("PIKMIN_NETPLAY_SNAPSHOT_SYNCTEST_PERIOD", 20));
		if (s->syncPeriod < 1) s->syncPeriod = 1;
		s->syncStart = uint64_t(envLong("PIKMIN_NETPLAY_SNAPSHOT_SYNCTEST_START", 600));
		const char* exitAfter = std::getenv("PIKMIN_NETPLAY_EXIT_AFTER_TICKS");
		const uint64_t endAt  = (exitAfter && *exitAfter) ? std::strtoull(exitAfter, nullptr, 10) : 0;
		s->syncEnd            = endAt > uint64_t(2 * kMaxK + 2) ? endAt - uint64_t(2 * kMaxK + 2) : ~0ull;
		syncOpen();
	}

	std::atexit(atExitReport);
	SetUnhandledExceptionFilter(crashFilter);
	const int hooks = patchImports();
	s->active       = true;
	std::printf("[snapshot] ON: region=%p arena=%ld MB heap_lo=+0x%llx globals=%llu pages depth=%ld slots=%ld guard=%d "
	            "measure=%d synctest_k=%d period=%d preserve=%u free_hooks=%d\n",
	    (void*)s->region.base(), arenaMb, (unsigned long long)(s->region.heapLo() - s->region.base()),
	    (unsigned long long)s->globPages, depth, slots, s->guard ? 1 : 0, s->measure ? 1 : 0, s->syncKArg, s->syncPeriod,
	    s->preserveCount, hooks);
	std::fflush(stdout);
}

bool pc_snapshot_active(void) { return S != nullptr && S->active; }

bool pc_snapshot_owns(const void* ptr) { return S && S->active && S->region.contains(ptr); }

void* pc_snapshot_new(size_t size, size_t align, void* ra)
{
	State* s = S;
	if (!s || !s->active) {
		sPreInitCount.fetch_add(1, std::memory_order_relaxed);
		sPreInitBytes.fetch_add((long long)size, std::memory_order_relaxed);
		return nullptr;
	}
	if (!onMain()) {
		InterlockedIncrement64(&s->offCount[kOffOtherThread]);
		InterlockedAdd64(&s->offBytes[kOffOtherThread], LONG64(size));
		return nullptr;
	}
	if (s->simDepth > 0 && s->infraDepth == 0) {
		void* p          = s->region.alloc(size, align);
		const size_t cap = align > 16 ? size : s->region.capOf(p);
		s->allocs++;
		s->totalAllocs++;
		s->totalAllocBytes += align > 16 ? size : cap;
		noteSite(s->regionSites, reinterpret_cast<uintptr_t>(ra), size, 0);
		return p;
	}
	int cat;
	if (s->infraDepth > 0) cat = kOffInfra;
	else if (!s->loopStarted) cat = kOffBoot;
	else if (s->inIdle) cat = kOffIdleNonSim;
	else cat = kOffOutsideIdle;
	s->offCount[cat]++;
	s->offBytes[cat] += LONG64(size);
	noteSite(s->offSites, reinterpret_cast<uintptr_t>(ra), size, uint32_t(cat));
	return nullptr;
}

bool pc_snapshot_delete(void* ptr, size_t align, void* ra)
{
	State* s = S;
	if (!s || !s->active || !ptr || !s->region.contains(ptr)) return false;
	if (!onMain()) {
		pcsnap::fatal("region free %p from a non-main thread (caller rva 0x%llx) tick=%llu", ptr,
		    (unsigned long long)(reinterpret_cast<uintptr_t>(ra) - exeBase()), (unsigned long long)pc_state_hash_tick());
	}
	const size_t cap = s->region.free(ptr, align);
	s->frees++;
	s->totalFrees++;
	s->totalFreeBytes += cap;
	return true;
}

void pc_snapshot_sim_push(int where)
{
	(void)where;
	if (!S || !S->active || !onMain()) return;
	S->simDepth++;
}

void pc_snapshot_sim_pop(void)
{
	if (!S || !S->active || !onMain()) return;
	if (S->simDepth > 0) S->simDepth--;
}

void pc_snapshot_infra_push(void)
{
	if (!S || !S->active || !onMain()) return;
	S->infraDepth++;
}

void pc_snapshot_infra_pop(void)
{
	if (!S || !S->active || !onMain()) return;
	if (S->infraDepth > 0) S->infraDepth--;
}

bool pc_snapshot_arena(void** lo, size_t* bytes)
{
	if (!S || !S->active) return false;
	*lo    = S->region.arenaLo();
	*bytes = S->region.arenaBytes();
	return true;
}

void pc_snapshot_loop_top(void)
{
	State* s = S;
	if (!s || !s->active || !s->guard || !s->tickDone) return;
	s->tickDone = false;
	const int64_t t0 = now();
	uint32_t first   = 0;
	s->guardTail     = s->region.peekWritten(&first);
	s->guardMs += msSince(t0);
	s->topChecked = true;
	if (s->guardTail) {
		s->guardTailPages += s->guardTail;
		s->guardTailTicks++;
		if (s->guardAbort) {
			pcsnap::fatal("%u region page(s) written between ticks after tick %llu (not by the tick), first at region offset 0x%llx",
			    s->guardTail, (unsigned long long)pc_state_hash_tick(), (unsigned long long)(uint64_t(first) * kPage));
		}
	}
}

void pc_snapshot_frame_begin(void)
{
	State* s = S;
	if (!s || !s->active) return;
	s->loopStarted = true;
	s->tFrame      = now();
	if (s->guard && s->guardFull && s->topChecked) {
		s->topChecked    = false;
		const int64_t t0 = now();
		uint32_t first   = 0;
		const uint32_t n = s->region.peekWritten(&first);
		s->guardMs += msSince(t0);
		s->guardPre = n > s->guardTail ? n - s->guardTail : 0;
		if (s->guardPre) {
			if (!s->guardPrePages) s->guardPreFirst = first;
			s->guardPrePages += s->guardPre;
			s->guardPreTicks++;
		}
	}
}

void pc_snapshot_idle_begin(void)
{
	State* s = S;
	if (!s || !s->active) return;
	s->inIdle   = true;
	s->authOpen = true;
	s->simDepth++;
	s->tIdle = now();
	s->tAuth = 0;
}

void pc_snapshot_auth_end(void)
{
	State* s = S;
	if (!s || !s->active || !s->authOpen) return;
	s->authOpen = false;
	if (s->simDepth > 0) s->simDepth--;
	s->tAuth = now();
}

void pc_snapshot_idle_end(void)
{
	State* s = S;
	if (!s || !s->active) return;
	if (s->authOpen) { // soft-reset idle (or single-pass): the whole idle was SIM
		s->authOpen = false;
		if (s->simDepth > 0) s->simDepth--;
	}
	s->inIdle   = false;
	s->tIdleEnd = now();
}

void pc_snapshot_barrier(const char* reason)
{
	if (!S || !S->active) return;
	S->barrierPending = true;
	S->barrierReason  = reason;
}

void pc_snapshot_preserve(const void* p, size_t bytes)
{
	if (!S) return;
	if (bytes != 0 && S->preserveRawCount < kPreserveMax) {
		uint8_t* lo                            = static_cast<uint8_t*>(const_cast<void*>(p));
		S->preserveRaw[S->preserveRawCount++] = { lo, lo + bytes };
	}
	if (S->preserveBatch || !S->globPagePtr) return;
	buildPreserveSegs();
}

bool pc_snapshot_resimulating(void) { return S && S->active && S->syncPhase == 2; }

bool pc_snapshot_has(uint64_t tick)
{
	return S && S->active && S->started && tick >= S->lastBarrier && S->ring.has(tick)
	    && (tick == S->savedTick || globalsHave(tick + 1, S->savedTick));
}

double pc_snapshot_restore(uint64_t tick)
{
	State* s = S;
	if (!s || !s->active) pcsnap::fatal("restore to tick %llu with the snapshot inactive", (unsigned long long)tick);
	if (tick < s->lastBarrier) {
		std::printf("[snapshot] rollback across barrier: target tick %llu, last barrier %llu\n", (unsigned long long)tick,
		    (unsigned long long)s->lastBarrier);
		std::fflush(stdout);
		pcsnap::fatal("rollback across barrier (target %llu < barrier %llu)", (unsigned long long)tick,
		    (unsigned long long)s->lastBarrier);
	}
	if (!pc_snapshot_has(tick)) {
		pcsnap::fatal("restore target tick %llu not in the ring [%llu, %llu]", (unsigned long long)tick,
		    (unsigned long long)s->ring.oldest(), (unsigned long long)s->ring.newest());
	}
	const int64_t t0 = now();
	// 1. collect what was written since the last save (joins the union)
	s->region.newEpoch();
	s->region.collect(true);
	s->rsPending     = s->region.dirtyCount();
	s->rsCollectMs   = msSince(t0);
	int64_t t        = now();
	const pcsnap::RestoreResult r = s->ring.restore(tick, s->region.base(), s->region.dirty(), s->region.dirtyCount());
	s->rsCopyMs      = msSince(t);
	t                = now();
	s->rsGlobPages   = globalsRestore(tick, s->savedTick);
	s->rsGlobMs      = msSince(t);
	// 2. the restore's own writes are not sim changes (a collect ran first)
	t = now();
	s->region.resetWatch();
	s->region.newEpoch();
	s->rsResetMs = msSince(t);
	s->rsTotalMs = msSince(t0);
	s->rsUnion   = r.unionPages;
	s->rsZero    = r.zeroPages;
	s->rsEntries = r.entries;
	s->savedTick = tick;
	return s->rsTotalMs;
}

void pc_snapshot_tick_end(void)
{
	// Crowd bootstrap: runs with the snapshot on or off (its own switch),
	// before the collect so its writes land in this tick.
	State* s = S;
	if (s && s->active) s->simDepth++;
	pc_snapshot_game_tick(pc_state_hash_tick());
	if (s && s->active && s->simDepth > 0) s->simDepth--;
	if (!s || !s->active) return;
	const int64_t tEnd  = now();
	const double frameMs = double(tEnd - s->tFrame) * s->msPerCount;
	const double authMs  = s->tAuth ? double(s->tAuth - s->tIdle) * s->msPerCount : double(s->tIdleEnd - s->tIdle) * s->msPerCount;
	const uint64_t tick  = pc_state_hash_tick();
	if (s->started && tick <= s->savedTick) {
		// The state-hash tick did not advance (the harness is draining after
		// its exit request): nothing new to save.
		s->region.newEpoch();
		s->tickDone = false;
		return;
	}
	const bool live      = naviMgr != nullptr;
	int phase = 0, pikis = 0;
	pc_snapshot_game_sample(&phase, &pikis);
	pc_snapshot_game_heap_sample();
	if (s->measure) {
		sampleLoad();
		sampleCpu();
		s->calibUs = calibrate();
	}
	s->barrierThisTick   = false;
	s->barrierThisReason = nullptr;
	s->rebaseMs          = 0.0;
	s->copyMs = s->gcmpMs = s->gsaveMs = 0.0;

	// 1. dirty pages since the last save: one GetWriteWatch(RESET) call
	int64_t t0       = now();
	const uint32_t n = (s->region.collect(true), s->region.dirtyCount());
	s->wwMs          = msSince(t0);
	// 2. save: single copy into the ring, or a re-baseline at a barrier
	if (!s->started || s->barrierPending || n > kForcedBarrierPages) {
		const char* why = !s->started ? "start" : s->barrierPending ? s->barrierReason : "large_tick";
		rebaseline(tick, why ? why : "barrier");
		s->barrierPending = false;
		s->started        = true;
	} else {
		t0 = now();
		if (!s->ring.save(tick, s->region.base(), s->region.dirty(), n)) {
			rebaseline(tick, "ring_full");
		} else {
			s->copyMs = msSince(t0);
			globalsSave(tick);
			if (!s->gring[tick % kGRing].valid) rebaseline(tick, "globals_full");
		}
	}
	s->savedTick = tick;
	const double saveMs = s->wwMs + s->copyMs + s->gcmpMs + s->gsaveMs;
	const pcsnap::RegionStats rs = s->region.stats();
	const pcsnap::RingStats gs   = s->ring.stats();
	if (rs.scannedBytes > s->maxScanned) s->maxScanned = rs.scannedBytes;
	if (rs.heapUsedBytes > s->maxHeapUsed) s->maxHeapUsed = rs.heapUsedBytes;
	if (gs.committedBytes > s->maxRingBytes) s->maxRingBytes = gs.committedBytes;
	if (gs.liveSlots > s->maxRingSlots) s->maxRingSlots = gs.liveSlots;

	if (s->csv) {
		const bool touchedNow = tick % 30 == 0;
		std::fprintf(s->csv,
		    "%llu,%u,%d,%d,%d,%d,%.4f,%.4f,%u,%.4f,%.4f,%.4f,%.4f,%.4f,"
		    "%u,%d,%.3f,%.2f,%.2f,%.2f,%llu,%u,%llu,%.2f,"
		    "%llu,%llu,%.4f,%u,%u,%d,%d,%.1f,%.1f,%.2f,%d\n",
		    (unsigned long long)tick, pc_netplay_tick(), live ? 1 : 0, s->syncPhase == 2 ? 1 : 0, phase, pikis, authMs,
		    frameMs, n, s->wwMs, s->copyMs, s->gcmpMs, s->gsaveMs, saveMs, s->globDirtyCount, s->barrierThisTick ? 1 : 0,
		    s->rebaseMs, double(rs.scannedBytes) / 1048576.0,
		    touchedNow ? double(s->region.touchedPages()) * kPage / 1048576.0 : -1.0, double(gs.committedBytes) / 1048576.0,
		    (unsigned long long)gs.liveSlots, gs.frames, (unsigned long long)rs.liveBlocks,
		    double(rs.heapUsedBytes) / 1048576.0, (unsigned long long)s->allocs, (unsigned long long)s->frees, s->guardMs,
		    s->guardTail, s->guardPre, s->cpuNow, s->clsNow, s->busyAll, s->busyOther, s->calibUs, s->syncPhase);
		if (tick % 300 == 0) std::fflush(s->csv);
	}

	// 3. synctest (may restore)
	syncOnTickEnd(tick, live);

	s->region.newEpoch();
	s->allocs = s->frees = 0;
	s->guardMs   = 0.0;
	s->guardTail = s->guardPre = 0;
	s->tickDone  = true;
}
