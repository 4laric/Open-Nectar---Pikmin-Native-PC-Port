#pragma once
#include <string>
class Navi;
class Piki;
class NaviState;
namespace pc_midday {
class AllocationOwner;
// Allocate only registered concrete handlers and their owned state topology.
// Requires an inert actor with null FSM/current state and a disposable owner.
// Does not enter a state, dispatch callbacks, or make the actor update-ready.
// On failure, discard the staging owner before retrying; partial allocations
// remain owned, and the actor's FSM pointer is not published.
bool allocate_navi_state_graph(Navi&, AllocationOwner&, std::string&);
bool allocate_piki_state_graph(Piki&, AllocationOwner&, std::string&);
// Source-local hooks retain the concrete private type's destructor in the owner.
NaviState* allocate_demon_drop_state(AllocationOwner&);
NaviState* allocate_demon_escape_state(AllocationOwner&);
}
