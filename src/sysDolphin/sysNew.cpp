#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#undef ERROR
#undef near
#undef far
#undef small
#endif
#include "sysNew.h"

#include "DebugLog.h"
#include "MemStat.h"
#include <cstdint>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <mutex>
#include <new>
#if defined(PIKMIN_NETPLAY_SNAPSHOT)
#include <malloc.h>
#endif
#ifndef _WIN32
#include <sys/mman.h>
#endif

/**
 * @todo: Documentation
 * @note UNUSED Size: 00009C
 */
DEFINE_ERROR(4)

/**
 * @todo: Documentation
 * @note UNUSED Size: 0000F0
 */
DEFINE_PRINT("sysNew");

#if defined(PIKI_PC_PORT)
// Tracks allocations made through the PC operator new so delete can match
// them. The fixed bucket table and intrusive headers never allocate through
// operator new, avoiding recursion during boot and static initialisation.
//
// Why everything goes to the C heap: the game's AyuHeap arenas are reset by
// moving a cursor, which requires every allocation between resets to die at
// once. Native secondary threads (DVD worker, SDL audio, card worker) would
// allocate into whatever heap the main thread has active, and a concurrent
// reset would corrupt them. Until allocation carries per-thread heap
// context, routing global new into arenas is unsafe; game systems that need
// arena lifetimes go through System::alloc explicitly.
struct alignas(std::max_align_t) BootBlockHeader {
	u32 mMagic;
	u32 mPad;
	BootBlockHeader* mNext;
	size_t mSize;
	size_t mPad2;
};

static const u32 BOOT_MAGIC = 0x50694B31; // "PiK1"
static constexpr size_t kMapThreshold = 64u << 20;

#ifdef _WIN32
static void* map_zero_pages(size_t bytes) { return VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE); }
static void unmap_pages(void* p, size_t) { VirtualFree(p, 0, MEM_RELEASE); }
#else
static void* map_zero_pages(size_t bytes)
{
	void* p = mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
	return p == MAP_FAILED ? nullptr : p;
}
static void unmap_pages(void* p, size_t bytes) { munmap(p, bytes); }
#endif
static constexpr size_t BOOT_BUCKET_COUNT = 4096;

static std::mutex sAllocMutex;
static BootBlockHeader* sBootBuckets[BOOT_BUCKET_COUNT] = {};
static size_t sLiveAllocations = 0;
static size_t sLiveBytes = 0;
static size_t sPeakAllocations = 0;
static size_t sPeakBytes = 0;
static size_t sTotalAllocations = 0;
static size_t sTotalFrees = 0;
static size_t sUnknownFrees = 0;
static bool sDumpRegistered = false;
// Where the bytes are, by block size. 810 MB across twelve thousand blocks is
// not a game object leak; it is a small number of very large ones, and the
// totals alone cannot say which.
static const size_t SIZE_CLASS_COUNT = 8;
static size_t sClassBytes[SIZE_CLASS_COUNT] = {};
static size_t sClassCount[SIZE_CLASS_COUNT] = {};
static size_t sLargestBlock = 0;

static size_t sizeClassOf(size_t bytes)
{
    if (bytes < 1024) return 0;
    if (bytes < 16 * 1024) return 1;
    if (bytes < 64 * 1024) return 2;
    if (bytes < 256 * 1024) return 3;
    if (bytes < 1024 * 1024) return 4;
    if (bytes < 8 * 1024 * 1024) return 5;
    if (bytes < 64 * 1024 * 1024) return 6;
    return 7;
}

static size_t allocationBucket(const void* ptr)
{
	uintptr_t value = reinterpret_cast<uintptr_t>(ptr);
	value >>= 4;
	value ^= value >> 17;
	return value & (BOOT_BUCKET_COUNT - 1);
}

void piki_pc_dump_alloc_stats(void)
{
	std::lock_guard<std::mutex> lock(sAllocMutex);
	static const char* kClassNames[SIZE_CLASS_COUNT] = {
		"<1K", "1K-16K", "16K-64K", "64K-256K", "256K-1M", "1M-8M", "8M-64M", ">64M"
	};
	fprintf(stderr, "[PC Alloc] largest single block %zu bytes\n", sLargestBlock);
	for (size_t i = 0; i < SIZE_CLASS_COUNT; ++i) {
		if (sClassCount[i] == 0) continue;
		fprintf(stderr, "[PC Alloc]   %-8s %zu live, %zu MB\n",
		        kClassNames[i], sClassCount[i], sClassBytes[i] >> 20);
	}
	fprintf(stderr,
	        "[PC Alloc] live=%zu bytes=%zu peak=%zu/%zu total=%zu frees=%zu unknown-frees=%zu\n",
	        sLiveAllocations, sLiveBytes, sPeakAllocations, sPeakBytes,
	        sTotalAllocations, sTotalFrees, sUnknownFrees);
}

void* piki_pc_alloc(size_t size)
{
	if (size == 0) {
		size = 1;
	}
	if (size & 0x3) {
		if (size > std::numeric_limits<size_t>::max() - 3) {
			throw std::bad_alloc();
		}
		size = (size + 3) & ~static_cast<size_t>(0x3);
	}
	if (size > std::numeric_limits<size_t>::max() - sizeof(BootBlockHeader)) {
		throw std::bad_alloc();
	}

	// Zeroed memory is part of the contract (the console heaps zeroed every
	// block). For big blocks calloc keeps it without touching the pages: a
	// fresh mapping is already zero and stays non-resident until written.
	// memset here made the unused 253 MB "ovl" and 245 MB "app" heap buffers
	// resident on Android (500 MB of PSS for nothing).
	// scudo (Android's malloc) still touches huge calloc blocks, so those go
	// to mmap directly: the kernel hands out zero pages lazily. The header
	// remembers the mapping so piki_pc_free can munmap it.
	const bool bigBlock = size >= (256u << 10);
	const bool mapped = size >= kMapThreshold;
	const size_t mappedBytes = mapped ? ((sizeof(BootBlockHeader) + size + 4095) & ~size_t(4095)) : 0;
	void* raw = nullptr;
	if (mapped) {
		raw = map_zero_pages(mappedBytes);
	} else {
		raw = bigBlock ? std::calloc(1, sizeof(BootBlockHeader) + size)
		               : std::malloc(sizeof(BootBlockHeader) + size);
	}
	if (!raw) {
		ERROR("allocation of %zu bytes failed", size);
		throw std::bad_alloc();
	}

	BootBlockHeader* header = static_cast<BootBlockHeader*>(raw);
	header->mMagic          = BOOT_MAGIC;
	header->mNext           = nullptr;
	header->mSize           = size;
	header->mPad2           = mappedBytes; // non-zero: whole block is one mapping

	void* result = header + 1;
	{
		std::lock_guard<std::mutex> lock(sAllocMutex);
		const size_t bucket = allocationBucket(result);
		header->mNext = sBootBuckets[bucket];
		sBootBuckets[bucket] = header;
		sLiveAllocations++;
		sLiveBytes += size;
		{
			const size_t cls = sizeClassOf(size);
			sClassBytes[cls] += size;
			sClassCount[cls]++;
			if (size > sLargestBlock) sLargestBlock = size;
			// Fase 7: name the big ones as they happen so the log around them
			// says what was loading. Reported once per size class step.
			// getenv here, not once: the first big blocks predate env.txt.
			if (size >= (4u << 20) && getenv("PIKMIN_ALLOC_TRACE")) {
				fprintf(stderr, "[PC Alloc] big block %zu MB (live now %zu MB in %zu blocks)\n",
				        size >> 20, (sLiveBytes) >> 20, sLiveAllocations);
			}
		}
		sTotalAllocations++;
		if (sLiveAllocations > sPeakAllocations) sPeakAllocations = sLiveAllocations;
		if (sLiveBytes > sPeakBytes) sPeakBytes = sLiveBytes;
		if (!sDumpRegistered) {
			std::atexit(piki_pc_dump_alloc_stats);
			sDumpRegistered = true;
		}
	}
	if (!bigBlock) std::memset(result, 0, size);
	return result;
}

#if defined(PIKMIN_NETPLAY_SNAPSHOT_SPIKE)
__attribute__((weak)) void pc_snapshot_spike_unknown_free(void* ptr);
#endif

void piki_pc_free(void* ptr)
{
	if (!ptr) {
		return;
	}

	std::lock_guard<std::mutex> lock(sAllocMutex);
	const size_t bucket = allocationBucket(ptr);
	for (BootBlockHeader** link = &sBootBuckets[bucket]; *link; link = &(*link)->mNext) {
		BootBlockHeader* header = *link;
		if (header->mMagic == BOOT_MAGIC && header + 1 == ptr) {
			*link = header->mNext;
			header->mMagic = 0;
			sLiveAllocations--;
			sLiveBytes -= header->mSize;
			{
				const size_t cls = sizeClassOf(header->mSize);
				sClassBytes[cls] -= header->mSize;
				sClassCount[cls]--;
			}
			sTotalFrees++;
			if (header->mPad2) unmap_pages(header, header->mPad2);
			else std::free(header);
			return;
		}
	}
	sUnknownFrees++;
#if defined(PIKMIN_NETPLAY_SNAPSHOT_SPIKE)
	if (pc_snapshot_spike_unknown_free) pc_snapshot_spike_unknown_free(ptr);
#endif
}

#if defined(PIKMIN_NETPLAY_SNAPSHOT_SPIKE)
// Netplay M6a snapshot spike (issue #896): main-thread SIM allocations go to
// the spike's write-watched region; everything else keeps the path above.
// Weak: targets that compile this TU without the spike TU still link, and
// the spike itself is inert unless PIKMIN_NETPLAY_SNAPSHOT_SPIKE=1.
__attribute__((weak)) void* pc_snapshot_spike_new(size_t size, void* returnAddress);
__attribute__((weak)) bool pc_snapshot_spike_delete(void* ptr, void* returnAddress);
__attribute__((weak)) void pc_snapshot_spike_preserve(const void* p, size_t bytes);
__attribute__((weak)) void* pc_snapshot_spike_note_off(void* p, size_t size, void* returnAddress);

unsigned long long piki_pc_spike_unknown_frees(void)
{
	return sUnknownFrees;
}

// The malloc tracking table is infrastructure: a synctest restore must never
// roll it back while the C heap itself stays where it is.
void piki_pc_spike_register_preserve(void)
{
	if (!pc_snapshot_spike_preserve) return;
	pc_snapshot_spike_preserve(&sAllocMutex, sizeof(sAllocMutex));
	pc_snapshot_spike_preserve(sBootBuckets, sizeof(sBootBuckets));
	pc_snapshot_spike_preserve(&sLiveAllocations, sizeof(sLiveAllocations));
	pc_snapshot_spike_preserve(&sLiveBytes, sizeof(sLiveBytes));
	pc_snapshot_spike_preserve(&sPeakAllocations, sizeof(sPeakAllocations));
	pc_snapshot_spike_preserve(&sPeakBytes, sizeof(sPeakBytes));
	pc_snapshot_spike_preserve(&sTotalAllocations, sizeof(sTotalAllocations));
	pc_snapshot_spike_preserve(&sTotalFrees, sizeof(sTotalFrees));
	pc_snapshot_spike_preserve(&sUnknownFrees, sizeof(sUnknownFrees));
	pc_snapshot_spike_preserve(&sDumpRegistered, sizeof(sDumpRegistered));
	pc_snapshot_spike_preserve(sClassBytes, sizeof(sClassBytes));
	pc_snapshot_spike_preserve(sClassCount, sizeof(sClassCount));
	pc_snapshot_spike_preserve(&sLargestBlock, sizeof(sLargestBlock));
}

#define PIKI_SPIKE_NEW(size) if (pc_snapshot_spike_new) { if (void* spikeBlock = pc_snapshot_spike_new((size), __builtin_return_address(0))) return spikeBlock; if (pc_snapshot_spike_note_off) return pc_snapshot_spike_note_off(piki_pc_alloc(size), (size), __builtin_return_address(0)); }
#define PIKI_SPIKE_DELETE(ptr) if (pc_snapshot_spike_delete && pc_snapshot_spike_delete(ptr, __builtin_return_address(0))) return;
#else
#define PIKI_SPIKE_NEW(size)
#define PIKI_SPIKE_DELETE(ptr)
#endif

#if defined(PIKMIN_NETPLAY_SNAPSHOT)
// Netplay M6b production snapshot (issue #896): main-thread operator new
// inside a SIM scope goes to the write-watched snapshot region; everything
// else keeps the path below. Weak: targets that compile this TU without the
// snapshot TUs still link, and the snapshot is inert unless
// PIKMIN_NETPLAY_SNAPSHOT=1.
__attribute__((weak)) void* pc_snapshot_new(size_t size, size_t align, void* returnAddress);
__attribute__((weak)) bool pc_snapshot_delete(void* ptr, size_t align, void* returnAddress);
__attribute__((weak)) void pc_snapshot_preserve(const void* p, size_t bytes);

unsigned long long piki_pc_snapshot_unknown_frees(void)
{
	return sUnknownFrees;
}

// The malloc tracking table is infrastructure: a restore must never roll it
// back while the C heap itself stays where it is.
void piki_pc_snapshot_register_preserve(void)
{
	if (!pc_snapshot_preserve) return;
	pc_snapshot_preserve(&sAllocMutex, sizeof(sAllocMutex));
	pc_snapshot_preserve(sBootBuckets, sizeof(sBootBuckets));
	pc_snapshot_preserve(&sLiveAllocations, sizeof(sLiveAllocations));
	pc_snapshot_preserve(&sLiveBytes, sizeof(sLiveBytes));
	pc_snapshot_preserve(&sPeakAllocations, sizeof(sPeakAllocations));
	pc_snapshot_preserve(&sPeakBytes, sizeof(sPeakBytes));
	pc_snapshot_preserve(&sTotalAllocations, sizeof(sTotalAllocations));
	pc_snapshot_preserve(&sTotalFrees, sizeof(sTotalFrees));
	pc_snapshot_preserve(&sUnknownFrees, sizeof(sUnknownFrees));
	pc_snapshot_preserve(&sDumpRegistered, sizeof(sDumpRegistered));
	pc_snapshot_preserve(sClassBytes, sizeof(sClassBytes));
	pc_snapshot_preserve(sClassCount, sizeof(sClassCount));
	pc_snapshot_preserve(&sLargestBlock, sizeof(sLargestBlock));
}

#define PIKI_SNAP_NEW(size) if (pc_snapshot_new) { if (void* snapBlock = pc_snapshot_new((size), 16, __builtin_return_address(0))) return snapBlock; }
#define PIKI_SNAP_DELETE(ptr) if (pc_snapshot_delete && pc_snapshot_delete((ptr), 16, __builtin_return_address(0))) return;
#else
#define PIKI_SNAP_NEW(size)
#define PIKI_SNAP_DELETE(ptr)
#endif

void* operator new(size_t size)
{
	PIKI_SPIKE_NEW(size) PIKI_SNAP_NEW(size)
	return piki_pc_alloc(size);
}

void* operator new[](size_t size)
{
	PIKI_SPIKE_NEW(size) PIKI_SNAP_NEW(size)
	return piki_pc_alloc(size);
}

void operator delete(void* ptr) noexcept
{
	PIKI_SPIKE_DELETE(ptr) PIKI_SNAP_DELETE(ptr)
	piki_pc_free(ptr);
}

void operator delete[](void* ptr) noexcept
{
	PIKI_SPIKE_DELETE(ptr) PIKI_SNAP_DELETE(ptr)
	piki_pc_free(ptr);
}

void operator delete(void* ptr, size_t) noexcept
{
	PIKI_SPIKE_DELETE(ptr) PIKI_SNAP_DELETE(ptr)
	piki_pc_free(ptr);
}

void operator delete[](void* ptr, size_t) noexcept
{
	PIKI_SPIKE_DELETE(ptr) PIKI_SNAP_DELETE(ptr)
	piki_pc_free(ptr);
}

#if defined(PIKMIN_NETPLAY_SNAPSHOT)
// M6b (#896, C2-9): over-aligned operator new/delete. libstdc++'s defaults
// go to _aligned_malloc, off the region; these keep SIM allocations in it
// and pair the rest with _aligned_malloc/_aligned_free themselves.
static void* piki_snap_aligned_new(size_t size, std::align_val_t al, void* ra)
{
	const size_t align = static_cast<size_t>(al);
	if (pc_snapshot_new) {
		if (void* snapBlock = pc_snapshot_new(size, align, ra)) return snapBlock;
	}
	void* p = _aligned_malloc(size ? size : 1, align);
	if (!p) throw std::bad_alloc();
	return p;
}

static void piki_snap_aligned_delete(void* ptr, std::align_val_t al, void* ra)
{
	if (!ptr) return;
	if (pc_snapshot_delete && pc_snapshot_delete(ptr, static_cast<size_t>(al), ra)) return;
	_aligned_free(ptr);
}

void* operator new(size_t size, std::align_val_t al)
{
	return piki_snap_aligned_new(size, al, __builtin_return_address(0));
}

void* operator new[](size_t size, std::align_val_t al)
{
	return piki_snap_aligned_new(size, al, __builtin_return_address(0));
}

void operator delete(void* ptr, std::align_val_t al) noexcept
{
	piki_snap_aligned_delete(ptr, al, __builtin_return_address(0));
}

void operator delete[](void* ptr, std::align_val_t al) noexcept
{
	piki_snap_aligned_delete(ptr, al, __builtin_return_address(0));
}

void operator delete(void* ptr, size_t, std::align_val_t al) noexcept
{
	piki_snap_aligned_delete(ptr, al, __builtin_return_address(0));
}

void operator delete[](void* ptr, size_t, std::align_val_t al) noexcept
{
	piki_snap_aligned_delete(ptr, al, __builtin_return_address(0));
}
#endif
#endif

/**
 * @todo: Documentation
 */
void* System::alloc(size_t size)
{
	void* result = nullptr;
	System* system = gsys;
	if (!system) {
		fprintf(stderr, "[PC Port Fatal] System::alloc(%zu) called without an active System instance\n", size);
		abort();
	}
	if (size & 0x3) {
		size = (size + 3) & ~0x3;
	}

	if (system->mActiveHeapIdx >= 0) {
		AyuHeap* heap = &system->mHeaps[system->mActiveHeapIdx];
		if (size == 0) {
			PRINT("trying to allocate %d bytes on heap\n", 0);
		}
		result = heap->push(size);
		if (!result) {
			ERROR("new[] %d failed in heap '%s'", size, heap->mName);
		}

		if (size == 0 || system->mForcePrint) {
			bool print             = system->mTogglePrint;
			system->mTogglePrint = TRUE;
			system->mTogglePrint = print;
		}

		MemInfo* info = system->mCurrMemInfo;
		while (info) {
			info->mMemorySize += size;
			info = static_cast<MemInfo*>(info->mParent);
		}

		if ((u32)(uintptr_t)result & 0x3) {
			ERROR("acquired memory not long aligned %08x!!\n", (u32)(uintptr_t)result);
		}

		u32* resPtr = (u32*)result;
		int length  = size / 4;
		for (int i = 0; i < length; i++) {
			resPtr[i] = 0;
		}
	} else {
#if defined(WIN32)
		// The DLL uses `GlobalAlloc` here and has an ERROR if that fails.  This branch of code is probably DLL exclusive,
		// since the GCN can't just ask WinAPI for unlimited memory.  We'll see once JPN Demo version matching begins.
		if (!result) {
			ERROR("new[] %d failed", size);
		}
#else
		ERROR("no heap specified\n");
#endif
	}

	return result;
}

/**
 * @todo: Documentation
 * @note UNUSED Size: 000044 (Matching by size)
 */
#if defined(PIKI_PC_PORT)
void* operator new(size_t size, PikiAlignment requestedAlignment)
#else
void* operator new(size_t size, int requestedAlignment)
#endif
{
	int alignment =
#if defined(PIKI_PC_PORT)
	    requestedAlignment.value;
#else
	    requestedAlignment;
#endif
	uintptr_t alloc  = reinterpret_cast<uintptr_t>(System::alloc(size + alignment));
	uintptr_t result = (alloc + (alignment - 1)) & ~static_cast<uintptr_t>(alignment - 1);
	return (void*)result;
}

/**
 * @todo: Documentation
 */
#if defined(PIKI_PC_PORT)
void* operator new[](size_t size, PikiAlignment requestedAlignment)
#else
void* operator new[](size_t size, int requestedAlignment)
#endif
{
	int alignment =
#if defined(PIKI_PC_PORT)
	    requestedAlignment.value;
#else
	    requestedAlignment;
#endif
	uintptr_t alloc  = reinterpret_cast<uintptr_t>(System::alloc(size + alignment));
	uintptr_t result = (alloc + (alignment - 1)) & ~static_cast<uintptr_t>(alignment - 1);
	return (void*)result;
}

/**
 * @todo: Documentation
 */
void dummy_delete(void*)
{
}

/**
 * @todo: Documentation
 */
void dummy_delete_arr(void*)
{
}
