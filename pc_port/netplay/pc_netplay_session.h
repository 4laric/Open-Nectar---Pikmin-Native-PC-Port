#pragma once
// Netplay M3 delay-based lockstep session over GekkoNet (issue #880).
//
// Active only when --netplay-host / --netplay-join (or PIKMIN_NETPLAY_HOST /
// PIKMIN_NETPLAY_JOIN) is set, and only in deterministic mode (netplay
// forces det mode on, plus co-op on via the M0 pc_coop_switch API).
//
// Wiring (all opt-in; without a netplay switch every function here is inert
// and the game behaves exactly as before):
//   pc_netplay_session_notify_argv  call once from pc_main, before the game
//                                   starts (stores argv for the bootstrap
//                                   path and CLI switches)
//   pc_netplay_session_active       true once a netplay switch parsed on
//   pc_netplay_session_drive        called from System::run each loop turn;
//                                   returns true when netplay owned the turn
//                                   (handshake, tick(s), wait or shutdown).
//                                   Weak-linked: the default build has no
//                                   definition and runs the normal path.
//
// GekkoNet config (brief section 5): num_players 2, max_spectators 0,
// input_prediction_window 0 (lockstep), input_size 16, state_size 8,
// desync_detection true, check_distance 7. Local delay from
// PIKMIN_NETPLAY_DELAY (default 2). Host = player 0 (P1), joiner = P1 (P2).

class System;
class BaseApp;

// argv capture (pc_main calls this at startup; env vars are read lazily).
void pc_netplay_session_notify_argv(int argc, char** argv);

// True when a netplay switch is set (host or join), even before the session
// starts. False in every non-netplay run.
bool pc_netplay_session_active(void);

// System::run driver hook. Returns true when netplay owned this loop turn.
// Strong-defined by the netplay session TU (netplay builds only); System
// calls it through a weak reference so the default build is untouched.
bool pc_netplay_session_drive(System* sys, BaseApp* app);

// Launch lane (issue #887) self-test hook: the exact config text the
// handshake hashes (build_config_string). Valid until the next call.
const char* pc_netplay_session_config_text(void);

// N3 stage-load hook: called (weakly) from GameFlow::softReset inside the
// synchronous load. Strong-defined here in netplay builds only; null in the
// default build. Implements PIKMIN_NETPLAY_TEST_LOAD_DELAY_MS (sleep once),
// and, inside a session tick, the load guard's load window and load-targeted
// test stall (M4 gap-fix lane S, pc_netplay_loadguard.h).
void pc_netplay_on_stage_load(void);

// M4 gap-fix lane S (issue #885): keep-alive entry for long main-thread loops
// (site = pc_netplay_loadguard::Site). Called weakly from pc_gfx.cpp (TEV
// program creation) and dvd_stubs.cpp (DVDOpen/DVDRead); inert outside a
// netplay session tick. Null in the default build.
void pc_netplay_load_keepalive(int site);
