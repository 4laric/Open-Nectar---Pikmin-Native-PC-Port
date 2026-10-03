#pragma once
#include "pc_p2_original_captain_throw.h"
namespace Sys {struct Triangle;}
namespace p2original {namespace captain {namespace physical {
using actions::Vec3;
struct BodyFrame {Vec3 position,scale,velocity,targetVelocity;float face=0,delta=0;};
struct Contact {const Sys::Triangle* triangle=nullptr;std::uint64_t lifetime=0;};
struct Landing {Contact contact;bool inWater=false;float seaHeight=0;};
// Concrete original body/commonMove contact authority. A queried contact must
// originate in the actual actor's physical bounce, including source WaterBox.
// No P1 water flag, fabricated floor, scripted contact or absent-height zero.
class PhysicalSource {
public:
 virtual ~PhysicalSource()=default;
 virtual const LoadedScene& scene()const=0;
 virtual bool frame(const Navi&,BodyFrame&,std::string&)const=0;
 virtual bool scale(Navi&,Vec3,std::string&)=0;
 virtual bool updateTrMatrix(Navi&,bool,std::string&)=0;
 virtual bool baseSRT(Navi&,Vec3 scale,Vec3 rotation,Vec3 position,std::string&)=0;
 virtual bool atari(Navi&,bool,std::string&)=0;
 virtual bool damageSoundAndOptionalDirector(Navi&,std::string&)=0;
 virtual bool velocities(Navi&,Vec3 actual,Vec3 target,std::string&)=0;
 virtual bool endStick(Navi&,std::string&)=0;
 virtual bool landing(const Navi&,Contact,Landing&,std::string&)const=0;
 virtual bool landingEffect(Navi&,bool water,Vec3 position,float scale,std::string&)=0;
 virtual bool nudgeRumble(Navi&,std::string&)=0;
 // Genuine Koke owner preflights/transfers NaviKokeDamageInitArg(1,0,null,
 // damage). No P1 flick state or raw numeric transition may implement these.
 virtual bool preflightKoke(const Navi&,float damage,std::string&)const=0;
 virtual bool enterKoke(Navi&,float damage,std::string&)=0;
};
// Null retail FallMeck arg means damage=0; an explicit arg retains its float.
bool beginFallMeck(Navi*,std::optional<float> damage,std::string&);
bool bounce(Navi*,Contact,std::string&);
} void registerPressedFallMeckStates(NaviStateMachine&);
}}
p2original::captain::physical::PhysicalSource* pc_p2_original_captain_physical_source(const Navi*);
bool pc_p2_original_captain_pressed_fall_preflight(Navi*,p2original::captain::StateId,std::string&);
