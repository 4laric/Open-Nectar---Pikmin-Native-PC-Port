#pragma once

// Netplay M6a snapshot spike (issue #896). Measurement only: crashes are
// tolerated, nothing here is production rollback code.
//
// Compiled only with the CMake option PIKMIN_NETPLAY_SNAPSHOT_SPIKE=ON (which
// also defines PIKMIN_NETPLAY_SNAPSHOT_SPIKE=1 for the handful of engine TUs
// that carry hooks), and active only at runtime with
// PIKMIN_NETPLAY_SNAPSHOT_SPIKE=1. With the option off none of this exists;
// with the option on but the env var unset, every hook returns immediately.
//
// What it does when active:
//   * reserves a fixed-VA region (2 GB, MEM_WRITE_WATCH, committed on demand)
//     holding a deterministic size-class allocator whose metadata lives in
//     the region; zero-filled blocks;
//   * routes main-thread SIM-domain operator new/delete there (see
//     pc_snapshot_spike.cpp for the routing rule); every other allocation
//     stays on malloc and is counted by category and by call site;
//   * moves the 256 MB simulated OS arena (sys heap) into the region;
//   * per tick: GetWriteWatch dirty pages (auth pass / rest of the tick),
//     compare-based dirty pages of the exe's .data/.bss, a real save
//     (undo copy + shadow copy), a content-identical restore, periodic full
//     copies, allocation churn; one CSV line per tick (snapshot_spike.csv in
//     the cwd, or PIKMIN_NETPLAY_SNAPSHOT_SPIKE_CSV);
//   * optional crude synctest: PIKMIN_NETPLAY_SNAPSHOT_SYNCTEST=k.
//
// All hooks are called through weak references from engine code, so targets
// that compile the engine TUs without this one still link.

#include <cstddef>
#include <cstdint>

// Called once from main(), before the game starts. Reads the env switch.
void pc_snapshot_spike_init(void);
// True once init ran with PIKMIN_NETPLAY_SNAPSHOT_SPIKE=1 and the region is up.
bool pc_snapshot_spike_active(void);

// operator new hook: returns a region block, or nullptr when this allocation
// stays on malloc (it is then counted by category and call site).
void* pc_snapshot_spike_new(size_t size, void* returnAddress);
// operator delete hook: true when ptr was a region block (and is now freed).
// returnAddress names the delete's caller for the unknown-free table.
bool pc_snapshot_spike_delete(void* ptr, void* returnAddress);
// Called by piki_pc_free when a pointer is neither a region block nor in the
// malloc tracking table (an "unknown free"): classifies it by memory kind and
// records the call site of the delete in flight.
void pc_snapshot_spike_unknown_free(void* ptr);

// Simulated OS arena inside the region (sys heap). False when inactive.
bool pc_snapshot_spike_arena(void** lo, size_t* bytes);

// Frame markers (System::run normal path / PlugPikiApp::idle).
void pc_snapshot_spike_frame_begin(void); // a logical tick is about to run
void pc_snapshot_spike_idle_begin(void);  // right before app->idle()
void pc_snapshot_spike_auth_end(void);    // authoritative pass finished
void pc_snapshot_spike_idle_end(void);    // right after app->idle()
void pc_snapshot_spike_tick_end(void);    // after the state hash: measure, log

// Infrastructure scope: allocations inside stay on malloc (category given).
enum PcSnapshotSpikeInfra {
	kPcSpikeInfraPresent    = 1, // presentation pass (local view, real GL)
	kPcSpikeInfraDoneRender = 2, // doneRender / buffer swap
	kPcSpikeInfraOther      = 3,
};
void pc_snapshot_spike_infra_push(int category);
void pc_snapshot_spike_infra_pop(void);

// Byte range of the exe's globals that a synctest restore must never roll
// back (infrastructure statics such as the malloc tracking table).
void pc_snapshot_spike_preserve(const void* p, size_t bytes);

// operator new post-hook for a block that stayed on malloc: records it in the
// off-region block table when the pointer-scan audit is on. Returns p.
void* pc_snapshot_spike_note_off(void* p, size_t size, void* returnAddress);

// Timing marks inside PlugPikiApp::idle (fix round 1, MV-5): the parts of a
// tick after the authoritative pass, so a resim tick can be costed as
// auth + the sim work that follows presentation.
enum PcSnapshotSpikeMark {
	kPcSpikeMarkPresentEnd = 1, // presentation pass finished
	kPcSpikeMarkDoneBegin  = 2, // gsys->doneRender() about to run
	kPcSpikeMarkDoneEnd    = 3, // gsys->doneRender() returned
	kPcSpikeMarkParseEnd   = 4, // parseMessages() returned (waitRetrace next)
};
void pc_snapshot_spike_mark(int which);

// True while the synctest re-advances ticks after a restore.
bool pc_snapshot_spike_resimulating(void);
