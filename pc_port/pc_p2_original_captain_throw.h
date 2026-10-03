#pragma once
#include "pc_p2_original_captain_damage.h"
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
class NaviStateMachine; class Piki;
namespace p2original { namespace captain { namespace actions {
struct Vec3 { float x=0,y=0,z=0; };
// Handles are source actor identities, not P1 state labels or numeric slots.
struct PikiHandle { Piki* actor=nullptr; std::uint64_t lifetime=0; };
enum class PikiState { Unsupported, Walk, GoHang, Hanged, Flying };
struct PikiFrame {
 PikiHandle handle; Vec3 position; PikiState state=PikiState::Unsupported;
 unsigned kind=0,happa=0; bool throwable=false; Navi* captain=nullptr;
};
struct ActorFrame {
 Vec3 position,velocity,hand,cursor; float face=0,delta=0,sceneAnimationTimer=0;
 bool controller=false,heldA=false,heldB=false,pressedA=false,pressedB=false,releasedB=false;
 bool right=false,left=false,up=false,down=false;
 // Literal source Navi::mThrowTimer; absent is refusal, never P1 cooldown.
 std::optional<std::uint8_t> throwDisableFrames;
 std::optional<Vec3> firstFormationSlot;
};
struct WhistleFrame { Vec3 cursor; float radius=0; bool timedOut=false; };
enum class GatherMode { Player, Automatic };
enum class Feedback { GatherStart, GatherLoop, GatherStop, AutomaticGatherStart,
 AutomaticGatherStop, Grab, PikiChange, StopHold, Throw };
// Canonical original body/party owner supplies concrete methods. Each query
// must authenticate actual source Piki lifetime/FSM/party; no P1 action
// delegates. scene() MUST be the same object as the canonical LoadedScene.
// A missing implementation refuses activation rather than using native P1.
class ActionSource {
public:
 virtual ~ActionSource()=default;
 virtual const LoadedScene& scene() const=0;
 virtual const std::string& parameterBytes() const=0; // selected GPVE01 naviParms
 virtual bool frame(const Navi&,ActorFrame&,std::string&) const=0;
 virtual bool squad(const Navi&,std::vector<PikiFrame>&,std::string&) const=0;
 virtual bool piki(const Navi&,PikiHandle,PikiFrame&,std::string&) const=0;
 virtual bool control(Navi&,std::string&)=0; // actual source control, not P1 Navi::control
 virtual bool whistle(const Navi&,WhistleFrame&,std::string&) const=0;
 virtual bool startWhistle(Navi&,std::string&)=0;
 virtual bool stopWhistle(Navi&,std::string&)=0;
 virtual bool updateWhistle(Navi&,Vec3,bool automatic,std::string&)=0;
 // Actual source sphere/CellIterator then InteractFue(false,true), including
 // partner captains. The shared party owner must implement source receivers.
 virtual bool callPikis(Navi&,std::string&)=0;
 virtual bool transitionPiki(Navi&,PikiHandle,PikiState,std::string&)=0;
 virtual bool positionPiki(Navi&,PikiHandle,Vec3,std::string&)=0;
 virtual bool sortFormation(Navi&,PikiHandle,int happa,std::string&)=0;
 virtual bool holdFields(Navi&,float timer,float distance,float height,std::string&)=0;
 virtual bool nextThrowPiki(Navi&,std::optional<PikiHandle>,std::string&)=0;
 virtual bool findNextThrowPiki(Navi&,std::string&)=0;
 // Actual body owner validates finite source gravity/getThrowHeight and
 // executes Navi::throwPiki ballistic mechanics BEFORE source Flying.
 virtual bool throwPiki(Navi&,PikiHandle,Vec3 cursor,std::string&)=0;
 virtual bool feedback(Navi&,Feedback,PikiHandle,std::string&)=0;
};
} // actions
void registerThrowStates(NaviStateMachine&);
} }
// Canonical source body/party lookup; missing producer is an explicit refusal.
p2original::captain::actions::ActionSource* pc_p2_original_captain_action_source(const Navi*);
// Source rhnd pose must already be updated. Root wires this actual animation
// callback; no synthetic hand coordinates or P1 animation delegates.
bool pc_p2_original_captain_throw_after_animation(Navi*,std::string&);
// Root calls in the actual source animation phase, even when FSM exec returns
// early. Call after-animation only AFTER the resulting rhnd pose is refreshed.
bool pc_p2_original_captain_throw_advance_animation(Navi*,float frames,std::string&);
bool pc_p2_original_captain_throw_preflight(Navi*,p2original::captain::StateId,std::string&);
// Typed retail NaviGatherArg._00; transfer is scoped to canonical actor/scene.
bool pc_p2_original_captain_begin_gather(Navi*,p2original::captain::actions::GatherMode,std::string&);
