#pragma once
#include "pc_midday_actor_archive.h"
#include "pc_midday_strong_storage.h"
class Piki;
namespace pc_midday {
struct PikiStrongPath {std::string key;std::vector<int> path;int type;std::string member;};
// Requires the full typed payload; derives keys from compiled topology, never targets.
bool piki_strong_paths(const ActorFields&,std::vector<PikiStrongPath>&,std::string&);
bool visit_piki_strong_storage(Piki&,const ActorFields&,StrongStorageVisitor&,std::string&);
}
