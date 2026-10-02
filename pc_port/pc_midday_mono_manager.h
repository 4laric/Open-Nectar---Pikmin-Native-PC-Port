#pragma once
#include "pc_midday_manager_pool.h"
#include "pc_midday_constructor.h"
#include "pc_midday_actor_graph.h"
#include <memory>
class Navi; class ViewPiki; class NaviMgr; class PikiMgr;
namespace pc_midday {
struct MonoManagerStageTag;
// Constructors/factories must produce every physical slot (also free slots)
// under the same held fence, without normal init or live singleton replacement.
// Retains whole typed graph owners, never root-only unique_ptr ownership.
// Free slots explicitly refuse until the reviewed native default graph route
// is available; an occupied actor snapshot is not a free-slot initializer.
// This seam owns those exact roots/backing; it does not construct actors, restore
// mutable manager fields/resources, or publish a world. Fence outlives the stage.
class IsolatedNaviManager {
 struct Impl; std::unique_ptr<Impl> impl_;
public:
 IsolatedNaviManager(); ~IsolatedNaviManager();
 bool prepare(const NaviMgr&,const MonoPoolPlan&,std::vector<std::unique_ptr<ActorAllocationGraph>>&,ConstructorFence&,std::string&);
 bool installStagedChannel(const RestoreGate&,std::string&);
 NaviMgr* manager()const;
 const std::map<uint64_t,Creature*>& roots()const;
};
class IsolatedPikiManager {
 struct Impl; std::unique_ptr<Impl> impl_;
public:
 IsolatedPikiManager(); ~IsolatedPikiManager();
 bool prepare(const PikiMgr&,const MonoPoolPlan&,std::vector<std::unique_ptr<ActorAllocationGraph>>&,ConstructorFence&,std::string&);
 bool installStagedChannel(const RestoreGate&,std::string&);
 PikiMgr* manager()const;
 const std::map<uint64_t,Creature*>& roots()const;
};
}
