#pragma once
// Curated per-tick simulation hash for the netplay determinism harness
// (issue #878). Every later netplay milestone uses this to prove, or
// disprove, that the game is deterministic.
//
// Log format (PIKMIN_STATE_HASH_LOG=<file>): after every tick, one text line:
//   <tick> <total> <navi> <piki> <teki> <item> <world> <rng>
// <tick> is the 1-based tick count in decimal; every other column is a 64-bit
// hash printed as 16 lowercase hex digits.
//
// Exactly what is hashed (stable simulation data only, never pointers):
//   navi:  per Navi in manager order: mNaviID; position (mSRT.t xyz),
//     rotation (mSRT.r xyz), velocity (mVelocity xyz) as float bits;
//     mHealth bits; current state id (mCurrState->getID(), -1 when null).
//   piki:  per Piki in manager order: mColor, mHappa, P2 species flags
//     (purple/white/bulbmin); position, rotation, velocity bits; mHealth
//     bits; current state id (-1 when null).
//   teki:  per Teki in manager order: mTekiType; position, rotation,
//     velocity bits; mHealth bits; mStateID.
//   item:  per itemMgr object in manager order: mObjType; position bits;
//     GoalItem extras: mOnionColour and mHeldPikis[3]; then per pelletMgr
//     Pellet in order: config pellet id + pellet type (0 when no config),
//     position bits, state id, carrier count; then per bossMgr Boss in
//     order: current/next state id, current life bits, position bits.
//     Onion/container counts per colour are covered twice on purpose: once
//     via the GoalItem objects above, once via itemMgr->getContainer(c) for
//     each Piki colour (0 when that colour has no Onion).
//   world: gameflow.mWorldClock.mTimeOfDay bits and mCurrentDay, plus
//     playerState counters (living/born/dead/plucked, 0 when null).
//   rng:   pc_sim_rng_state() when the m1-det lane's weak symbol resolves,
//     else 0. (Wall-clock rand() is NOT hashed; it is expected to diverge
//     until the det lane lands.)
//   total: FNV-1a 64 over the six sub-hashes in the order above.
// A null manager (title screen, loading) contributes a sub-hash of 0.
// Mixing uses FNV-1a 64 throughout.
//
// Runtime behaviour:
//   PIKMIN_STATE_HASH_LOG=<file>: write one line per tick (buffered; the
//     buffer is flushed every 300 ticks and on exit).
//   PIKMIN_NETPLAY_EXIT_AFTER_TICKS=<n>: after tick n is hashed, flush every
//     log and exit the process cleanly with code 0.
//   PIKMIN_NETPLAY_TEST_PREROLL_RAND=<n>: before the first tick, call rand()
//     n times, and pc_sim_rand() n times when the weak symbol resolves.
//   With none of these set, pc_state_hash_tick_end() only bumps the tick
//   counter bookkeeping needed for nothing else: no files, no log lines.

#include <cstdint>

// argv capture must happen before the first tick (pc_main calls it at
// startup); env vars are read lazily on the first tick end.
void pc_state_hash_notify_argv(int argc, char** argv);

// Menu-dwell simulation: call once, before the first tick's simulation.
// Called by pc_input_log_tick(); do not call directly.
void pc_state_hash_before_first_tick(void);

// Per-tick hook: call after app->idle() returns.
void pc_state_hash_tick_end(void);

// Flush the hash log, if any.
void pc_state_hash_flush(void);

// Current 1-based tick count (0 before the first tick end).
uint64_t pc_state_hash_tick(void);
