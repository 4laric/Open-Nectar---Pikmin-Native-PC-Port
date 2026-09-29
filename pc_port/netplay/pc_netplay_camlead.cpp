// Netplay M5c lane A (issue #887): the instant ("lead") camera, engine side.
// Design and maths: pc_netplay_camlead_core.h. Contract: pc_netplay_camlead.h.
//
// Per presented frame (det single view, presentation pass):
//   1. Snapshot the local captain's sim PcamCamera and its Camera (bytes).
//   2. Replay PcamCamera::controlPad + update over the local inputs the sim
//      camera has not consumed yet (the Kontroller's current input, then the
//      submitted frames past the current one), camera sounds muted.
//   3. Read the control-derived targets (polar vector, target watchpoint,
//      goal fov), restore the snapshot, read the same from the sim camera.
//   4. Step the correction (one tick of the camera's own homing filters on
//      the difference) when the sim camera updated this tick; hold it
//      otherwise (pause, overlays).
//   5. While the correction is non-zero, build the lead Camera: the posture
//      the sim camera shows plus the correction, through the same
//      NCamera::makeMatrix/makeCamera, into a separate Camera object.
// The sim never sees the lead camera: the sim objects are restored bytes,
// the lead Camera is referenced only by gfx.mCamera during the presentation
// pass (put back to the sim camera at its end) and by the local yaw sampler.

#include "netplay/pc_netplay_camlead.h"
#include "netplay/pc_netplay_camlead_core.h"

#include "Camera.h"
#include "Controller.h"
#include "Dolphin/pad.h"
#include "Graphics.h"
#include "Pcam/Camera.h"
#include "Pcam/CameraManager.h"
#include "Pcam/CameraParameters.h"
#include "gl/pc_gfx.h"
#include "netplay/pc_input_log.h"
#include "pc_window.h"
#include "settings/pc_settings.h"
#include "system.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

static_assert(pc_netplay_camlead::kKeyX == KBBTN_X, "KBBTN_X");
static_assert(pc_netplay_camlead::kKeyZ == KBBTN_Z, "KBBTN_Z");
static_assert(pc_netplay_camlead::kKeyL == KBBTN_L, "KBBTN_L");
static_assert(pc_netplay_camlead::kKeyR == KBBTN_R, "KBBTN_R");
static_assert(pc_netplay_camlead::kKeyStart == KBBTN_START, "KBBTN_START");
static_assert(pc_netplay_camlead::kPadZ == PAD_TRIGGER_Z, "PAD_TRIGGER_Z");
static_assert(pc_netplay_camlead::kPadR == PAD_TRIGGER_R, "PAD_TRIGGER_R");
static_assert(pc_netplay_camlead::kPadL == PAD_TRIGGER_L, "PAD_TRIGGER_L");
static_assert(pc_netplay_camlead::kPadX == PAD_BUTTON_X, "PAD_BUTTON_X");
static_assert(pc_netplay_camlead::kPadStart == PAD_BUTTON_START, "PAD_BUTTON_START");

namespace {
using namespace pc_netplay_camlead;

bool sArmed      = false; // session running with the lead on
bool sSession    = false; // session running (lead on or off): trace/stats
int sRole        = 0;     // local pad / captain (0 host, 1 joiner)
bool sTrace      = false; // PIKMIN_NETPLAY_CAMERA_TRACE=1
uint64_t sFrame  = 0;     // GekkoNet frame of the tick being run
bool sFrameValid = false;
InputHistory sHist;
Correction sCorr;
PcamCamera* sCorrCam = nullptr; // the sim camera the correction belongs to
bool sCorrSnapped    = false;   // that camera was snapped since the last view
bool sPredicting     = false;

// The lead camera. Static storage (never the game heaps), referenced only by
// the presentation pass and the local yaw sampler.
Camera sLeadCam;
bool sLeadValid      = false;   // the last presented frame used sLeadCam
Camera* sLeadSimCam  = nullptr; // the sim camera it replaced
Camera* sViewSimCam  = nullptr; // the sim camera offered this frame (trace)
bool sViewCalled     = false;   // a det single view was presented this tick

// Posture each sim camera showed after its last update (before vibration).
struct Posture {
	PcamCameraManager* mgr = nullptr;
	uint64_t frame         = 0;
	bool valid             = false;
	NVector3f view;
	NVector3f watch;
};
Posture sPost[2];

// Byte snapshots of the sim camera (static: no heap traffic).
alignas(PcamCamera) unsigned char sSavePcam[sizeof(PcamCamera)];
alignas(Camera) unsigned char sSaveCam[sizeof(Camera)];

// Per-frame trace values (filled by view, printed at end of presentation).
struct TraceRow {
	int steps       = -1;  // prediction steps run (-1: no prediction)
	bool startStop  = false;
	bool simUpdated = false;
	float simDist = 0.0f, viewDist = 0.0f;
	float simPitch = 0.0f, viewPitch = 0.0f;
	float simFov = 0.0f, viewFov = 0.0f;
};
TraceRow sRow;

// Diagnostics: PIKMIN_NETPLAY_CAMERA_SHOT=<dir>:<f1>,<f2>,...
std::string sShotDir;
std::vector<uint64_t> sShotFrames;
// Test hook: PIKMIN_NETPLAY_TEST_CAMERA_DRAG=<frame>:<amount>,...
struct TestDrag {
	uint64_t frame;
	float amount;
};
std::vector<TestDrag> sTestDrags;

struct Stats {
	uint64_t views       = 0; // det single-view presentations
	uint64_t leadFrames  = 0; // of which rendered the lead camera
	uint64_t predictions = 0; // prediction runs
	uint64_t steps       = 0; // prediction steps in total
	int maxSteps         = 0;
	uint64_t startStops  = 0; // windows cut at a pending Start press
	uint64_t held        = 0; // frames the sim camera did not update (hold)
	uint64_t snaps       = 0; // corrections dropped for a sim camera snap
	uint64_t drops       = 0; // corrections dropped (no view, first person...)
	uint64_t simSawLead  = 0; // ticks that began with gfx.mCamera == lead (must be 0)
	float maxCorr        = 0.0f;
};
Stats sStats;

int slot_of(PcamCameraManager* mgr)
{
	if (mgr == nullptr) return -1;
	if (mgr == cameraMgrP1) return 0;
	if (mgr == cameraMgrP2) return 1;
	return -1;
}

void drop_correction()
{
	if (!sCorr.zero()) ++sStats.drops;
	sCorr.reset();
	sCorrCam = nullptr;
}

float dist3(const NVector3f& a, const NVector3f& b)
{
	const float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
	return std::sqrt(dx * dx + dy * dy + dz * dz);
}

// Camera pitch in degrees (positive: looking down on the watchpoint).
float pitch_deg(const NVector3f& view, const NVector3f& watch)
{
	const float d = dist3(view, watch);
	if (d <= 0.0f) return 0.0f;
	return std::asin((view.y - watch.y) / d) * 57.29578f;
}

uint16_t yaw_of(const Camera* cam)
{
	if (cam == nullptr) return 0;
	return pc_input_log_yaw_quantise(std::atan2(cam->mViewXAxis.z, cam->mViewXAxis.x));
}

void parse_shots()
{
	sShotDir.clear();
	sShotFrames.clear();
	const char* v = std::getenv("PIKMIN_NETPLAY_CAMERA_SHOT");
	if (v == nullptr || *v == '\0') return;
	const std::string s(v);
	// The directory may itself contain ':' (a drive letter), so split at
	// the last one.
	const std::string::size_type colon = s.rfind(':');
	if (colon == std::string::npos || colon == 0) return;
	sShotDir = s.substr(0, colon);
	std::string list = s.substr(colon + 1);
	size_t pos = 0;
	while (pos < list.size()) {
		size_t end = list.find(',', pos);
		if (end == std::string::npos) end = list.size();
		const std::string item = list.substr(pos, end - pos);
		if (!item.empty()) sShotFrames.push_back(std::strtoull(item.c_str(), nullptr, 10));
		pos = end + 1;
	}
}

void parse_test_drags()
{
	sTestDrags.clear();
	const char* v = std::getenv("PIKMIN_NETPLAY_TEST_CAMERA_DRAG");
	if (v == nullptr || *v == '\0') return;
	const std::string s(v);
	size_t pos = 0;
	while (pos < s.size()) {
		size_t end = s.find(',', pos);
		if (end == std::string::npos) end = s.size();
		const std::string item = s.substr(pos, end - pos);
		const std::string::size_type colon = item.find(':');
		if (colon != std::string::npos) {
			TestDrag d;
			d.frame  = std::strtoull(item.substr(0, colon).c_str(), nullptr, 10);
			d.amount = (float)std::atof(item.substr(colon + 1).c_str());
			sTestDrags.push_back(d);
		}
		pos = end + 1;
	}
}

// Steps 2-3 of the header: returns false when nothing could be predicted.
bool predict(PcamCameraManager* mgr, PcamCamera* pc, float dTgt[3], float dPv[3], float* dFov)
{
	Camera* cam      = pc->mCamera;
	Controller* ctrl = mgr->mController;
	Step cur;
	cur.keys      = ctrl->mCurrentInput;
	cur.pressed   = ctrl->mInputPressed;
	cur.triggerL  = ctrl->mTriggerL;
	cur.stickX    = ctrl->mMainStickX;
	cur.substickY = ctrl->mSubStickY;
	Step steps[kMaxSteps];
	bool startStop = false;
	const int n    = build_steps(sHist, sFrame, cur, ctrl->mIsControllerFrozen, steps, kMaxSteps, &startStop);
	sRow.steps     = n;
	sRow.startStop = startStop;
	if (startStop) ++sStats.startStops;
	++sStats.predictions;
	sStats.steps += (uint64_t)n;
	if (n > sStats.maxSteps) sStats.maxSteps = n;

	std::memcpy(sSavePcam, static_cast<const void*>(pc), sizeof(PcamCamera));
	std::memcpy(sSaveCam, static_cast<const void*>(cam), sizeof(Camera));
	sPredicting = true;
	for (int i = 0; i < n; ++i) {
		pc->controlPad(steps[i].keys, steps[i].pressed, steps[i].triggerL, steps[i].stickX, steps[i].substickY);
		pc->update();
	}
	NVector3f pvPred, tgtPred;
	pc->mPolarDir.output(pvPred);
	pc->outputTargetWatchpoint(tgtPred);
	const f32 fovPred = pc->getCurrentFov();
	sPredicting       = false;
	std::memcpy(static_cast<void*>(pc), sSavePcam, sizeof(PcamCamera));
	std::memcpy(static_cast<void*>(cam), sSaveCam, sizeof(Camera));

	NVector3f pvSim, tgtSim;
	pc->mPolarDir.output(pvSim);
	pc->outputTargetWatchpoint(tgtSim);
	const f32 fovSim = pc->getCurrentFov();
	dPv[0]           = pvPred.x - pvSim.x;
	dPv[1]           = pvPred.y - pvSim.y;
	dPv[2]           = pvPred.z - pvSim.z;
	dTgt[0]          = tgtPred.x - tgtSim.x;
	dTgt[1]          = tgtPred.y - tgtSim.y;
	dTgt[2]          = tgtPred.z - tgtSim.z;
	*dFov            = fovPred - fovSim;
	return true;
}

// Step 5 of the header: the lead Camera from the shown sim posture plus the
// correction, through the PcamCamera's own NCamera matrix code pointed at
// sLeadCam (restored bytes afterwards).
void build_lead(PcamCamera* pc, const Posture& post)
{
	Camera* cam = pc->mCamera;
	sLeadCam    = *cam; // plane pointers are rebuilt by the caller's update()
	NVector3f view(post.view.x + sCorr.dv[0], post.view.y + sCorr.dv[1], post.view.z + sCorr.dv[2]);
	NVector3f watch(post.watch.x + sCorr.dw[0], post.watch.y + sCorr.dw[1], post.watch.z + sCorr.dw[2]);
	std::memcpy(sSavePcam, static_cast<const void*>(pc), sizeof(PcamCamera));
	pc->mCamera = &sLeadCam;
	pc->inputViewpoint(view);
	pc->inputWatchpoint(watch);
	pc->makeMatrix();
	pc->makeCamera();
	std::memcpy(static_cast<void*>(pc), sSavePcam, sizeof(PcamCamera));
	sLeadCam.mFov = cam->mFov + sCorr.df;
	sRow.viewDist  = dist3(view, watch);
	sRow.viewPitch = pitch_deg(view, watch);
	sRow.viewFov   = sLeadCam.mFov;
}
} // namespace

// ---- Session side ----

void pc_netplay_camlead_session_begin(int localRole)
{
	sSession    = true;
	sRole       = (localRole == 1) ? 1 : 0;
	sArmed      = env_lead_enabled(std::getenv("PIKMIN_NETPLAY_CAMERA_LEAD"));
	const char* t = std::getenv("PIKMIN_NETPLAY_CAMERA_TRACE");
	sTrace      = (t != nullptr && t[0] == '1' && t[1] == '\0');
	sFrameValid = false;
	sHist.clear();
	sCorr.reset();
	sCorrCam     = nullptr;
	sCorrSnapped = false;
	sLeadValid   = false;
	sLeadSimCam  = nullptr;
	sPost[0]     = Posture();
	sPost[1]     = Posture();
	sStats       = Stats();
	parse_shots();
	parse_test_drags();
	std::printf("[netplay] camera lead: %s (local captain P%d%s)\n",
	            sArmed ? "on" : "off, PIKMIN_NETPLAY_CAMERA_LEAD=0", sRole + 1, sTrace ? ", trace on" : "");
	std::fflush(stdout);
}

void pc_netplay_camlead_session_end(void)
{
	if (!sSession) return;
	std::printf("[netplay] camera lead summary: %s views=%llu lead_frames=%llu predictions=%llu steps=%llu "
	            "max_steps=%d start_cuts=%llu held=%llu snaps=%llu drops=%llu max_corr=%.2f sim_saw_lead=%llu\n",
	            sArmed ? "on" : "off", (unsigned long long)sStats.views, (unsigned long long)sStats.leadFrames,
	            (unsigned long long)sStats.predictions, (unsigned long long)sStats.steps, sStats.maxSteps,
	            (unsigned long long)sStats.startStops, (unsigned long long)sStats.held,
	            (unsigned long long)sStats.snaps, (unsigned long long)sStats.drops, sStats.maxCorr,
	            (unsigned long long)sStats.simSawLead);
	std::fflush(stdout);
	sSession    = false;
	sArmed      = false;
	sFrameValid = false;
	sLeadValid  = false;
	sLeadSimCam = nullptr;
	sCorr.reset();
	sCorrCam = nullptr;
}

void pc_netplay_camlead_note_local_input(uint64_t frame, const PcNetplayInput& in)
{
	if (!sArmed) return;
	sHist.note(frame, in);
}

void pc_netplay_camlead_begin_frame(uint64_t frame)
{
	if (!sSession) return;
	sFrame      = frame;
	sFrameValid = true;
	sViewCalled = false;
	sRow        = TraceRow();
	for (const TestDrag& d : sTestDrags) {
		if (d.frame == frame) {
			// Test-only: the mouse free-camera accumulator, which the window
			// code files under P1 whatever this peer's role.
			pc_window_add_camera_drag(d.amount);
			std::printf("[netplay] test camera drag: frame=%llu amount=%.3f\n", (unsigned long long)frame, d.amount);
			std::fflush(stdout);
		}
	}
	// Evidence for "the sim never reads the lead camera": the tick's sim
	// (update and the authoritative pass) starts from gfx.mCamera, which
	// end_presentation put back to the sim camera.
	if (gsys != nullptr && gsys->mDGXGfx != nullptr && gsys->mDGXGfx->mCamera == &sLeadCam) {
		++sStats.simSawLead;
	}
}

bool pc_netplay_camlead_armed(void)
{
	return sArmed;
}

// ---- Engine side ----

bool pc_netplay_camlead_predicting(void)
{
	return sPredicting;
}

int pc_netplay_camlead_drag_owner(void)
{
	return sArmed ? sRole : -1;
}

void pc_netplay_camlead_note_snap(PcamCamera* cam)
{
	if (!sArmed || cam == nullptr) return;
	if (cam == sCorrCam) sCorrSnapped = true;
}

void pc_netplay_camlead_note_sim_update(PcamCameraManager* mgr)
{
	// Recorded with the lead off too (read-only), so the opt-out trace
	// reports the same posture columns.
	if (!sSession || !sFrameValid || sPredicting) return;
	const int s = slot_of(mgr);
	if (s < 0 || mgr->mCamera == nullptr) return;
	Posture& p = sPost[s];
	p.mgr      = mgr;
	p.frame    = sFrame;
	p.valid    = true;
	p.view.input(mgr->mCamera->getViewpoint());
	p.watch.input(mgr->mCamera->getWatchpoint());
}

Camera* pc_netplay_camlead_view(int localPlayer, Camera* simView)
{
	if (!sSession) return simView;
	sViewCalled = true;
	sLeadValid  = false;
	sLeadSimCam = nullptr;
	sViewSimCam = simView;
	++sStats.views;
	PcamCameraManager* mgr = (localPlayer == 1) ? cameraMgrP2 : cameraMgrP1;
	PcamCamera* pc         = (mgr != nullptr) ? mgr->mCamera : nullptr;
	const int s            = slot_of(mgr);
	const Posture& post    = sPost[s < 0 ? 0 : s];
	const bool postOk      = s >= 0 && post.valid && post.mgr == mgr;
	if (postOk) {
		sRow.simDist  = dist3(post.view, post.watch);
		sRow.simPitch = pitch_deg(post.view, post.watch);
	}
	sRow.simFov    = (pc != nullptr && pc->mCamera != nullptr) ? pc->mCamera->mFov : simView->mFov;
	sRow.viewDist  = sRow.simDist;
	sRow.viewPitch = sRow.simPitch;
	sRow.viewFov   = sRow.simFov;
	if (!sArmed || !sFrameValid) return simView;
	// The lead replays this peer's own inputs, so it only applies to this
	// peer's own captain (PIKMIN_NETPLAY_LOCAL_PLAYER can show the other).
	if (localPlayer != sRole || pc == nullptr || pc->mCamera == nullptr || mgr->mController == nullptr
	    || pc->mTargetCreature == nullptr || !pc->mIsActive || pc_first_person_active()) {
		drop_correction();
		return simView;
	}
	if (pc != sCorrCam) {
		sCorr.reset();
		sCorrCam     = pc;
		sCorrSnapped = false;
	}
	if (sCorrSnapped) {
		// makeCurrentPosition moved the sim camera outright (movie end, stage
		// start, demo events): the old difference no longer applies.
		if (!sCorr.zero()) ++sStats.snaps;
		sCorr.reset();
		sCorrSnapped = false;
	}
	const bool simUpdated = postOk && post.frame == sFrame;
	sRow.simUpdated       = simUpdated;
	if (simUpdated) {
		float dTgt[3], dPv[3], dFov = 0.0f;
		if (predict(mgr, pc, dTgt, dPv, &dFov)) {
			sCorr.step(dTgt, dPv, dFov, pc->getCurrentHomingSpeed(), pc->getParameterF(PCAMF_FovHomingSpeed));
		}
	} else {
		// The sim camera did not update this tick (paused, an overlay, a
		// frozen section): hold the correction as it is.
		++sStats.held;
	}
	if (sCorr.zero() || !postOk) return simView;
	const float mag = sCorr.magnitude();
	if (mag > sStats.maxCorr) sStats.maxCorr = mag;
	build_lead(pc, post);
	sLeadValid  = true;
	sLeadSimCam = simView;
	++sStats.leadFrames;
	return &sLeadCam;
}

void pc_netplay_camlead_end_presentation(Graphics& gfx)
{
	if (!sSession) return;
	if (sLeadValid && gfx.mCamera == &sLeadCam && sLeadSimCam != nullptr) {
		// Leave exactly what the frame would have left without the lead.
		gfx.setCamera(sLeadSimCam);
	}
	if (!sViewCalled) {
		// No gameplay view this tick (movie, cutscene, results, menus that
		// replace the view): the lead restarts from the sim camera later.
		drop_correction();
		sLeadValid = false;
		return;
	}
	if (sTrace) {
		// The presented camera: gfx.mCamera no longer says (the 2D effect
		// pass installs its own camera after the world is drawn).
		const Camera* simCam  = sViewSimCam;
		const Camera* viewCam = sLeadValid ? &sLeadCam : sViewSimCam;
		std::printf("[netplay] camlead f=%llu lead=%d upd=%d steps=%d%s sim_yaw=%u view_yaw=%u sim_dist=%.2f "
		            "view_dist=%.2f sim_pitch=%.3f view_pitch=%.3f sim_fov=%.3f view_fov=%.3f corr=%.3f\n",
		            (unsigned long long)sFrame, sLeadValid ? 1 : 0, sRow.simUpdated ? 1 : 0, sRow.steps,
		            sRow.startStop ? " start_cut" : "", yaw_of(simCam), yaw_of(viewCam), sRow.simDist, sRow.viewDist,
		            sRow.simPitch, sRow.viewPitch, sRow.simFov, sRow.viewFov, sCorr.magnitude());
		std::fflush(stdout);
	}
	if (!sShotDir.empty()) {
		for (uint64_t f : sShotFrames) {
			if (f == sFrame) {
				char path[1024];
				std::snprintf(path, sizeof(path), "%s/%s-f%llu-%s.bmp", sShotDir.c_str(), sRole == 0 ? "host" : "join",
				              (unsigned long long)sFrame, sArmed ? "lead" : "optout");
				pc_gfx_request_frame_shot(path);
				break;
			}
		}
	}
}

Camera* pc_netplay_camlead_control_camera(int pad, Camera* cam)
{
	if (!sArmed || !sLeadValid || pad != sRole) return cam;
	return &sLeadCam;
}
