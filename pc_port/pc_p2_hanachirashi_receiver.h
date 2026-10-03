#pragma once
class BTeki; class Piki; class Navi; class PikiState; class NaviState; struct Vector3f;
PikiState* pc_p2_hanachirashi_piki_state_create();
NaviState* pc_p2_hanachirashi_navi_state_create();
bool pc_p2_hanachirashi_wind_piki(BTeki*, Piki*, const Vector3f&);
bool pc_p2_hanachirashi_wind_navi(BTeki*, Navi*, const Vector3f&);
bool pc_p2_hanachirashi_flick_piki(BTeki*, Piki*);
bool pc_p2_hanachirashi_flick_navi(BTeki*, Navi*);
// Retail source receivers: Pikmin ignore damage; captain damage is applied
// on the Koke END event. Angle below -10 uses the receiver facing.
bool pc_p2_source_flick_piki(BTeki*, Piki*, float knockback, float angle);
bool pc_p2_source_flick_navi(BTeki*, Navi*, float knockback, float damage, float angle);
// Retail InteractFlick rejects Flick/Panic but permits the Blow state that
// its previous acceptance entered. Keep native state IDs outside this policy.
constexpr bool pc_p2_source_flick_reaction_blocked(int current,int flick,int panic){return current==flick||current==panic;}
// Supported only for the currently active owned original receiver state.
// A false return means unsupported, not that an arbitrary native state is immune.
enum class PcSourceNaviReactionKind { Unsupported, Flick, KokeDamage };
struct PcSourceNaviReactionGate {
    PcSourceNaviReactionKind kind=PcSourceNaviReactionKind::Unsupported;
    unsigned phase=0; // Hit0, Fling1, Koke2, Timer3, GetUp4.
    unsigned long long activation=0;
    bool inheritedInvincible=false; // Retail NaviState default, both source states.
};
bool pc_p2_source_navi_reaction_gate(Navi*,PcSourceNaviReactionGate&);
#include <cstdint>
#include <string>
// Called only by the canonical common source animator, never a P1 key broadcast.
bool pc_p2_source_navi_reaction_animation_key(Navi*,const NaviState* expectedState,
    std::uint64_t expectedSelfGeneration,int sourceKey,std::string& error);
