#pragma once
class BTeki;
class Teki;

// Family-owned sheargrub source behavior for the campaign-identity Uji family:
// Female Sheargrub (UjiA, EnemyID 12), Male Sheargrub (UjiB, 13) and Shearwig
// (Tobi, 14) on the P1 KabekuiA/B/C placement vehicles (#871, inst-bugs).
// Implements the source Stay->Appear->Move->Attack1 cycle (UjiaState.cpp),
// plus Attack2->Eat for UjiB (UjibState.cpp) and the Fly loop for Tobi
// (TobiState.cpp). Every hook is a no-op for unregistered actors.
void pc_p2_uji_setup();
void pc_p2_uji_reset();
void pc_p2_uji_forget(BTeki*);
void pc_p2_uji_update(BTeki*);
float pc_p2_uji_param_f(const BTeki*, int idx, float fallback);
bool pc_p2_uji_clip(const BTeki*, const char*& name, float& phase);
// Read-only FSM state name for the engine-free unit test fixture;
// nullptr when the actor is not a registered Uji.
const char* pc_p2_uji_state_name(const BTeki*);
// Registration observability (mirrors pc_p2_sokkuri/elecbug) so the
// lifecycle fixture can prove forget clears a stale binding. Additive.
unsigned long pc_p2_uji_count();
bool pc_p2_uji_registered(BTeki*);
