#include "pc_p2_teki_lifetime.h"

#include "pc_p2_armor.h"
#include "pc_p2_batch2.h"
#include "pc_p2_batch3.h"
#include "pc_p2_breadbug_actor.h"
#include "pc_p2_enemy.h"
#include "pc_p2_frog.h"
#include "pc_p2_giant_breadbug_actor.h"
#include "pc_p2_kochappy.h"
#include "pc_p2_kogane.h"
#include "pc_p2_kurage_teki.h"
#include "pc_p2_long_legs.h"
#include "pc_p2_mamuta.h"
#include "pc_p2_onikurage_teki.h"
#include "pc_p2_projectiles.h"
#include "pc_p2_qurione.h"
#include "pc_p2_sheargrub.h"
#include "pc_p2_sokkuri.h"
#include "pc_p2_tank.h"

// Single authoritative list of families that hold a BTeki* registration map.
// Mirrors the pre-existing inline forget list in TekiMgr::newTeki exactly (no
// family added or dropped) so this change is behaviour-neutral at the call
// sites. Queen/King track Piki*, not BTeki*, and keep their own Piki forgets.
void pc_p2_forget_teki(BTeki* actor)
{
	if (!actor) {
		return;
	}

	pc_p2_snow_forget(actor);
	pc_p2_sheargrub_forget(actor);
	pc_p2_kochappy_forget(actor);
	pc_p2_giant_breadbug_actor_forget(actor);
	pc_p2_breadbug_actor_forget(actor);
	pc_p2_frog_forget(actor);
	pc_p2_kogane_forget(actor);
	pc_p2_mamuta_forget(actor);
	pc_p2_tank_forget(actor);
	pc_p2_qurione_forget(actor);
	pc_p2_kurage_teki_forget(actor);
	pc_p2_onikurage_teki_forget(actor);
	pc_p2_batch2_forget(actor);
	pc_p2_projectiles_forget(actor);
	pc_p2_sokkuri_forget(actor);
	pc_p2_armor_forget(actor);
	pc_p2_batch3_forget(actor);
	pc_p2_long_legs_forget(actor);
}
