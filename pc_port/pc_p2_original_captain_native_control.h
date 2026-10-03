#pragma once
#include "pc_p2_original_captain_control.h"
#include "pc_p2_original_captain_damage.h"
#include "pc_p2_original_captain_motion.h"
#include <functional>
#include <memory>
#include <optional>
#include <string>
class Navi;
namespace p2original { namespace captain { namespace nativecontrol {
// Genuine source whistle/CPlate/Rappa owner implements this extension beside
// ActionSource. Preparing is read-only. No native P1 control/whistle/formation
// functions may implement it. Missing producer explicitly refuses control.
struct Request {
 bool makeVelocity=false, hasController=false, playRappa=false;
 control::Vec2 whistleDirection, subStick;
 control::Vec3 proposedTargetVelocity;
 float proposedFaceDirection=0;
 bool proposedMoveRotation=true;
 control::CameraBasis camera;
 Demo demo=Demo::Unknown;
 float proposedSceneAnimationTimer=0;
};
class PreparedEffects {
public:
 virtual ~PreparedEffects()=default;
 // Actual source makeCStick may reset the timer on projected strength>.05.
 // The genuine CPlate owner preflights full source makeCStick(false), including
 // slot distances, angle/command fields and timers, not just this reset.
 virtual float resultingSceneAnimationTimer() const=0;
 // Commits the preflighted source whistle update(result,false), CPlate effects,
 // then source Rappa only where requested. A callback may retire authority;
 // report refusal before executing any subsequent source command.
 // The plan is bound to exact scene incarnation/actor/current source State.
 virtual bool commit(Navi&,std::string&)=0;
};
struct AnimationFrame {
 // Source FakePiki position-minus-previousPosition (after real movement),
 // and NaviMgr's faceDirOffset captured before the actor update pass.
 control::Vec2 displacement;
 float deltaTime=0, faceDirectionOffset=0;
 // Actual GameSystem::mIsFrozen observation. Missing authority refuses;
 // Self JKOKE identity is queried from the real dual animator bank.
 std::optional<bool> gameFrozen;
};
struct ControlFacts {
 bool movieFlagActive=false, movieActor=false, storyMode=false, activeActor=false;
};
class Effects {
public:
 virtual ~Effects()=default;
 virtual const LoadedScene& scene()const=0;
 virtual bool facts(const Navi&,ControlFacts&,std::string&)const=0;
 virtual bool prepare(const Navi&,const Request&,std::unique_ptr<PreparedEffects>&,std::string&)const=0;
 virtual bool animationFrame(const Navi&,AnimationFrame&,std::string&)const=0;
};
// Owns only actual source scene timer/locomotion selector. Bootstrap calls reset
// AFTER genuine two-body reset/bank binding; duplicate reset refuses. No public
// source-ready/authentication bool and no lazy inference from P1 actor flags.
bool resetAfterBootstrap(Navi*,std::string&);
std::optional<float> sceneAnimationTimer(const Navi*);
std::optional<float> animationSpeed(const Navi*);
// Literal NaviThrowState::init / held NaviThrowWaitState::init speed assignment.
// Only the genuine typed Throw/ThrowWait init owner may invoke this event.
bool resetThrowAnimationSpeed(Navi*,std::string&);
// Actual native Walk/action states call once BEFORE inspecting post-control
// timer. Walk command application must not execute this same control twice.
bool control(Navi*,std::string&);
// Actual FakePiki::doAnimation phase, before physics/input AI. Consumes the
// previously selected rate; never selects locomotion or a next-frame rate.
bool advanceAnimation(Navi*,const std::function<bool(Animator,Listener,int)>& emit,std::string&);
// Actual FakePiki::doSimulation phase, after eligible actual movement. Selects
// locomotion/next-frame rate; never advances either animator clock.
bool selectWalkAnimation(Navi*,std::string&);
// Common FakePiki locomotion for every genuine source state. Bound motion
// controls selection; locked Self motion survives. Advances Self then Bound
// using the prior simulation's actual rate, then selects the next motion/rate.
// Actual body owner supplies eligible postmove facts from the common body phase;
// movieMotion/mapless/stuck routes require their own literal source phase owner.
// Source listeners and generation guards apply; frozen skips both clocks.
// Controlled-test convenience composition ONLY. Production must invoke the
// two phase functions above at their actual source body phases independently.
bool animateWalk(Navi*,const std::function<bool(Animator,Listener,int)>& emit,std::string&);
// Untyped legacy receiver is valid only when neither animator has SourceState.
bool animateWalk(Navi*,const std::function<bool(int)>& emit,std::string&);
void forget(Navi*);
}}}
const p2original::captain::nativecontrol::Effects* pc_p2_original_captain_control_effects(const Navi*);
