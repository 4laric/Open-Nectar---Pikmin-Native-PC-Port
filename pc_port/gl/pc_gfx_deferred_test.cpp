// Host test for the polish deferred texture-init contract (issue #880 item 6).
//
// The real upload needs a GL context, so this tests the bookkeeping rules the
// pc_gfx.cpp implementation follows (mirrored here without GL):
//   - an authoritative-pass init retains its parameters for lazy presentation
//     upload;
//   - a re-init of an already-cached key during auth invalidates the stale
//     cache entry (GL name deferred, not drawn stale);
//   - rgba, non-CI and CI defers are mutually exclusive per key (M2: a stale
//     CI entry never shadows a newer non-CI deferred init);
//   - a same-signature auth re-init over a live entry is a no-op (m4: no churn);
//   - release clears every per-key record, even with no live entry (m4: no leak);
//   - a presentation upload clears any stale deferred entry for the key (M2);
//   - first presentation use takes (and clears) the deferred entry.
//
// NOTE (review m3, accepted gap): this is still a contract model, not the
// real code: pc_gfx.cpp's maps are file-static and its upload needs GL, so
// the test cannot link it. The model mirrors null_auth_invalidate() and the
// three auth paths rule for rule; runtime coverage is the null_gl=0 +
// no-missing-texture logs + frame dumps in the handoff.
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
// Mirrors the M2 mutual-exclusivity rules: exactly one of ci / deferred-rgba /
// deferred-tex per key; release clears all three; a same-signature re-init
// over a live cache entry is a no-op (no churn); a presentation upload clears
// any stale deferred entry for the key.
static std::map<uintptr_t, int> modelCache; // key -> fake GL id
static std::map<uintptr_t, std::vector<uint8_t>> modelDeferredRgba;
static std::map<uintptr_t, int> modelDeferredTex; // key -> fake format
static std::map<uintptr_t, int> modelCi;          // key -> fake CI format
static std::map<uintptr_t, int> modelSig;         // key -> fake signature
static std::vector<int> modelDoomed;

static void model_invalidate(uintptr_t key)
{
	auto cached = modelCache.find(key);
	if (cached != modelCache.end()) {
		modelDoomed.push_back(cached->second);
		modelCache.erase(cached);
	}
	modelSig.erase(key);
	modelCi.erase(key);
	modelDeferredRgba.erase(key);
	modelDeferredTex.erase(key);
}

static void model_auth_init_rgba(uintptr_t key, const std::vector<uint8_t>& bytes)
{
	// RGBA always retains (movie: same pointer, new picture each frame).
	model_invalidate(key);
	modelDeferredRgba[key] = bytes;
}

static void model_auth_init_tex(uintptr_t key, int format, int sig)
{
	auto s = modelSig.find(key);
	if (s != modelSig.end() && s->second == sig
	    && modelCache.find(key) != modelCache.end())
		return; // same-signature no-op: no churn
	model_invalidate(key);
	modelDeferredTex[key] = format;
	modelSig[key]         = sig;
}

static void model_auth_init_ci(uintptr_t key, int format, int sig)
{
	auto s = modelSig.find(key);
	if (s != modelSig.end() && s->second == sig
	    && modelCache.find(key) != modelCache.end())
		return;
	model_invalidate(key);
	modelCi[key]  = format;
	modelSig[key] = sig;
}

static void model_present_init(uintptr_t key, int glId, int sig)
{
	// A real upload supersedes any stale deferred entry for the key.
	modelDeferredRgba.erase(key);
	modelDeferredTex.erase(key);
	modelCache[key] = glId;
	modelSig[key]   = sig;
}

static void model_release(uintptr_t key)
{
	modelCi.erase(key);
	modelDeferredRgba.erase(key);
	modelDeferredTex.erase(key);
	auto cached = modelCache.find(key);
	if (cached != modelCache.end()) {
		modelDoomed.push_back(cached->second);
		modelCache.erase(cached);
	}
	modelSig.erase(key);
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
	modelCi.clear();
	modelSig.clear();
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
	modelSig[k1]   = 7;
	model_auth_init_tex(k1, 7, 8);
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
	modelCi.clear();
	modelSig.clear();
	modelDoomed.clear();
	const uintptr_t k2 = 0x2002;
	model_auth_init_rgba(k2, std::vector<uint8_t> { 9, 9 });
	model_auth_init_tex(k2, 3, 30);
	check(modelDeferredRgba.find(k2) == modelDeferredRgba.end(),
	      "tex init must supersede a pending rgba defer");
	check(modelDeferredTex.find(k2) != modelDeferredTex.end(),
	      "tex defer must remain after supersede");
	model_auth_init_rgba(k2, std::vector<uint8_t> { 5 });
	check(modelDeferredTex.find(k2) == modelDeferredTex.end(),
	      "rgba init must supersede a pending tex defer");

	// 4. M2: a CI auth init dooms the mutual-exclusion rivals. A key that
	// was CI once and is re-inited as non-CI in auth must not re-upload the
	// stale CI description (the M2 stale-image case).
	modelCache.clear();
	modelDeferredRgba.clear();
	modelDeferredTex.clear();
	modelCi.clear();
	modelSig.clear();
	modelDoomed.clear();
	const uintptr_t k3 = 0x3003;
	modelCache[k3] = 77;
	modelSig[k3]   = 1;
	model_auth_init_ci(k3, 5, 2);
	check(modelCi.find(k3) != modelCi.end(), "auth CI init must retain the CI description");
	check(modelDeferredRgba.find(k3) == modelDeferredRgba.end()
	      && modelDeferredTex.find(k3) == modelDeferredTex.end(),
	      "CI init must clear rival deferred entries");
	model_auth_init_tex(k3, 9, 3); // heap reuse: same address, now non-CI
	check(modelCi.find(k3) == modelCi.end(),
	      "non-CI auth re-init must erase the stale CI entry (M2)");
	check(modelDeferredTex.find(k3) != modelDeferredTex.end(),
	      "non-CI auth re-init must retain its own parameters");
	check(model_presentation_load(k3), "presentation must upload the non-CI re-init");

	// 5. m4: same-signature auth re-init over a live cache entry is a no-op.
	modelCache.clear();
	modelDeferredRgba.clear();
	modelDeferredTex.clear();
	modelCi.clear();
	modelSig.clear();
	modelDoomed.clear();
	const uintptr_t k4 = 0x4004;
	model_present_init(k4, 55, 11);
	model_auth_init_tex(k4, 9, 11);
	check(modelCache.find(k4) != modelCache.end() && modelCache[k4] == 55,
	      "same-signature auth re-init must keep the live entry (no churn)");
	check(modelDoomed.empty(), "same-signature re-init must doom nothing");
	check(modelDeferredTex.find(k4) == modelDeferredTex.end(),
	      "same-signature re-init must retain nothing");

	// 6. m4/M2: release clears every per-key record, even with no live entry.
	modelCi[k4]           = 5;
	modelDeferredRgba[k4] = std::vector<uint8_t> { 1 };
	modelDeferredTex[k4]  = 9;
	model_release(k4);
	check(modelCache.find(k4) == modelCache.end()
	      && modelCi.find(k4) == modelCi.end()
	      && modelDeferredRgba.find(k4) == modelDeferredRgba.end()
	      && modelDeferredTex.find(k4) == modelDeferredTex.end()
	      && modelSig.find(k4) == modelSig.end(),
	      "release must clear cache, CI, both defers and signature");

	// 7. M2: a presentation upload supersedes a stale deferred entry.
	const uintptr_t k5 = 0x5005;
	model_auth_init_tex(k5, 4, 40); // auth retains while presentation was away
	model_present_init(k5, 66, 41); // fresh real upload wins
	check(modelDeferredTex.find(k5) == modelDeferredTex.end(),
	      "presentation upload must clear a stale deferred entry");

	std::printf("PcGfxDeferred: %s\n", failures ? "FAILED" : "all tests passed");
	return failures ? 1 : 0;
}
