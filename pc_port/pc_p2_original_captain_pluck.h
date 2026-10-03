#pragma once
#include "pc_p2_original_captain_throw.h"
class Creature;
namespace p2original {namespace captain {namespace pluck {
using actions::Vec3;using actions::PikiHandle;
struct HeadHandle {Creature* actor=nullptr;std::uint64_t lifetime=0;};
struct HeadFrame {HeadHandle handle;Vec3 position;unsigned color=0,happa=0;bool alive=false;};
struct BodyFrame {std::uint8_t pluckingCounter=0;};
enum class Feedback {Pulling,Pullout};
// Exact canonical source body/ItemPikihead/PikiMgr owner. No P1 birth/action
// adapter is an implementation. Queries validate actual actor/tree lifetimes.
class PluckSource {
public:
 virtual ~PluckSource()=default;
 virtual const LoadedScene& scene()const=0;
 virtual bool body(const Navi&,BodyFrame&,std::string&)const=0;
 virtual bool head(HeadHandle,HeadFrame&,std::string&)const=0;
 virtual bool setPluckingCounter(Navi&,std::uint8_t,std::string&)=0;
 virtual bool setMass(Navi&,float,std::string&)=0;
 virtual bool setMoveRotation(Navi&,bool,std::string&)=0;
 virtual bool setFace(Navi&,float,std::string&)=0;
 virtual bool setVelocities(Navi&,Vec3 actual,Vec3 target,std::string&)=0;
 virtual bool makeCStick(Navi&,bool,std::string&)=0;
 virtual bool markFirstPluck(std::string&)=0; // source DEMO_Pluck_First_Pikmin
 // Source PSM_Force around birth, then restore PSM_Normal even on failure.
 // Empty is actual capacity/birth failure, never an invented native Piki.
 virtual bool birthForced(std::optional<PikiHandle>&,std::string&)=0;
 // Source init(nullptr), changeShape, changeHappa, setPosition(pos,false).
 virtual bool initializeBorn(PikiHandle,unsigned color,unsigned happa,Vec3,std::string&)=0;
 virtual bool killHead(HeadHandle,std::string&)=0;
 // Exact PIKISTATE_Nukare with NukareStateArg{counter!=0,actual captain}.
 virtual bool enterNukare(PikiHandle,Navi&,bool alreadyPlucking,std::string&)=0;
 virtual bool feedback(Navi&,Feedback,std::string&)=0;
 virtual bool startThrowDisable(Navi&,std::string&)=0; // literal actor mThrowTimer write
 // Actual procActionButton result; caller retains source continuation semantics.
 virtual bool actionButton(Navi&,bool& handled,std::string&)=0;
 // Source NaviFollowArg(false) for Nuku exits; Adjust passes nullptr.
 virtual bool follow(Navi&,bool explicitNotNew,std::string&)=0;
};
bool beginAdjust(Navi*,HeadHandle,bool following,std::string&);
bool beginNuku(Navi*,bool following,std::string&);
// Actual source native collision bridge must deliver these callbacks.
bool wall(Navi*,std::string&);
bool collision(Navi*,Vec3,bool isPiki,bool isNavi,bool collisionFlick,std::string&);
bool ignoreAtari(bool isNavi,bool isOnyon);
} void registerPluckStates(NaviStateMachine&);
}}
p2original::captain::pluck::PluckSource* pc_p2_original_captain_pluck_source(const Navi*);
bool pc_p2_original_captain_pluck_preflight(Navi*,p2original::captain::StateId,std::string&);
bool pc_p2_original_captain_pluck_advance_animation(Navi*,float,std::string&);
