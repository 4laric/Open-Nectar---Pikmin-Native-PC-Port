#pragma once

// Netplay M2b two-pass frame: authoritative sim pass + local presentation pass
// (issue #879).
//
// All behaviour is opt-in: unless deterministic mode is on
// (pc_netplay_deterministic()), every helper below is inert and the game
// behaves exactly as before.
//
// Pass driver (see PlugPikiApp::idle):
//   - det mode: authoritative pass (SimCamera + null GX, sim blocks run) then
//     presentation pass (real camera + real GL, sim blocks skipped) in one
//     logical tick.
//   - non-det: single pass, exactly as today.
//   - pc_netplay_present_set_skip_presentation(true) runs authoritative only,
//     for future rollback resimulation (m3 lane hook).

#if defined(PIKI_PC_PORT) && !defined(PC_NETPLAY_PRESENT_HOST)
class Camera;
class Graphics;
class Matrix4f;
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Future rollback hook: when true, det ticks run the authoritative pass only.
void pc_netplay_present_set_skip_presentation(int skip);
int pc_netplay_present_skip_presentation(void);

// True when the two-pass frame is active this tick (det mode, engine up).
// Safe to call from engine code (returns 0 when the switch is off).
int pc_netplay_present_two_pass_active(void);

// Null-GX backend flag (authoritative pass): submission/uploads/present are
// skipped. Counts attempts (skipped calls) and real GL issued while active
// (must stay 0). Engine-free counters (host testable).
void pc_netplay_present_set_null_gx(int on);
int pc_netplay_present_null_active(void);
// GL calls issued while null was active (acceptance: 0 in auth passes).
unsigned long long pc_netplay_present_null_gl_calls(void);
// Submissions/uploads/presents skipped while null was active (diagnostic).
unsigned long long pc_netplay_present_null_attempted(void);
void pc_netplay_present_note_attempt(void);
void pc_netplay_present_note_real(void);
void pc_netplay_present_reset_counters(void);

// Local player's view: PIKMIN_NETPLAY_LOCAL_PLAYER=0|1, default 0. Cached.
int pc_netplay_present_local_player(void);

// Presentation matrix save/restore accounting (diagnostic).
unsigned long long pc_netplay_present_saved_shapes(void);

#ifdef __cplusplus
}
#endif

#if defined(PIKI_PC_PORT) && !defined(PC_NETPLAY_PRESENT_HOST) && defined(__cplusplus)
// Engine side (defined in pc_netplay_present.cpp, uses Camera/Graphics).
// SimCamera: identity lookAt/inverse (world-space pose), valid fixed
// projection copied from the real camera at pass start.
Camera* pc_netplay_present_sim_camera(void);
void pc_netplay_present_begin_authoritative(Graphics& gfx);
void pc_netplay_present_end_authoritative(Graphics& gfx);
void pc_netplay_present_begin_presentation(Graphics& gfx);
void pc_netplay_present_end_presentation(Graphics& gfx);
// Called from BaseShape::updateAnim in the presentation pass: records the
// sim pointer once per shape per presentation pass so end_presentation can
// restore it. Returns true if the shape was newly saved.
bool pc_netplay_present_save_shape_ptr(void* shape, void* savedPtr);
// Internal iteration for shapeBase.cpp restore (engine only).
size_t pc_netplay_present_saved_count(void);
void* pc_netplay_present_saved_shape_at(size_t i, void** outSaved);
void pc_netplay_present_clear_saved(void);
// Restores BaseShape::mAnimMatrices for every shape saved this presentation
// pass (defined in shapeBase.cpp).
void pc_netplay_present_restore_all_shapes(void);
#endif
