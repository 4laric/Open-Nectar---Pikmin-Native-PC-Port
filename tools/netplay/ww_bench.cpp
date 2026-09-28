// Netplay M6a spike (issue #896): GetWriteWatch cost versus committed size
// and dirty-page count, outside the game. Standalone; build by hand:
//   g++ -O2 -o ww_bench.exe tools/netplay/ww_bench.cpp
// Prints one line per (committed MB, dirty pages) cell: the median over 200
// reps of GetWriteWatch(RESET) and of the first-write cost of dirtying the
// pages again (the write-watch "fault tax"), plus the same first-write cost
// on a region without MEM_WRITE_WATCH for comparison.

#include <windows.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
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

int main()
{
	const size_t kPage = 4096;
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
