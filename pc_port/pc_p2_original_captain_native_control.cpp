#include "pc_p2_original_captain_native_control.h"
#include "pc_p2_original_captain_throw.h"
#include "pc_p2_original_captain_motion.h"
#include "pc_p2_equipment.h"
#include "Navi.h"
#include "NaviState.h"
#include "Kontroller.h"
#include "Camera.h"
#include <array>
#include <cmath>
#include <limits>

extern const p2original::captain::nativecontrol::Effects* pc_p2_original_captain_control_effects(const Navi*) __attribute__((weak));
extern p2original::captain::actions::ActionSource* pc_p2_original_captain_action_source(const Navi*) __attribute__((weak));
namespace p2original { namespace captain { namespace nativecontrol {
namespace {
constexpr float pi=3.14159265358979323846f;
struct ActorControl {
 const LoadedScene* scene=nullptr;Navi* actor=nullptr;std::uint64_t incarnation=0,generation=0;
 control::RuntimeControl runtime;control::AnimationState animation;
 float animationSpeed=30;
};
std::array<ActorControl,2> actors;
std::uint64_t actorGeneration=0;
struct Binding {
 const LoadedScene* scene=nullptr;const World* world=nullptr;State* state=nullptr;
 actions::ActionSource* source=nullptr;SourceBank* bank=nullptr;
 unsigned slot=0;NaviState* native=nullptr;control::Params params;actions::ActorFrame frame;
};
bool fail(std::string& e,const char* text){e=text;return false;}
bool finite(actions::Vec3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
control::Vec3 vector(const Vector3f& v){return {v.x,v.y,v.z};}
bool bind(Navi* n,Binding& b,std::string& e,bool needState,bool active){
 if(!n||!pc_p2_original_captain_action_source)return fail(e,"missing canonical source control actor/provider");
 b.scene=pc_p2_original_captain_loaded_scene();b.world=pc_p2_original_captain_world();b.source=pc_p2_original_captain_action_source(n);
 if(!b.scene||!b.world||!b.source||&b.source->scene()!=b.scene||!b.scene->incarnation()
 ||b.scene->incarnation()!=b.world->incarnation()||b.scene->selectedCampaign()!=b.world->selectedCampaign()
 ||b.scene->selectedFingerprint()!=b.world->selectedFingerprint()||b.scene->sourceCatalog()!=b.world->sourceCatalog()
 ||b.scene->selectedCampaign().empty()||b.scene->selectedFingerprint().empty()||b.scene->sourceCatalog().empty())return fail(e,"noncanonical source control scene/world");
 if(b.scene->captainAt(0)==n)b.slot=0;else if(b.scene->captainAt(1)==n)b.slot=1;else return fail(e,"source control actor absent from real roster");
 if(!b.scene->captainAt(0)||!b.scene->captainAt(1)||b.scene->captainAt(0)==b.scene->captainAt(1))return fail(e,"source control roster lacks two distinct actual captains");
 if(active&&(b.world->phase()!=Phase::GameWorldActive||!pc_p2_original_captain_actor_alive(n)))return fail(e,"source control actor/world inactive");
 const auto epoch=b.scene->incarnation();const auto campaign=b.scene->selectedCampaign(),fingerprint=b.scene->selectedFingerprint(),catalog=b.scene->sourceCatalog();
 b.native=n->getCurrState();
 if(needState){auto* native=b.native;b.state=dynamic_cast<State*>(native);if(!b.state||b.state->nativeState()!=native)return fail(e,"source control missing exact owned source State");}
 if(!control::parseParameters(b.source->parameterBytes(),b.params,e))return false;
 b.bank=pc_p2_original_captain_source_bank();SourceParameters bankParams;MotionState motion;
 if(!b.bank||!b.bank->ready()||!b.bank->parameters(bankParams,e)||bankParams.rawSourceSha!=control::parameterSha256()
 ||!b.bank->state(n,motion,e))return fail(e,"source control lacks actual selected resource bank/actor binding");
 if(!b.source->frame(*n,b.frame,e)||!finite(b.frame.position)||!finite(b.frame.cursor)||!std::isfinite(b.frame.delta)||b.frame.delta<0||!std::isfinite(b.frame.face)||b.frame.face<0||b.frame.face>=2*pi)return fail(e,"source control missing finite actual actor frame");
 if(pc_p2_original_captain_loaded_scene()!=b.scene||pc_p2_original_captain_world()!=b.world
  ||b.scene->incarnation()!=epoch||b.scene->selectedCampaign()!=campaign||b.scene->selectedFingerprint()!=fingerprint||b.scene->sourceCatalog()!=catalog
  ||pc_p2_original_captain_action_source(n)!=b.source||pc_p2_original_captain_source_bank()!=b.bank
  ||b.scene->captainAt(b.slot)!=n||(needState&&n->getCurrState()!=b.native))return fail(e,"source control binding callback expired authority");
 if(b.frame.controller!=(n->mKontroller!=nullptr))return fail(e,"source control controller identity disagrees with actual actor binding");
 return true;
}
ActorControl* live(Navi* n,const Binding& b,std::string& e){
 auto& a=actors[b.slot];if(a.scene!=b.scene||a.actor!=n||a.incarnation!=b.scene->incarnation())return fail(e,"source control timer not reset by genuine bootstrap"),nullptr;return &a;
}
control::Motion selectorMotion(Motion motion){
 switch(motion){case Motion::Wait:return control::Motion::Wait;case Motion::Asibumi:return control::Motion::Step;case Motion::Walk:return control::Motion::Walk;case Motion::Run2:return control::Motion::Run;case Motion::Nigeru:return control::Motion::Escape;default:return control::Motion::Unsupported;}
}
Motion sourceMotion(control::Motion motion){
 switch(motion){case control::Motion::Step:return Motion::Asibumi;case control::Motion::Walk:return Motion::Walk;case control::Motion::Run:return Motion::Run2;case control::Motion::Escape:return Motion::Nigeru;default:return Motion::Wait;}
}
}
bool resetAfterBootstrap(Navi* n,std::string& e){
 e.clear();Binding b;if(!bind(n,b,e,false,false))return false;
 auto& a=actors[b.slot];if(a.scene==b.scene&&a.actor==n&&a.incarnation==b.scene->incarnation())return fail(e,"duplicate source control bootstrap reset");
 // Retail Navi::onInit sets mSceneAnimationTimer=0. Lifecycle owner invokes
 // only after genuine reset; mere prepared()/session-ready does not invoke it.
 if(actorGeneration==std::numeric_limits<std::uint64_t>::max())return fail(e,"source control actor generation exhausted");
 a={};a.generation=++actorGeneration;a.scene=b.scene;a.actor=n;a.incarnation=b.scene->incarnation();a.runtime.sceneAnimationTimer=0;
 a.runtime.faceDirection=b.frame.face;a.runtime.moveRotation=true;
 return true;
}
std::optional<float> sceneAnimationTimer(const Navi* n){
 if(!n)return {};
 auto* scene=pc_p2_original_captain_loaded_scene();auto* world=pc_p2_original_captain_world();
 if(!scene||!world||scene->incarnation()!=world->incarnation()||scene->selectedFingerprint()!=world->selectedFingerprint()||scene->selectedCampaign()!=world->selectedCampaign()||scene->sourceCatalog()!=world->sourceCatalog())return {};
 for(const auto& a:actors)if(a.actor==n&&a.scene==scene&&a.incarnation==scene->incarnation()&&(scene->captainAt(0)==n||scene->captainAt(1)==n))return a.runtime.sceneAnimationTimer;
 return {};
}
std::optional<float> animationSpeed(const Navi* n){
 if(!sceneAnimationTimer(n))return {};
 for(const auto& a:actors)if(a.actor==n)return a.animationSpeed;
 return {};
}
bool resetThrowAnimationSpeed(Navi* n,std::string& e){
 e.clear();Binding b;if(!bind(n,b,e,true,true))return false;auto* actor=live(n,b,e);if(!actor)return false;
 const auto id=b.state->sourceStateId();if(id!=StateId::Throw&&id!=StateId::ThrowWait)return fail(e,"animation speed reset requires genuine source Throw/ThrowWait init");
 actor->animationSpeed=30;actor->animation.playbackSpeed=30;return true;
}
bool control(Navi* n,std::string& e){
 e.clear();Binding b;if(!bind(n,b,e,true,true))return false;auto* actor=live(n,b,e);if(!actor)return false;
 const auto token=actor->generation,epoch=actor->incarnation;const auto campaign=b.scene->selectedCampaign(),fingerprint=b.scene->selectedFingerprint(),catalog=b.scene->sourceCatalog();
 if(b.world->demo()==Demo::Unknown)return fail(e,"source control missing actual movie authority");
 if(!pc_p2_original_captain_control_effects)return fail(e,"missing genuine source whistle/CPlate/Rappa effects provider");
 const auto* effects=pc_p2_original_captain_control_effects(n);if(!effects||&effects->scene()!=b.scene)return fail(e,"source control effects provider is not canonical scene");
 ControlFacts facts;if(!effects->facts(*n,facts,e))return false;
 auto* camera=n->controlCamera();if(!camera)return fail(e,"missing actual captain control camera");
 control::CameraBasis basis{vector(camera->mViewXAxis),vector(camera->mViewYAxis),vector(camera->mViewZAxis)};
 control::Input input;input.hasController=b.frame.controller;input.deltaTime=b.frame.delta;
 input.cursorOffset={b.frame.cursor.x-b.frame.position.x,b.frame.cursor.z-b.frame.position.z};
 if(n->mKontroller){
  input.stickX=n->mKontroller->getMainStickX();input.stickY=n->mKontroller->getMainStickY();
  constexpr unsigned mask=KBBTN_A|KBBTN_B|KBBTN_X|KBBTN_Y|KBBTN_Z|KBBTN_L|KBBTN_R|KBBTN_DPAD_UP|KBBTN_DPAD_DOWN|KBBTN_DPAD_LEFT|KBBTN_DPAD_RIGHT;
  input.activityButton=(n->mKontroller->mCurrentInput&mask)!=0;
 }
 control::RuntimeControl proposed=actor->runtime;proposed.faceDirection=b.frame.face;
 control::VelocityOutput velocity;bool make=!facts.movieFlagActive;
 if(make&&!control::makeVelocity(b.params,input,basis,proposed,velocity,e))return false;
 float ratio=1;
 if(make){float speed=pc_p2_equipment_speed(b.params.values().moveSpeed);if(!std::isfinite(speed)||speed<=0)return fail(e,"invalid authenticated source equipment speed");ratio=speed/b.params.values().moveSpeed;}
 Request request;request.makeVelocity=make;request.hasController=input.hasController;
 request.playRappa=!facts.movieActor&&(!facts.storyMode||facts.activeActor);
 request.camera=basis;request.demo=b.world->demo();request.proposedSceneAnimationTimer=proposed.sceneAnimationTimer;
 request.whistleDirection=velocity.whistleDirection;
 request.proposedTargetVelocity=make?control::Vec3{velocity.targetVelocity.x*ratio,0,velocity.targetVelocity.z*ratio}:vector(n->mTargetVelocity);
 request.proposedFaceDirection=proposed.faceDirection;request.proposedMoveRotation=proposed.moveRotation;
 if(n->mKontroller){request.subStick={-n->mKontroller->getSubStickX(),n->mKontroller->getSubStickY()};}
 std::unique_ptr<PreparedEffects> plan;if(!effects->prepare(*n,request,plan,e)||!plan)return fail(e,"missing prepared genuine source control effects");
 float timer=plan->resultingSceneAnimationTimer();if(!std::isfinite(timer)||timer<0)return fail(e,"invalid source CStick/scene-animation timer result");
 // Recheck exact source pointers after all read-only provider preflights.
 auto current=[&](){return pc_p2_original_captain_loaded_scene()==b.scene&&pc_p2_original_captain_world()==b.world
  &&b.scene->incarnation()==epoch&&b.scene->selectedCampaign()==campaign&&b.scene->selectedFingerprint()==fingerprint&&b.scene->sourceCatalog()==catalog
  &&b.world->phase()==Phase::GameWorldActive&&pc_p2_original_captain_actor_alive(n)&&n->getCurrState()==b.native
  &&pc_p2_original_captain_action_source(n)==b.source&&pc_p2_original_captain_control_effects(n)==effects
  &&pc_p2_original_captain_source_bank()==b.bank&&actors[b.slot].actor==n&&actors[b.slot].scene==b.scene&&actors[b.slot].incarnation==epoch&&actors[b.slot].generation==token;};
 if(!current())return fail(e,"source control authority changed during preflight");
 if(make){n->mTargetVelocity.set(velocity.targetVelocity.x*ratio,0,velocity.targetVelocity.z*ratio);n->mFaceDirection=proposed.faceDirection;
  if(proposed.moveRotation)n->resetCreatureFlag(CF_UsePriorityFaceDir);else n->setCreatureFlag(CF_UsePriorityFaceDir);
 }
 if(!plan->commit(*n,e))return false;
 if(!current())return fail(e,"source control authority changed during effects commit");
 proposed.sceneAnimationTimer=timer;actors[b.slot].runtime=proposed;return true;
}
static bool animate(Navi* n,const std::function<bool(Animator,Listener,int)>& emit,bool legacy,bool clocks,bool selection,std::string& e){
 e.clear();Binding b;if(!bind(n,b,e,true,true))return false;auto* actor=live(n,b,e);if(!actor)return false;
 if(!pc_p2_original_captain_control_effects)return fail(e,"missing source animation observation provider");
 const auto* effects=pc_p2_original_captain_control_effects(n);if(!effects||&effects->scene()!=b.scene)return fail(e,"noncanonical source animation provider");
 AnimationFrame frame;if(!effects->animationFrame(*n,frame,e))return false;
 if(!frame.gameFrozen)return fail(e,"missing actual source gameFrozen observation");
 if(!std::isfinite(frame.deltaTime)||frame.deltaTime<=0)return fail(e,"invalid actual source animation delta");
 MotionState self,bound;int lock=-1;
 if(!b.bank->stateAnimator(n,Animator::Self,self,e)||!b.bank->stateAnimator(n,Animator::Bound,bound,e))return false;
 if(selection&&(!b.bank->boundMotionLock(n,lock,e)||lock<-1))return fail(e,"invalid actual source bound motion lock");
 Listener selfListener=Listener::None,boundListener=Listener::None;
 if(clocks&&(!b.bank->listenerAnimator(n,Animator::Self,selfListener,e)||!b.bank->listenerAnimator(n,Animator::Bound,boundListener,e)))return false;
 if(legacy&&(selfListener==Listener::SourceState||boundListener==Listener::SourceState))return fail(e,"untyped animation callback cannot receive source state listener");
 auto next=actor->animation;next.bound=selectorMotion(bound.motion);
 control::AnimationOutput selected;
 if(selection&&!control::updateWalkAnimation(b.params,frame.displacement,frame.deltaTime,b.frame.face,frame.faceDirectionOffset,self.motion==Motion::Jkoke,next,selected,e))return false;
 if(clocks&&!emit)return fail(e,"missing actual source animator event receiver");
 const auto epoch=b.scene->incarnation();const auto* native=b.state->nativeState();
 auto current=[&](){
  return pc_p2_original_captain_loaded_scene()==b.scene&&pc_p2_original_captain_world()==b.world
   &&b.scene->incarnation()==epoch&&actor->scene==b.scene&&actor->actor==n&&actor->incarnation==epoch
   &&b.scene->captainAt(b.slot)==n&&pc_p2_original_captain_actor_alive(n)&&n->getCurrState()==native
   &&pc_p2_original_captain_source_bank()==b.bank&&pc_p2_original_captain_action_source(n)==b.source
   &&pc_p2_original_captain_control_effects(n)==effects;
 };
 if(!current())return fail(e,"source animation authority changed during preflight");
 const auto selfGeneration=self.generation,boundGeneration=bound.generation;
 auto generations=[&](){
  MotionState nowSelf,nowBound;
  return current()&&b.bank->stateAnimator(n,Animator::Self,nowSelf,e)&&b.bank->stateAnimator(n,Animator::Bound,nowBound,e)
   &&nowSelf.generation==selfGeneration&&nowBound.generation==boundGeneration;
 };
 if(clocks&&!*frame.gameFrozen){
  bool stopped=false;
  const float amount=actor->animationSpeed*frame.deltaTime;
  auto advance=[&](Animator channel,Listener listener){
   return b.bank->advanceAnimator(n,channel,amount,[&](int key){
    if(!generations())return false;
    const bool keep=emit(channel,listener,key);if(!keep)stopped=true;return keep&&generations();
   },e);
  };
  // FakePiki::doAnimation consumes the rate set by the PRIOR simulation pass.
  if(!advance(Animator::Self,selfListener))return false;
  if(stopped||!generations()){e.clear();return true;}
  if(!advance(Animator::Bound,boundListener))return false;
  if(stopped||!generations()){e.clear();return true;}
 }
 // FakePiki::doSimulation selects the next rate/motion after actual movement.
 if(selection&&selected.transition){
  Motion target=sourceMotion(selected.motion);
  if(!b.bank->supports(n,target,e))return false;
  const Listener listener=selected.listener?Listener::SourceActor:Listener::None;
  if(selected.preserveFrame){
   // Literal source order saves each clock separately: moving Bound first,
   // then unlocked Self. Preserving Bound's frame cannot replace Self's frame.
   if(!b.bank->startAnimator(n,Animator::Bound,target,true,listener,e))return false;
   if(lock==-1&&!b.bank->startAnimator(n,Animator::Self,target,true,Listener::None,e))return false;
  }else {
   // Wait/ASIBUMI boundary resets Self first only when no motion blend lock.
   if(lock==-1&&!b.bank->startAnimator(n,Animator::Self,target,false,Listener::None,e))return false;
   if(!b.bank->startAnimator(n,Animator::Bound,target,false,listener,e))return false;
  }
 }
 if(selection){actor->animation=next;actor->animationSpeed=selected.playbackSpeed;}
 return true;
}
bool advanceAnimation(Navi* n,const std::function<bool(Animator,Listener,int)>& emit,std::string& e){return animate(n,emit,false,true,false,e);}
bool selectWalkAnimation(Navi* n,std::string& e){return animate(n,{},false,false,true,e);}
bool animateWalk(Navi* n,const std::function<bool(Animator,Listener,int)>& emit,std::string& e){return animate(n,emit,false,true,true,e);}
bool animateWalk(Navi* n,const std::function<bool(int)>& emit,std::string& e){
 if(!emit)return fail(e,"missing actual source animator event receiver");
 return animate(n,[&](Animator,Listener,int key){return emit(key);},true,true,true,e);
}
void forget(Navi* n){for(auto& a:actors)if(a.actor==n)a={};}
}}}
