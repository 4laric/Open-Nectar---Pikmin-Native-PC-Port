// Netplay M6b snapshot ring (issue #896). See pc_snapshot_ring.h.
//
// This TU never calls operator new (it runs at tick end while the game's
// allocator hooks are live): all storage comes from VirtualAlloc.

#include "netplay/pc_snapshot_ring.h"

#include "netplay/pc_snapshot_region.h"

#include <cstring>

namespace pcsnap {

bool PageRing::init(uint32_t maxSlots, int depth, size_t regionPages)
{
	if (mStore) return true;
	if (maxSlots == 0 || depth < 1) return false;
	mMaxSlots    = maxSlots;
	mDepth       = depth;
	mRegionPages = regionPages;
	mStore       = static_cast<uint8_t*>(osReserve(size_t(maxSlots) * kPage));
	mSlotPage    = static_cast<uint32_t*>(osAllocZero(size_t(maxSlots) * sizeof(uint32_t)));
	mSlotPrev    = static_cast<uint32_t*>(osAllocZero(size_t(maxSlots) * sizeof(uint32_t)));
	mSlotFrame   = static_cast<uint64_t*>(osAllocZero(size_t(maxSlots) * sizeof(uint64_t)));
	mFreeStack   = static_cast<uint32_t*>(osAllocZero(size_t(maxSlots) * sizeof(uint32_t)));
	mEntries     = static_cast<uint32_t*>(osAllocZero(size_t(maxSlots) * sizeof(uint32_t)));
	mHead        = static_cast<uint32_t*>(osAllocZero(regionPages * sizeof(uint32_t)));
	mStamp       = static_cast<uint32_t*>(osAllocZero(regionPages * sizeof(uint32_t)));
	mUnion       = static_cast<uint32_t*>(osAllocZero(regionPages * sizeof(uint32_t)));
	mFrameCap    = uint32_t(depth) + 2;
	mFrames      = static_cast<FrameRec*>(osAllocZero(size_t(mFrameCap) * sizeof(FrameRec)));
	if (!mStore || !mSlotPage || !mSlotPrev || !mSlotFrame || !mFreeStack || !mEntries || !mHead || !mStamp || !mUnion || !mFrames) {
		shutdown();
		return false;
	}
	std::memset(mHead, 0xFF, regionPages * sizeof(uint32_t));
	return true;
}

void PageRing::shutdown()
{
	osRelease(mStore);
	osRelease(mSlotPage);
	osRelease(mSlotPrev);
	osRelease(mSlotFrame);
	osRelease(mFreeStack);
	osRelease(mEntries);
	osRelease(mHead);
	osRelease(mStamp);
	osRelease(mUnion);
	osRelease(mFrames);
	*this = PageRing();
}

uint32_t PageRing::allocSlot()
{
	uint32_t s;
	if (mFreeTop) {
		s = mFreeStack[--mFreeTop];
	} else {
		s = mFresh++;
		const size_t end = size_t(mFresh) * kPage;
		if (end > mCommitted) {
			size_t target = (end + kGranule - 1) & ~(kGranule - 1);
			const size_t cap = size_t(mMaxSlots) * kPage;
			if (target > cap) target = cap;
			if (!osCommit(mStore + mCommitted, target - mCommitted)) fatal("ring slot commit failed at %zu MB", mCommitted >> 20);
			mCommitted = target;
		}
	}
	mLive++;
	return s;
}

void PageRing::freeSlot(uint32_t s)
{
	mFreeStack[mFreeTop++] = s;
	mLive--;
}

// The oldest window frame becomes the base: its versions are now the newest
// at or before the new oldest restorable frame, so every older version of
// those pages is released.
void PageRing::ageOldest()
{
	FrameRec rec = frameAt(0);
	for (uint32_t i = 0; i < rec.count; ++i) {
		const uint32_t s = mEntries[(rec.start + i) % mMaxSlots];
		uint32_t v       = mSlotPrev[s];
		while (v != kNone) {
			const uint32_t older = mSlotPrev[v];
			freeSlot(v);
			v = older;
		}
		mSlotPrev[s] = kNone;
	}
	mEntryHead = rec.start + rec.count;
	mFrameHead = (mFrameHead + 1) % mFrameCap;
	mFrameCount--;
	mOldest = rec.frame;
}

bool PageRing::save(uint64_t frame, const uint8_t* region, const uint32_t* pages, uint32_t n)
{
	if (!mValid || frame <= mNewest) fatal("ring save of frame %llu out of order (newest %llu, valid %d)",
	    (unsigned long long)frame, (unsigned long long)mNewest, mValid ? 1 : 0);
	const uint64_t available = uint64_t(mFreeTop) + uint64_t(mMaxSlots - mFresh);
	if (n > available) return false;
	FrameRec rec = { frame, mEntryTail, n };
	for (uint32_t i = 0; i < n; ++i) {
		const uint32_t p = pages[i];
		const uint32_t s = allocSlot();
		std::memcpy(mStore + size_t(s) * kPage, region + size_t(p) * kPage, kPage);
		mSlotPage[s]  = p;
		mSlotPrev[s]  = mHead[p];
		mSlotFrame[s] = frame;
		mHead[p]      = s;
		mEntries[mEntryTail++ % mMaxSlots] = s;
	}
	frameAt(mFrameCount) = rec;
	mFrameCount++;
	mNewest = frame;
	while (mFrameCount > uint32_t(mDepth)) ageOldest();
	return true;
}

bool PageRing::rebaseline(uint64_t frame, const uint8_t* region, const uint32_t* pages, uint32_t n)
{
	// drop every version
	mFreeTop = 0;
	mFresh   = 0;
	mLive    = 0;
	std::memset(mHead, 0xFF, mRegionPages * sizeof(uint32_t));
	mFrameHead = mFrameCount = 0;
	mEntryHead = mEntryTail = 0;
	mValid = false;
	if (n > mMaxSlots) return false;
	for (uint32_t i = 0; i < n; ++i) {
		const uint32_t p = pages[i];
		const uint32_t s = allocSlot();
		std::memcpy(mStore + size_t(s) * kPage, region + size_t(p) * kPage, kPage);
		mSlotPage[s]  = p;
		mSlotPrev[s]  = kNone;
		mSlotFrame[s] = frame;
		mHead[p]      = s;
	}
	mValid    = true;
	mOldest   = frame;
	mNewest   = frame;
	mBaseline = frame;
	return true;
}

RestoreResult PageRing::restore(uint64_t T, uint8_t* region, const uint32_t* extra, uint32_t nExtra)
{
	RestoreResult r = {};
	if (!has(T)) fatal("ring restore to frame %llu outside [%llu, %llu]", (unsigned long long)T,
	    (unsigned long long)mOldest, (unsigned long long)mNewest);
	if (++mStampVal == 0) {
		std::memset(mStamp, 0, mRegionPages * sizeof(uint32_t));
		mStampVal = 1;
	}
	// union of the pages dirtied after T and the pending pages
	uint32_t nu     = 0;
	uint32_t keep   = mFrameCount;
	while (keep > 0 && frameAt(keep - 1).frame > T) --keep;
	for (uint32_t f = keep; f < mFrameCount; ++f) {
		const FrameRec& rec = frameAt(f);
		for (uint32_t i = 0; i < rec.count; ++i) {
			const uint32_t p = mSlotPage[mEntries[(rec.start + i) % mMaxSlots]];
			++r.entries;
			if (mStamp[p] == mStampVal) continue;
			mStamp[p]    = mStampVal;
			mUnion[nu++] = p;
		}
	}
	for (uint32_t i = 0; i < nExtra; ++i) {
		const uint32_t p = extra[i];
		if (mStamp[p] == mStampVal) continue;
		mStamp[p]    = mStampVal;
		mUnion[nu++] = p;
	}
	// newest version at or before T, once per page
	for (uint32_t i = 0; i < nu; ++i) {
		const uint32_t p = mUnion[i];
		uint32_t v       = mHead[p];
		while (v != kNone && mSlotFrame[v] > T) v = mSlotPrev[v];
		uint8_t* dst = region + size_t(p) * kPage;
		if (v != kNone) {
			std::memcpy(dst, mStore + size_t(v) * kPage, kPage);
		} else {
			std::memset(dst, 0, kPage);
			++r.zeroPages;
		}
		mHead[p] = v;
	}
	r.unionPages = nu;
	// drop the frames after T
	for (uint32_t f = keep; f < mFrameCount; ++f) {
		const FrameRec& rec = frameAt(f);
		for (uint32_t i = 0; i < rec.count; ++i) freeSlot(mEntries[(rec.start + i) % mMaxSlots]);
	}
	if (keep < mFrameCount) mEntryTail = frameAt(keep).start;
	mFrameCount = keep;
	mNewest     = T;
	return r;
}

const uint8_t* PageRing::versionAt(uint32_t page, uint64_t frame) const
{
	if (page >= mRegionPages) return nullptr;
	uint32_t v = mHead[page];
	while (v != kNone && mSlotFrame[v] > frame) v = mSlotPrev[v];
	return v == kNone ? nullptr : mStore + size_t(v) * kPage;
}

RingStats PageRing::stats() const
{
	RingStats s      = {};
	s.liveSlots      = mLive;
	s.committedBytes = mCommitted;
	s.frames         = mFrameCount;
	s.oldest         = mOldest;
	s.newest         = mNewest;
	s.baseline       = mBaseline;
	s.valid          = mValid;
	return s;
}

} // namespace pcsnap
