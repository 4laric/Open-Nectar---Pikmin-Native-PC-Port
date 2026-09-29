// Netplay M6b production snapshot region (issue #896). See pc_snapshot_region.h.
//
// Promoted from the M6a spike's allocator core (pc_snapshot_spike.cpp):
// size classes plus address-ordered best-fit with coalescing, metadata in the
// region, zero-filled blocks, freed large spans kept committed. Changes for
// production (review round 2, C2-5/6/7/9):
//   * one fixed base, no fallback bases; region full and oversized requests
//     abort instead of falling back to malloc;
//   * one contiguous range: the small size classes live in 256 KB slabs carved
//     from the same wilderness as the large blocks, so meta + arena + heap is
//     scanned by a single GetWriteWatch call up to the commit high-water mark;
//   * aligned requests (align_val_t) stay in the region;
//   * no locks: only the main thread allocates and frees region blocks (the
//     hooks abort on a region free from any other thread).
//
// This TU never calls operator new: it runs inside it.

#include "netplay/pc_snapshot_region.h"

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
#error "the netplay snapshot region is Windows-only (MEM_WRITE_WATCH); CMake adds this TU only under WIN32"
#endif

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace pcsnap {

namespace {

constexpr uint32_t kMagicLive    = 0x534E504Cu; // "SNPL"
constexpr uint32_t kMagicFreeS   = 0x534E5053u; // "SNPS"
constexpr uint32_t kMagicFreeL   = 0x534E5046u; // "SNPF"
constexpr uint32_t kMagicAligned = 0x534E5041u; // "SNPA"
constexpr uint32_t kLargeCls     = 0xFFFFu;
constexpr uint32_t kSlabCls      = 0xFFFEu;
constexpr uint64_t kMetaMagic    = 0x4D36424D45544131ull;

inline size_t alignUp(size_t v, size_t a) { return (v + a - 1) & ~(a - 1); }
inline uint8_t* alignUpPtr(uint8_t* p, size_t a)
{
	return reinterpret_cast<uint8_t*>(alignUp(reinterpret_cast<uintptr_t>(p), a));
}

// Aligned blocks (align > 16): a back header just below the returned pointer
// names the raw block.
struct AlignedHdr {
	uint32_t magic;
	uint32_t align;
	uint64_t rawOffset; // returned pointer - raw payload pointer
};
static_assert(sizeof(AlignedHdr) == 16, "aligned back header is 16 bytes");

} // namespace

void (*gFatalHook)(void) = nullptr;

void fatal(const char* fmt, ...)
{
	char msg[1024];
	va_list ap;
	va_start(ap, fmt);
	std::vsnprintf(msg, sizeof(msg), fmt, ap);
	va_end(ap);
	std::printf("[snapshot] FATAL %s\n", msg);
	std::fflush(stdout);
	std::fprintf(stderr, "[snapshot] FATAL %s\n", msg);
	std::fflush(stderr);
	if (gFatalHook) gFatalHook();
	std::abort();
}

void* osReserve(size_t bytes) { return VirtualAlloc(nullptr, bytes, MEM_RESERVE, PAGE_READWRITE); }
bool osCommit(void* p, size_t bytes) { return VirtualAlloc(p, bytes, MEM_COMMIT, PAGE_READWRITE) != nullptr; }
void* osAllocZero(size_t bytes) { return VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE); }
void osRelease(void* p)
{
	if (p) VirtualFree(p, 0, MEM_RELEASE);
}

struct Region::BlockHdr {
	uint32_t magic;
	uint32_t cls;
	uint64_t cap; // payload bytes (small, live large); whole span (free large)
};
static_assert(sizeof(Region::BlockHdr) == 16, "block header must keep 16-byte payload alignment");

struct Region::LargeFree {
	BlockHdr h;
	LargeFree* next;
	LargeFree* prev;
};

// Allocator state, at the region base: snapshotted and restored together
// with the blocks it describes.
struct Region::Meta {
	uint64_t magic;
	uint8_t* wild;      // heap zone wilderness
	uint8_t* commitHw;  // logical commit high-water mark (above it: zero pages)
	LargeFree* largeHead; // address ordered
	void* smallFree[kNumClasses];
	uint8_t* slabCur;
	uint8_t* slabEnd;
	uint64_t liveBytes;
	uint64_t liveBlocks;
	uint64_t slabs;
};

bool Region::init(uintptr_t base, size_t arenaBytes, bool writeWatch)
{
	if (mBase) return true;
	arenaBytes = alignUp(arenaBytes, kPage);
	if (kArenaOff + arenaBytes + kGranule > kReserve) fatal("arena of %zu MB does not fit the region", arenaBytes >> 20);
	uint8_t* b = static_cast<uint8_t*>(VirtualAlloc(reinterpret_cast<void*>(base), kReserve,
	    writeWatch ? (MEM_RESERVE | MEM_WRITE_WATCH) : MEM_RESERVE, PAGE_READWRITE));
	if (!b) return false;
	if (reinterpret_cast<uintptr_t>(b) != base) {
		VirtualFree(b, 0, MEM_RELEASE);
		return false;
	}
	mWwAddrs  = static_cast<void**>(osAllocZero(kPages * sizeof(void*)));
	mTouched  = static_cast<uint64_t*>(osAllocZero(kPages / 8));
	mPageMark = static_cast<uint32_t*>(osAllocZero(kPages * sizeof(uint32_t)));
	mDirty    = static_cast<uint32_t*>(osAllocZero(kPages * sizeof(uint32_t)));
	if (!mWwAddrs || !mTouched || !mPageMark || !mDirty) fatal("cannot allocate write-watch tables");
	mBase       = b;
	mArenaBytes = arenaBytes;
	mWriteWatch = writeWatch;
	mHeapLo     = alignUpPtr(b + kArenaOff + arenaBytes, kGranule);
	mPhysHw     = mHeapLo;
	mEpoch      = 1;
	mDirtyCount = 0;
	if (!osCommit(b, kMetaCommit)) fatal("cannot commit region meta");
	if (arenaBytes && !osCommit(b + kArenaOff, arenaBytes)) fatal("cannot commit the %zu MB arena", arenaBytes >> 20);
	buildClasses();
	Meta* m     = meta();
	m->magic    = kMetaMagic;
	m->wild     = mHeapLo;
	m->commitHw = mHeapLo;
	return true;
}

void Region::shutdown()
{
	if (!mBase) return;
	VirtualFree(mBase, 0, MEM_RELEASE);
	osRelease(mWwAddrs);
	osRelease(mTouched);
	osRelease(mPageMark);
	osRelease(mDirty);
	mBase = nullptr;
	mWwAddrs = nullptr;
	mTouched = nullptr;
	mPageMark = nullptr;
	mDirty = nullptr;
}

void Region::buildClasses()
{
	int n = 0;
	for (uint32_t c = 16; c <= 256; c += 16) mCaps[n++] = c;
	for (uint32_t b = 256; b < kSmallMax; b *= 2) {
		for (uint32_t q = 1; q <= 4; ++q) mCaps[n++] = b + b * q / 4;
	}
	if (n != kNumClasses) fatal("class table size %d != %d", n, kNumClasses);
}

uint32_t Region::classOf(size_t n) const
{
	if (n <= 256) return uint32_t((n + 15) / 16) - 1;
	uint32_t lo = 16, hi = kNumClasses - 1;
	while (lo < hi) {
		const uint32_t mid = (lo + hi) / 2;
		if (mCaps[mid] >= n) hi = mid;
		else lo = mid + 1;
	}
	return lo;
}

// Commit the heap zone up to `end` (granule aligned) and advance the
// physical high-water mark. Pages stay committed for the process lifetime.
void Region::ensureCommitted(uint8_t* end)
{
	if (end <= mPhysHw) return;
	uint8_t* target = alignUpPtr(end, kGranule);
	if (target > mBase + kReserve) target = mBase + kReserve;
	if (!osCommit(mPhysHw, size_t(target - mPhysHw))) {
		fatal("commit of %zu KB at region offset 0x%llx failed (%lu)", size_t(target - mPhysHw) >> 10,
		    (unsigned long long)(mPhysHw - mBase), (unsigned long)GetLastError());
	}
	mPhysHw = target;
}

// Zero [p, p + bytes) without writing (or reading) pages that were never
// written: such a page is still the zero page it was committed as. On
// Windows a first read of a demand-zero page in a MEM_WRITE_WATCH range is
// reported as a write, so untouched pages are not even read (M6a fix1).
void Region::zeroSpan(uint8_t* p, size_t bytes)
{
	uint8_t* end = p + bytes;
	uint8_t* lo  = reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(p) & ~uintptr_t(kPage - 1));
	ULONG_PTR count = 0;
	bool known      = false;
	if (mWriteWatch && end > lo) {
		count      = kPages;
		DWORD gran = 0;
		known      = GetWriteWatch(0, lo, size_t(end - lo), mWwAddrs, &count, &gran) == 0;
	}
	ULONG_PTR k = 0;
	while (p < end) {
		uint8_t* pageLo = reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(p) & ~uintptr_t(kPage - 1));
		uint8_t* pageHi = pageLo + kPage < end ? pageLo + kPage : end;
		if (p != pageLo || pageHi != pageLo + kPage) {
			std::memset(p, 0, size_t(pageHi - p));
		} else {
			bool written = !known || touched(size_t(pageLo - mBase) / kPage);
			if (!written) {
				while (k < count && static_cast<uint8_t*>(mWwAddrs[k]) < pageLo) ++k;
				written = k < count && static_cast<uint8_t*>(mWwAddrs[k]) == pageLo;
			}
			if (written) std::memset(pageLo, 0, kPage);
		}
		p = pageHi;
	}
}

void Region::largeUnlink(LargeFree* f)
{
	if (f->prev) f->prev->next = f->next;
	else meta()->largeHead = f->next;
	if (f->next) f->next->prev = f->prev;
}

void* Region::largeAlloc(size_t n, uint32_t cls)
{
	Meta* m           = meta();
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
			LargeFree* r = reinterpret_cast<LargeFree*>(blk + need);
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
		blk = m->wild;
		if (need > size_t((mBase + kReserve) - blk)) {
			fatal("region full: %zu KB requested, wilderness at offset 0x%llx of 0x%llx", need >> 10,
			    (unsigned long long)(blk - mBase), (unsigned long long)kReserve);
		}
		span = need;
		ensureCommitted(blk + need);
		// Below the logical commit high-water mark the pages may hold a
		// freed block's bytes (a tail given back to the wilderness, or state
		// a rollback rewound past): zero them. Above it they are zero.
		uint8_t* dirtyEnd = m->commitHw < blk + need ? m->commitHw : blk + need;
		if (dirtyEnd > blk) zeroSpan(blk, size_t(dirtyEnd - blk));
		if (blk + need > m->commitHw) m->commitHw = blk + need;
		m->wild = blk + need;
	}
	BlockHdr* h = reinterpret_cast<BlockHdr*>(blk);
	h->magic    = kMagicLive;
	h->cls      = cls;
	h->cap      = span - sizeof(BlockHdr);
	if (cls == kLargeCls) {
		m->liveBytes += h->cap;
		m->liveBlocks++;
	}
	return h + 1;
}

void Region::largeFree(BlockHdr* h)
{
	Meta* m      = meta();
	uint8_t* blk = reinterpret_cast<uint8_t*>(h);
	const size_t span = h->cap + sizeof(BlockHdr);
	m->liveBytes -= h->cap;
	m->liveBlocks--;
	LargeFree* f = reinterpret_cast<LargeFree*>(blk);
	f->h.magic   = kMagicFreeL;
	f->h.cls     = kLargeCls;
	f->h.cap     = span;
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
	if (next && blk + f->h.cap == reinterpret_cast<uint8_t*>(next)) {
		f->h.cap += next->h.cap;
		largeUnlink(next);
	}
	if (prev && reinterpret_cast<uint8_t*>(prev) + prev->h.cap == blk) {
		prev->h.cap += f->h.cap;
		largeUnlink(f);
		f = prev;
	}
	// give the tail back to the wilderness (it stays committed)
	if (reinterpret_cast<uint8_t*>(f) + f->h.cap == m->wild) {
		largeUnlink(f);
		m->wild = reinterpret_cast<uint8_t*>(f);
	}
}

void* Region::smallAlloc(size_t n)
{
	Meta* m          = meta();
	const uint32_t c = classOf(n);
	const size_t cap = mCaps[c];
	void* p          = m->smallFree[c];
	if (p) {
		m->smallFree[c] = *static_cast<void**>(p);
		BlockHdr* h     = static_cast<BlockHdr*>(p) - 1;
		if (h->magic != kMagicFreeS || h->cls != c) {
			fatal("small free list of class %u corrupt at %p (magic 0x%08x cls %u)", c, p, h->magic, h->cls);
		}
		h->magic = kMagicLive;
		std::memset(p, 0, cap);
	} else {
		const size_t need = sizeof(BlockHdr) + cap;
		if (!m->slabCur || size_t(m->slabEnd - m->slabCur) < need) {
			// New slab. The old slab's tail is abandoned (deterministic).
			uint8_t* s = static_cast<uint8_t*>(largeAlloc(kSlabBytes - sizeof(BlockHdr), kSlabCls));
			m->slabCur = s;
			m->slabEnd = s + (kSlabBytes - sizeof(BlockHdr));
			m->slabs++;
		}
		BlockHdr* h = reinterpret_cast<BlockHdr*>(m->slabCur);
		h->magic    = kMagicLive;
		h->cls      = c;
		h->cap      = cap;
		m->slabCur += need;
		p = h + 1; // slab memory is zero (fresh or zeroed by largeAlloc)
	}
	m->liveBytes += cap;
	m->liveBlocks++;
	return p;
}

void* Region::alloc(size_t n, size_t align)
{
	if (n > kMaxRequest) fatal("oversized region request: %zu bytes (limit %zu)", n, kMaxRequest);
	if (align <= 16) {
		if (n == 0) n = 1;
		return n > kSmallMax ? largeAlloc(n, kLargeCls) : smallAlloc(n);
	}
	if (align > kPage || (align & (align - 1)) != 0) fatal("unsupported region alignment %zu", align);
	uint8_t* raw = static_cast<uint8_t*>(alloc(n + align + sizeof(AlignedHdr), 16));
	uint8_t* p   = alignUpPtr(raw + sizeof(AlignedHdr), align);
	AlignedHdr* a = reinterpret_cast<AlignedHdr*>(p) - 1;
	a->magic     = kMagicAligned;
	a->align     = uint32_t(align);
	a->rawOffset = uint64_t(p - raw);
	return p;
}

size_t Region::free(void* ptr, size_t align)
{
	uint8_t* p = static_cast<uint8_t*>(ptr);
	if (align > 16) {
		AlignedHdr* a = reinterpret_cast<AlignedHdr*>(p) - 1;
		if (p < mHeapLo + 2 * sizeof(BlockHdr) || p >= meta()->wild || a->magic != kMagicAligned || a->align != align) {
			fatal("bad aligned region free %p (align %zu)", ptr, align);
		}
		p -= a->rawOffset;
		a->magic = 0;
	}
	if (p < mHeapLo + sizeof(BlockHdr) || p >= meta()->wild) fatal("region free of %p outside the heap zone", ptr);
	BlockHdr* h = reinterpret_cast<BlockHdr*>(p) - 1;
	if (h->magic != kMagicLive) fatal("bad region free %p (magic 0x%08x)", ptr, h->magic);
	const size_t cap = h->cap;
	if (h->cls == kLargeCls) {
		largeFree(h);
		return cap;
	}
	if (h->cls >= uint32_t(kNumClasses)) fatal("region free of %p with class 0x%x", ptr, h->cls);
	Meta* m                 = meta();
	h->magic                = kMagicFreeS;
	*static_cast<void**>(static_cast<void*>(p)) = m->smallFree[h->cls];
	m->smallFree[h->cls]    = p;
	m->liveBytes -= cap;
	m->liveBlocks--;
	return cap;
}

size_t Region::capOf(const void* ptr) const
{
	const BlockHdr* h = static_cast<const BlockHdr*>(ptr) - 1;
	return h->magic == kMagicLive ? h->cap : 0;
}

RegionStats Region::stats() const
{
	RegionStats s = {};
	if (!mBase) return s;
	const Meta* m    = meta();
	s.liveBlocks     = m->liveBlocks;
	s.liveBytes      = m->liveBytes;
	s.slabs          = m->slabs;
	s.heapUsedBytes  = uint64_t(m->wild - mHeapLo);
	s.scannedBytes   = uint64_t(mPhysHw - mBase);
	return s;
}

uint32_t Region::collect(bool reset)
{
	if (!mWriteWatch) return 0;
	ULONG_PTR count = kPages;
	DWORD gran      = 0;
	const UINT rc   = GetWriteWatch(reset ? WRITE_WATCH_FLAG_RESET : 0, mBase, size_t(mPhysHw - mBase), mWwAddrs, &count, &gran);
	if (rc != 0) fatal("GetWriteWatch failed (%lu)", (unsigned long)GetLastError());
	for (ULONG_PTR i = 0; i < count; ++i) {
		const size_t idx = size_t(static_cast<uint8_t*>(mWwAddrs[i]) - mBase) / kPage;
		markTouched(idx);
		if (mPageMark[idx] != mEpoch) {
			mPageMark[idx]         = mEpoch;
			mDirty[mDirtyCount++] = uint32_t(idx);
		}
	}
	return uint32_t(count);
}

void Region::resetWatch()
{
	if (mWriteWatch) ResetWriteWatch(mBase, size_t(mPhysHw - mBase));
}

uint32_t Region::peekWritten(uint32_t* first)
{
	*first = 0;
	if (!mWriteWatch) return 0;
	ULONG_PTR count = kPages;
	DWORD gran      = 0;
	if (GetWriteWatch(0, mBase, size_t(mPhysHw - mBase), mWwAddrs, &count, &gran) != 0) return 0;
	if (count) *first = uint32_t(size_t(static_cast<uint8_t*>(mWwAddrs[0]) - mBase) / kPage);
	return uint32_t(count);
}

void Region::newEpoch()
{
	mDirtyCount = 0;
	if (++mEpoch == 0) {
		std::memset(mPageMark, 0, kPages * sizeof(uint32_t));
		mEpoch = 1;
	}
}

uint64_t Region::touchedPages() const
{
	uint64_t n        = 0;
	const size_t words = (size_t(mPhysHw - mBase) / kPage + 63) / 64;
	for (size_t w = 0; w < words; ++w) n += uint64_t(__builtin_popcountll(mTouched[w]));
	return n;
}

uint32_t Region::listTouched(uint32_t* out) const
{
	uint32_t n         = 0;
	const size_t pages = size_t(mPhysHw - mBase) / kPage;
	for (size_t w = 0; w * 64 < pages; ++w) {
		uint64_t bits = mTouched[w];
		while (bits) {
			const int b = __builtin_ctzll(bits);
			bits &= bits - 1;
			const size_t idx = w * 64 + size_t(b);
			if (idx < pages) out[n++] = uint32_t(idx);
		}
	}
	return n;
}

} // namespace pcsnap
