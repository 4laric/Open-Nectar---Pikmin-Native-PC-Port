#pragma once
#include "pc_p2_original_captain_control.h"
#include "pc_p2_original_captain_damage.h"
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
 // then source Rappa only where requested. Must not fail after preparation.
 // The plan is bound to exact scene incarnation/actor/current source State.
 virtual void commit(Navi&)=0;
};
struct AnimationFrame {
 // Source FakePiki position-minus-previousPosition (after real movement),
 // and NaviMgr's faceDirOffset captured before the actor update pass.
 control::Vec2 displacement;
 float deltaTime=0, faceDirectionOffset=0;
 bool selfIsJKoke=false;
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
// Actual native Walk/action states call once BEFORE inspecting post-control
// timer. Walk command application must not execute this same control twice.
bool control(Navi*,std::string&);
// Source Walk locomotion only: authored motion selection, frame-preserving
// Walk/Run/Escape transition and real event callback. No P1 Pani animator.
bool animateWalk(Navi*,const std::function<bool(int)>& emit,std::string&);
void forget(Navi*);
}}}
const p2original::captain::nativecontrol::Effects* pc_p2_original_captain_control_effects(const Navi*);
