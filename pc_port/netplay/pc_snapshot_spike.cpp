// Netplay M6a snapshot spike (issue #896). See pc_snapshot_spike.h.
//
// Measurement code, not production rollback: it favours visibility over
// speed and tolerates crashes. Nothing here runs unless the CMake option
// PIKMIN_NETPLAY_SNAPSHOT_SPIKE is ON *and* PIKMIN_NETPLAY_SNAPSHOT_SPIKE=1.
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
//     os_stubs thread/mutex/queue maps);
//   - the System::run loop has started and we are outside app->idle()
//     (input polling, input log, state hash, window events).
// Everything else (other threads, static init before main, the scopes
// above, region exhaustion) stays on malloc and is counted per category and
// per call site (return address of operator new).

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
};

enum OffCategory {
	kOffPreInit = 0,     // before pc_snapshot_spike_init (static init)
	kOffOtherThread,     // not the main thread
	kOffPresent,         // presentation pass scope
	kOffDoneRender,      // doneRender scope
	kOffInfraOther,      // other infra scope (os_stubs maps)
	kOffOutsideIdle,     // main loop, outside app->idle()
	kOffRegionFull,      // region exhausted
	kOffCount
};
const char* const kOffNames[kOffCount] = {
	"pre_init", "other_thread", "present", "done_render", "infra_os_stubs", "outside_idle", "region_full",
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

struct SyncResult {
	uint64_t anchor;
	int k;
	int firstBad;     // 0 = match; else 1-based offset of first mismatching tick
	unsigned badMask; // sub-hash columns that differed at firstBad
	uint32_t regionDiffPages;
	uint32_t globalDiffPages;
	uint32_t onlyFirst;  // pages dirtied in the first pass but not the resim
	uint32_t onlySecond; // pages dirtied in the resim but not the first pass
};

struct State {
	// switches
	bool active;
	bool loopStarted;
	bool inIdle;
	bool authMarked;
	bool doRestore;  // content-identical real restore each tick (default on)
	bool writeWatch; // region reserved with MEM_WRITE_WATCH and measured (default on)
	bool splitAuth;  // extra GetWriteWatch at the end of the auth pass (default off)
	double wwZoneMs[3];
	uint32_t wwCalls;
	void* lastDeleteRa; // main thread: return address of the delete in flight
	uint64_t unknownByKind[4];
	int infraDepth;
	int infraCat;
	DWORD mainTid;
	SRWLOCK lock;
	SRWLOCK siteLock;

	// region
	uint8_t* base;
	uint8_t* shadow;
	uint8_t* scratch; // full-copy target
	Meta* meta;
	uint32_t caps[kNumClasses];
	bool shadowCommitted[kGranules];
	bool scratchCommitted[kGranules];

	// write watch
	void** wwAddrs;
	ULONG_PTR wwCap;
	uint32_t* pageMark; // epoch per region page
	uint32_t* dirtyList;
	uint32_t dirtyCount;
	uint32_t epoch;
	uint64_t* touched; // bitmap: pages ever written and still committed
	uint32_t rdAuth;
	uint32_t rdPost;
	double wwMs;

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
	bool barrier; // set by decommit / oversize ticks; cleared each tick end

	// globals
	Range glob[4];
	int nGlob;
	uint64_t globPages;
	uint64_t globBytes;
	uint8_t* globShadow;
	uint8_t** globPagePtr; // page index -> address
	uint32_t* globDirty;
	uint32_t globDirtyCount;
	PreserveSeg* preserveSegs;
	uint32_t preserveCount;
	uint32_t preserveCap;
	uint8_t* globPreserved; // per global page: 1 if any preserve seg on it
	Range* preserveRaw; // kPreserveMax entries
	int preserveRawCount;
	bool preserveBatch; // loading a preserve file: rebuild the segments once at the end

	// timing
	double msPerCount;
	int64_t tFrame;
	int64_t tIdle;
	int64_t tAuth;
	int64_t tIdleEnd;
	double instrInIdleMs;

	// counters (this tick)
	uint64_t allocs, allocBytes, frees, freeBytes;
	uint64_t offAllocs, offBytes;
	uint64_t regionFreesOffMain;
	uint64_t badFrees;
	// totals
	volatile LONG64 offCount[kOffCount];
	volatile LONG64 offBytesTotal[kOffCount];
	uint64_t totalAllocs, totalAllocBytes, totalFrees, totalFreeBytes;
	uint64_t totalRegionFreesOffMain;
	uint64_t decommitCalls;
	uint64_t smallHwBytes;

	Site* regionSites;
	Site* offSites;
	Site* unknownSites;
	uint64_t siteOverflow;

	FILE* csv;
	FILE* syncCsv;
	uint64_t ticksLogged;

	// synctest
	int syncK;
	int syncPeriod;
	uint64_t syncStart;
	int syncPhase; // 0 idle, 1 first pass, 2 resim
	uint64_t syncAnchor;
	int syncStep;
	uint64_t syncHash[kMaxK + 1][8];
	uint8_t* syncStash;      // first-pass post-state of the union pages
	uint32_t* syncStashIdx;  // region page index (bit31 = globals page)
	uint32_t syncStashCount;
	uint32_t syncStashCap;
	uint32_t* syncMark;      // per region page: 1 first pass, 2 resim, 3 both
	uint8_t* syncGMark;
	uint64_t syncTests, syncMatches, syncAborted;
	int syncFirstBad;
	unsigned syncBadMask;
	uint64_t syncNextAt;
	double syncRestoreMs;
	uint32_t syncRestorePages;
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

inline bool onMain() { return GetCurrentThreadId() == S->mainTid; }

inline size_t alignUp(size_t v, size_t a) { return (v + a - 1) & ~(a - 1); }

void* osReserve(size_t bytes)
{
	return VirtualAlloc(nullptr, bytes, MEM_RESERVE, PAGE_READWRITE);
}

bool osCommit(void* p, size_t bytes)
{
	return VirtualAlloc(p, bytes, MEM_COMMIT, PAGE_READWRITE) != nullptr;
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

void clearTouched(size_t page0, size_t pages)
{
	for (size_t p = page0; p < page0 + pages; ++p) S->touched[p >> 6] &= ~(1ull << (p & 63));
}

// Pages that are decommitted read as zero once recommitted and are not
// reported by write watch, so the shadow is zeroed to match and rollback
// across this tick is refused (barrier).
void regionDecommit(uint8_t* p, size_t bytes)
{
	if (bytes == 0) return;
	VirtualFree(p, bytes, MEM_DECOMMIT);
	S->decommitCalls++;
	const size_t off = size_t(p - S->base);
	clearTouched(off / kPage, bytes / kPage);
	for (size_t o = off; o < off + bytes;) {
		size_t g    = o / kGranule;
		size_t gEnd = (g + 1) * kGranule;
		size_t e    = gEnd < off + bytes ? gEnd : off + bytes;
		if (S->shadowCommitted[g]) std::memset(S->shadow + o, 0, e - o);
		o = e;
	}
	S->barrier = true;
}

void largeUnlink(LargeFree* f)
{
	if (f->prev) f->prev->next = f->next;
	else S->meta->largeHead = f->next;
	if (f->next) f->next->prev = f->prev;
}

void* largeAlloc(size_t n)
{
	Meta* m           = S->meta;
	const size_t need = alignUp(sizeof(BlockHdr) + n, kPage);
	LargeFree* best   = nullptr;
	for (LargeFree* f = m->largeHead; f; f = f->next) {
		if (f->h.cap >= need && (!best || f->h.cap < best->h.cap)) best = f;
	}
	uint8_t* blk = nullptr;
	if (best) {
		blk          = reinterpret_cast<uint8_t*>(best);
		size_t span  = best->h.cap;
		LargeFree* p = best->prev;
		LargeFree* x = best->next;
		largeUnlink(best);
		if (span - need >= kLargeSplitMin) {
			uint8_t* rem = blk + need;
			osCommit(rem, kPage);
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
		if (span > kPage) osCommit(blk + kPage, span - kPage);
		std::memset(blk + sizeof(BlockHdr), 0, kPage - sizeof(BlockHdr));
		BlockHdr* h = reinterpret_cast<BlockHdr*>(blk);
		h->magic    = kMagicLive;
		h->cls      = kLargeCls;
		h->cap      = span - sizeof(BlockHdr);
	} else {
		blk = m->largeWild;
		if (blk + need > S->base + kLargeEnd) return nullptr;
		if (!osCommit(blk, need)) return nullptr;
		m->largeWild += need;
		BlockHdr* h = reinterpret_cast<BlockHdr*>(blk);
		h->magic    = kMagicLive;
		h->cls      = kLargeCls;
		h->cap      = need - sizeof(BlockHdr);
	}
	BlockHdr* h = reinterpret_cast<BlockHdr*>(blk);
	m->liveBytes += h->cap;
	m->liveBlocks++;
	return h + 1;
}

void largeFree(BlockHdr* h)
{
	Meta* m      = S->meta;
	uint8_t* blk = reinterpret_cast<uint8_t*>(h);
	size_t span  = h->cap + sizeof(BlockHdr);
	m->liveBytes -= h->cap;
	m->liveBlocks--;
	if (span > kPage) regionDecommit(blk + kPage, span - kPage);
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
		regionDecommit(reinterpret_cast<uint8_t*>(next), kPage);
	}
	// coalesce backward
	if (prev && reinterpret_cast<uint8_t*>(prev) + prev->h.cap == blk) {
		prev->h.cap += f->h.cap;
		largeUnlink(f);
		regionDecommit(blk, kPage);
		f = prev;
	}
	// give the tail back to the wilderness
	if (reinterpret_cast<uint8_t*>(f) + f->h.cap == m->largeWild) {
		largeUnlink(f);
		regionDecommit(reinterpret_cast<uint8_t*>(f), kPage);
		m->largeWild = reinterpret_cast<uint8_t*>(f);
	}
}

void* regionAlloc(size_t n)
{
	if (n == 0) n = 1;
	if (n > kSmallMax) return largeAlloc(n);
	Meta* m          = S->meta;
	const uint32_t c = classOf(n);
	const size_t cap = S->caps[c];
	void* p          = m->smallFree[c];
	if (p) {
		m->smallFree[c] = *static_cast<void**>(p);
		BlockHdr* h     = static_cast<BlockHdr*>(p) - 1;
		h->magic        = kMagicLive;
		std::memset(p, 0, cap);
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
	}
	m->liveBytes += cap;
	m->liveBlocks++;
	return p;
}

// Returns freed payload bytes, or 0 for a bad pointer.
size_t regionFree(void* p)
{
	BlockHdr* h = static_cast<BlockHdr*>(p) - 1;
	if (h->magic != kMagicLive) {
		S->badFrees++;
		return 0;
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
	S->globShadow  = static_cast<uint8_t*>(VirtualAlloc(nullptr, S->globBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
	S->globPagePtr = static_cast<uint8_t**>(std::calloc(S->globPages, sizeof(uint8_t*)));
	S->globDirty   = static_cast<uint32_t*>(std::calloc(S->globPages, sizeof(uint32_t)));
	S->globPreserved = static_cast<uint8_t*>(std::calloc(S->globPages, 1));
	S->syncGMark   = static_cast<uint8_t*>(std::calloc(S->globPages, 1));
	uint64_t gp = 0;
	for (int i = 0; i < S->nGlob; ++i) {
		for (uint8_t* p = S->glob[i].lo; p < S->glob[i].hi; p += kPage) S->globPagePtr[gp++] = p;
	}
	// Undo pool for globals: enough for every page on every ring tick.
	S->gUndoPages = S->globPages * 4;
	S->gUndo      = static_cast<uint8_t*>(VirtualAlloc(nullptr, S->gUndoPages * kPage, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
	S->gUndoIdx   = static_cast<uint32_t*>(std::calloc(S->gUndoPages, sizeof(uint32_t)));
	// Initial shadow: the globals as they are right now.
	for (uint64_t i = 0; i < S->globPages; ++i) std::memcpy(S->globShadow + i * kPage, S->globPagePtr[i], kPage);
}

// Map a global byte range to per-page preserve segments.
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
			S->globPreserved[i] = 1;
		}
	}
}

// Restore one global page from src, keeping preserved bytes as they are now.
void restoreGlobalPage(uint64_t gi, const uint8_t* src)
{
	uint8_t* dst = S->globPagePtr[gi];
	if (!S->globPreserved[gi]) {
		std::memcpy(dst, src, kPage);
		return;
	}
	uint8_t keep[kPage];
	std::memcpy(keep, dst, kPage);
	std::memcpy(dst, src, kPage);
	for (uint32_t s = 0; s < S->preserveCount; ++s) {
		const PreserveSeg& seg = S->preserveSegs[s];
		if (seg.page == gi) std::memcpy(dst + seg.off, keep + seg.off, seg.len);
	}
}

// ---------------------------------------------------------------------------
// Write watch
// ---------------------------------------------------------------------------
// Write watch is queried per used zone (meta + arena, the committed small
// zone, the large zone below its wilderness) rather than over the whole
// 2 GB reservation; the per-zone cost is logged so the scan cost can be
// related to the committed size.
struct Zone {
	uint8_t* lo;
	uint8_t* hi;
};

int usedZones(Zone* z)
{
	z[0] = { S->base, S->base + kArenaOff + kArenaSize };
	z[1] = { S->base + kSmallOff, S->meta->smallCommitted };
	z[2] = { S->base + kLargeOff, S->meta->largeWild };
	return 3;
}

uint32_t collectWriteWatch()
{
	if (!S->writeWatch) return 0;
	Zone z[3];
	const int n    = usedZones(z);
	uint32_t total = 0;
	for (int k = 0; k < n; ++k) {
		if (z[k].hi <= z[k].lo) continue;
		const int64_t t0 = now();
		ULONG_PTR count  = S->wwCap;
		DWORD gran       = 0;
		const UINT rc    = GetWriteWatch(WRITE_WATCH_FLAG_RESET, z[k].lo, size_t(z[k].hi - z[k].lo), S->wwAddrs, &count, &gran);
		S->wwZoneMs[k] += msSince(t0);
		S->wwCalls++;
		if (rc != 0) {
			std::fprintf(stderr, "[m6a] GetWriteWatch failed (%lu)\n", GetLastError());
			continue;
		}
		for (ULONG_PTR i = 0; i < count; ++i) {
			const size_t idx = size_t(static_cast<uint8_t*>(S->wwAddrs[i]) - S->base) / kPage;
			if (idx >= kPages) continue;
			S->touched[idx >> 6] |= 1ull << (idx & 63);
			if (S->pageMark[idx] != S->epoch) {
				S->pageMark[idx]              = S->epoch;
				S->dirtyList[S->dirtyCount++] = uint32_t(idx);
			}
		}
		total += uint32_t(count);
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
	    "ww_z0_ms,ww_z1_ms,ww_z2_ms,ww_calls,committed_mb\n");
}

double touchedMb()
{
	uint64_t n = 0;
	for (size_t w = 0; w < kPages / 64; ++w) n += __builtin_popcountll(S->touched[w]);
	return double(n) * kPage / (1024.0 * 1024.0);
}

// Copy every touched run of the region into the scratch reservation.
double fullCopy(double* mbOut)
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
		while (e < kPages && (S->touched[e >> 6] & (1ull << (e & 63)))) ++e;
		const size_t off   = p * kPage;
		const size_t bytes = (e - p) * kPage;
		ensureGranules(S->scratch, S->scratchCommitted, kGranules, off, bytes);
		std::memcpy(S->scratch + off, S->base + off, bytes);
		pages += e - p;
		p = e;
	}
	*mbOut = double(pages) * kPage / (1024.0 * 1024.0);
	return msSince(t0);
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
		std::fprintf(S->syncCsv, "anchor,k,first_bad,bad_mask,region_diff_pages,global_diff_pages,only_first,only_second,restore_ms,restored_pages\n");
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
}

void stashAdd(uint32_t tag, const uint8_t* src)
{
	if (S->syncStashCount == S->syncStashCap) return;
	std::memcpy(S->syncStash + size_t(S->syncStashCount) * kPage, src, kPage);
	S->syncStashIdx[S->syncStashCount++] = tag;
}

// Roll region and globals back from the end of tick `to` to the end of the
// anchor tick by replaying undo entries newest first.
void syncRollback(uint64_t from, uint64_t anchor, double* ms, uint32_t* pages)
{
	const int64_t t0 = now();
	uint32_t n       = 0;
	for (uint64_t t = from; t > anchor; --t) {
		const UndoTick& u = S->ring[t % kUndoRing];
		for (uint32_t i = 0; i < u.rCount; ++i) {
			const uint64_t slot = (u.rStart + i) % kUndoPoolPages;
			const uint32_t idx  = S->rUndoIdx[slot];
			const uint8_t* src  = S->rUndo + slot * kPage;
			std::memcpy(S->base + size_t(idx) * kPage, src, kPage);
			std::memcpy(S->shadow + size_t(idx) * kPage, src, kPage);
			++n;
		}
		for (uint32_t i = 0; i < u.gCount; ++i) {
			const uint64_t slot = (u.gStart + i) % S->gUndoPages;
			const uint32_t gi   = S->gUndoIdx[slot];
			const uint8_t* src  = S->gUndo + slot * kPage;
			restoreGlobalPage(gi, src);
			std::memcpy(S->globShadow + size_t(gi) * kPage, src, kPage);
			++n;
		}
	}
	// The restore's own writes are not sim changes.
	resetWriteWatch();
	*ms    = msSince(t0);
	*pages = n;
}

// First byte of global page gi that differs from `was`, ignoring preserved
// bytes (they are never restored, so they legitimately differ); -1 if none.
int firstGlobalDiff(uint64_t gi, const uint8_t* was)
{
	const uint8_t* now = S->globPagePtr[gi];
	if (!S->globPreserved[gi]) {
		if (std::memcmp(was, now, kPage) == 0) return -1;
		int at = 0;
		while (at < int(kPage) && was[at] == now[at]) ++at;
		return at;
	}
	uint8_t mask[kPage];
	std::memset(mask, 0, sizeof(mask));
	for (uint32_t k = 0; k < S->preserveCount; ++k) {
		const PreserveSeg& seg = S->preserveSegs[k];
		if (seg.page == gi) std::memset(mask + seg.off, 1, seg.len);
	}
	for (int at = 0; at < int(kPage); ++at) {
		if (!mask[at] && was[at] != now[at]) return at;
	}
	return -1;
}

void syncOnTickEnd(uint64_t tick, bool live, bool undoOk)
{
	if (S->syncK <= 0) return;
	if (S->syncPhase == 0) {
		if (!live || !undoOk || tick < S->syncStart || tick < S->syncNextAt) return;
		S->syncPhase  = 1;
		S->syncAnchor = tick;
		S->syncStep   = 0;
		syncCaptureHash(0);
		for (size_t w = 0; w < kPages; ++w) S->syncMark[w] = 0;
		std::memset(S->syncGMark, 0, S->globPages);
		return;
	}
	if (S->syncPhase == 1) {
		S->syncStep++;
		syncCaptureHash(S->syncStep);
		if (!undoOk || !live) {
			S->syncAborted++;
			S->syncPhase  = 0;
			S->syncNextAt = tick + S->syncPeriod;
			return;
		}
		if (S->syncStep < S->syncK) return;
		// First pass done: stash the post-state of every page it dirtied,
		// then roll back to the anchor.
		S->syncStashCount = 0;
		for (uint64_t t = S->syncAnchor + 1; t <= tick; ++t) {
			const UndoTick& u = S->ring[t % kUndoRing];
			for (uint32_t i = 0; i < u.rCount; ++i) {
				const uint32_t idx = S->rUndoIdx[(u.rStart + i) % kUndoPoolPages];
				if (!(S->syncMark[idx] & 1)) {
					S->syncMark[idx] |= 1;
					stashAdd(idx, S->base + size_t(idx) * kPage);
				}
			}
			for (uint32_t i = 0; i < u.gCount; ++i) {
				const uint32_t gi = S->gUndoIdx[(u.gStart + i) % S->gUndoPages];
				if (!(S->syncGMark[gi] & 1)) {
					S->syncGMark[gi] |= 1;
					stashAdd(gi | 0x80000000u, S->globPagePtr[gi]);
				}
			}
		}
		double ms       = 0;
		uint32_t pages  = 0;
		for (uint64_t t = S->syncAnchor + 1; t <= tick; ++t) {
			if (!ringHas(t)) {
				S->syncAborted++;
				S->syncPhase  = 0;
				S->syncNextAt = tick + S->syncPeriod;
				return;
			}
		}
		syncRollback(tick, S->syncAnchor, &ms, &pages);
		S->syncRestoreMs    = ms;
		S->syncRestorePages = pages;
		S->syncPhase        = 2;
		S->syncStep         = 0;
		S->syncFirstBad     = 0;
		S->syncBadMask      = 0;
		pc_state_hash_spike_suppress_log(true);
		return;
	}
	// syncPhase == 2: resimulating
	S->syncStep++;
	{
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
	}
	if (S->syncStep < S->syncK) return;
	// Resim done: compare the pages the first pass dirtied against its stash.
	SyncResult r = {};
	r.anchor     = S->syncAnchor;
	r.k          = S->syncK;
	r.firstBad   = S->syncFirstBad;
	r.badMask    = S->syncBadMask;
	for (uint64_t t = S->syncAnchor + 1; t <= tick; ++t) {
		const UndoTick& u = S->ring[t % kUndoRing];
		if (!u.valid || u.tick != t) continue;
		for (uint32_t i = 0; i < u.rCount; ++i) {
			const uint32_t idx = S->rUndoIdx[(u.rStart + i) % kUndoPoolPages];
			S->syncMark[idx] |= 2;
		}
		for (uint32_t i = 0; i < u.gCount; ++i) {
			const uint32_t gi = S->gUndoIdx[(u.gStart + i) % S->gUndoPages];
			S->syncGMark[gi] |= 2;
		}
	}
	for (uint32_t s = 0; s < S->syncStashCount; ++s) {
		const uint32_t tag = S->syncStashIdx[s];
		const uint8_t* was = S->syncStash + size_t(s) * kPage;
		if (tag & 0x80000000u) {
			const uint32_t gi = tag & 0x7fffffffu;
			const int at      = firstGlobalDiff(gi, was);
			if (at >= 0) {
				if (r.globalDiffPages < 16 && S->syncCsv) {
					std::fprintf(S->syncCsv, "#gdiff anchor=%llu rva=0x%llx\n", (unsigned long long)r.anchor,
					    (unsigned long long)(uintptr_t(S->globPagePtr[gi]) + at - uintptr_t(GetModuleHandleA(nullptr))));
				}
				r.globalDiffPages++;
			}
		} else {
			const uint8_t* now = S->base + size_t(tag) * kPage;
			if (std::memcmp(was, now, kPage) != 0) {
				int at = 0;
				while (at < int(kPage) && was[at] == now[at]) ++at;
				if (r.regionDiffPages < 16 && S->syncCsv) {
					std::fprintf(S->syncCsv, "#rdiff anchor=%llu off=0x%llx\n", (unsigned long long)r.anchor,
					    (unsigned long long)(size_t(tag) * kPage + at));
				}
				r.regionDiffPages++;
			}
		}
	}
	for (size_t w = 0; w < kPages; ++w) {
		if (S->syncMark[w] == 1) r.onlyFirst++;
		else if (S->syncMark[w] == 2) r.onlySecond++;
	}
	for (uint64_t g = 0; g < S->globPages; ++g) {
		if (S->syncGMark[g] == 1) r.onlyFirst++;
		else if (S->syncGMark[g] == 2) r.onlySecond++;
	}
	S->syncTests++;
	if (!r.firstBad) S->syncMatches++;
	if (S->syncCsv) {
		std::fprintf(S->syncCsv, "%llu,%d,%d,0x%x,%u,%u,%u,%u,%.3f,%llu\n", (unsigned long long)r.anchor, r.k,
		    r.firstBad, r.badMask, r.regionDiffPages, r.globalDiffPages, r.onlyFirst, r.onlySecond,
		    S->syncRestoreMs, (unsigned long long)S->syncRestorePages);
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
	Site* copy = static_cast<Site*>(std::malloc(sizeof(Site) * kSites));
	if (!copy) return;
	size_t n = 0;
	for (size_t i = 0; i < kSites; ++i)
		if (table[i].ra) copy[n++] = table[i];
	std::qsort(copy, n, sizeof(Site), cmpSiteCount);
	std::fprintf(out, "[m6a] %s call sites: %zu distinct\n", label, n);
	for (size_t i = 0; i < n && i < 400; ++i) {
		HMODULE mod = nullptr;
		char name[MAX_PATH] = "?";
		GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
		    reinterpret_cast<LPCSTR>(copy[i].ra), &mod);
		if (mod) {
			GetModuleFileNameA(mod, name, sizeof(name));
			const char* slash = std::strrchr(name, '\\');
			if (slash) std::memmove(name, slash + 1, std::strlen(slash + 1) + 1);
		}
		std::fprintf(out, "[m6a] site %s cat=%s module=%s rva=0x%llx count=%llu bytes=%llu\n", label,
		    table == S->offSites ? kOffNames[copy[i].cat]
		    : table == S->unknownSites ? kUnknownNames[copy[i].cat & 3] : "region", name,
		    (unsigned long long)(copy[i].ra - reinterpret_cast<uintptr_t>(mod)),
		    (unsigned long long)copy[i].count, (unsigned long long)copy[i].bytes);
	}
	std::free(copy);
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
	std::fprintf(out, "[m6a] exe_base=0x%llx region_base=0x%llx\n",
	    (unsigned long long)reinterpret_cast<uintptr_t>(GetModuleHandleA(nullptr)),
	    (unsigned long long)reinterpret_cast<uintptr_t>(S->base));
	std::fprintf(out, "[m6a] region allocs=%llu bytes=%llu frees=%llu bytes=%llu live_blocks=%llu live_bytes=%llu frees_off_main=%llu bad_frees=%llu decommits=%llu\n",
	    (unsigned long long)S->totalAllocs, (unsigned long long)S->totalAllocBytes,
	    (unsigned long long)S->totalFrees, (unsigned long long)S->totalFreeBytes,
	    (unsigned long long)S->meta->liveBlocks, (unsigned long long)S->meta->liveBytes,
	    (unsigned long long)S->totalRegionFreesOffMain, (unsigned long long)S->badFrees,
	    (unsigned long long)S->decommitCalls);
	std::fprintf(out, "[m6a] off-region pre_init(count only, before the spike state) count=%lld bytes=%lld\n",
	    sPreInitCount, sPreInitBytes);
	for (int c = 0; c < kOffCount; ++c) {
		std::fprintf(out, "[m6a] off-region %s count=%lld bytes=%lld\n", kOffNames[c],
		    (long long)S->offCount[c], (long long)S->offBytesTotal[c]);
	}
	std::fprintf(out, "[m6a] unknown_frees=%llu site_overflow=%llu globals_pages=%llu preserve_segs=%u\n",
	    piki_pc_spike_unknown_frees(),
	    (unsigned long long)S->siteOverflow, (unsigned long long)S->globPages, S->preserveCount);
	if (S->syncK > 0) {
		std::fprintf(out, "[m6a] synctest k=%d tests=%llu matches=%llu aborted=%llu\n", S->syncK,
		    (unsigned long long)S->syncTests, (unsigned long long)S->syncMatches,
		    (unsigned long long)S->syncAborted);
	}
	std::fprintf(out, "[m6a] unknown frees by kind: arena_zone=%llu image=%llu private_heap=%llu other=%llu\n",
	    (unsigned long long)S->unknownByKind[0], (unsigned long long)S->unknownByKind[1],
	    (unsigned long long)S->unknownByKind[2], (unsigned long long)S->unknownByKind[3]);
	dumpSites(out, "off", S->offSites);
	dumpSites(out, "unknown", S->unknownSites);
	dumpSites(out, "region", S->regionSites);
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
		HMODULE mod = nullptr;
		char name[MAX_PATH] = "?";
		GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
		    static_cast<LPCSTR>(e->ExceptionAddress), &mod);
		if (mod) GetModuleFileNameA(mod, name, sizeof(name));
		const unsigned long long data = e->NumberParameters >= 2 ? (unsigned long long)e->ExceptionInformation[1] : 0ull;
		std::printf("[m6a] CRASH code=0x%08lx module=%s rva=0x%llx data=0x%llx tid_main=%d tick=%llu sync_phase=%d sync_anchor=%llu sync_step=%d\n",
		    (unsigned long)e->ExceptionCode, name,
		    (unsigned long long)(reinterpret_cast<uintptr_t>(e->ExceptionAddress) - reinterpret_cast<uintptr_t>(mod)), data,
		    GetCurrentThreadId() == S->mainTid ? 1 : 0, (unsigned long long)pc_state_hash_tick(), S->syncPhase,
		    (unsigned long long)S->syncAnchor, S->syncStep);
		// Crude stack scan: every qword near RSP that points into the exe
		// image, as an rva (the first ones are the most recent callers).
		const uintptr_t exe = reinterpret_cast<uintptr_t>(GetModuleHandleA(nullptr));
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
	const uintptr_t exe = reinterpret_cast<uintptr_t>(GetModuleHandleA(nullptr));
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

} // namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
void pc_snapshot_spike_init(void)
{
	const char* on = std::getenv("PIKMIN_NETPLAY_SNAPSHOT_SPIKE");
	if (!on || on[0] != '1' || on[1] != '\0' || S) return;

	State* s = static_cast<State*>(VirtualAlloc(nullptr, sizeof(State), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
	if (!s) return;
	InitializeSRWLock(&s->lock);
	InitializeSRWLock(&s->siteLock);
	s->mainTid = GetCurrentThreadId();
	LARGE_INTEGER f;
	QueryPerformanceFrequency(&f);
	s->msPerCount = 1000.0 / double(f.QuadPart);

	const char* wwEnv = std::getenv("PIKMIN_NETPLAY_SNAPSHOT_SPIKE_WW");
	s->writeWatch = !(wwEnv && wwEnv[0] == '0');
	const char* splitEnv = std::getenv("PIKMIN_NETPLAY_SNAPSHOT_SPIKE_SPLIT");
	s->splitAuth = splitEnv && splitEnv[0] == '1';
	uint8_t* base = nullptr;
	for (uintptr_t want : kPreferredBases) {
		base = static_cast<uint8_t*>(VirtualAlloc(reinterpret_cast<void*>(want), kReserve,
		    wwEnv && wwEnv[0] == '0' ? MEM_RESERVE : (MEM_RESERVE | MEM_WRITE_WATCH), PAGE_READWRITE));
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
	Meta* m          = reinterpret_cast<Meta*>(base + kMetaOff);
	m->magic         = 0x4D36414D45544131ull;
	m->smallWild     = base + kSmallOff;
	m->smallCommitted = base + kSmallOff;
	m->largeWild     = base + kLargeOff;
	s->meta          = m;

	s->shadow   = static_cast<uint8_t*>(osReserve(kReserve));
	s->scratch  = static_cast<uint8_t*>(osReserve(kReserve));
	s->wwCap    = kPages;
	s->wwAddrs  = static_cast<void**>(VirtualAlloc(nullptr, kPages * sizeof(void*), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
	s->pageMark = static_cast<uint32_t*>(VirtualAlloc(nullptr, kPages * sizeof(uint32_t), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
	s->dirtyList = static_cast<uint32_t*>(VirtualAlloc(nullptr, kPages * sizeof(uint32_t), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
	s->touched  = static_cast<uint64_t*>(VirtualAlloc(nullptr, kPages / 8, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
	s->rUndo    = static_cast<uint8_t*>(osReserve(kUndoPoolPages * kPage));
	s->rUndoIdx = static_cast<uint32_t*>(VirtualAlloc(nullptr, kUndoPoolPages * sizeof(uint32_t), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
	s->syncMark = static_cast<uint32_t*>(VirtualAlloc(nullptr, kPages * sizeof(uint32_t), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
	s->regionSites = static_cast<Site*>(VirtualAlloc(nullptr, sizeof(Site) * kSites, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
	s->offSites    = static_cast<Site*>(VirtualAlloc(nullptr, sizeof(Site) * kSites, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
	s->unknownSites = static_cast<Site*>(VirtualAlloc(nullptr, sizeof(Site) * kSites, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
	s->epoch       = 1;
	s->preserveRaw = static_cast<Range*>(std::calloc(kPreserveMax, sizeof(Range)));
	const char* restore = std::getenv("PIKMIN_NETPLAY_SNAPSHOT_SPIKE_RESTORE");
	s->doRestore   = !(restore && restore[0] == '0');

	setupGlobals();
	piki_pc_spike_register_preserve();
	pc_os_stubs_spike_register_preserve();
	loadPreserveFile();

	const char* k = std::getenv("PIKMIN_NETPLAY_SNAPSHOT_SYNCTEST");
	if (k && *k) {
		s->syncK = std::atoi(k);
		if (s->syncK < 0) s->syncK = 0;
		if (s->syncK > kMaxK) s->syncK = kMaxK;
		const char* period = std::getenv("PIKMIN_NETPLAY_SNAPSHOT_SYNCTEST_PERIOD");
		s->syncPeriod      = (period && *period) ? std::atoi(period) : 30;
		if (s->syncPeriod < 1) s->syncPeriod = 1;
		const char* start = std::getenv("PIKMIN_NETPLAY_SNAPSHOT_SYNCTEST_START");
		s->syncStart      = (start && *start) ? std::strtoull(start, nullptr, 10) : 600;
		s->syncStashCap   = 16384;
		s->syncStash      = static_cast<uint8_t*>(VirtualAlloc(nullptr, size_t(s->syncStashCap) * kPage, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
		s->syncStashIdx   = static_cast<uint32_t*>(std::calloc(s->syncStashCap, sizeof(uint32_t)));
		if (!s->syncStash) s->syncStashCap = 0;
		syncLogOpen();
	}

	openCsv();
	std::atexit(atExitReport);
	SetUnhandledExceptionFilter(crashFilter);
	s->active = true;
	std::printf("[m6a] snapshot spike ON: region=%p reserve=%zu MB globals=%llu pages (%llu KB) restore=%d synctest_k=%d period=%d preserve=%u\n",
	    (void*)base, kReserve >> 20, (unsigned long long)s->globPages, (unsigned long long)(s->globBytes >> 10),
	    s->doRestore ? 1 : 0, s->syncK, s->syncPeriod, s->preserveCount);
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
		void* p = regionAlloc(size);
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
				    (unsigned long long)(reinterpret_cast<uintptr_t>(ra) - reinterpret_cast<uintptr_t>(GetModuleHandleA(nullptr))),
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

bool pc_snapshot_spike_delete(void* ptr, void* ra)
{
	State* s = S;
	if (!s || !ptr) return false;
	uint8_t* p = static_cast<uint8_t*>(ptr);
	if (p < s->base + kSmallOff || p >= s->base + kReserve) {
		if (onMain()) s->lastDeleteRa = ra;
		return false;
	}
	AcquireSRWLockExclusive(&s->lock);
	const size_t cap = regionFree(ptr);
	s->frees++;
	s->freeBytes += cap;
	s->totalFrees++;
	s->totalFreeBytes += cap;
	if (!onMain()) {
		s->regionFreesOffMain++;
		s->totalRegionFreesOffMain++;
	}
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
	S->inIdle        = true;
	S->authMarked    = false;
	S->instrInIdleMs = 0.0;
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
	if (S->globPreserved) std::memset(S->globPreserved, 0, S->globPages);
	buildPreserveSegs();
}

bool pc_snapshot_spike_resimulating(void) { return S && S->active && S->syncPhase == 2; }

void pc_snapshot_spike_tick_end(void)
{
	State* s = S;
	if (!s || !s->active) return;
	const int64_t tEnd  = now();
	const double frameMs = double(tEnd - s->tFrame) * s->msPerCount;
	const double idleMs  = double(s->tIdleEnd - s->tIdle) * s->msPerCount - s->instrInIdleMs;
	const double authMs  = s->authMarked ? double(s->tAuth - s->tIdle) * s->msPerCount : idleMs;
	const uint64_t tick  = pc_state_hash_tick();
	const bool live      = naviMgr != nullptr;

	// 1. dirty pages since the auth-end collection (or since the last tick)
	int64_t t0 = now();
	s->rdPost  = collectWriteWatch();
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
	UndoTick& u    = s->ring[tick % kUndoRing];
	u.tick         = tick;
	u.valid        = false;
	const bool fits = s->dirtyCount <= kUndoPoolPages / 2 && !s->barrier;
	u.rStart       = s->rUndoCursor;
	u.rCount       = 0;
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

	// 3. restore (content-identical): shadow -> region for the same pages
	double restoreMs = 0.0, restoreWwMs = 0.0;
	if (s->doRestore) {
		t0 = now();
		for (uint32_t i = 0; i < s->dirtyCount; ++i) {
			const size_t off = size_t(s->dirtyList[i]) * kPage;
			std::memcpy(s->base + off, s->shadow + off, kPage);
		}
		restoreMs = msSince(t0);
		t0        = now();
		resetWriteWatch();
		restoreWwMs = msSince(t0);
	} else {
		t0 = now();
		for (uint32_t i = 0; i < s->dirtyCount; ++i) {
			const size_t off = size_t(s->dirtyList[i]) * kPage;
			ensureGranules(s->scratch, s->scratchCommitted, kGranules, off, kPage);
			std::memcpy(s->scratch + off, s->shadow + off, kPage);
		}
		restoreMs = msSince(t0);
	}

	// 4. globals: compare against the shadow; changed pages are saved
	// (skipped with the write-watch measurement off: alloc-only A/B mode)
	t0 = now();
	s->globDirtyCount = 0;
	for (uint64_t i = 0; s->writeWatch && i < s->globPages; ++i) {
		if (std::memcmp(s->globPagePtr[i], s->globShadow + i * kPage, kPage) != 0) {
			s->globDirty[s->globDirtyCount++] = uint32_t(i);
		}
	}
	const double gcmpMs = msSince(t0);
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

	// 5. periodic full copies
	double fullMs = -1.0, fullMb = -1.0, gfullMs = -1.0;
	if (s->writeWatch && tick % kFullEvery == 0) {
		fullMs = fullCopy(&fullMb);
		t0     = now();
		ensureGranules(s->scratch, s->scratchCommitted, kGranules, 0, s->globBytes);
		for (uint64_t i = 0; i < s->globPages; ++i) std::memcpy(s->scratch + i * kPage, s->globPagePtr[i], kPage);
		gfullMs = msSince(t0);
	}

	const unsigned long long unknownFrees = piki_pc_spike_unknown_frees();
	if (s->csv) {
		std::fprintf(s->csv,
		    "%llu,%u,%d,%d,%.4f,%.4f,%.4f,"
		    "%u,%u,%u,%u,%u,%u,%u,%u,"
		    "%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.2f,%.4f,"
		    "%llu,%llu,%llu,%llu,%llu,%.3f,%.3f,%.3f,%.2f,"
		    "%llu,%llu,%llu,%llu,%d,%d,"
		    "%.4f,%.4f,%.4f,%u,%.2f\n",
		    (unsigned long long)tick, pc_netplay_tick(), live ? 1 : 0, s->syncPhase == 2 ? 1 : 0, authMs, idleMs,
		    frameMs, s->rdAuth, s->rdPost, s->dirtyCount, rdMeta, rdArena, rdSmall, rdLarge, s->globDirtyCount,
		    s->wwMs, saveMs, restoreMs, restoreWwMs, gcmpMs, gsaveMs, fullMs, fullMb, gfullMs,
		    (unsigned long long)s->allocs, (unsigned long long)s->allocBytes, (unsigned long long)s->frees,
		    (unsigned long long)s->freeBytes, (unsigned long long)s->meta->liveBlocks,
		    double(s->meta->liveBytes) / (1024.0 * 1024.0),
		    double(s->meta->smallWild - (s->base + kSmallOff)) / (1024.0 * 1024.0),
		    double(s->meta->largeWild - (s->base + kLargeOff)) / (1024.0 * 1024.0),
		    (tick % kFullEvery == 0) ? touchedMb() : -1.0, (unsigned long long)s->offAllocs,
		    (unsigned long long)s->offBytes, (unsigned long long)s->regionFreesOffMain, unknownFrees,
		    u.valid ? 1 : 0, s->barrier ? 1 : 0, s->wwZoneMs[0], s->wwZoneMs[1], s->wwZoneMs[2], s->wwCalls,
		    double((s->meta->smallCommitted - (s->base + kSmallOff)) + (s->meta->largeWild - (s->base + kLargeOff))
		        + kArenaSize + kMetaCommit) / (1024.0 * 1024.0));
		if (tick % 300 == 0) std::fflush(s->csv);
	}
	s->ticksLogged++;

	// 6. synctest state machine (may roll region + globals back)
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
	s->wwCalls  = 0;
	s->allocs = s->allocBytes = s->frees = s->freeBytes = 0;
	s->offAllocs = s->offBytes = 0;
	s->regionFreesOffMain = 0;
	s->barrier = false;
	s->authMarked = false;
}

#else // !_WIN32: the spike is Windows-only (write watch); inert elsewhere.

void pc_snapshot_spike_init(void) {}
bool pc_snapshot_spike_active(void) { return false; }
void* pc_snapshot_spike_new(size_t, void*) { return nullptr; }
bool pc_snapshot_spike_delete(void*) { return false; }
bool pc_snapshot_spike_arena(void**, size_t*) { return false; }
void pc_snapshot_spike_frame_begin(void) {}
void pc_snapshot_spike_idle_begin(void) {}
void pc_snapshot_spike_auth_end(void) {}
void pc_snapshot_spike_idle_end(void) {}
void pc_snapshot_spike_tick_end(void) {}
void pc_snapshot_spike_infra_push(int) {}
void pc_snapshot_spike_infra_pop(void) {}
void pc_snapshot_spike_preserve(const void*, size_t) {}
bool pc_snapshot_spike_resimulating(void) { return false; }

#endif
