#pragma once

// Generic held ship part (#901). See pc_held_part_policy.h for the contract.
// Adoption for new enemy families: nothing to do as long as a real death goes
// through BTeki::die() (P2-bound actors) or BTeki::dieSoon() (everything,
// including pcEscapeNow). Do not spawn the part from family code.

class BTeki;

// P2 source bound to this actor (family binding or seed generator uid), 0 for
// a P1 actor.
unsigned pc_held_part_p2_source(BTeki* teki);

// Ship part carried by the P1 boss generator's pellet config, or 0 when the
// arena boss holds none. Resolved at runtime (BossMgr::setBossParam rule).
unsigned pc_held_part_for_pellet_config(int pelletConfigIdx);

// BTeki::startAI: returns true when the holder keeps its part (register the
// radar marker and use list), false after clearing an already-existing part.
bool pc_held_part_birth(BTeki* teki);

// Real-death funnel: drops the held part once. `via` names the funnel for
// the log. Returns true when a part pellet was spawned.
bool pc_held_part_drop(BTeki* teki, const char* via);

// BTeki::spawnItems part branch (P1 strategy deaths): true when the vanilla
// spawn should run now; latches so no other funnel drops it again.
bool pc_held_part_claim_spawn_items(BTeki* teki);

// A P1 teki slot whose only protection is a held ship part (mID a UFO part,
// Parameter0 unset) stops being protected when the seed binds a P2 source
// there: the P2 occupant is born from the same personality and holds the part.
bool pc_held_part_transfers(unsigned heldId, int parameter0, const void* generator);

// Make sure the part's pellet shape exists before a late spawn
// (PCT_LoadIfExists / un** parts are otherwise built only at stage init).
void pc_held_part_ensure_shape(unsigned partId);

// Log a held part assigned to an arena-born P2 boss.
void pc_held_part_log_assign(unsigned partId, unsigned source, unsigned target, int p1Boss);
