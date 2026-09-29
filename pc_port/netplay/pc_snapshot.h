#pragma once

// Netplay M6b production snapshot/restore for rollback (issue #896).
//
// Compiled only with the CMake option PIKMIN_NETPLAY_SNAPSHOT=ON (which
// requires PIKMIN_NETPLAY_BUILD, Windows, not Android, and defines
// PIKMIN_NETPLAY_SNAPSHOT=1 for exactly the engine TUs that carry hooks).
// Active only at runtime with PIKMIN_NETPLAY_SNAPSHOT=1; with the option on
// and the variable unset every hook returns at once and the game runs as it
// does without the option (hash-identical; M1 neutrality).
//
// When active:
//   * a fixed-VA write-watched region holds the sim heap (pc_snapshot_region):
//     main-thread operator new goes there only inside a SIM scope (the
//     authoritative pass, parseMessages, soft-reset idles and the boot heap
//     setup); everything else stays on malloc and is reported by call site;
//   * the simulated OS arena and the ovl/app AyuHeap backings live in the
//     region, nested as on the console;
//   * every tick end saves the frame into a single-copy page ring
//     (pc_snapshot_ring) plus a compare-and-undo globals log;
//   * pc_snapshot_restore(T) rolls the region and the globals back to the end
//     of frame T through the same path GekkoNet's load event will use;
//   * PIKMIN_NETPLAY_SNAPSHOT_MEASURE=1 turns on the controlled measurement
//     protocol (M6b item 0) and the per-tick CSV (snapshot.csv).

#include <cstddef>
#include <cstdint>

// Called once from main(), after pc_netplay_det_init. Reads the env switch.
void pc_snapshot_init(void);
bool pc_snapshot_active(void);

// operator new/delete hooks (sysNew.cpp). new returns nullptr when the block
// stays on malloc; delete returns true when ptr was a region block.
void* pc_snapshot_new(size_t size, size_t align, void* returnAddress);
bool pc_snapshot_delete(void* ptr, size_t align, void* returnAddress);
// True when ptr lies inside the region (free/realloc hooks abort on it).
bool pc_snapshot_owns(const void* ptr);

// SIM scope (opt-in routing). Nested scopes count; an infra scope inside a
// SIM scope sends allocations back to malloc until it closes.
enum PcSnapshotSim {
	kPcSnapSimAuth  = 1, // authoritative pass (idle begin .. auth end)
	kPcSnapSimParse = 2, // parseMessages
	kPcSnapSimBoot  = 3, // PlugPikiApp construction: AyuHeaps, gameflow
};
void pc_snapshot_sim_push(int where);
void pc_snapshot_sim_pop(void);
void pc_snapshot_infra_push(void);
void pc_snapshot_infra_pop(void);

// Simulated OS arena inside the region. False when inactive.
bool pc_snapshot_arena(void** lo, size_t* bytes);

// Frame markers (System::run / PlugPikiApp::idle).
void pc_snapshot_loop_top(void);    // top of a System::run loop iteration
void pc_snapshot_frame_begin(void); // a logical tick is about to run
void pc_snapshot_idle_begin(void);  // right before app->idle(): opens SIM
void pc_snapshot_auth_end(void);    // authoritative pass finished: closes SIM
void pc_snapshot_idle_end(void);    // right after app->idle()
void pc_snapshot_tick_end(void);    // after the state hash: save the frame

// Barrier: the next save re-baselines the ring and no restore may target a
// frame before it. `reason` is a static string (logged).
void pc_snapshot_barrier(const char* reason);

// Restore region + globals to the end of frame `tick` (a pc_state_hash
// tick). Aborts when the frame is not in the ring or precedes a barrier.
// Returns the restore time in ms.
double pc_snapshot_restore(uint64_t tick);
bool pc_snapshot_has(uint64_t tick);

// Byte range of the exe's globals a restore must never roll back
// (infrastructure statics such as the malloc tracking table).
void pc_snapshot_preserve(const void* p, size_t bytes);

// True while the in-process synctest re-advances ticks after a restore.
bool pc_snapshot_resimulating(void);
