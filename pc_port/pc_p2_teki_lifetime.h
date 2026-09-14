#pragma once

class BTeki;

// Shared Teki actor-lifetime seam (lane 07; #186 feedback "family registration
// maps are never cleared on Teki death", #397/#404).
//
// Clears every P2 family's registration-map entry for a Teki. Each family hook
// is an idempotent map erase, so this is safe for actors no family registered
// and safe to call more than once for the same actor. It is invoked from the
// death funnel (BTeki::doKill) and reused for slot reuse (TekiMgr::newTeki), so
// a stale BTeki* key can never outlive the actor, per
// docs/PIKMIN2_ENEMY_IMPORT_PIPELINE.md section 5.
//
// The family list lives only in pc_p2_teki_lifetime.cpp. Add a family there, not
// at the call sites, so the death funnel and slot reuse stay in sync.
void pc_p2_forget_teki(BTeki* actor);
