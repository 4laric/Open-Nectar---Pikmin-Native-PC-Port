#pragma once
class BTeki; class Piki; class Navi; class PikiState; class NaviState; struct Vector3f;
PikiState* pc_p2_hanachirashi_piki_state_create();
NaviState* pc_p2_hanachirashi_navi_state_create();
bool pc_p2_hanachirashi_wind_piki(BTeki*, Piki*, const Vector3f&);
bool pc_p2_hanachirashi_wind_navi(BTeki*, Navi*, const Vector3f&);
bool pc_p2_hanachirashi_flick_piki(BTeki*, Piki*);
bool pc_p2_hanachirashi_flick_navi(BTeki*, Navi*);
