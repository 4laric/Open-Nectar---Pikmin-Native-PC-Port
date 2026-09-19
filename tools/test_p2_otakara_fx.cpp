// p2_otakara_fx_test (#170, rd-p2-otakara-fx): the discharge visual mapping
// covers every elemental Otakara source (59-62) with the closest existing P1
// hazard effect, and the slot ledger refreshes/expires/clears cleanly.
// Engine-free: builds pc_p2_otakara_fx.cpp with P2_OTAKARA_FX_NO_ENGINE.
#include "pc_p2_otakara_fx.h"

#include <cassert>
#include <cstdio>

int main() {
    // Mapping: each of 59-62 resolves to its P1 effect id.
    assert(pc_p2_otakara_fx_effect_for_species(59) == 227); // EFF_Hiba_Fire
    assert(pc_p2_otakara_fx_effect_for_species(60) == 15);  // EFF_P_Bubbles
    assert(pc_p2_otakara_fx_effect_for_species(61) == 154); // EFF_Kinoko_AttackSpores
    assert(pc_p2_otakara_fx_effect_for_species(62) == 268); // EFF_Rocket_Biri
    // Unknown species (BombOtakara 93 delegates to the Bomb payload; anything
    // else is not an elemental dweevil) have no visual.
    assert(pc_p2_otakara_fx_effect_for_species(0) == -1);
    assert(pc_p2_otakara_fx_effect_for_species(93) == -1);
    assert(pc_p2_otakara_fx_effect_for_species(99) == -1);

    // Names resolve for the mapped ids.
    assert(pc_p2_otakara_fx_effect_name(227) != nullptr);
    assert(pc_p2_otakara_fx_effect_name(15) != nullptr);
    assert(pc_p2_otakara_fx_effect_name(154) != nullptr);
    assert(pc_p2_otakara_fx_effect_name(268) != nullptr);

    // Lifecycle: reset -> discharge each species -> refresh -> expiry.
    pc_p2_otakara_fx_reset();
    assert(pc_p2_otakara_fx_active_count() == 0);
    assert(pc_p2_otakara_fx_on_discharge(59, 1.0f, 2.0f, 3.0f) == 227);
    assert(pc_p2_otakara_fx_on_discharge(60, 4.0f, 5.0f, 6.0f) == 15);
    assert(pc_p2_otakara_fx_on_discharge(61, 7.0f, 8.0f, 9.0f) == 154);
    assert(pc_p2_otakara_fx_on_discharge(62, 1.0f, 1.0f, 1.0f) == 268);
    assert(pc_p2_otakara_fx_active_count() == 4);
    // Unknown species discharge nothing and disturb no slot.
    assert(pc_p2_otakara_fx_on_discharge(93, 0.0f, 0.0f, 0.0f) == -1);
    assert(pc_p2_otakara_fx_active_count() == 4);
    // Repeat discharge refreshes the same species slot (no growth).
    assert(pc_p2_otakara_fx_on_discharge(59, 9.0f, 9.0f, 9.0f) == 227);
    assert(pc_p2_otakara_fx_active_count() == 4);
    // Visual tails expire.
    pc_p2_otakara_fx_update(P2_OTAKARA_FX_TTL + 0.1f);
    assert(pc_p2_otakara_fx_active_count() == 0);

    // Death/scene-end cleanup finishes outstanding visuals.
    assert(pc_p2_otakara_fx_on_discharge(62, 2.0f, 2.0f, 2.0f) == 268);
    assert(pc_p2_otakara_fx_active_count() == 1);
    pc_p2_otakara_fx_clear();
    assert(pc_p2_otakara_fx_active_count() == 0);

    std::printf("P2_OTAKARA_FX_TEST_PASS species=59..62 effects=227,15,154,268\n");
    return 0;
}
