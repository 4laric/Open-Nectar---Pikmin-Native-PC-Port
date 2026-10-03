// Actual receiver TU with observable captain authority/bank doubles.
// Canonical damage policy itself is independently tested by its owner.
#include "engine.h"
#include "pc_p2_hanachirashi_receiver.h"
#include "pc_p2_original_captain_damage.h"
#include "pc_p2_original_captain_motion.h"
#include "pc_p2_original_captain_states.h"
#include "pc_p2_equipment.h"
#include <cassert>
#include <iostream>
using namespace p2original::captain;
FakeSystem systemControl;FakeSystem* gsys=&systemControl;
unsigned pc_p2_original_actor_token(const Creature*){return 17;}
namespace {
Navi* roster=nullptr;bool lifetimePresent=true,cfAlive=true,bankReady=true,equipment=false;
std::uint8_t frames=0;unsigned damageCalls=0,recoveryCalls=0;float rawSeen=0;bool phaseBeforeDamage=false,killOnDamage=false;
Refusal admission=Refusal::None,damageRefusal=Refusal::None;MotionState selfMotion,boundMotion;Listener selfListener,boundListener;std::uint64_t serial=1;
struct WorldControl:World{
 std::string campaign="source",fingerprint="selected",catalog="catalog";std::uint64_t epoch=1;Phase active=Phase::GameWorldActive;Demo movie=Demo::Absent;
 const std::string& selectedCampaign()const override{return campaign;}const std::string& selectedFingerprint()const override{return fingerprint;}const std::string& sourceCatalog()const override{return catalog;}std::uint64_t incarnation()const override{return epoch;}Phase phase()const override{return active;}Demo demo()const override{return movie;}Navi* captainAt(unsigned slot)const override{return slot==0?roster:nullptr;}
} world;
}
namespace p2original {namespace captain {
struct SourceBank::Impl{};
SourceBank::SourceBank():m(new Impl){}SourceBank::~SourceBank()=default;
bool SourceBank::ready()const{return bankReady;}
bool SourceBank::supports(Navi* n,Motion,std::string& e)const{if(n!=roster||!bankReady){e="missing bank";return false;}e.clear();return true;}
bool SourceBank::stateAnimator(const Navi* n,Animator a,MotionState& out,std::string& e)const{if(n!=roster||!bankReady){e="missing binding";return false;}out=a==Animator::Self?selfMotion:boundMotion;e.clear();return true;}
bool SourceBank::startMotion(Navi* n,Motion a,Motion b,Listener sl,Listener bl,std::string& e){if(n!=roster||!bankReady)return false;selfMotion={a,0,++serial,false,false};boundMotion={b,0,++serial,false,false};selfListener=sl;boundListener=bl;e.clear();return true;}
Refusal flickAdmission(const Creature*,const Navi*){return admission;}
DamageResult addDamage(Navi* n,float raw,bool feedback){++damageCalls;rawSeen=raw;assert(!feedback);auto* state=dynamic_cast<State*>(n->getCurrState());assert(state&&state->nativeState()==n->getCurrState());PcSourceNaviReactionGate gate;assert(pc_p2_source_navi_reaction_gate(n,gate));phaseBeforeDamage=gate.phase==3&&state->sourceStateId()==StateId::KokeDamage;DamageResult result;result.refusal=damageRefusal;if(damageRefusal!=Refusal::None)return result;if(killOnDamage){state->enterSourceDead(*n);result.knockedOut=true;}return result;}
}}
SourceBank bankControl;
const World* pc_p2_original_captain_world(){return lifetimePresent?&world:nullptr;}
SourceBank* pc_p2_original_captain_source_bank(){return &bankControl;}
bool pc_p2_original_captain_actor_alive(const Navi* n){return lifetimePresent&&n==roster&&cfAlive;}
bool pc_p2_original_captain_actor_lifetime(const Navi* n,bool& out){if(!lifetimePresent||n!=roster)return false;out=cfAlive;return true;}
bool pc_p2_original_captain_actor_frames(const Navi* n,std::uint8_t& out){if(!lifetimePresent||n!=roster)return false;out=frames;return true;}
bool pc_p2_original_captain_can_enter_dead(const Navi* n){return n==roster;}
bool pc_p2_original_captain_enter_dead(Navi* n){n->mStateMachine->transit(n,67);cfAlive=false;return true;}
p2original::captain::WalkEnvironment* pc_p2_original_captain_walk_environment(const Navi*){return nullptr;}
const LoadedScene* pc_p2_original_captain_loaded_scene(){return nullptr;}
bool pc_p2_original_captain_recover_reaction(Navi* n,std::string& e){++recoveryCalls;n->mStateMachine->transit(n,48);e.clear();return true;}
PcOriginalCaptainRoute pc_p2_original_captain_route_transition(Navi* n,int request,int& mapped){
 if(!lifetimePresent||n!=roster||world.active!=Phase::GameWorldActive||!n->mStateMachine)return PcOriginalCaptainRoute::Refused;
 auto it=n->mStateMachine->states.find(request);if(it==n->mStateMachine->states.end())return PcOriginalCaptainRoute::Refused;
 auto* native=it->second;auto* typed=dynamic_cast<State*>(native);
 if(!typed||typed->nativeState()!=native||native->getID()!=request||(typed->sourceStateId()!=StateId::Flick&&typed->sourceStateId()!=StateId::KokeDamage))return PcOriginalCaptainRoute::Refused;
 mapped=request;return PcOriginalCaptainRoute::Handled;
}
bool pc_p2_original_captain_transit(Navi* n,StateId id,std::string& e){assert(id==StateId::Walk);n->mStateMachine->transit(n,48);e.clear();return true;}
bool pc_p2_equipment_has(p2equipment::Item i){return i==p2equipment::RepugnantAppendage&&equipment;}
int main(){
 BTeki owner;Navi n;roster=&n;Machine<Navi> machine;NaviState walk(48),dead(67);auto* receiver=pc_p2_hanachirashi_navi_state_create();machine.states={{48,&walk},{67,&dead},{38,receiver}};n.mStateMachine=&machine;machine.transit(&n,48);std::string error;
 struct UnknownInteraction:Interaction{using Interaction::Interaction;bool actCommon(Creature*)const override{++gsys->commonCalls;return true;}bool actNavi(Navi* target)const override{target->mHealth=0;return true;}} unknown(&owner);
 bool handled=true;assert(!pc_p2_source_navi_interaction_dispatch(unknown,&n,handled)&&!handled);assert(!n.stimulate(unknown)&&n.mHealth==100&&systemControl.commonCalls==0);
 handled=true;assert(!pc_p2_source_navi_interaction_dispatch(unknown,nullptr,handled)&&!handled);
 auto* savedState=n.current;auto* savedMachine=n.mStateMachine;int absentDraws=systemControl.draws;n.current=nullptr;assert(!pc_p2_source_flick_navi(&owner,&n,80,7,0));assert(!pc_p2_hanachirashi_wind_navi(&owner,&n,Vector3f(5,40,10)));n.current=savedState;n.mStateMachine=nullptr;assert(!pc_p2_source_flick_navi(&owner,&n,80,7,0));assert(!pc_p2_hanachirashi_wind_navi(&owner,&n,Vector3f(5,40,10)));assert(systemControl.draws==absentDraws&&damageCalls==0&&selfMotion.generation==0);n.mStateMachine=savedMachine;
 int targetDraws=systemControl.draws;const auto targetGeneration=selfMotion.generation;machine.states.erase(38);assert(!pc_p2_source_flick_navi(&owner,&n,80,7,0));assert(!pc_p2_hanachirashi_wind_navi(&owner,&n,Vector3f(5,40,10)));NaviState fakeTarget(38);machine.states[38]=&fakeTarget;assert(!pc_p2_source_flick_navi(&owner,&n,80,7,0));assert(!pc_p2_hanachirashi_wind_navi(&owner,&n,Vector3f(5,40,10)));assert(systemControl.draws==targetDraws&&selfMotion.generation==targetGeneration&&n.current==&walk&&damageCalls==0);machine.states[38]=receiver;
 NaviState spoof(38);n.current=&spoof;int beforeSpoof=systemControl.draws;assert(!pc_p2_hanachirashi_wind_navi(&owner,&n,Vector3f(5,40,10))&&systemControl.draws==beforeSpoof);n.current=&walk;
 admission=Refusal::NotReunited;int draws=systemControl.draws;assert(!pc_p2_source_flick_navi(&owner,&n,80,7,0)&&systemControl.draws==draws);admission=Refusal::None;
 assert(pc_p2_source_flick_navi(&owner,&n,80,7,0));auto* typed=dynamic_cast<State*>(n.getCurrState());assert(typed&&typed->nativeState()==receiver&&typed->sourceStateId()==StateId::Flick&&!typed->sourceInvincible());assert(typed->actorInvincibleFrames(n)==0);frames=60;assert(typed->actorInvincibleFrames(n)==60);frames=0;assert(selfMotion.motion==Motion::Jhit&&selfListener==Listener::SourceActor&&boundListener==Listener::None);
 const auto oldGeneration=selfMotion.generation;PcSourceNaviReactionGate gate;assert(pc_p2_source_navi_reaction_gate(&n,gate));const auto oldActivation=gate.activation;
 assert(!pc_p2_source_navi_reaction_animation_key(&n,&walk,oldGeneration,1000,error));assert(!pc_p2_source_navi_reaction_animation_key(&n,receiver,oldGeneration+1,1000,error));assert(damageCalls==0&&selfMotion.motion==Motion::Jhit);
 assert(pc_p2_source_navi_reaction_animation_key(&n,receiver,oldGeneration,2,error));assert(error.empty()&&selfMotion.generation==oldGeneration);
 KeyEvent end;MsgAnim p1{&end};n.current->procAnimMsg(&n,&p1);assert(selfMotion.generation==oldGeneration);
 assert(pc_p2_source_navi_reaction_animation_key(&n,receiver,oldGeneration,1000,error));assert(error.empty()&&selfMotion.motion==Motion::Jkoke&&selfListener==Listener::None&&boundListener==Listener::None);assert(pc_p2_source_navi_reaction_gate(&n,gate)&&gate.phase==1);
 assert(!pc_p2_source_navi_reaction_animation_key(&n,receiver,oldGeneration,1000,error));MsgBounce bounce;n.current->procBounceMsg(&n,&bounce);assert(pc_p2_source_navi_reaction_gate(&n,gate)&&gate.phase==1);n.current->exec(&n);assert(pc_p2_source_navi_reaction_gate(&n,gate)&&gate.phase==1);
 // Actual source assertMotion mismatch path enters Koke retaining flicker.
 assert(pc_p2_source_flick_navi(&owner,&n,80,7,0));assert(pc_p2_source_navi_reaction_gate(&n,gate)&&gate.activation!=oldActivation);selfMotion.motion=Motion::Wait;n.current->exec(&n);assert(pc_p2_source_navi_reaction_gate(&n,gate)&&gate.phase==2);assert(selfMotion.motion==Motion::Jkoke&&selfListener==Listener::SourceActor);
 world.movie=Demo::Unknown;n.current->exec(&n);assert(pc_p2_source_navi_reaction_gate(&n,gate)&&gate.phase==2);world.movie=Demo::Absent;
 float unchanged=n.mHealth;assert(pc_p2_source_navi_reaction_animation_key(&n,receiver,selfMotion.generation,1000,error));assert(error.empty()&&damageCalls==1&&rawSeen==7&&phaseBeforeDamage&&n.mHealth==unchanged);
 assert(pc_p2_source_navi_reaction_animation_key(&n,receiver,selfMotion.generation,1000,error));assert(damageCalls==1);systemControl.dt=1;n.current->exec(&n);assert(selfMotion.motion==Motion::Getup&&selfListener==Listener::SourceActor);auto getupGeneration=selfMotion.generation;assert(pc_p2_source_navi_reaction_animation_key(&n,receiver,getupGeneration,1000,error));assert(error.empty()&&n.current==&walk&&recoveryCalls==1);
 assert(!pc_p2_source_navi_reaction_animation_key(&n,receiver,getupGeneration,1000,error));
 // Canonical damage may replace the state: END succeeds without stale access.
 assert(pc_p2_source_flick_navi(&owner,&n,80,7,0));selfMotion.motion=Motion::Wait;n.current->exec(&n);killOnDamage=true;assert(pc_p2_source_navi_reaction_animation_key(&n,receiver,selfMotion.generation,1000,error));assert(error.empty()&&n.current==&dead&&damageCalls==2);
 cfAlive=true;killOnDamage=false;machine.transit(&n,48);assert(pc_p2_source_flick_navi(&owner,&n,80,7,0));selfMotion.motion=Motion::Wait;n.current->exec(&n);damageRefusal=Refusal::ActorInvincible;assert(!pc_p2_source_navi_reaction_animation_key(&n,receiver,selfMotion.generation,1000,error));assert(pc_p2_source_navi_reaction_gate(&n,gate)&&gate.phase==3);auto refusedCount=damageCalls;assert(pc_p2_source_navi_reaction_animation_key(&n,receiver,selfMotion.generation,1000,error)&&damageCalls==refusedCount);damageRefusal=Refusal::None;
 cfAlive=true;killOnDamage=false;machine.transit(&n,48);assert(pc_p2_source_flick_navi(&owner,&n,80,7,0));auto currentGeneration=selfMotion.generation;auto inactiveDraws=systemControl.draws;world.active=Phase::Inactive;assert(!pc_p2_source_navi_reaction_animation_key(&n,receiver,currentGeneration,1000,error));assert(!pc_p2_source_flick_navi(&owner,&n,80,7,0));assert(!pc_p2_hanachirashi_wind_navi(&owner,&n,Vector3f(5,40,10)));assert(selfMotion.generation==currentGeneration&&systemControl.draws==inactiveDraws&&damageCalls==refusedCount);world.active=Phase::GameWorldActive;lifetimePresent=false;assert(!pc_p2_source_navi_reaction_animation_key(&n,receiver,currentGeneration,1000,error));lifetimePresent=true;++world.epoch;assert(!pc_p2_source_navi_reaction_animation_key(&n,receiver,currentGeneration,1000,error));assert(damageCalls==refusedCount);
 assert(systemControl.sourceDispatches>0&&systemControl.commonCalls==0);
 delete receiver;std::cout<<"actual receiver typed authority/key/listener/stale/reentry/rawdamage-once/recovery/owned-dispatch controls PASS; physical contact remains unsupported\n";
}
