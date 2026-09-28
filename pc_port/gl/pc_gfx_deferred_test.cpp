// Host test for the polish deferred texture-init contract (issue #880 item 6).
//
// The real upload needs a GL context, so this tests the bookkeeping rules the
// pc_gfx.cpp implementation follows (mirrored here without GL):
//   - an authoritative-pass init retains its parameters for lazy presentation
//     upload;
//   - a re-init of an already-cached key during auth invalidates the stale
//     cache entry (GL name deferred, not drawn stale);
//   - rgba and non-CI defers supersede each other per key;
//   - first presentation use takes (and clears) the deferred entry.
//
// Uses the check()/failures pattern (never bare assert(): Release defines
// NDEBUG).

#include <cstdint>
#include <cstdio>
#include <map>
#include <vector>

static int failures = 0;

static void check(bool condition, const char* message)
{
	if (!condition) {
		std::fprintf(stderr, "FAIL: %s\n", message);
		++failures;
	}
}

// Minimal model of the pc_gfx.cpp maps (keys only; values are opaque here).
static std::map<uintptr_t, int> modelCache; // key -> fake GL id
static std::map<uintptr_t, std::vector<uint8_t>> modelDeferredRgba;
static std::map<uintptr_t, int> modelDeferredTex; // key -> fake format
static std::vector<int> modelDoomed;

static void model_auth_init_rgba(uintptr_t key, const std::vector<uint8_t>& bytes)
{
	auto cached = modelCache.find(key);
	if (cached != modelCache.end()) {
		modelDoomed.push_back(cached->second);
		modelCache.erase(cached);
	}
	modelDeferredRgba[key] = bytes;
	modelDeferredTex.erase(key);
}

static void model_auth_init_tex(uintptr_t key, int format)
{
	auto cached = modelCache.find(key);
	if (cached != modelCache.end()) {
		modelDoomed.push_back(cached->second);
		modelCache.erase(cached);
	}
	modelDeferredTex[key] = format;
	modelDeferredRgba.erase(key);
}

// Returns true when a presentation upload would run (deferred taken).
static bool model_presentation_load(uintptr_t key)
{
	if (modelCache.find(key) != modelCache.end()) return false; // already live
	auto dr = modelDeferredRgba.find(key);
	if (dr != modelDeferredRgba.end()) {
		check(!dr->second.empty(), "deferred rgba must retain bytes");
		modelDeferredRgba.erase(dr);
		modelDeferredTex.erase(key);
		modelCache[key] = 1000 + (int)(key & 0xFF);
		return true;
	}
	auto dt = modelDeferredTex.find(key);
	if (dt != modelDeferredTex.end()) {
		modelDeferredTex.erase(dt);
		modelDeferredRgba.erase(key);
		modelCache[key] = 2000 + (int)(key & 0xFF);
		return true;
	}
	return false;
}

int main()
{
	// 1. Auth rgba init retains bytes and lazily uploads on first use.
	modelCache.clear();
	modelDeferredRgba.clear();
	modelDeferredTex.clear();
	modelDoomed.clear();
	const uintptr_t k1 = 0x1001;
	model_auth_init_rgba(k1, std::vector<uint8_t> { 1, 2, 3, 4 });
	check(modelCache.find(k1) == modelCache.end(), "auth init must not populate cache");
	check(modelDeferredRgba.find(k1) != modelDeferredRgba.end(),
	      "auth rgba init must retain parameters");
	check(model_presentation_load(k1), "first presentation use must upload deferred rgba");
	check(modelCache.find(k1) != modelCache.end(), "upload must populate cache");
	check(modelDeferredRgba.find(k1) == modelDeferredRgba.end(),
	      "deferred rgba must clear after upload");
	check(!model_presentation_load(k1), "second use must be a plain rebind");

	// 2. Re-init of a cached key in auth invalidates the stale entry.
	modelCache[k1] = 42;
	model_auth_init_tex(k1, 7);
	check(modelCache.find(k1) == modelCache.end(), "stale cache must be erased on auth re-init");
	check(modelDoomed.size() == 1 && modelDoomed[0] == 42,
	      "stale GL name must be deferred, not leaked");
	check(modelDeferredTex.find(k1) != modelDeferredTex.end(),
	      "auth tex init must retain parameters");
	check(model_presentation_load(k1), "presentation must upload the re-init");

	// 3. Rgba and tex defers supersede each other per key.
	modelCache.clear();
	modelDeferredRgba.clear();
	modelDeferredTex.clear();
	modelDoomed.clear();
	const uintptr_t k2 = 0x2002;
	model_auth_init_rgba(k2, std::vector<uint8_t> { 9, 9 });
	model_auth_init_tex(k2, 3);
	check(modelDeferredRgba.find(k2) == modelDeferredRgba.end(),
	      "tex init must supersede a pending rgba defer");
	check(modelDeferredTex.find(k2) != modelDeferredTex.end(),
	      "tex defer must remain after supersede");
	model_auth_init_rgba(k2, std::vector<uint8_t> { 5 });
	check(modelDeferredTex.find(k2) == modelDeferredTex.end(),
	      "rgba init must supersede a pending tex defer");

	std::printf("PcGfxDeferred: %s\n", failures ? "FAILED" : "all tests passed");
	return failures ? 1 : 0;
}
