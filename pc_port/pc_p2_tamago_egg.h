#pragma once
#include "pc_p2_original_catalog.h"
#include <array>
class Creature;

namespace p2tamago {
struct GroupIdentity {
    p2original::InstanceIdentity parent;
    unsigned slot = 0; // Egg contents slot0 is the group; slot1 is fallback honey
    bool operator<(const GroupIdentity& b) const {
        return parent < b.parent || (!(b.parent < parent) && slot < b.slot);
    }
};
struct EggGroupRequest {
    GroupIdentity identity;
    p2original::Position position, velocity;
    float facing = 0; // Already drawn by Egg::genItem; producer never redraws it
};
struct MemberIdentity {
    GroupIdentity group;
    unsigned member = 0; // distinct nested source68 member0..9
    static constexpr unsigned source = 68;
};
enum class Terminal { None, Death, Hide, Scene };
struct MemberFrontier {
    bool born=false, retired=false; Terminal terminal=Terminal::None;
    bool honeyAttempted=false,honeyBorn=false,honeyRetired=false,honeyConsumed=false,honeyFault=false;
};
struct GroupFrontier {
    GroupIdentity identity;
    bool attempted=false, leaderBorn=false;
    std::array<MemberFrontier,10> members{};
};
}
// Caller authenticates Egg outcome provenance (standalone37 or carried Egg16).
// No catalog row is fabricated; resources need a bank, never an AP actor file.
bool pc_p2_tamago_prepare_original_egg(std::string& error,unsigned managerLimit=10);
bool pc_p2_tamago_original_manager_available();
bool pc_p2_tamago_birth_original_egg(const p2tamago::EggGroupRequest&) noexcept;
bool pc_p2_tamago_original_child(const Creature*, p2tamago::MemberIdentity&, unsigned* token=nullptr);
bool pc_p2_tamago_original_group(const p2tamago::GroupIdentity&, p2tamago::GroupFrontier&);
// Typed ItemHoney owner (#1252) owns actual birth/init and its identity.
enum class P2TamagoHoneyBirth { Born, PoolEmpty, ResourceError };
struct P2TamagoHoneyProvider {
    bool (*preflight)(void*,std::string&)=nullptr;
    P2TamagoHoneyBirth (*birth)(void*,const p2tamago::MemberIdentity&,unsigned,
                              const p2original::Position&,const p2original::Position&) noexcept=nullptr;
    void* context=nullptr;
};
bool pc_p2_tamago_original_honey_provider(const P2TamagoHoneyProvider&);
// First absorb is a consumption event; the actual Honey body can remain live.
bool pc_p2_tamago_original_honey_consumed(const p2tamago::MemberIdentity&,unsigned rewardSlot);
// Actual Honey owner reports terminal/consumed events by the typed ancestry.
bool pc_p2_tamago_original_honey_retired(const p2tamago::MemberIdentity&,unsigned rewardSlot,bool consumed);
// Outside a Teki update: retire only this producer's actual scene children.
void pc_p2_tamago_original_egg_scene_release();
