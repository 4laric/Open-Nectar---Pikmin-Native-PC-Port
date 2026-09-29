#pragma once
// Netplay M5c lane A (issue #887): the instant ("lead") camera.
//
// In a lockstep session the presentation pass renders the local captain's
// camera with the local camera controls applied on the frame they are
// sampled, instead of `delay` + 1 frames later when the synced input reaches
// the sim camera. Design and maths: pc_netplay_camlead_core.h.
//
// Contract:
//   - presentation only: the lead camera is a separate Camera object that
//     only the presentation pass renders with (and the local input sampler
//     reads its yaw from, see pc_netplay_camlead_control_camera). The sim
//     camera objects are byte-for-byte what they would be without the lead:
//     the prediction runs on a snapshot of the sim camera and restores it,
//     with the camera sounds muted, and gfx.mCamera is put back to the sim
//     camera at the end of the presentation pass;
//   - netplay only: every entry point is inert until the lockstep session
//     calls pc_netplay_camlead_session_begin, which only netplay builds do;
//   - opt-out: PIKMIN_NETPLAY_CAMERA_LEAD=0 renders the sim camera, exactly
//     as before (the yaw sampler reads the sim camera too, and the
//     free-camera drag keeps its per-slot routing).
// Diagnostics: PIKMIN_NETPLAY_CAMERA_TRACE=1 logs one `[netplay] camlead`
// line per presented frame; PIKMIN_NETPLAY_CAMERA_SHOT=<dir>:<f1>,<f2>,...
// writes the presented frame at those GekkoNet frames as BMP files;
// PIKMIN_NETPLAY_TEST_CAMERA_DRAG=<frame>:<amount>,... adds a mouse
// free-camera drag (pc_window_add_camera_drag) before those frames' ticks.

#include "netplay/pc_netplay_gekko_input.h"

#include <cstdint>

// ---- Session side (pc_netplay_session.cpp, netplay builds) ----
// Session configured: arms the lead for `localRole` (0 host/P1, 1
// joiner/P2) unless PIKMIN_NETPLAY_CAMERA_LEAD=0. Logs one line.
void pc_netplay_camlead_session_begin(int localRole);
// Session stopped: logs the summary line and disarms everything.
void pc_netplay_camlead_session_end(void);
// A local input was submitted for GekkoNet frame `frame` (submit + delay).
void pc_netplay_camlead_note_local_input(uint64_t frame, const PcNetplayInput& in);
// The Advance of GekkoNet frame `frame` is about to run its tick.
void pc_netplay_camlead_begin_frame(uint64_t frame);
// True while the session has the lead armed (env on).
bool pc_netplay_camlead_armed(void);

#if defined(PIKI_PC_PORT) && defined(__cplusplus)
class Camera;
class Graphics;
class PcamCamera;
class PcamCameraManager;

// ---- Engine side ----
// Presentation pass, det single view: `simView` is the local captain's sim
// camera, already updated for this frame's aspect. Returns the camera to
// render with: the lead camera while a correction is active, else simView.
Camera* pc_netplay_camlead_view(int localPlayer, Camera* simView);
// End of the presentation pass: puts gfx.mCamera back to the sim camera,
// logs the trace line, and drops the correction when no view was presented.
void pc_netplay_camlead_end_presentation(Graphics& gfx);
// Local control-yaw sampler (navi.cpp): the camera whose yaw the local pad
// `pad` submits. The lead camera while it is what the player sees.
Camera* pc_netplay_camlead_control_camera(int pad, Camera* cam);
// PcamCameraManager::update, right after the sim camera's own update and
// before its vibration events: records the posture the sim camera shows.
void pc_netplay_camlead_note_sim_update(PcamCameraManager* mgr);
// PcamCamera::makeCurrentPosition (every startCamera ends there): the sim
// camera was snapped, so a stale correction must not carry over.
void pc_netplay_camlead_note_snap(PcamCamera* cam);
// True while the lead runs its prediction on the sim camera: camera sounds
// are muted (they play when the sim applies the input).
bool pc_netplay_camlead_predicting(void);
// Free-camera drag routing (PcamCamera::control): the captain whose camera
// takes every local drag in a lockstep session with the lead on, else -1
// (the drag keeps its per-slot routing).
int pc_netplay_camlead_drag_owner(void);
#endif
