#ifndef _SYSNEW_H
#define _SYSNEW_H

#include "types.h"
#include <stddef.h>

#if defined(PIKI_PC_PORT)
void* piki_pc_alloc(size_t size);
void piki_pc_free(void* ptr);
void piki_pc_dump_alloc_stats(void);
struct PikiPcAllocationStats { size_t liveBlocks, liveBytes, unknownFrees; };
// Read-only checkpoint for bounded stopped-engine ownership tests.
PikiPcAllocationStats piki_pc_allocation_stats();

// Opt-in native resource arena. Capture is confined to its creating thread;
// ordinary new/System::alloc and other threads retain their existing behavior.
// This owns storage only: the resource owner MUST detach graphics/native
// references and run required typed cleanup before releaseStorage(). No pointer
// or lifetime identity from this arena is serialized.
class PikiPcAllocationArena {
public:
    PikiPcAllocationArena() noexcept = default;
    ~PikiPcAllocationArena() noexcept;
    PikiPcAllocationArena(const PikiPcAllocationArena&) = delete;
    PikiPcAllocationArena& operator=(const PikiPcAllocationArena&) = delete;
    bool beginCapture() noexcept;
    bool endCapture() noexcept;
    bool capturing() const noexcept;
    bool owns(const void*) const noexcept;
    PikiPcAllocationStats storage() const noexcept;
    // Read-only physical-thread/capture authority checked before native/GPU cleanup.
    bool canReleaseStorage() const noexcept;
    bool releaseStorage() noexcept;
private:
    unsigned long long mTag = 0;
    unsigned long long mThread = 0;
};

class PikiPcAllocationCapture {
public:
    explicit PikiPcAllocationCapture(PikiPcAllocationArena& arena) noexcept
        : mArena(arena), mActive(arena.beginCapture()) { }
    ~PikiPcAllocationCapture() noexcept { if (mActive) mArena.endCapture(); }
    PikiPcAllocationCapture(const PikiPcAllocationCapture&) = delete;
    PikiPcAllocationCapture& operator=(const PikiPcAllocationCapture&) = delete;
    bool valid() const noexcept { return mActive; }
private:
    PikiPcAllocationArena& mArena;
    bool mActive;
};

// These must be strong, program-wide replacements. Inline definitions only
// affected translation units that happened to include this header, while the
// final executable still imported the libstdc++ operators.
void* operator new(size_t size);
void* operator new[](size_t size);
void operator delete(void* ptr) noexcept;
void operator delete[](void* ptr) noexcept;
void operator delete(void* ptr, size_t) noexcept;
void operator delete[](void* ptr, size_t) noexcept;
#else
#include "system.h"
inline void* operator new(size_t size)
{
	return System::alloc(size);
}
inline void* operator new[](size_t size)
{
	return System::alloc(size);
}
#endif
#if defined(PIKI_PC_PORT)
struct PikiAlignment {
	int value;
};
#define PIKI_ALIGNED(value) PikiAlignment { value }
void* operator new(size_t size, PikiAlignment alignment);
void* operator new[](size_t size, PikiAlignment alignment);
#else
#define PIKI_ALIGNED(value) value
void* operator new(size_t size, int alignment);
void* operator new[](size_t size, int alignment);
void operator delete(void* ptr);
void operator delete[](void* ptr);
#endif

#if defined(__MWERKS__)
#define stack_new(...) &__VA_ARGS__
#elif defined(_MSC_VER) && _MSC_VER < 1400
#define stack_new(type) &type
#else
#define stack_new(...) &__VA_ARGS__
#endif

#endif
