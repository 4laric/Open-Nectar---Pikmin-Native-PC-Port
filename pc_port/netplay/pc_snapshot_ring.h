#pragma once

// Netplay M6b snapshot ring (issue #896): single-copy, log-structured page
// history of the snapshot region (decision M6A_DECISION.md section 2, option C).
//
//   * save: the post-image of every page dirtied during a frame is copied
//     once into a slot; slots of one page form a version chain (newest first);
//   * window: the last `depth` frames keep their versions; when a frame ages
//     out of the window its versions become the base of their pages and every
//     older version of those pages is released (no copy);
//   * restore to frame T: for each page in the union of the pages dirtied
//     after T (plus the pages written since the last save), the newest version
//     at or before T is copied back, or the page is zeroed when it has none
//     (it was never written before T: it was still a zero page). The frames
//     after T are dropped;
//   * barrier: every version is dropped and the base becomes a full copy of the
//     touched set (re-baseline).
//
// Engine-free and OS-light (slot storage comes from pc_snapshot_region's OS
// helpers), so the host test drives it directly.

#include <cstddef>
#include <cstdint>

namespace pcsnap {

struct RingStats {
	uint64_t liveSlots;       // slots holding a version (bases + window)
	uint64_t committedBytes;  // slot storage committed (high-water, never shrinks)
	uint32_t frames;          // frames in the window
	uint64_t oldest;          // oldest restorable frame
	uint64_t newest;          // newest saved frame
	uint64_t baseline;        // frame of the last re-baseline
	bool valid;               // false until the first re-baseline
};

struct RestoreResult {
	uint32_t unionPages; // pages restored (each once)
	uint32_t zeroPages;  // of those, zeroed (no version at or before T)
	uint32_t entries;    // slot entries visited in the dropped frames
};

class PageRing {
public:
	static constexpr uint32_t kNone = 0xFFFFFFFFu;

	// maxSlots pages of storage are reserved and committed lazily; depth is
	// the number of frames kept after the base (restorable targets are the
	// base frame and the frames in the window); regionPages bounds the page
	// indexes.
	bool init(uint32_t maxSlots, int depth, size_t regionPages);
	void shutdown();

	// Post-images of `pages` at the end of `frame` (frame > newest). Returns
	// false, saving nothing, when the slot pool cannot hold them: the caller
	// then re-baselines (a forced barrier).
	bool save(uint64_t frame, const uint8_t* region, const uint32_t* pages, uint32_t n);
	// Drop every version; the base becomes a copy of `pages` at `frame`.
	// Returns false when even the base does not fit the pool.
	bool rebaseline(uint64_t frame, const uint8_t* region, const uint32_t* pages, uint32_t n);

	bool has(uint64_t frame) const { return mValid && frame >= mOldest && frame <= mNewest; }
	uint64_t oldest() const { return mOldest; }
	uint64_t newest() const { return mNewest; }
	uint64_t baseline() const { return mBaseline; }

	// Restore the region to the end of frame T (has(T) must hold). `extra`
	// lists pages written since the last save (they join the union).
	RestoreResult restore(uint64_t T, uint8_t* region, const uint32_t* extra, uint32_t nExtra);

	// The newest version of `page` at or before `frame`; nullptr when the
	// page has none (a zero page then). For tests and audits.
	const uint8_t* versionAt(uint32_t page, uint64_t frame) const;

	RingStats stats() const;

private:
	struct FrameRec {
		uint64_t frame;
		uint64_t start; // absolute entry cursor
		uint32_t count;
	};

	uint32_t allocSlot();
	void freeSlot(uint32_t s);
	void ageOldest();
	FrameRec& frameAt(uint32_t i) { return mFrames[(mFrameHead + i) % mFrameCap]; }

	uint8_t* mStore       = nullptr;
	size_t mCommitted     = 0; // bytes of mStore committed
	uint32_t mMaxSlots    = 0;
	uint32_t* mSlotPage   = nullptr;
	uint32_t* mSlotPrev   = nullptr;
	uint64_t* mSlotFrame  = nullptr;
	uint32_t* mFreeStack  = nullptr;
	uint32_t mFreeTop     = 0;
	uint32_t mFresh       = 0; // slots [mFresh, mMaxSlots) never used
	uint64_t mLive        = 0;
	uint32_t* mHead       = nullptr; // per region page: newest version
	uint32_t* mStamp      = nullptr; // per region page: restore dedup
	uint32_t mStampVal    = 0;
	uint32_t* mUnion      = nullptr; // restore scratch
	size_t mRegionPages   = 0;
	uint32_t* mEntries    = nullptr; // circular: slot ids of window frames
	uint64_t mEntryHead   = 0;       // absolute cursor of the oldest entry
	uint64_t mEntryTail   = 0;       // absolute cursor past the newest entry
	FrameRec* mFrames     = nullptr;
	uint32_t mFrameCap    = 0;
	uint32_t mFrameHead   = 0;
	uint32_t mFrameCount  = 0;
	int mDepth            = 0;
	bool mValid           = false;
	uint64_t mOldest      = 0;
	uint64_t mNewest      = 0;
	uint64_t mBaseline    = 0;
};

} // namespace pcsnap
