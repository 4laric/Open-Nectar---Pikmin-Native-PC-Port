#pragma once
#include "pc_p2_original_captain_damage.h"
#include "pc_p2_original_captain_motion.h"
#include "pc_p2_original_captain_native_control.h"
#include <memory>
namespace p2original {namespace captain {namespace bodyphases {
using Vec3=control::Vec3;
struct Sphere {Vec3 center;float radius=8.5f;};
// Scene-owned numeric trace identity, never a native P1 floor alias. Only the
// concrete SourceSceneTrace produces/validates these handles and their facts.
struct FloorHandle {const void* triangle=nullptr;std::uint64_t incarnation=0;};
struct FloorFacts {unsigned slip=0,contents=0;Vec3 planeNormal;};
struct TraceInfo {Sphere sphere;Vec3 velocity,floorNormal,wallNormal;FloorHandle floor,wall;float traceRadius=0;int roomIndex=-1;};
class SourceSceneTrace {
public:
 virtual ~SourceSceneTrace()=default;
 virtual const LoadedScene& scene()const=0;
 virtual bool floor(FloorHandle,FloorFacts&,std::string&)const=0;
 virtual bool map(Navi&,TraceInfo&,float rate,std::string&)=0;
 virtual bool platforms(Navi&,TraceInfo&,float rate,std::string&)=0;
 virtual bool constrain(Navi&,Sphere&,std::string&)=0;
 virtual bool room(Navi&,int,std::string&)=0;
};
struct Facts {
 float deltaTime=0,gravity=0;
 std::optional<bool> mapPresent,movieMotion,movieActor,movieExtra,gameFrozen;
 std::optional<bool> movieActive,naviManagerFlag1,stuck,targetCollision;
 std::optional<bool> platformsPresent,hiddenCollision,inWater,rushBoots;
 std::optional<bool> gamePaused,managerSlotOpen;std::optional<unsigned> frameTimer;
};
// Source fields initialized by Creature::init/FakePiki::initFakePiki and owned
// independently of P1 floor/flags/acceleration. previous is unknown until the
// actual animation phase assigns it. Serial changes on every genuine cold bind.
struct Fields {
 std::uint64_t initializationSerial=0;unsigned fpFlags=0;
 Vec3 acceleration,simPosition,floorNormal{0,1,0};
 FloorHandle floor,fakeBounce;
 std::optional<Vec3> previous;std::optional<float> faceDirectionOffset;
 std::optional<Sphere> bounding;float boundingRadius=8.5f,traceRadius=0;int roomIndex=-1;bool dontUseWallCallback=false;
};
enum class UpdateEvent {SoundExec,DemoCheck,Look,LookCreature,Effects,Footmarks,VersusCard};
class Provider {
public:
 virtual ~Provider()=default;
 virtual const LoadedScene& scene()const=0;
 virtual bool facts(const Navi&,Facts&,std::string&)const=0;
 virtual bool cellLOD(Navi&,float far,float close,std::string&)=0;
 virtual bool animationKey(Navi&,Animator,Listener,int,std::string&)=0;
 // Genuine water owner owns its cache. This is exactly after PreviousPosition
 // and eligible velocity/rotation, before gravity/geometry; no duplicate cache.
 virtual bool water(Navi&,const Sphere&,std::string&)=0;
 // Update source transform iff requested, then selected model/collision data.
 virtual bool geometry(Navi&,bool updateTransform,std::string&)=0;
 virtual bool cursor(Navi&,std::string&)=0;
 virtual bool event(Navi&,UpdateEvent,std::string&)=0;
 virtual bool menus(Navi&,bool& handled,std::string&)=0;
 virtual bool plateUpdate(Navi&,std::string&)=0;
 virtual bool bounce(Navi&,FloorHandle,std::string&)=0;
 virtual bool wall(Navi&,Vec3,std::string&)=0;
 virtual bool random(float&,std::string&)=0;
 virtual bool walkEffect(Navi&,bool waterTerrain,std::string&)=0;
 virtual bool belowMap(const Navi&,bool&,std::string&)const=0;
 virtual bool recoverBelowMap(Navi&,std::string&)=0;
 virtual bool managerSimulationEnded(std::string&)=0;
};
class Owner {
public:
 static std::unique_ptr<Owner> create(Provider&,SourceSceneTrace&,SourceBank&,std::string&);
 ~Owner();
 bool initializeAfterBodyReset(Navi*,std::string&);
 bool animation(Navi*,std::string&);
 bool simulation(Navi*,float actualRate,std::string&);
 bool update(Navi*,std::string&);
 bool managerAnimation(std::string&);
 bool managerSimulation(float actualRate,std::string&);
 bool readFields(const Navi*,Fields&,std::string&)const;
 // Read-only proof of the exact private post-map room callback. A body guard
 // outside this simulation phase cannot authorize scene visit mutation.
 bool roomVisitCurrent(const Navi*,const SourceSceneTrace&,int room,std::string&)const;
 bool setMoveRotation(Navi*,bool,std::string&);
 bool canRetire(std::string&)const; // actual in-flight callback ownership
 void forget(Navi*)noexcept;
private:
 struct Impl;std::unique_ptr<Impl> m;explicit Owner(std::unique_ptr<Impl>);
};
// Root's canonical actual stage owner publishes this real composition; no
// workflow packet/ready setter, accepting null substitute or lazy P1 fallback.
bool body_animation(Navi*,std::string&);
bool body_simulation(Navi*,float actualRate,std::string&);
bool body_update(Navi*,std::string&);
bool setMoveRotation(Navi*,bool,std::string&);
bool readFlag(const Navi*,unsigned mask,bool&,std::string&);
}}}
p2original::captain::bodyphases::Owner* pc_p2_original_captain_body_phase_owner(const Navi*);
