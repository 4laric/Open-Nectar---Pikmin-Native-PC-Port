#pragma once

// Netplay M6b production snapshot region (issue #896): the write-watched sim
// region and its deterministic allocator.
//
// Compiled only with the CMake option PIKMIN_NETPLAY_SNAPSHOT=ON (Windows,
// not Android). Engine-free: the host test links this TU on its own.
//
// Layout (one contiguous range, scanned by one GetWriteWatch call per tick
// from the region base up to the physical commit high-water mark):
//
//   [0, 64 KB)                meta: allocator state (snapshotted with the
//                             blocks it describes)
//   [1 MB, 1 MB + arena)      simulated OS arena (sys heap); the ovl and app
//                             AyuHeap backings are carved from it (console
//                             nesting), so the three heaps share one range
//   [heapLo, wilderness)      heap zone: address-ordered best-fit large
//                             blocks and 256 KB slabs of size-classed small
//                             blocks, grown on demand and never decommitted
//
// Every failure the M6a spike tolerated is fatal here: an unavailable fixed
// base, region full, an oversized request, a free of a pointer inside the
// region that is not a live block.

#include <cstddef>
#include <cstdint>

namespace pcsnap {

constexpr size_t kPage          = 4096;
constexpr uintptr_t kFixedBase  = 0x00000E0000000000ull;
constexpr size_t kReserve       = size_t(2) << 30; // VA reserved for the region
constexpr size_t kPages         = kReserve / kPage;
constexpr size_t kMetaCommit    = size_t(64) << 10;
constexpr size_t kArenaOff      = size_t(1) << 20;
constexpr size_t kGranule       = size_t(1) << 20;
constexpr size_t kSmallMax      = 32768;
constexpr size_t kSlabBytes     = size_t(256) << 10;
constexpr size_t kLargeSplitMin = size_t(64) << 10;
constexpr size_t kMaxRequest    = size_t(1) << 30; // larger requests abort
constexpr int kNumClasses       = 44;

// Fatal error: prints "[snapshot] FATAL <msg>" and aborts. Never returns.
// gFatalHook (optional) runs first, e.g. to flush the per-tick logs.
[[noreturn]] void fatal(const char* fmt, ...);
extern void (*gFatalHook)(void);

struct RegionStats {
	uint64_t liveBlocks;
	uint64_t liveBytes;
	uint64_t slabs;
	uint64_t heapUsedBytes;   // wilderness - heapLo
	uint64_t scannedBytes;    // [base, physical commit high-water)
};

// The region and its allocator. One instance per process (the game), or
// several in sequence (the host test, via shutdown()).
class Region {
public:
	// Reserves kReserve bytes at `base` (MEM_WRITE_WATCH when writeWatch),
	// commits meta and the arena zone. Returns false when the base is not
	// available (the caller aborts: there are no fallback bases).
	bool init(uintptr_t base, size_t arenaBytes, bool writeWatch);
	void shutdown();

	bool ready() const { return mBase != nullptr; }
	bool contains(const void* p) const
	{
		return static_cast<const uint8_t*>(p) >= mBase && static_cast<const uint8_t*>(p) < mBase + kReserve;
	}
	uint8_t* base() const { return mBase; }
	uint8_t* arenaLo() const { return mBase + kArenaOff; }
	size_t arenaBytes() const { return mArenaBytes; }
	uint8_t* heapLo() const { return mHeapLo; }
	// End of the scanned range: the physical commit high-water mark (never
	// moves down, not even after a rollback rewinds the meta).
	uint8_t* scanEnd() const { return mPhysHw; }

	// 16-byte aligned, zero-filled. align > 16 is supported up to kPage.
	// Aborts on region full or n > kMaxRequest.
	void* alloc(size_t n, size_t align = 16);
	// Frees a live block. Aborts when p is not a live block of this region.
	// Returns the freed payload bytes.
	size_t free(void* p, size_t align = 16);
	// Payload capacity of a live block (for statistics).
	size_t capOf(const void* p) const;

	RegionStats stats() const;

	// Write watch over [base, scanEnd). collect() appends every page reported
	// written since the last reset to the dirty list (once per epoch), marks
	// it touched, and resets the watch when reset is true.
	uint32_t collect(bool reset);
	void resetWatch();
	// Reported pages without resetting and without recording them (the
	// between-tick guard). Returns the count; the first page index in *first.
	uint32_t peekWritten(uint32_t* first);

	const uint32_t* dirty() const { return mDirty; }
	uint32_t dirtyCount() const { return mDirtyCount; }
	void newEpoch();
	bool touched(size_t page) const { return (mTouched[page >> 6] >> (page & 63)) & 1; }
	uint64_t touchedPages() const;
	// Appends every touched page index below scanEnd to out (ascending).
	uint32_t listTouched(uint32_t* out) const;

	// Layouts live in the .cpp.
	struct Meta;
	struct BlockHdr;
	struct LargeFree;

private:

	Meta* meta() const { return reinterpret_cast<Meta*>(mBase); }
	void buildClasses();
	uint32_t classOf(size_t n) const;
	void ensureCommitted(uint8_t* end);
	void zeroSpan(uint8_t* p, size_t bytes);
	void* largeAlloc(size_t n, uint32_t cls);
	void largeFree(BlockHdr* h);
	void largeUnlink(LargeFree* f);
	void* smallAlloc(size_t n);
	void markTouched(size_t page)
	{
		mTouched[page >> 6] |= 1ull << (page & 63);
	}

	uint8_t* mBase      = nullptr;
	size_t mArenaBytes  = 0;
	uint8_t* mHeapLo    = nullptr;
	uint8_t* mPhysHw    = nullptr;
	bool mWriteWatch    = false;
	uint32_t mCaps[kNumClasses] = {};
	// write watch (all outside the region)
	void** mWwAddrs       = nullptr;
	uint64_t* mTouched    = nullptr;
	uint32_t* mPageMark   = nullptr;
	uint32_t* mDirty      = nullptr;
	uint32_t mDirtyCount  = 0;
	uint32_t mEpoch       = 1;
};

// OS helpers shared by the snapshot TUs (VirtualAlloc'd, outside the region
// and outside the globals bracket).
void* osReserve(size_t bytes);
bool osCommit(void* p, size_t bytes);
void* osAllocZero(size_t bytes);
void osRelease(void* p);

} // namespace pcsnap
