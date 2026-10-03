#pragma once
#include "pc_p2_original_captain_throw.h"
#include "pc_p2_original_captain_actions_party.h"
class CollPart;
namespace p2original { namespace captain { namespace punch {
using actions::Vec3;
struct Sphere { Vec3 center;float radius=0; };
struct TargetHandle { Creature* actor=nullptr;std::uint64_t lifetime=0; };
struct TargetFrame {
 TargetHandle handle;std::uint64_t collisionTree=0;
 bool alive=false,navi=false,teki=false;
};
struct PartHandle {
 TargetHandle target;CollPart* part=nullptr;std::uint64_t collisionTree=0;
};
struct PartFrame { PartHandle handle;Vec3 position; };
enum class RedPikminFlag { Unknown,NotMet,Met };
enum class Feedback { Swing,Hit,StoneHit };
// Source collision/receiver owner. Canonical scene identity is required in
// addition to actual lifetime/tree handles; no native P1 enemy attack delegate.
class PunchSource {
public:
 virtual ~PunchSource()=default;
 virtual const LoadedScene& scene()const=0;
 virtual bool redPikminFlag(RedPikminFlag&,std::string&)const=0;
 // Actual source CellIterator order, including non-enemy punchable objects.
 virtual bool query(const Navi&,Sphere,std::vector<TargetFrame>&,std::string&)const=0;
 virtual bool target(TargetHandle,TargetFrame&,std::string&)const=0;
 // Actual source CollTree::checkCollision callback order and live part positions.
 virtual bool collision(TargetHandle,Sphere,std::vector<PartFrame>&,std::string&)const=0;
 // Re-read the same live part AFTER stimulate(), matching retail feedback.
 virtual bool part(PartHandle,PartFrame&,std::string&)const=0;
 // Literal source InteractAttack(captain,rawDamage,part). accepted is the
 // genuine receiver result, not an authorization flag or assumed success.
 virtual bool attack(Navi&,PartHandle,float rawDamage,bool& accepted,std::string&)=0;
 // Hit positions are source-normalized(part-rhnd)*15+rhnd; Swing has no effect.
 virtual bool feedback(Navi&,Feedback,Vec3,std::string&)=0;
 // Literal source enableMotionBlend after each PUNCH start; authentic pose owner.
 virtual bool enableMotionBlend(Navi&,std::string&)=0;
};
// Typed source NaviPunchArg, scoped to actual canonical actor/scene. Following
// disallows player A combo queuing and returns to the requested source state.
bool begin(Navi*,bool following,StateId next,std::string&);
bool hitSphere(const actions::ActorFrame&,bool thirdPunch,Sphere&,std::string&);
bool effectPosition(Vec3 part,Vec3 hand,Vec3&,std::string&);
} // punch
void registerPunchState(NaviStateMachine&);
} }
p2original::captain::punch::PunchSource* pc_p2_original_captain_punch_source(const Navi*);
bool pc_p2_original_captain_punch_preflight(Navi*,std::string&);
bool pc_p2_original_captain_punch_advance_animation(Navi*,float actualFrames,std::string&);
