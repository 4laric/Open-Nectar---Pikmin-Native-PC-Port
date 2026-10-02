#pragma once
#include "pc_midday_player_resources.h"
#include "pc_midday_constructor.h"
#include <memory>
class Material; struct PermanentEffect;
namespace pc_midday {
struct PlayerResourcePlan {
 int registered=0,repair=-1;
 std::array<int,30> materials{};
 std::array<uint8_t,30> visibility{};
};
// Typed snapshot validation precedes allocation; registration must agree with
// the independently decoded PlayerCore record. Outputs are transactional.
bool planPlayerResources(const ActorBytes&,const PlayerCoreFields&,const LogicalResolver&,PlayerResourcePlan&,std::string&);
class IsolatedPlayerResources {
 struct Impl;std::unique_ptr<Impl> impl_;size_t attempts_=0;
public:
 IsolatedPlayerResources();~IsolatedPlayerResources();
 bool allocate(const ActorBytes&,const PlayerCoreFields&,const LogicalResolver&,const RestoreGate&,ConstructorFence&,std::string&,size_t failAt=0);
 // Counts owned allocation sites only, not codec/bookkeeping allocations.
 size_t allocationAttempts()const{return attempts_;}
 bool heldBy(const ConstructorFence&)const;
 // Borrowed canonical allocation roots for catalog registration. Parts is a
 // native PlayerState::UfoParts[30], not an actor and not a process wire address.
 void* parts()const;
 Material* materials(size_t part)const;
 PermanentEffect* light(bool glow)const;
 const PlayerResourcePlan* plan()const;
 // Allocation only: caller MUST bind every saved typed payload and shared
 // animation/material/particle resource before any world publication.
};
}
