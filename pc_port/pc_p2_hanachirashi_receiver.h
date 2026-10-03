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
