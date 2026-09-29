// Netplay M6a spike (issue #896): GetWriteWatch cost versus committed size
// and dirty-page count, outside the game. Standalone; build by hand:
//   g++ -O2 -o ww_bench.exe tools/netplay/ww_bench.cpp
//
//   ww_bench.exe                 (default) size x dirty grid, single thread:
//       one line per (committed MB, dirty pages) cell: the median over 200
//       reps of GetWriteWatch(RESET) and of the first-write cost of dirtying
//       the pages again (the write-watch "fault tax"), plus the same
//       first-write cost on a region without MEM_WRITE_WATCH for comparison.
//   ww_bench.exe threads         (fix round 1, MV-2) the same GetWriteWatch
//       (RESET) cost with 0/2/4/8 other threads of this process running on
//       other cores (spinning over their own memory). The in-game per-page
//       cost is far above the single-threaded bench; if the reset's TLB
//       shootdown IPIs to the process's other running threads explain it,
//       the per-dirty-page cost grows with the thread count here.
//   ww_bench.exe commit          (fix round 1, MV-3) coverage cases: pages
//       committed after the last reset, commit-then-write in one interval,
//       decommit + recommit + write, and a write from another thread between
//       a RESET collect and the next one. Prints reported / written counts.
//   ww_bench.exe layout          (M6b item 3) one GetWriteWatch(RESET) call
//       per rep, constant call count, over ranges of 32/72/128/200/512/1024
//       MB whose tail past a fixed 32 MB touched prefix is (a) touched,
//       (b) committed but never touched, (c) reserved and never committed.
//       800 pages of the prefix are dirtied before each call. Prints the
//       p50/p95 per cell and a least-squares fit ms = a + b * MB per tail
//       state (a = per-call cost, b = per-MB cost), plus system busy % over
//       the bench (GetSystemTimes) so a loaded run is visible.

#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <thread>
#include <vector>

static double msNow()
{
	static LARGE_INTEGER f = [] {
		LARGE_INTEGER x;
		QueryPerformanceFrequency(&x);
		return x;
	}();
	LARGE_INTEGER t;
	QueryPerformanceCounter(&t);
	return double(t.QuadPart) * 1000.0 / double(f.QuadPart);
}

static double median(std::vector<double> v)
{
	std::sort(v.begin(), v.end());
	return v[v.size() / 2];
}

static const size_t kPage = 4096;

static int gridBench()
{
	const size_t sizesMb[] = { 16, 64, 128, 256, 512, 800 };
	const size_t dirtyCounts[] = { 0, 100, 400, 800, 1600, 3200 };
	const int reps = 200;
	std::vector<void*> addrs(size_t(800) << 8);
	std::printf("committed_mb,dirty_pages,ww_ms_p50,write_ms_p50_ww,write_ms_p50_plain,us_per_page_tax\n");
	for (size_t mb : sizesMb) {
		const size_t bytes = mb << 20;
		const size_t pages = bytes / kPage;
		uint8_t* ww = static_cast<uint8_t*>(VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT | MEM_WRITE_WATCH, PAGE_READWRITE));
		uint8_t* plain = static_cast<uint8_t*>(VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
		if (!ww || !plain) {
			std::printf("alloc failed at %zu MB\n", mb);
			return 1;
		}
		// make every page resident first, as in the game (touched working set)
		for (size_t p = 0; p < pages; ++p) {
			ww[p * kPage] = 1;
			plain[p * kPage] = 1;
		}
		ULONG_PTR count = addrs.size();
		DWORD gran = 0;
		GetWriteWatch(WRITE_WATCH_FLAG_RESET, ww, bytes, addrs.data(), &count, &gran);
		for (size_t dirty : dirtyCounts) {
			if (dirty > pages) continue;
			const size_t stride = dirty ? pages / dirty : 1;
			std::vector<double> wwMs, wrWw, wrPlain;
			for (int r = 0; r < reps; ++r) {
				double t0 = msNow();
				for (size_t i = 0; i < dirty; ++i) ww[(i * stride) * kPage + (r & 63) * 8] = uint8_t(r);
				wrWw.push_back(msNow() - t0);
				t0 = msNow();
				for (size_t i = 0; i < dirty; ++i) plain[(i * stride) * kPage + (r & 63) * 8] = uint8_t(r);
				wrPlain.push_back(msNow() - t0);
				count = addrs.size();
				t0 = msNow();
				GetWriteWatch(WRITE_WATCH_FLAG_RESET, ww, bytes, addrs.data(), &count, &gran);
				wwMs.push_back(msNow() - t0);
			}
			const double tax = dirty ? (median(wrWw) - median(wrPlain)) * 1000.0 / double(dirty) : 0.0;
			std::printf("%zu,%zu,%.4f,%.4f,%.4f,%.3f\n", mb, dirty, median(wwMs), median(wrWw), median(wrPlain), tax);
			std::fflush(stdout);
		}
		VirtualFree(ww, 0, MEM_RELEASE);
		VirtualFree(plain, 0, MEM_RELEASE);
	}
	return 0;
}

static int threadBench()
{
	const size_t sizesMb[] = { 32, 800 };
	const size_t dirtyCounts[] = { 0, 200, 500, 800, 1600 };
	const int threadCounts[] = { 0, 2, 4, 8 };
	const int reps = 150;
	std::vector<void*> addrs(size_t(800) << 8);
	std::printf("committed_mb,threads,dirty_pages,ww_reset_ms_p50,ww_noreset_ms_p50,us_per_dirty_page_reset\n");
	for (size_t mb : sizesMb) {
		const size_t bytes = mb << 20;
		const size_t pages = bytes / kPage;
		uint8_t* ww = static_cast<uint8_t*>(VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT | MEM_WRITE_WATCH, PAGE_READWRITE));
		if (!ww) return 1;
		for (size_t p = 0; p < pages; ++p) ww[p * kPage] = 1;
		for (int nt : threadCounts) {
			std::atomic<bool> stop{ false };
			std::vector<std::thread> spin;
			for (int t = 0; t < nt; ++t) {
				spin.emplace_back([&stop] {
					std::vector<uint8_t> mine(size_t(4) << 20);
					uint64_t acc = 0;
					while (!stop.load(std::memory_order_relaxed)) {
						for (size_t i = 0; i < mine.size(); i += 64) acc += mine[i]++;
					}
					if (acc == 42) std::printf(" ");
				});
			}
			Sleep(50);
			double base0 = 0.0;
			for (size_t dirty : dirtyCounts) {
				if (dirty > pages) continue;
				const size_t stride = dirty ? pages / dirty : 1;
				std::vector<double> rs, nr;
				ULONG_PTR count = addrs.size();
				DWORD gran      = 0;
				GetWriteWatch(WRITE_WATCH_FLAG_RESET, ww, bytes, addrs.data(), &count, &gran);
				for (int r = 0; r < reps; ++r) {
					for (size_t i = 0; i < dirty; ++i) ww[(i * stride) * kPage + (r & 63) * 8] = uint8_t(r);
					count     = addrs.size();
					double t0 = msNow();
					GetWriteWatch(0, ww, bytes, addrs.data(), &count, &gran);
					nr.push_back(msNow() - t0);
					count = addrs.size();
					t0    = msNow();
					GetWriteWatch(WRITE_WATCH_FLAG_RESET, ww, bytes, addrs.data(), &count, &gran);
					rs.push_back(msNow() - t0);
				}
				const double m = median(rs);
				if (dirty == 0) base0 = m;
				const double perPage = dirty ? (m - base0) * 1000.0 / double(dirty) : 0.0;
				std::printf("%zu,%d,%zu,%.4f,%.4f,%.3f\n", mb, nt, dirty, m, median(nr), perPage);
				std::fflush(stdout);
			}
			stop = true;
			for (auto& t : spin) t.join();
		}
		VirtualFree(ww, 0, MEM_RELEASE);
	}
	return 0;
}

static ULONG_PTR reported(uint8_t* lo, size_t bytes, std::vector<void*>& addrs, DWORD flags)
{
	ULONG_PTR count = addrs.size();
	DWORD gran      = 0;
	if (GetWriteWatch(flags, lo, bytes, addrs.data(), &count, &gran) != 0) return ULONG_PTR(-1);
	return count;
}

static int commitBench()
{
	const size_t reserve = size_t(64) << 20;
	uint8_t* base = static_cast<uint8_t*>(VirtualAlloc(nullptr, reserve, MEM_RESERVE | MEM_WRITE_WATCH, PAGE_READWRITE));
	if (!base) return 1;
	std::vector<void*> addrs(reserve / kPage);
	std::printf("case,written,reported,verdict\n");
	auto line = [](const char* name, size_t written, ULONG_PTR rep) {
		std::printf("%s,%zu,%llu,%s\n", name, written, (unsigned long long)rep, rep == written ? "OK" : "MISMATCH");
	};
	// A: commit 16 MB, write 100 pages, collect (baseline)
	VirtualAlloc(base, size_t(16) << 20, MEM_COMMIT, PAGE_READWRITE);
	for (size_t i = 0; i < 100; ++i) base[i * 40 * kPage] = 1;
	line("commit_then_write_before_first_reset", 100, reported(base, reserve, addrs, WRITE_WATCH_FLAG_RESET));
	// B: after the reset, commit a new 16 MB and write 100 pages in it
	uint8_t* b = base + (size_t(16) << 20);
	VirtualAlloc(b, size_t(16) << 20, MEM_COMMIT, PAGE_READWRITE);
	for (size_t i = 0; i < 100; ++i) b[i * 40 * kPage] = 2;
	line("commit_after_reset_then_write", 100, reported(base, reserve, addrs, WRITE_WATCH_FLAG_RESET));
	// C: commit after reset, no write (fresh zero pages must not be reported)
	uint8_t* c = base + (size_t(32) << 20);
	VirtualAlloc(c, size_t(8) << 20, MEM_COMMIT, PAGE_READWRITE);
	line("commit_after_reset_no_write", 0, reported(base, reserve, addrs, WRITE_WATCH_FLAG_RESET));
	// D: write, decommit, recommit, write again in the same interval
	for (size_t i = 0; i < 50; ++i) c[i * kPage] = 3;
	VirtualFree(c, size_t(8) << 20, MEM_DECOMMIT);
	VirtualAlloc(c, size_t(8) << 20, MEM_COMMIT, PAGE_READWRITE);
	for (size_t i = 0; i < 20; ++i) c[i * kPage] = 4;
	line("write_decommit_recommit_write_20", 20, reported(base, reserve, addrs, WRITE_WATCH_FLAG_RESET));
	// E: write, decommit, recommit, no write (were the pre-decommit writes kept?)
	for (size_t i = 0; i < 50; ++i) c[i * kPage] = 5;
	VirtualFree(c, size_t(8) << 20, MEM_DECOMMIT);
	VirtualAlloc(c, size_t(8) << 20, MEM_COMMIT, PAGE_READWRITE);
	line("write_decommit_recommit_nowrite", 0, reported(base, reserve, addrs, WRITE_WATCH_FLAG_RESET));
	// F: a write from another thread between two RESET collects
	std::thread other([base] {
		for (size_t i = 0; i < 64; ++i) base[(1000 + i) * kPage] = 6;
	});
	other.join();
	line("other_thread_write", 64, reported(base, reserve, addrs, WRITE_WATCH_FLAG_RESET));
	// G: ResetWriteWatch after writes discards them (the spike's post-restore reset)
	for (size_t i = 0; i < 30; ++i) base[(2000 + i) * kPage] = 7;
	ResetWriteWatch(base, reserve);
	line("write_then_ResetWriteWatch", 0, reported(base, reserve, addrs, WRITE_WATCH_FLAG_RESET));
	VirtualFree(base, 0, MEM_RELEASE);
	return 0;
}

static double pctOf(std::vector<double> v, double p)
{
	std::sort(v.begin(), v.end());
	const double i = (double(v.size()) - 1.0) * p / 100.0;
	const size_t lo = size_t(i);
	const size_t hi = std::min(lo + 1, v.size() - 1);
	return v[lo] + (v[hi] - v[lo]) * (i - double(lo));
}

static uint64_t ft64(const FILETIME& f) { return (uint64_t(f.dwHighDateTime) << 32) | f.dwLowDateTime; }

struct BusySample {
	uint64_t idle = 0, kernel = 0, user = 0;
	void take()
	{
		FILETIME i, k, u;
		GetSystemTimes(&i, &k, &u);
		idle   = ft64(i);
		kernel = ft64(k);
		user   = ft64(u);
	}
	double busySince(const BusySample& a) const
	{
		const uint64_t tot = (kernel - a.kernel) + (user - a.user);
		return tot ? 100.0 * double(tot - (idle - a.idle)) / double(tot) : -1.0;
	}
};

// M6b item 3: per-call vs per-MB cost of GetWriteWatch(RESET) depending on
// the state of the scanned range's tail (touched / committed untouched /
// reserved). One call per rep, so the call count is constant across cells.
static int layoutBench()
{
	const size_t prefixMb   = 32;
	const size_t rangesMb[] = { 32, 72, 128, 200, 512, 1024 };
	const char* tails[]     = { "touched", "committed", "reserved" };
	const size_t dirty      = 800;
	const int reps          = 300;
	std::vector<void*> addrs(size_t(1024) << 8);
	BusySample b0;
	b0.take();
	std::printf("tail,range_mb,dirty_pages,reps,ww_ms_p50,ww_ms_p95,reported_p50\n");
	struct Pt {
		int tail;
		double mb, ms;
	};
	std::vector<Pt> pts;
	for (int t = 0; t < 3; ++t) {
		for (size_t mb : rangesMb) {
			if (t > 0 && mb == prefixMb) continue; // no tail: same as touched
			const size_t bytes  = mb << 20;
			const size_t prefix = prefixMb << 20;
			uint8_t* base = static_cast<uint8_t*>(VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_WRITE_WATCH, PAGE_READWRITE));
			if (!base) {
				std::printf("reserve failed at %zu MB\n", mb);
				return 1;
			}
			const size_t commitBytes = t == 2 ? prefix : bytes;
			if (!VirtualAlloc(base, commitBytes, MEM_COMMIT, PAGE_READWRITE)) {
				std::printf("commit failed at %zu MB\n", mb);
				return 1;
			}
			const size_t touchBytes = t == 0 ? bytes : prefix;
			for (size_t o = 0; o < touchBytes; o += kPage) base[o] = 1;
			ULONG_PTR count = addrs.size();
			DWORD gran      = 0;
			GetWriteWatch(WRITE_WATCH_FLAG_RESET, base, bytes, addrs.data(), &count, &gran);
			const size_t prefixPages = prefix / kPage;
			const size_t stride      = prefixPages / dirty;
			std::vector<double> ms;
			std::vector<double> rep;
			for (int r = 0; r < reps; ++r) {
				for (size_t i = 0; i < dirty; ++i) base[(i * stride) * kPage + (r & 63) * 8] = uint8_t(r);
				count           = addrs.size();
				const double t0 = msNow();
				GetWriteWatch(WRITE_WATCH_FLAG_RESET, base, bytes, addrs.data(), &count, &gran);
				ms.push_back(msNow() - t0);
				rep.push_back(double(count));
			}
			std::printf("%s,%zu,%zu,%d,%.4f,%.4f,%.0f\n", tails[t], mb, dirty, reps, pctOf(ms, 50), pctOf(ms, 95),
			    pctOf(rep, 50));
			std::fflush(stdout);
			pts.push_back({ t, double(mb), pctOf(ms, 50) });
			VirtualFree(base, 0, MEM_RELEASE);
		}
	}
	// fit per tail state over its points plus the shared 32 MB touched point
	std::printf("fit,tail,a_ms_per_call,b_us_per_mb,points\n");
	for (int t = 0; t < 3; ++t) {
		double sx = 0, sy = 0, sxx = 0, sxy = 0;
		int n = 0;
		for (const Pt& p : pts) {
			if (p.tail != t && !(p.tail == 0 && p.mb == double(prefixMb))) continue;
			sx += p.mb;
			sy += p.ms;
			sxx += p.mb * p.mb;
			sxy += p.mb * p.ms;
			++n;
		}
		const double den = double(n) * sxx - sx * sx;
		const double b   = den != 0.0 ? (double(n) * sxy - sx * sy) / den : 0.0;
		const double a   = n ? (sy - b * sx) / double(n) : 0.0;
		std::printf("fit,%s,%.4f,%.3f,%d\n", tails[t], a, b * 1000.0, n);
	}
	BusySample b1;
	b1.take();
	std::printf("busy_pct_over_bench,%.1f\n", b1.busySince(b0));
	return 0;
}

int main(int argc, char** argv)
{
	if (argc > 1 && !std::strcmp(argv[1], "threads")) return threadBench();
	if (argc > 1 && !std::strcmp(argv[1], "commit")) return commitBench();
	if (argc > 1 && !std::strcmp(argv[1], "layout")) return layoutBench();
	return gridBench();
}
