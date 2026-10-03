#include "pc_p2_original_captain_states.h"
#include "pc_p2_original_captain_down.h"
#include "pc_p2_original_captain_motion.h"
#include "pc_p2_original_captain_native_control.h"
#include "pc_p2_original_captain_throw.h"
#include <cstdio>
#include <map>
#include <array>

extern p2original::captain::SourceBank* pc_p2_original_captain_source_bank() __attribute__((weak));
extern p2original::captain::WalkEnvironment* pc_p2_original_captain_walk_environment(const Navi*) __attribute__((weak));
extern bool pc_p2_original_captain_throw_preflight(Navi*,p2original::captain::StateId,std::string&) __attribute__((weak));
extern bool pc_p2_original_captain_pluck_preflight(Navi*,p2original::captain::StateId,std::string&) __attribute__((weak));
extern bool pc_p2_original_captain_punch_preflight(Navi*,std::string&) __attribute__((weak));
extern bool pc_p2_original_captain_party_preflight(Navi*,p2original::captain::StateId,std::string&) __attribute__((weak));
extern bool pc_p2_original_captain_dope_preflight(Navi*,std::string&) __attribute__((weak));
extern bool pc_p2_source_navi_reaction_animation_key(Navi*,const NaviState*,std::uint64_t,int,std::string&) __attribute__((weak));
using namespace p2original::captain;
namespace {
struct Backup {const LoadedScene* scene=nullptr;std::uint64_t epoch=0;StateId state=StateId::Walk;};
std::map<Navi*,Backup> backups;
bool actor(Navi* n,std::string& e){
 const auto* scene=pc_p2_original_captain_loaded_scene();const auto* world=pc_p2_original_captain_world();
 if(!scene||!world||world->incarnation()!=scene->incarnation()
  ||world->selectedCampaign()!=scene->selectedCampaign()||world->selectedFingerprint()!=scene->selectedFingerprint()
  ||world->sourceCatalog()!=scene->sourceCatalog()||world->captainAt(0)!=scene->captainAt(0)||world->captainAt(1)!=scene->captainAt(1)||!n
  ||(scene->captainAt(0)!=n&&scene->captainAt(1)!=n)){e="missing canonical source captain actor";return false;}return true;
}
SourceBank* bankFor(Navi* n,std::string& e){
 if(!actor(n,e))return nullptr;
 auto* bank=pc_p2_original_captain_source_bank?pc_p2_original_captain_source_bank():nullptr;
 MotionState state;if(!bank||!bank->ready()||!bank->state(n,state,e)){e="missing actual source captain motion binding";return nullptr;}return bank;
}
WalkEnvironment* environment(Navi* n,std::string& e){
 if(!actor(n,e))return nullptr;
 auto* env=pc_p2_original_captain_walk_environment?pc_p2_original_captain_walk_environment(n):nullptr;
 if(!env||&env->scene()!=pc_p2_original_captain_loaded_scene()){e="missing actual source Walk environment";return nullptr;}return env;
}
bool registered(Navi* n,StateId id){
 const int native=nativeId(id);auto* fsm=n->mStateMachine;
 if(!fsm||native>=fsm->mStateLimit)return false;
 for(int i=0;i<fsm->mStateCount;++i){auto* state=dynamic_cast<State*>(fsm->mStates[i]);
  if(state&&state->nativeState()==fsm->mStates[i]&&state->sourceStateId()==id&&fsm->mStates[i]->getID()==native)return true;}
 return false;
}
StateId backup(Navi* n){
 auto it=backups.find(n);auto* scene=pc_p2_original_captain_loaded_scene();
 return it!=backups.end()&&scene&&it->second.scene==scene&&it->second.epoch==scene->incarnation()?it->second.state:StateId::Walk;
}
class CoreState:public NativeState {
public:
 explicit CoreState(StateId id):NativeState(id){}
 std::string previousError;
 void report(const std::string& e){if(e!=previousError){std::fprintf(stderr,"[original captain state %d refused] %s\n",static_cast<int>(id_),e.c_str());previousError=e;}}
 void recover(Navi* n){std::string e;if(!pc_p2_original_captain_transit(n,backup(n),e))report(e);}
};
class WalkState final:public CoreState {
 walk::State walkState;
 const LoadedScene* initializedScene=nullptr;std::uint64_t initializedEpoch=0;Navi* initializedActor=nullptr;
public:
 WalkState():CoreState(StateId::Walk){}
 bool sourceInvincible()const override{return false;}
 bool apply(Navi* n,WalkEnvironment& env,const walk::Output& out,std::string& e,bool firstControlDone=false){
  if(!env.preflight(*n,out.commands,e))return false;
  bool beforeFirstThrowQuery=firstControlDone,controlBlock=false;
  for(const auto& command:out.commands){
   const bool controlMarker=command.kind==walk::Kind::MakeVelocity||command.kind==walk::Kind::MakeCStick||command.kind==walk::Kind::Rappa;
   if(controlMarker){
    // Only the leading control was already executed. Retail Attack/Escape
    // calls control again: preserve that timer, whistle and CPlate cadence.
    if(!beforeFirstThrowQuery&&!controlBlock&&!nativecontrol::control(n,e))return false;
    controlBlock=true;continue;
   }
   controlBlock=false;
   if(command.kind==walk::Kind::FindNextThrowPiki)beforeFirstThrowQuery=false;
   if(!env.execute(*n,command,e))return false;
  }
  return true;
 }
 void init(Navi* n)override{
  initializedScene=nullptr;initializedEpoch=0;initializedActor=nullptr;
  std::string e;auto* env=environment(n,e);walk::Frame frame;walk::Output out;
  if(!env||!env->capture(*n,frame,e)||!frame.actor||!walk::init(*frame.actor,walkState,out,e)||!apply(n,*env,out,e)){report(e);return;}
  if(!actor(n,e)||n->getCurrState()!=this){report("source Walk initialization changed actor/state ownership");return;}
  initializedScene=pc_p2_original_captain_loaded_scene();initializedEpoch=initializedScene->incarnation();initializedActor=n;previousError.clear();
 }
 bool initialized(const Navi* n,const LoadedScene& scene)const{return initializedActor==n&&initializedScene==&scene&&initializedEpoch==scene.incarnation();}
 void exec(Navi* n)override{
  std::string e;auto* env=environment(n,e);auto* bank=bankFor(n,e);if(!env||!bank){report(e);return;}
  // Control precedes the source >9 idle decision. Never apply it again when
  // interpreting the policy's ordered MakeVelocity/MakeCStick/Rappa markers.
  if(sourceAlive(*n)&&!nativecontrol::control(n,e)){report(e);return;}
  walk::Frame frame;control::Params params;std::string bytes;walk::Output out;
  if(!env->capture(*n,frame,e)||!bank->sourceBytes(SourceResource::Parameters,bytes,e)
   ||!control::parseParameters(bytes,params,e)){report(e);return;}
  frame.postControlSceneAnimationTimer=nativecontrol::sceneAnimationTimer(n);
  if(!walk::step(params,frame,walkState,out,e)||!apply(n,*env,out,e,true)){report(e);return;}
  const auto appliedCommands=out.commands.size();
  if(out.continuation==walk::Stage::ActionButton){bool handled=false;std::optional<bool> throwable;
   if(!env->actionButton(*n,handled,throwable,e)||!walk::resumeActionButton(handled,throwable,out,e)){report(e);return;}
   out.commands.erase(out.commands.begin(),out.commands.begin()+appliedCommands);
   if(!apply(n,*env,out,e)){report(e);return;}}
  else if(out.continuation==walk::Stage::Dismiss){bool released=false;
   if(!env->dismiss(*n,released,e)||!walk::resumeDismiss(frame,released,walkState,out,e)){report(e);return;}
   out.commands.erase(out.commands.begin(),out.commands.begin()+appliedCommands);
   if(!apply(n,*env,out,e)){report(e);return;}}
  previousError.clear();
 }
 bool sourceAnimationKey(Navi* n,int event,std::string& e)override{return key(n,event,e);}
 bool key(Navi* n,int event,std::string& e){
  auto* env=environment(n,e);if(!env)return false;walk::Output out;
  if(event==1000){if(!walk::keyEventEnd(walkState,out,e))return false;}
  else if(event==200){walk::Frame frame;if(!env->capture(*n,frame,e)||!walk::jumpKey200(frame,out,e))return false;}
  return apply(n,*env,out,e);
 }
};
class DamagedState final:public CoreState {
public:
 DamagedState():CoreState(StateId::Damaged){}
 bool sourceInvincible()const override{return false;}
 void init(Navi* n)override{std::string e;auto* bank=bankFor(n,e);if(!bank||(!bank->start(n,Motion::Damage,e)||!bank->enableMotionBlend(n,e)))report(e);}
 void exec(Navi* n)override{std::string e;auto* bank=bankFor(n,e);MotionState motion;
  if(!bank||!bank->state(n,motion,e)){report(e);return;}if(motion.motion!=Motion::Damage)recover(n);}
 void cleanup(Navi* n)override{if(!pc_p2_original_captain_damaged_cleanup(n))report("missing genuine Damaged cleanup authority");}
 void key(Navi* n,int event){if(event==1000)recover(n);}
 bool sourceAnimationKey(Navi* n,int event,std::string& e)override{e.clear();key(n,event);return n->getCurrState()==this;}
};
class DeadState final:public CoreState {
public:
 DeadState():CoreState(StateId::Dead){}
 bool sourceInvincible()const override{return true;}
 void init(Navi* n)override{std::string e;
  // Literal source order: section gmOrimaDown first, then clear CF_IsAlive.
  if(!pc_p2_original_captain_down_begin(n,e)){report(e);return;}
  if(!pc_p2_original_captain_dead_entered(n))report("missing genuine Dead lifetime event");
 }
 void exec(Navi* n)override{n->mTargetVelocity.set(0,0,0);n->mVelocity.set(0,0,0);}
};
}
namespace p2original { namespace captain {
bool NativeState::sourceAlive(const Navi& n)const{return pc_p2_original_captain_actor_alive(&n);}
std::optional<std::uint8_t> NativeState::actorInvincibleFrames(const Navi& n)const{std::uint8_t value;if(!pc_p2_original_captain_actor_frames(&n,value))return {};return value;}
bool NativeState::canEnterSourceDead(const Navi& n)const{return pc_p2_original_captain_can_enter_dead(&n);}
void NativeState::enterSourceDead(Navi& n){pc_p2_original_captain_enter_dead(&n);}
bool NativeState::canEnterSourceDamaged(const Navi& n)const{std::string e;return pc_p2_original_captain_core_preflight(const_cast<Navi*>(&n),StateId::Damaged,e);}
void NativeState::enterSourceDamaged(Navi& n,float){std::string e;pc_p2_original_captain_transit(&n,StateId::Damaged,e);}
void NativeState::sourceDamageFeedback(Navi& n){std::string e;auto* env=environment(&n,e);if(!env||!env->damageFeedback(n,e))std::fprintf(stderr,"[original captain damage presentation unavailable] %s\n",e.c_str());}
void registerCoreStates(NaviStateMachine& machine){machine.registerState(new WalkState);machine.registerState(new DamagedState);machine.registerState(new DeadState);}
} }
bool pc_p2_original_captain_core_preflight(Navi* n,StateId id,std::string& e){
 if(!actor(n,e)||pc_p2_original_captain_world()->phase()!=Phase::GameWorldActive){e="source captain world is inactive";return false;}
 if(!bankFor(n,e)||!registered(n,id)){e="missing registered source captain state/bank";return false;}
 if(id==StateId::Dead)return pc_p2_original_captain_down_preflight(n,e);
 if(id==StateId::Walk){auto* env=environment(n,e);walk::Frame frame;return env&&nativecontrol::sceneAnimationTimer(n)&&env->capture(*n,frame,e)&&frame.actor.has_value();}
 if(id==StateId::Damaged){auto* bank=bankFor(n,e);return bank&&bank->supports(n,Motion::Damage,e)&&bank->supports(n,Motion::Nigeru,e);}
 if(id==StateId::Nuku||id==StateId::NukuAdjust)return pc_p2_original_captain_pluck_preflight&&pc_p2_original_captain_pluck_preflight(n,id,e);
 if(id==StateId::Punch)return pc_p2_original_captain_punch_preflight&&pc_p2_original_captain_punch_preflight(n,e);
 if(id==StateId::Dope)return pc_p2_original_captain_dope_preflight&&pc_p2_original_captain_dope_preflight(n,e);
 if(id==StateId::Gather||id==StateId::Throw||id==StateId::ThrowWait)return pc_p2_original_captain_throw_preflight&&pc_p2_original_captain_throw_preflight(n,id,e);
 if(id==StateId::Follow||id==StateId::Change)return pc_p2_original_captain_party_preflight&&pc_p2_original_captain_party_preflight(n,id,e);
 e="source action preflight provider is unavailable";return false;
}
bool pc_p2_original_captain_can_enter_dead(const Navi* n){std::string e;return pc_p2_original_captain_core_preflight(const_cast<Navi*>(n),StateId::Dead,e);}
bool pc_p2_original_captain_enter_dead(Navi* n){std::string e;return pc_p2_original_captain_transit(n,StateId::Dead,e);}
std::optional<StateId> pc_p2_original_captain_reaction_backup(Navi* n,std::string& e){
 e.clear();if(!actor(n,e)||pc_p2_original_captain_world()->phase()!=Phase::GameWorldActive){e="source reaction world is inactive";return {};}
 auto* native=n->getCurrState();auto* state=dynamic_cast<State*>(native);
 if(!state||state->nativeState()!=native||(state->sourceStateId()!=StateId::Flick&&state->sourceStateId()!=StateId::KokeDamage)||!n->mStateMachine){e="source reaction backup requires actual current Flick/Koke state";return {};}
 bool owned=false;for(int i=0;i<n->mStateMachine->mStateCount;++i)if(n->mStateMachine->mStates[i]==native){owned=true;break;}
 if(!owned){e="source reaction state is not owned by actual captain FSM";return {};}
 return backup(n);
}
bool pc_p2_original_captain_recover_reaction(Navi* n,std::string& e){auto saved=pc_p2_original_captain_reaction_backup(n,e);return saved&&pc_p2_original_captain_transit(n,*saved,e);}
bool pc_p2_original_captain_continuation_valid(const LoadedScene& scene,std::string& e){
 e.clear();if(pc_p2_original_captain_loaded_scene()!=&scene||!scene.incarnation()){e="source bootstrap scene is not canonical";return false;}
 for(unsigned slot=0;slot<2;++slot){auto* n=scene.captainAt(slot);if(!actor(n,e))return false;
  auto* native=n->getCurrState();auto* typed=dynamic_cast<State*>(native);auto* bank=bankFor(n,e);MotionState motion;
  if(!typed||typed->nativeState()!=native||!n->mStateMachine||!bank||!bank->state(n,motion,e)||!nativecontrol::sceneAnimationTimer(n)||!nativecontrol::animationSpeed(n)){e="source continuation lacks current typed state/bank/control owner";return false;}
  bool found=false;for(int i=0;i<n->mStateMachine->mStateCount;++i)if(n->mStateMachine->mStates[i]==native){found=true;break;}
  if(!found){e="source current state is not owned by actual actor FSM";return false;}
 }
 return pc_p2_original_captain_loaded_scene()==&scene;
}
bool pc_p2_original_captain_bootstrap_complete(const LoadedScene& scene,std::string& e){
 if(!pc_p2_original_captain_continuation_valid(scene,e))return false;
 for(unsigned slot=0;slot<2;++slot){auto* n=scene.captainAt(slot);auto* state=dynamic_cast<WalkState*>(n->getCurrState());
  if(!state||!state->initialized(n,scene)){e="both actual source initial Walk states have not initialized";return false;}}
 return true;
}
bool pc_p2_original_captain_bootstrap_roster(std::string& e){
 e.clear();auto* scene=pc_p2_original_captain_loaded_scene();auto* world=pc_p2_original_captain_world();
 if(!scene||!world||world->phase()!=Phase::Loading){e="source initial Walk bootstrap requires concrete loaded body reset";return false;}
 std::array<WalkState*,2> states{};
 for(unsigned slot=0;slot<2;++slot){auto* n=scene->captainAt(slot);if(!actor(n,e)||!bankFor(n,e)||!nativecontrol::sceneAnimationTimer(n)||!registered(n,StateId::Walk)){e="source initial Walk lacks actual body/bank/control/FSM";return false;}
  for(int i=0;i<n->mStateMachine->mStateCount;++i)if(auto* state=dynamic_cast<WalkState*>(n->mStateMachine->mStates[i]))states[slot]=state;
  auto* env=environment(n,e);walk::Frame frame;walk::State trial;walk::Output out;
  if(!states[slot]||!env||!env->capture(*n,frame,e)||!frame.actor||!walk::init(*frame.actor,trial,out,e)||!env->preflight(*n,out.commands,e))return false;
 }
 if(states[0]==states[1]){e="source initial Walk requires separate actor-owned FSM instances";return false;}
 if(pc_p2_original_captain_loaded_scene()!=scene||pc_p2_original_captain_world()!=world||world->phase()!=Phase::Loading){e="source bootstrap ownership changed during preflight";return false;}
 for(unsigned slot=0;slot<2;++slot){auto* n=scene->captainAt(slot);if(states[slot]->initialized(n,*scene)&&n->getCurrState()==states[slot])continue;
  n->setCurrState(states[slot]);states[slot]->init(n);
  if(pc_p2_original_captain_loaded_scene()!=scene||!actor(n,e)||pc_p2_original_captain_world()!=world||world->phase()!=Phase::Loading||n->getCurrState()!=states[slot]||!states[slot]->initialized(n,*scene)){e="source initial Walk refused after actual body reset";return false;}}
 return pc_p2_original_captain_bootstrap_complete(*scene,e);
}
bool pc_p2_original_captain_transit(Navi* n,StateId id,std::string& e){
 if(!pc_p2_original_captain_core_preflight(n,id,e))return false;
 pc_p2_original_captain_before_transition(n);
 n->mStateMachine->StateMachine<Navi>::transit(n,nativeId(id));return true;
}
void pc_p2_original_captain_before_transition(Navi* n){
 std::string e;if(!actor(n,e))return;
 auto* old=dynamic_cast<State*>(n->getCurrState());if(old&&old->nativeState()==n->getCurrState()
  &&(old->sourceStateId()==StateId::Walk||old->sourceStateId()==StateId::Follow)){
  auto* scene=pc_p2_original_captain_loaded_scene();backups[n]={scene,scene->incarnation(),old->sourceStateId()};}
}
PcOriginalCaptainRoute pc_p2_original_captain_route_transition(Navi* n,int request,int& mapped){
 std::string e;if(!actor(n,e))return PcOriginalCaptainRoute::NonSource;
 if(pc_p2_original_captain_world()->phase()!=Phase::GameWorldActive)return PcOriginalCaptainRoute::Refused;
 StateId id;
 if(request==NAVISTATE_Walk)id=StateId::Walk;else if(request==NAVISTATE_Dead)id=StateId::Dead;
 else if(request>=NativeStateBase&&request<NativeStateLimit)id=static_cast<StateId>(request-NativeStateBase);
 else {
  // Independently owned source Flick/recovery uses a separate native slot.
  // Allow only an actually registered typed source receiver; never send an
  // authenticated original actor into a merely similarly named P1 state.
  auto* machine=n->mStateMachine;
  if(machine)for(int i=0;i<machine->mStateCount;++i){auto* native=machine->mStates[i];auto* typed=dynamic_cast<State*>(native);
   if(native->getID()==request&&typed&&typed->nativeState()==native
    &&(typed->sourceStateId()==StateId::Flick||typed->sourceStateId()==StateId::KokeDamage)){
    mapped=request;return PcOriginalCaptainRoute::Handled;
   }}
  e="requested native state has no genuine original captain implementation";
  std::fprintf(stderr,"[original captain transition refused] %s\n",e.c_str());return PcOriginalCaptainRoute::Refused;
 }
 if(!pc_p2_original_captain_core_preflight(n,id,e)){std::fprintf(stderr,"[original captain transition refused] %s\n",e.c_str());return PcOriginalCaptainRoute::Refused;}
 mapped=nativeId(id);return PcOriginalCaptainRoute::Handled;
}
bool pc_p2_original_captain_core_advance_animation(Navi* n,float frames,std::string& e){
 auto* bank=bankFor(n,e);auto* state=n?dynamic_cast<State*>(n->getCurrState()):nullptr;
 if(!bank||!state||state->nativeState()!=n->getCurrState()){e="missing current source animation state";return false;}
 if(auto* damaged=dynamic_cast<DamagedState*>(n->getCurrState()))return bank->advance(n,frames,[&](int key){damaged->key(n,key);return true;},e);
 if(auto* walk=dynamic_cast<WalkState*>(n->getCurrState()))return bank->advance(n,frames,[&](int key){return walk->key(n,key,e);},e);
 // Dead's movie BCK advances under the genuine Studio owner, not this clock.
 return state->sourceStateId()==StateId::Dead;
}

bool pc_p2_original_captain_animation_key(Navi* n,int key,std::string& e){
 e.clear();if(!actor(n,e))return false;auto* current=n->getCurrState();auto* typed=dynamic_cast<State*>(current);
 if(!typed||typed->nativeState()!=current){e="missing exact current source animation state";return false;}
 if(auto* state=dynamic_cast<NativeState*>(current))return state->sourceAnimationKey(n,key,e);
 if((typed->sourceStateId()==StateId::Flick||typed->sourceStateId()==StateId::KokeDamage)
  &&pc_p2_source_navi_reaction_animation_key){auto* bank=bankFor(n,e);MotionState self;
   if(!bank||!bank->stateAnimator(n,p2original::captain::Animator::Self,self,e))return false;
   return pc_p2_source_navi_reaction_animation_key(n,typed->nativeState(),self.generation,key,e);}
 e="actual source receiver key handler is unavailable";return false;
}

bool pc_p2_original_captain_actor_animation_key(Navi* n,int key,std::string& e){
 e.clear();if(!actor(n,e))return false;auto* current=n->getCurrState();auto* typed=dynamic_cast<State*>(current);
 if(!typed||typed->nativeState()!=current){e="missing exact current source actor key state";return false;}
 if(auto* state=dynamic_cast<NativeState*>(current))return state->sourceActorAnimationKey(n,key,e);
 if((typed->sourceStateId()==StateId::Flick||typed->sourceStateId()==StateId::KokeDamage)
  &&pc_p2_source_navi_reaction_animation_key){auto* bank=bankFor(n,e);MotionState self;
   if(!bank||!bank->stateAnimator(n,p2original::captain::Animator::Self,self,e))return false;
   return pc_p2_source_navi_reaction_animation_key(n,typed->nativeState(),self.generation,key,e);}
 e="actual source receiver actor key handler is unavailable";return false;
}
