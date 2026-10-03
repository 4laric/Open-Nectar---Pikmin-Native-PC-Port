#pragma once
#include <string>
#include "pc_p2_original_captain_damage.h"
class BTeki;
namespace p2hana {
// Hanachirashi.cpp780 filters the actual source Creature alive flag before
// constructing Wind. Native Navi::isAlive tests HP and cannot supply it.
inline bool naviTargetAlive(const Navi* n){return n&&pc_p2_original_captain_actor_alive(n);}
bool resources(std::string&);
bool birth(BTeki*,unsigned uid,unsigned ordinal,std::string&);
bool registry(BTeki*,unsigned,std::string&);
bool has(const BTeki*);
bool active();
bool update(BTeki*);
bool clip(const BTeki*,const char*&,float&);
void forget(BTeki*);
}
