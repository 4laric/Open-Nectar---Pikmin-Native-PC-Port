// Netplay M6a snapshot spike (issue #896), fix round 1 (F12): host-run test
// of the spike's region allocator through its public hooks. No game code:
// the engine-side hooks the spike TU calls are stubbed below.
//
// Covers: 16-byte payload alignment, size-class boundaries, zero-filled
// blocks (fresh and reused), small free-list reuse, large best-fit split and
// coalescing, large spans staying committed and zeroed on reuse (F8), and
// region deletes returning true while foreign pointers return false.

#include "netplay/pc_snapshot_spike.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// ---- stubs for the hooks pc_snapshot_spike.cpp calls ----------------------
class NaviMgr;
NaviMgr* naviMgr = nullptr;
void pc_state_hash_spike_suppress_log(bool) {}
unsigned long long piki_pc_spike_unknown_frees(void) { return 0; }
void piki_pc_spike_register_preserve(void) {}
extern "C" bool pc_os_stubs_static_arena(void**, size_t*) { return false; }
extern "C" void pc_os_stubs_spike_register_preserve(void) {}
void pc_snapshot_spike_game_tick(uint64_t) {}
void pc_snapshot_spike_game_sample(int* phase, int* pikis)
{
	*phase = 2;
	*pikis = 0;
}
uint64_t pc_snapshot_spike_game_audio_hash(void) { return 0; }
void pc_snapshot_spike_game_perturb_input(void) {}
void pc_snapshot_spike_game_describe(void) {}
uint64_t pc_state_hash_tick(void) { return 0; }
bool pc_state_hash_current(uint64_t* total, uint64_t subs[7], uint64_t* tick)
{
	*total = 0;
	for (int i = 0; i < 7; ++i) subs[i] = 0;
	*tick = 0;
	return true;
}
unsigned pc_netplay_tick(void) { return 0; }

// ---------------------------------------------------------------------------
static int sFailures = 0;

static void check(bool ok, const char* what)
{
	if (!ok) {
		std::printf("FAIL: %s\n", what);
		++sFailures;
	}
}

static void* ra() { return reinterpret_cast<void*>(&check); }

static void* nw(size_t n) { return pc_snapshot_spike_new(n, ra()); }
static bool del(void* p) { return pc_snapshot_spike_delete(p, ra()); }

static bool allZero(const void* p, size_t n)
{
	const unsigned char* b = static_cast<const unsigned char*>(p);
	for (size_t i = 0; i < n; ++i)
		if (b[i]) return false;
	return true;
}

static size_t capOf(void* p) { return size_t(static_cast<uint64_t*>(p)[-1]); }

int main()
{
	_putenv("PIKMIN_NETPLAY_SNAPSHOT_SPIKE=1");
	_putenv("PIKMIN_NETPLAY_SNAPSHOT_SPIKE_CSV=NUL");
	_putenv("PIKMIN_NETPLAY_SNAPSHOT_SPIKE_REPORT=NUL");
	_putenv("PIKMIN_NETPLAY_SNAPSHOT_SPIKE_GDIRTY_LOG=NUL");
	pc_snapshot_spike_init();
	check(pc_snapshot_spike_active(), "spike active after init");
	if (!pc_snapshot_spike_active()) return 1;

	void* arenaLo = nullptr;
	size_t arenaBytes = 0;
	check(pc_snapshot_spike_arena(&arenaLo, &arenaBytes) && arenaBytes == (size_t(256) << 20), "arena zone is 256 MB");

	// alignment, class boundaries, zero fill
	const size_t sizes[] = { 1, 15, 16, 17, 255, 256, 257, 320, 321, 1000, 4096, 32767, 32768 };
	const size_t caps[]  = { 16, 16, 16, 32, 256, 256, 320, 320, 384, 1024, 4096, 32768, 32768 };
	void* blocks[sizeof(sizes) / sizeof(sizes[0])];
	for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); ++i) {
		blocks[i] = nw(sizes[i]);
		check(blocks[i] != nullptr, "small alloc routed to the region");
		check((reinterpret_cast<uintptr_t>(blocks[i]) & 15) == 0, "16-byte payload alignment");
		check(capOf(blocks[i]) == caps[i], "size class capacity");
		check(allZero(blocks[i], caps[i]), "fresh small block is zero");
		std::memset(blocks[i], 0xAB, sizes[i]);
	}
	// free-list reuse: same class returns the same block, zeroed
	void* again = nullptr;
	check(del(blocks[9]), "region delete returns true");
	again = nw(900); // 1000 and 900 share the 1024 class
	check(again == blocks[9], "small free list reuses the block (LIFO)");
	check(allZero(again, 1024), "reused small block is zeroed");

	// large: best fit, split, coalesce, zero on reuse
	void* a = nw(100000);
	void* b = nw(300000);
	void* c = nw(100000);
	void* d = nw(50000);
	check(a && b && c && d, "large allocs routed to the region");
	check((reinterpret_cast<uintptr_t>(a) & 15) == 0 && (reinterpret_cast<uintptr_t>(a) & 4095) == 16,
	    "large payload sits 16 bytes into a page");
	std::memset(b, 0xCD, 300000);
	check(del(b), "large delete");
	void* e = nw(200000); // best fit: b's span, split
	check(e == b, "large best fit reuses the freed span");
	check(allZero(e, 200000), "reused large span is zeroed (kept committed, F8)");
	void* f = nw(60000); // the split remainder (>= 64 KB) serves this
	check(reinterpret_cast<uintptr_t>(f) > reinterpret_cast<uintptr_t>(e)
	        && reinterpret_cast<uintptr_t>(f) < reinterpret_cast<uintptr_t>(c),
	    "split remainder serves the next fitting request");
	check(allZero(f, 60000), "remainder block is zeroed");
	check(del(e) && del(f), "large deletes");
	void* g = nw(290000); // e + f coalesced back to b's span
	check(g == b, "freed neighbours coalesce");
	check(del(a) && del(c) && del(d) && del(g), "large deletes (tail back to the wilderness)");
	void* h = nw(100000);
	check(h == a, "the wilderness is reused from the lowest free address");
	check(allZero(h, 100000), "wilderness below the commit high-water mark is zeroed");

	// foreign pointers are not region blocks
	int local = 0;
	check(!pc_snapshot_spike_delete(&local, ra()), "a stack pointer is not a region block");
	void* heap = std::malloc(64);
	check(!pc_snapshot_spike_delete(heap, ra()), "a malloc pointer is not a region block");
	std::free(heap);

	if (sFailures) {
		std::printf("pc_snapshot_spike_test: %d failure(s)\n", sFailures);
		return 1;
	}
	std::printf("pc_snapshot_spike_test: all checks passed\n");
	return 0;
}
