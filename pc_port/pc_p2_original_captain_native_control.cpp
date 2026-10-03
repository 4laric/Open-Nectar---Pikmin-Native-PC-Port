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

extern const p2original::captain::nativecontrol::Effects* pc_p2_original_captain_control_effects(const Navi*) __attribute__((weak));
extern p2original::captain::actions::ActionSource* pc_p2_original_captain_action_source(const Navi*) __attribute__((weak));
namespace p2original { namespace captain { namespace nativecontrol {
namespace {
constexpr float pi=3.14159265358979323846f;
struct ActorControl {
 const LoadedScene* scene=nullptr;Navi* actor=nullptr;std::uint64_t incarnation=0;
 control::RuntimeControl runtime;control::AnimationState animation;
};
std::array<ActorControl,2> actors;
struct Binding {
 const LoadedScene* scene=nullptr;const World* world=nullptr;State* state=nullptr;
 actions::ActionSource* source=nullptr;SourceBank* bank=nullptr;
 unsigned slot=0;control::Params params;actions::ActorFrame frame;
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
 if(needState){auto* native=n->getCurrState();b.state=dynamic_cast<State*>(native);if(!b.state||b.state->nativeState()!=native)return fail(e,"source control missing exact owned source State");}
 if(!control::parseParameters(b.source->parameterBytes(),b.params,e))return false;
 b.bank=pc_p2_original_captain_source_bank();SourceParameters bankParams;MotionState motion;
 if(!b.bank||!b.bank->ready()||!b.bank->parameters(bankParams,e)||bankParams.rawSourceSha!=control::parameterSha256()
 ||!b.bank->state(n,motion,e))return fail(e,"source control lacks actual selected resource bank/actor binding");
 if(!b.source->frame(*n,b.frame,e)||!finite(b.frame.position)||!finite(b.frame.cursor)||!std::isfinite(b.frame.delta)||b.frame.delta<0||!std::isfinite(b.frame.face)||b.frame.face<0||b.frame.face>=2*pi)return fail(e,"source control missing finite actual actor frame");
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
 a={};a.scene=b.scene;a.actor=n;a.incarnation=b.scene->incarnation();a.runtime.sceneAnimationTimer=0;
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
bool control(Navi* n,std::string& e){
 e.clear();Binding b;if(!bind(n,b,e,true,true))return false;auto* actor=live(n,b,e);if(!actor)return false;
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
 if(pc_p2_original_captain_loaded_scene()!=b.scene||pc_p2_original_captain_world()!=b.world||n->getCurrState()!=b.state->nativeState()||pc_p2_original_captain_action_source(n)!=b.source||pc_p2_original_captain_control_effects(n)!=effects)return fail(e,"source control authority changed during preflight");
 if(make){n->mTargetVelocity.set(velocity.targetVelocity.x*ratio,0,velocity.targetVelocity.z*ratio);n->mFaceDirection=proposed.faceDirection;
  if(proposed.moveRotation)n->resetCreatureFlag(CF_UsePriorityFaceDir);else n->setCreatureFlag(CF_UsePriorityFaceDir);
 }
 plan->commit(*n);
 proposed.sceneAnimationTimer=timer;actor->runtime=proposed;return true;
}
bool animateWalk(Navi* n,const std::function<bool(int)>& emit,std::string& e){
 e.clear();Binding b;if(!bind(n,b,e,true,true))return false;auto* actor=live(n,b,e);if(!actor)return false;
 if(b.state->sourceStateId()!=StateId::Walk)return fail(e,"locomotion selector requires exact source Walk state");
 if(!pc_p2_original_captain_control_effects)return fail(e,"missing source animation observation provider");
 const auto* effects=pc_p2_original_captain_control_effects(n);if(!effects||&effects->scene()!=b.scene)return fail(e,"noncanonical source animation provider");
 AnimationFrame frame;if(!effects->animationFrame(*n,frame,e))return false;
 MotionState current;if(!b.bank->state(n,current,e))return false;
 auto next=actor->animation;next.bound=selectorMotion(current.motion);
 control::AnimationOutput selected;if(!control::updateWalkAnimation(b.params,frame.displacement,frame.deltaTime,b.frame.face,frame.faceDirectionOffset,frame.selfIsJKoke,next,selected,e))return false;
 if(selected.transition){
  bool started=selected.preserveFrame?b.bank->startPreservingFrame(n,sourceMotion(selected.motion),e):b.bank->start(n,sourceMotion(selected.motion),e);
  if(!started)return false;
 }
 if(!b.bank->advance(n,selected.playbackSpeed*frame.deltaTime,emit,e))return false;
 if(n->getCurrState()==b.state->nativeState()&&pc_p2_original_captain_loaded_scene()==b.scene&&actor->actor==n&&actor->incarnation==b.scene->incarnation())actor->animation=next;
 return true;
}
void forget(Navi* n){for(auto& a:actors)if(a.actor==n)a={};}
}}}
