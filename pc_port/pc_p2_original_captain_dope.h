#pragma once
#include "pc_p2_original_captain_throw.h"
class Creature;
namespace p2originalresource {class ResourceState;}
namespace p2original {namespace captain {namespace dope {
using actions::Vec3;
enum class Spray:int {Spicy=0,Bitter=1};
struct Target {Creature* actor=nullptr;std::uint64_t lifetime=0;};
struct TargetFrame {Target handle;Vec3 position;bool piki=false;};
struct Sphere {Vec3 center;float radius=140;};
// Canonical original story Course binds the campaign inventory and actual
// CPlate/CellMgr/receiver objects. P1 spray use/state helpers are not providers.
class DopeSource {
public:
 virtual ~DopeSource()=default;
 virtual const LoadedScene& scene()const=0;
 // Exact current story PlayData-backed resource owner; no private stock copy.
 virtual p2originalresource::ResourceState* inventory()const=0;
 virtual bool squad(const Navi&,std::vector<TargetFrame>&,std::string&)const=0;
 virtual bool mapSearch(Sphere,std::vector<Target>&,std::string&)const=0;
 virtual bool target(Target,TargetFrame&,std::string&)const=0;
 // Actual target.stimulate(InteractDope(captain,type)); preserve source dopable,
 // Dope FSM/timer extension and enemy bitter receivers. Rejection still spends
 // one spray. No P1 Normal/Dope/Piki state substitutes.
 virtual bool stimulate(Navi&,Target,Spray,bool& accepted,std::string&)=0;
 // Literal emit-red/black sound then TDopingSmoke(position,direction,type).
 virtual bool smoke(Navi&,Spray,Vec3 origin,Vec3 direction,std::string&)=0;
};
bool begin(Navi*,Spray,std::string&);
} void registerDopeState(NaviStateMachine&);
}}
p2original::captain::dope::DopeSource* pc_p2_original_captain_dope_source(const Navi*);
bool pc_p2_original_captain_dope_preflight(Navi*,std::string&);
bool pc_p2_original_captain_dope_advance_animation(Navi*,float,std::string&);
