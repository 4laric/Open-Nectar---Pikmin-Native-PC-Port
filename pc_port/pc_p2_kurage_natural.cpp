#include "pc_p2_kurage_natural.h"

namespace p2kurage_natural {

const char* const kNaturalCorpseMarker = "P2_KURAGE57_CORPSE pellet=1";
const char* const kNaturalCarryMarker = "P2_KURAGE57_CARRY";
const char* const kNaturalDeliveredMarker = "P2_KURAGE57_DELIVERED_TO_GOAL";

bool free_squad_acquires(const AcquisitionFacts& facts)
{
    // src/plugPikiKando/piki.cpp:951, verbatim predicate.
    return facts.visible && facts.alive && !facts.flying && facts.organic && !facts.stickTo;
}

bool attack_holds(bool pikiStickTo, bool targetFlying, bool targetVisible)
{
    // src/plugPikiKando/aiAttack.cpp:297: the abandon branch fires only when the
    // Pikmin is not stuck and the target is airborne or invisible.
    const bool abandons = !pikiStickTo && (targetFlying || !targetVisible);
    return !abandons;
}

bool damage_receiver_accepts(unsigned sourceId)
{
    // Source ids with a family-local InteractAttack rejection in
    // src/plugPikiNakata/tekiinteraction.cpp:44-60.  Source 57 (Kurage) is not
    // in that list, so the receiver accepts a generated attack.
    switch (sourceId) {
    case 28: // ElecBug  -- pc_p2_elecbug_attacked
    case 9:  // Kogane   -- pc_p2_kogane_attacked
    case 23: // Sarai    -- pc_p2_hana_rejects_attack
    case 71: // Armor    -- pc_p2_armor_receiver_rejects
    case 88: // Dangomushi -- pc_p2_dangomushi_invulnerable
    case 52: // Snakejoint -- pc_p2_snakejoint_invulnerable
    case 55: // LongLegs -- pc_p2_long_legs_receiver_rejects
        return false;
    default:
        return true;
    }
}

const char* stall_token(Stall stall)
{
    switch (stall) {
    case Stall::None: return "none";
    case Stall::NoEngagementFreeSquadSkipsFlying: return "no_engagement_free_squad_skips_flying";
    case Stall::NoEngagementFlyingRetarget: return "no_engagement_flying_retarget";
    case Stall::NoDamageReceiver: return "no_damage_receiver";
    case Stall::NoDeath: return "no_death";
    case Stall::NoCorpse: return "no_corpse";
    case Stall::NoCarry: return "no_carry";
    case Stall::NoReceipt: return "no_receipt";
    }
    return "unknown";
}

const char* stall_callsite(Stall stall)
{
    switch (stall) {
    case Stall::None: return "none";
    case Stall::NoEngagementFreeSquadSkipsFlying:
        return "src/plugPikiKando/piki.cpp:951 (graspSituation skips isFlying())";
    case Stall::NoEngagementFlyingRetarget:
        return "src/plugPikiKando/aiAttack.cpp:297 (ActAttack abandons airborne target)";
    case Stall::NoDamageReceiver:
        return "src/plugPikiNakata/tekiinteraction.cpp:41 (InteractAttack::actTeki)";
    case Stall::NoDeath:
        return "src/plugPikiNakata/tekibteki.cpp:873 (BTeki::makeDamaged)";
    case Stall::NoCorpse:
        return "pc_port/pc_p2_kurage_teki.cpp:427 (mPellet corpse registration)";
    case Stall::NoCarry:
        return "pc_port/pc_p2_kurage_teki.cpp:203 (corpseTail FreeMode carry)";
    case Stall::NoReceipt:
        return "pc_port/pc_p2_kurage_teki.cpp:620 (pc_p2_kurage_receipt)";
    }
    return "unknown";
}

Audit audit_private_adapter(bool adapterAirborneWithoutInjection,
                            bool groundedOnlyByInjection)
{
    Audit audit{};
    audit.damage_receiver_open = damage_receiver_accepts(57);
    // A FreeMode squad evaluates the adapter as it is on the field.  If the
    // adapter is airborne unless something grounds it, the squad skips it.
    audit.free_squad_acquires =
        free_squad_acquires(AcquisitionFacts{ true, true, true, false,
                                              adapterAirborneWithoutInjection });
    audit.grounded_only_by_injection = groundedOnlyByInjection;
    if (!audit.damage_receiver_open) {
        audit.stall = Stall::NoDamageReceiver;
    } else if (adapterAirborneWithoutInjection && groundedOnlyByInjection) {
        // The only path that makes the adapter targetable is the injected
        // groundAndSeal concession, which the natural gate must not cite.
        audit.stall = Stall::NoEngagementFreeSquadSkipsFlying;
    } else if (!audit.free_squad_acquires) {
        audit.stall = Stall::NoEngagementFlyingRetarget;
    } else {
        audit.stall = Stall::None;
    }
    return audit;
}

} // namespace p2kurage_natural
