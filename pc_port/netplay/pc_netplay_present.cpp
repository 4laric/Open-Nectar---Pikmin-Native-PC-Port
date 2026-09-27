// Netplay M2b two-pass present module (issue #879). See header for contract.
//
// Engine-free core (skip flag, null-GX counters, local player) lives here
// without engine includes so host tests can link it. Engine helpers
// (SimCamera + pass begin/end + shape-pointer save/restore) are guarded by
// PIKI_PC_PORT.

#include "netplay/pc_netplay_present.h"

#include <cstdlib>
#include <cstring>

#if defined(PIKI_PC_PORT) && !defined(PC_NETPLAY_PRESENT_HOST)
#include "Camera.h"
#include "Graphics.h"
#include "Matrix4f.h"
#include "netplay/pc_netplay_det.h"
#include <unordered_map>
#endif

namespace {
bool sSkipPresentation = false;
bool sNullGx = false;
unsigned long long sNullReal = 0;
unsigned long long sNullAttempted = 0;
unsigned long long sSavedShapes = 0;
int sLocalPlayerCached = 0;
bool sLocalPlayerInit = false;
#if defined(PIKI_PC_PORT) && !defined(PC_NETPLAY_PRESENT_HOST)
Camera* sSimCamera = nullptr;
Graphics* sSavedGfx = nullptr;
Camera* sSavedCamera = nullptr;
std::unordered_map<void*, void*> sSavedPtrs;
#endif
} // namespace

extern "C" {

void pc_netplay_present_set_skip_presentation(int skip)
{
	sSkipPresentation = skip != 0;
}

int pc_netplay_present_skip_presentation(void)
{
	return sSkipPresentation ? 1 : 0;
}

int pc_netplay_present_two_pass_active(void)
{
#if defined(PIKI_PC_PORT) && !defined(PC_NETPLAY_PRESENT_HOST)
	if (!pc_netplay_deterministic()) {
		return 0;
	}
	return 1;
#else
	return 0;
#endif
}

void pc_netplay_present_set_null_gx(int on)
{
	sNullGx = on != 0;
}

int pc_netplay_present_null_active(void)
{
	return sNullGx ? 1 : 0;
}

unsigned long long pc_netplay_present_null_gl_calls(void)
{
	return sNullReal;
}

unsigned long long pc_netplay_present_null_attempted(void)
{
	return sNullAttempted;
}

void pc_netplay_present_note_attempt(void)
{
	++sNullAttempted;
}

void pc_netplay_present_note_real(void)
{
	++sNullReal;
}

void pc_netplay_present_reset_counters(void)
{
	sNullReal = 0;
	sNullAttempted = 0;
	sSavedShapes = 0;
}

int pc_netplay_present_local_player(void)
{
	if (!sLocalPlayerInit) {
		sLocalPlayerInit = true;
		const char* v = std::getenv("PIKMIN_NETPLAY_LOCAL_PLAYER");
		sLocalPlayerCached = (v && v[0] == '1') ? 1 : 0;
	}
	return sLocalPlayerCached;
}

unsigned long long pc_netplay_present_saved_shapes(void)
{
	return sSavedShapes;
}

} // extern "C"

#if defined(PIKI_PC_PORT) && !defined(PC_NETPLAY_PRESENT_HOST) && defined(__cplusplus)
Camera* pc_netplay_present_sim_camera(void)
{
	if (!sSimCamera) {
		sSimCamera = new Camera();
	}
	return sSimCamera;
}

void pc_netplay_present_begin_authoritative(Graphics& gfx)
{
	// Route pose math through identity: lookAt * world * joint becomes
	// world-space. Keep a valid fixed projection by copying the live
	// camera's projection state at pass start (no sim reader depends on
	// the aspect after M2a; submission is null-op'd anyway).
	Camera* sim = pc_netplay_present_sim_camera();
	if (gfx.mCamera) {
		sSavedGfx = &gfx;
		sSavedCamera = gfx.mCamera;
		sim->mPerspectiveMatrix = gfx.mCamera->mPerspectiveMatrix;
		sim->mProjectionMatrix = gfx.mCamera->mProjectionMatrix;
		sim->mFov = gfx.mCamera->mFov;
		sim->mNear = gfx.mCamera->mNear;
		sim->mFar = gfx.mCamera->mFar;
		sim->mAspectRatio = 16.0f / 9.0f;
		gfx.mCamera = sim;
	}
	sim->mLookAtMtx.makeIdentity();
	sim->mInverseLookAtMtx.makeIdentity();
	pc_netplay_present_set_null_gx(1);
}

void pc_netplay_present_end_authoritative(Graphics& gfx)
{
	pc_netplay_present_set_null_gx(0);
	if (sSavedGfx == &gfx && sSavedCamera) {
		gfx.mCamera = sSavedCamera;
	}
	sSavedGfx = nullptr;
	sSavedCamera = nullptr;
}

void pc_netplay_present_begin_presentation(Graphics& gfx)
{
	(void)gfx;
	sSavedPtrs.clear();
	sSavedShapes = 0;
	// Presentation matrix pool is reset in Graphics::resetPresentBuffer by
	// the driver (needs gfx). Kept here for symmetry; actual reset below.
}

void pc_netplay_present_end_presentation(Graphics& gfx)
{
	(void)gfx;
	// Actual pointer restore runs in shapeBase.cpp
	// (pc_netplay_present_restore_all_shapes) so it can write
	// BaseShape::mAnimMatrices directly. The sim pool was never touched
	// (presentation allocates from the separate present pool), so restoring
	// pointers restores the exact authoritative matrix state.
}

bool pc_netplay_present_save_shape_ptr(void* shape, void* savedPtr)
{
	if (!shape) {
		return false;
	}
	auto it = sSavedPtrs.find(shape);
	if (it != sSavedPtrs.end()) {
		return false;
	}
	sSavedPtrs.emplace(shape, savedPtr);
	++sSavedShapes;
	return true;
}

size_t pc_netplay_present_saved_count(void)
{
	return sSavedPtrs.size();
}

void* pc_netplay_present_saved_shape_at(size_t i, void** outSaved)
{
	size_t k = 0;
	for (auto& kv : sSavedPtrs) {
		if (k == i) {
			if (outSaved) {
				*outSaved = kv.second;
			}
			return kv.first;
		}
		++k;
	}
	if (outSaved) {
		*outSaved = nullptr;
	}
	return nullptr;
}

void pc_netplay_present_clear_saved(void)
{
	sSavedPtrs.clear();
}
#endif
