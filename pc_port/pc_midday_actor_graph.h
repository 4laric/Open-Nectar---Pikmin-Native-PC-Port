#pragma once
#include "pc_midday_actor_archive.h"
#include "pc_midday_actor_ancillary.h"
#include <limits>
#include <memory>
class ViewPiki;
struct NaviProp;
struct PikiProp;
namespace pc_midday {
class ConstructorFence;
// Owns one physical actor root AND its constructor-owned nested graph. Borrowed
// resources remain scene-owned. This is allocation completion, not saved-state
// binding, pool reuse readiness or whole-world publication. No release API:
// manager staging must retain this owner for the complete actor lifetime.
class ActorAllocationGraph {
 struct Impl;
 std::unique_ptr<Impl> impl_;
 std::size_t attempts_=0;
public:
 ActorAllocationGraph();
 ~ActorAllocationGraph();
 ActorAllocationGraph(const ActorAllocationGraph&)=delete;
 ActorAllocationGraph& operator=(const ActorAllocationGraph&)=delete;
 bool prepareNavi(const ActorBytes&,LogicalResolver&,ConstructorFence&,int controllerPort,std::string&,
                  std::size_t failAt=std::numeric_limits<std::size_t>::max());
 bool prepareViewPiki(const ActorBytes&,const ActorBytes&,LogicalResolver&,ConstructorFence&,std::string&,
                      std::size_t failAt=std::numeric_limits<std::size_t>::max());
 // Explicit free-slot allocation: no ActorBytes, resolver, saved selectors or
 // live actor snapshots. Properties are exact compiled scene-owned types.
 // The source-defined defaults are INERT; caller must bind scene resources and
 // validate birth readiness before exposing a free slot to MonoObjectMgr.
 // In particular MonoObjectMgr::birth does not initialize a returned slot.
 bool prepareFreeNavi(NaviProp&,int slot,const NaviAncillaryConfig&,ConstructorFence&,std::string&,
                      std::size_t failAt=std::numeric_limits<std::size_t>::max());
 bool prepareFreeViewPiki(PikiProp&,const PikiAncillaryConfig&,ConstructorFence&,std::string&,
                          std::size_t failAt=std::numeric_limits<std::size_t>::max());
 Navi* stagedNavi()const;
 ViewPiki* stagedPiki()const;
 bool empty()const;
 // Identity/held-state observation only; the fence must still outlive this graph.
 bool heldBy(const ConstructorFence&)const;
 std::size_t allocationAttempts()const{return attempts_;}
 // Construction owner fence must remain held through cleanup. This owner has
 // no live-world transfer protocol; publication support is intentionally absent.
 void reset();
};
}
