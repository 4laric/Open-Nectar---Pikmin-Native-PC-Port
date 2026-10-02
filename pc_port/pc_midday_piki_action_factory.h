#pragma once
#include <string>
class Piki;class TopAction;
namespace pc_midday {
class AllocationOwner;
// Creates only action topology in an unpublished shell. Caller must discard the
// ENTIRE owner on any exception; does not install into Piki or initialize AI.
bool inspect_piki_action_graph(Piki&,TopAction&,std::string&);
TopAction* allocate_piki_action_graph(Piki&,AllocationOwner&);
}
