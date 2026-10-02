#pragma once
#include "pc_midday_actor_archive.h"
#include <limits>
#include <memory>
class ViewPiki;
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
 Navi* stagedNavi()const;
 ViewPiki* stagedPiki()const;
 bool empty()const;
 std::size_t allocationAttempts()const{return attempts_;}
 // Construction owner fence must remain held through cleanup. This owner has
 // no live-world transfer protocol; publication support is intentionally absent.
 void reset();
};
}
