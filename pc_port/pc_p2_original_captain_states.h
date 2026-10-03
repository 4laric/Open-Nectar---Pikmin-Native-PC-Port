#pragma once
#include "NaviState.h"
#include "pc_p2_original_captain_damage.h"
#include "pc_p2_original_captain_walk.h"
#include <string>

namespace p2original { namespace captain {
// Reserve a separate native range. Retail IDs above remain source identities;
// original actor callbacks must never transit to the similarly named P1 ID.
constexpr int NativeStateBase=48;
constexpr int NativeStateLimit=NativeStateBase+27;
constexpr int nativeId(StateId id){return NativeStateBase+static_cast<int>(id);}
class NativeState:public NaviState,public State,public DamageTransitions {
public:
 explicit NativeState(StateId id):NaviState(nativeId(id)),id_(id){}
 const NaviState* nativeState()const final{return this;}
 StateId sourceStateId()const final{return id_;}
 bool sourceAlive(const Navi&)const final;
 std::optional<std::uint8_t> actorInvincibleFrames(const Navi&)const final;
 bool canEnterSourceDead(const Navi&)const final;
 void enterSourceDead(Navi&) final;
 void sourceDamageFeedback(Navi&) final;
 bool canEnterSourceDamaged(const Navi&)const final;
 void enterSourceDamaged(Navi&,float) final;
 bool invincible(Navi*) final{return sourceInvincible();}
 // Called only by the common actual animator phase; never advances a clock.
 virtual bool sourceAnimationKey(Navi*,int,std::string& error){error="source state key handler unavailable";return false;}
 virtual bool sourceActorAnimationKey(Navi* n,int key,std::string& error){return sourceAnimationKey(n,key,error);}
protected:
 StateId id_;
};
// Leaf factories return actual NativeState implementations. Registration does
// not attest runtime authority; canonical scene/bank are checked on activation.
void registerCoreStates(NaviStateMachine&);
// Actual selected world/body/party adapters supply the completed source Walk
// observations and action continuations. No P1 state/action delegates belong
// here. The canonical scene identity is checked again for every invocation.
class WalkEnvironment {
public:
 virtual ~WalkEnvironment()=default;
 virtual const LoadedScene& scene()const=0;
 virtual bool capture(const Navi&,walk::Frame&,std::string&)const=0;
 virtual bool preflight(const Navi&,const std::vector<walk::Command>&,std::string&)const=0;
 virtual bool execute(Navi&,const walk::Command&,std::string&)=0;
 virtual bool actionButton(Navi&,bool& handled,std::optional<bool>& throwable,std::string&)=0;
 virtual bool dismiss(Navi&,bool& released,std::string&)=0;
 virtual bool damageFeedback(Navi&,std::string&)=0;
};
} }
p2original::captain::WalkEnvironment* pc_p2_original_captain_walk_environment(const Navi*);
bool pc_p2_original_captain_can_enter_dead(const Navi*);
bool pc_p2_original_captain_enter_dead(Navi*);
bool pc_p2_original_captain_transit(Navi*,p2original::captain::StateId,std::string&);
// The native FSM routes receiver recovery Walk/Dead into source IDs. Refused
// means stop the transition; it must never fall through to a P1 state.
enum class PcOriginalCaptainRoute {NonSource,Handled,Refused};
PcOriginalCaptainRoute pc_p2_original_captain_route_transition(Navi*,int nativeRequest,int& sourceRequest);
bool pc_p2_original_captain_core_preflight(Navi*,p2original::captain::StateId,std::string&);
bool pc_p2_original_captain_core_advance_animation(Navi*,float sourceFrames,std::string&);
void pc_p2_original_captain_before_transition(Navi*);

bool pc_p2_original_captain_animation_key(Navi*,int,std::string&);

bool pc_p2_original_captain_actor_animation_key(Navi*,int,std::string&);
