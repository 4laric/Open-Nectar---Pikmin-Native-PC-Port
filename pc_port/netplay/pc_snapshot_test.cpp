// Netplay M6b production snapshot (issue #896): host test of the region
// allocator (pc_snapshot_region) and the page ring (pc_snapshot_ring).
//
//   alignment (16 and the aligned path), class boundaries, split / coalesce,
//   zero fill, determinism of the address sequence, the ring's save /
//   restore / aging / re-baseline against full copies, and the fatal paths
//   (region full, oversized request, bad free) as death tests: the test runs
//   itself as a child with --death <case> and expects a non-zero exit and the
//   "[snapshot] FATAL" line.

#include "netplay/pc_snapshot_region.h"
#include "netplay/pc_snapshot_ring.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

using namespace pcsnap;

namespace {

int gFailures = 0;

#define CHECK(cond)                                                               \
	do {                                                                          \
		if (!(cond)) {                                                            \
			std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);           \
			++gFailures;                                                          \
		}                                                                         \
	} while (0)

bool allZero(const void* p, size_t n)
{
	const uint8_t* b = static_cast<const uint8_t*>(p);
	for (size_t i = 0; i < n; ++i)
		if (b[i]) return false;
	return true;
}

void testAlignmentAndClasses()
{
	Region r;
	CHECK(r.init(kFixedBase, size_t(24) << 20, true));
	CHECK(reinterpret_cast<uintptr_t>(r.base()) == kFixedBase);
	const size_t sizes[] = { 0, 1, 15, 16, 17, 255, 256, 257, 320, 321, 1000, 4096, 32767, 32768, 32769, 100000, 1 << 20 };
	std::vector<void*> blocks;
	for (size_t n : sizes) {
		void* p = r.alloc(n);
		CHECK(p != nullptr);
		CHECK((reinterpret_cast<uintptr_t>(p) & 15) == 0);
		CHECK(r.contains(p));
		CHECK(r.capOf(p) >= (n ? n : 1));
		CHECK(allZero(p, n));
		if (n <= kSmallMax && n > 0) CHECK(r.capOf(p) <= n + n / 4 + 16); // classes: 16-byte steps, then 25%
		std::memset(p, 0xAB, n);
		blocks.push_back(p);
	}
	for (size_t align : { size_t(32), size_t(64), size_t(256), size_t(4096) }) {
		for (size_t n : { size_t(1), size_t(100), size_t(5000), size_t(70000) }) {
			void* p = r.alloc(n, align);
			CHECK((reinterpret_cast<uintptr_t>(p) & (align - 1)) == 0);
			CHECK(allZero(p, n));
			std::memset(p, 0xCD, n);
			r.free(p, align);
		}
	}
	for (void* p : blocks) r.free(p);
	CHECK(r.stats().liveBlocks == 0);
	CHECK(r.stats().liveBytes == 0);
	r.shutdown();
}

void testSplitCoalesceZero()
{
	Region r;
	CHECK(r.init(kFixedBase, 0, true));
	const size_t big = 100000;
	uint8_t* a = static_cast<uint8_t*>(r.alloc(big));
	uint8_t* b = static_cast<uint8_t*>(r.alloc(big));
	uint8_t* c = static_cast<uint8_t*>(r.alloc(big));
	CHECK(a < b && b < c);
	std::memset(a, 1, big);
	std::memset(b, 2, big);
	std::memset(c, 3, big);
	r.free(b);
	r.free(a); // coalesces with b
	uint8_t* d = static_cast<uint8_t*>(r.alloc(2 * big));
	CHECK(d == a); // best fit is the coalesced a+b span
	CHECK(allZero(d, 2 * big));
	// split: a small large block reuses the head of a freed span
	r.free(d);
	uint8_t* e = static_cast<uint8_t*>(r.alloc(40000));
	CHECK(e == a);
	CHECK(allZero(e, 40000));
	// the tail goes back to the wilderness
	const uint64_t used = r.stats().heapUsedBytes;
	r.free(c);
	CHECK(r.stats().heapUsedBytes < used);
	// small blocks: a freed block is reused zero-filled
	uint8_t* s1 = static_cast<uint8_t*>(r.alloc(48));
	std::memset(s1, 0x77, 48);
	r.free(s1);
	uint8_t* s2 = static_cast<uint8_t*>(r.alloc(40)); // same class
	CHECK(s2 == s1);
	CHECK(allZero(s2, 48));
	r.free(s2);
	r.free(e);
	r.shutdown();
}

// Scripted pseudo-random allocation sequence; returns the offsets it saw.
std::vector<uint64_t> scriptedSequence()
{
	Region r;
	std::vector<uint64_t> offs;
	if (!r.init(kFixedBase, size_t(8) << 20, true)) return offs;
	std::vector<void*> live;
	uint32_t x = 12345;
	for (int i = 0; i < 20000; ++i) {
		x = x * 1664525u + 1013904223u;
		if (!live.empty() && (x >> 28) < 6) {
			const size_t k = (x >> 8) % live.size();
			r.free(live[k]);
			live[k] = live.back();
			live.pop_back();
			continue;
		}
		size_t n = (x >> 12) & 0x3FF;
		if ((x & 0x70) == 0x70) n = (x >> 8) % 200000;
		void* p = r.alloc(n);
		offs.push_back(uint64_t(static_cast<uint8_t*>(p) - r.base()));
		live.push_back(p);
	}
	for (void* p : live) r.free(p);
	offs.push_back(r.stats().liveBlocks);
	r.shutdown();
	return offs;
}

void testDeterminism()
{
	const std::vector<uint64_t> a = scriptedSequence();
	const std::vector<uint64_t> b = scriptedSequence();
	CHECK(!a.empty());
	CHECK(a == b);
	CHECK(a.back() == 0);
}

// Ring: several frames of page writes; every restore to T must reproduce the
// full copy taken at the end of T, for every k within the depth.
void testRing()
{
	Region r;
	CHECK(r.init(kFixedBase, size_t(4) << 20, true));
	uint8_t* blocks[8];
	for (int i = 0; i < 8; ++i) blocks[i] = static_cast<uint8_t*>(r.alloc(64 * 1024));
	PageRing ring;
	const int depth = 12;
	CHECK(ring.init(4096, depth, kPages));
	std::vector<uint32_t> touched(kPages);
	auto frameEnd = [&](uint64_t f, bool barrier) {
		r.collect(true);
		if (barrier) {
			const uint32_t n = r.listTouched(touched.data());
			CHECK(ring.rebaseline(f, r.base(), touched.data(), n));
		} else {
			CHECK(ring.save(f, r.base(), r.dirty(), r.dirtyCount()));
		}
		r.newEpoch();
	};
	auto snapshot = [&]() {
		std::vector<uint8_t> s;
		const size_t bytes = size_t(r.scanEnd() - r.base());
		s.resize(bytes);
		for (size_t p = 0; p < bytes / kPage; ++p) {
			if (r.touched(p)) std::memcpy(&s[p * kPage], r.base() + p * kPage, kPage);
		}
		return s;
	};
	auto same = [&](const std::vector<uint8_t>& s) {
		const size_t bytes = size_t(r.scanEnd() - r.base());
		for (size_t p = 0; p < bytes / kPage; ++p) {
			if (!r.touched(p)) continue;
			const uint8_t* want = p * kPage < s.size() ? &s[p * kPage] : nullptr;
			if (want ? std::memcmp(r.base() + p * kPage, want, kPage) != 0 : !allZero(r.base() + p * kPage, kPage)) return false;
		}
		return true;
	};
	uint32_t x = 99;
	auto mutate = [&](uint64_t f) {
		for (int i = 0; i < 40; ++i) {
			x = x * 1664525u + 1013904223u;
			blocks[(x >> 20) & 7][(x >> 4) % (64 * 1024)] = uint8_t(f + i);
		}
		if (f % 5 == 0) {
			// a fresh block: its pages were zero before this frame
			uint8_t* nb = static_cast<uint8_t*>(r.alloc(20000));
			nb[100]     = uint8_t(f);
		}
	};
	frameEnd(1, true);
	std::vector<std::vector<uint8_t>> at(200);
	at[1] = snapshot();
	uint64_t f = 1;
	int restores = 0;
	for (int round = 0; round < 40; ++round) {
		const int k = 1 + round % 7;
		for (int i = 0; i < k + 3; ++i) {
			++f;
			mutate(f);
			frameEnd(f, false);
			at[f] = snapshot();
		}
		// restore k frames back, compare, then re-run the same frames
		const uint64_t T = f - uint64_t(k);
		CHECK(ring.has(T));
		mutate(f + 1000); // pending writes after the last save join the union
		r.collect(true);
		ring.restore(T, r.base(), r.dirty(), r.dirtyCount());
		r.resetWatch();
		r.newEpoch();
		CHECK(same(at[T]));
		++restores;
		// discard the rolled-back frames' bookkeeping; the test's allocator
		// meta was rolled back with the region, so re-mutating reproduces them
		f = T;
		if (f + 20 >= at.size()) break;
	}
	CHECK(restores > 20);
	// aging: more than depth frames saved, the oldest restorable moves up
	for (int i = 0; i < depth + 5; ++i) {
		++f;
		if (f >= at.size()) at.resize(f + 1);
		mutate(f);
		frameEnd(f, false);
		at[f] = snapshot();
	}
	CHECK(!ring.has(f - uint64_t(depth) - 1));
	CHECK(ring.has(f - uint64_t(depth)));
	const uint64_t T = f - uint64_t(depth);
	ring.restore(T, r.base(), nullptr, 0);
	r.resetWatch();
	r.newEpoch();
	CHECK(same(at[T]));
	CHECK(ring.stats().frames == 0);
	ring.shutdown();
	r.shutdown();
}

int runDeath(const char* which)
{
	Region r;
	if (!r.init(kFixedBase, 0, true)) return 2;
	if (!std::strcmp(which, "oversized")) r.alloc(kMaxRequest + 1);
	if (!std::strcmp(which, "full")) {
		r.alloc(kMaxRequest);
		r.alloc(kMaxRequest); // the 2 GB reservation cannot hold two
	}
	if (!std::strcmp(which, "badfree")) {
		uint8_t* p = static_cast<uint8_t*>(r.alloc(100));
		r.free(p + 16);
	}
	if (!std::strcmp(which, "doublefree")) {
		void* p = r.alloc(5000);
		r.free(p);
		r.free(p);
	}
	return 0; // not reached when the case aborts
}

void testDeath(const char* self, const char* which)
{
	char cmd[1024];
	std::snprintf(cmd, sizeof(cmd), "\"\"%s\" --death %s 2>&1\"", self, which);
	FILE* pipe = _popen(cmd, "r");
	CHECK(pipe != nullptr);
	if (!pipe) return;
	char line[512];
	bool fatal = false;
	while (std::fgets(line, sizeof(line), pipe)) {
		if (std::strstr(line, "[snapshot] FATAL")) fatal = true;
	}
	const int rc = _pclose(pipe);
	std::printf("death %s: rc=%d fatal_line=%d\n", which, rc, fatal ? 1 : 0);
	CHECK(rc != 0);
	CHECK(fatal);
}

} // namespace

int main(int argc, char** argv)
{
	if (argc >= 3 && !std::strcmp(argv[1], "--death")) return runDeath(argv[2]);
	testAlignmentAndClasses();
	testSplitCoalesceZero();
	testDeterminism();
	testRing();
	char self[MAX_PATH];
	GetModuleFileNameA(nullptr, self, MAX_PATH);
	testDeath(self, "oversized");
	testDeath(self, "full");
	testDeath(self, "badfree");
	testDeath(self, "doublefree");
	if (gFailures) {
		std::printf("pc_snapshot_test: %d failure(s)\n", gFailures);
		return 1;
	}
	std::printf("pc_snapshot_test: all passed\n");
	return 0;
}
