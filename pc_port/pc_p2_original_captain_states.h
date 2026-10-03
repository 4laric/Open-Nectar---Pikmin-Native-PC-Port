#pragma once
#include "NaviState.h"
#include "pc_p2_original_captain_damage.h"
#include <string>

namespace p2original { namespace captain {
// Reserve a separate native range. Retail IDs above remain source identities;
// original actor callbacks must never transit to the similarly named P1 ID.
constexpr int NativeStateBase=48;
constexpr int NativeStateLimit=NativeStateBase+27;
constexpr int nativeId(StateId id){return NativeStateBase+static_cast<int>(id);}
class NativeState:public NaviState,public State {
public:
 explicit NativeState(StateId id):NaviState(nativeId(id)),id_(id){}
 const NaviState* nativeState()const final{return this;}
 StateId sourceStateId()const final{return id_;}
 bool sourceAlive(const Navi&)const final;
 std::optional<std::uint8_t> actorInvincibleFrames(const Navi&)const final;
 bool canEnterSourceDead(const Navi&)const final;
 void enterSourceDead(Navi&) final;
 void sourceDamageFeedback(Navi&) final;
 bool invincible(Navi*) final{return sourceInvincible();}
protected:
 StateId id_;
};
// Leaf factories return actual NativeState implementations. Registration does
// not attest runtime authority; canonical scene/bank are checked on activation.
void registerCoreStates(NaviStateMachine&);
} }
bool pc_p2_original_captain_can_enter_dead(const Navi*);
bool pc_p2_original_captain_enter_dead(Navi*);
bool pc_p2_original_captain_transit(Navi*,p2original::captain::StateId,std::string&);
// Return true only when the requested source transition was handled. The
// native FSM uses this to route receiver recovery Walk/Dead into source IDs.
bool pc_p2_original_captain_route_transition(Navi*,int nativeRequest,int& sourceRequest);
