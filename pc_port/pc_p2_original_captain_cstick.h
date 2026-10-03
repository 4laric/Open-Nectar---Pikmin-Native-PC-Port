#pragma once
#include "pc_p2_original_captain_control.h"
#include "pc_p2_original_captain_throw.h"
#include <cstdint>
#include <optional>
namespace p2original {namespace captain {namespace cstick {
using Vec3=actions::Vec3;
class Parameters {
public:
 bool authenticated()const{return authenticated_;}
 float waitRange()const{return wait_;}float changeRange()const{return change_;}
private:
 bool authenticated_=false;float wait_=0,change_=0;
 friend bool parseParameters(const std::string&,Parameters&,std::string&);
};
bool parseParameters(const std::string& selectedNaviBytes,Parameters&,std::string&);
// Only these two fields are assigned by retail Navi::onInit (104-111).
// The other source fields remain unknown until an actual source branch writes
// them. Do not replace unknowns with native P1 flags or guessed initial values.
struct State {
 std::optional<std::uint8_t> scaleTimer,neutralTurn,needRearrange;
 std::optional<int> distanceState,increment;
 std::optional<bool> commandOn1,commandOn2;
 std::optional<float> angle;
 std::optional<Vec3> position,targetVector;
};
void onInit(State&); // caller invokes only at genuine source actor onInit
struct Input {
 bool controller=false;std::optional<bool> demoInactive;
 float stickX=0,stickY=0;
 control::CameraBasis camera;
 Vec3 position,velocity;
 std::optional<Vec3> targetVelocity;
 float face=0;std::optional<StateId> sourceState;
 std::optional<float> plateAngle;
 unsigned slotCount=0;
};
enum class CommandKind {Refresh,SetPos,SetPosGray,Rearrange};
struct Command {
 CommandKind kind=CommandKind::Refresh;
 unsigned count=0;float strength=0;
 Vec3 position,velocity;float angle=0,scale=1;
};
struct AfterRefresh {
 // Entire actual CPlate census, including CF-dead/unreleasable members. The
 // producer authenticates exact handles and obtains these AFTER Refresh.
 std::vector<actions::PikiFrame> members;
 std::optional<Vec3> maxPositionOffset;
};
struct Plan;
class NeutralContinuation {
public:
 const State& prefixState()const{return state_;}
private:
 State state_;Vec3 position_,velocity_;float face_=0;bool issued_=false;
 friend bool prepare(const Parameters&,const Input&,const State&,Plan&,std::string&);
 friend bool completeNeutral(const Parameters&,const NeutralContinuation&,const AfterRefresh&,Plan&,std::string&);
};
struct Plan {
 State state;std::vector<Command> commands;
 bool resetSceneAnimationTimer=false;
 std::optional<NeutralContinuation> neutral;
};
// makeCStick(false): active output is complete; neutral output is an ordered
// prefix ending with Refresh. Execute that prefix through the real Plate,
// then supply fresh observations to completeNeutral. The source owner checks
// actor/state/scene/generation across callbacks and commits actual fields in
// source order. Pure plans are neither lifetime proofs nor permission grants.
bool prepare(const Parameters&,const Input&,const State&,Plan&,std::string&);
bool completeNeutral(const Parameters&,const NeutralContinuation&,const AfterRefresh&,Plan&,std::string&);
}}}
