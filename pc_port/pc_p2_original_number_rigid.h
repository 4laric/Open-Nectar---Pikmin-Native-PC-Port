#pragma once
#include <array>
#include <string>

// Source Five math/contact foundation only. No birth, identity, LOD, gameplay
// admission or SAVE implementation. Ordinary native DynCreature is not used.
namespace p2originalnumber { namespace rigid {
struct Vec3 { float x=0, y=0, z=0; };
struct Quaternion { float w=1, x=0, y=0, z=0; };
using Matrix3 = std::array<float,9>; // row-major
struct Config {
 Vec3 position, velocity, force, rotatedMomentum, momentum, torque;
 Quaternion rotation;
 Matrix3 rotatedTransform{{1,0,0,0,1,0,0,0,1}};
};
struct Particle {
 Vec3 local, position, collisionNormal;
 float radius=7;
 bool touching=false;
};
struct State {
 Config current, previous;
 Matrix3 transformation{{1,0,0,0,1,0,0,0,1}};
 float timeStep=1; // source inverse mass, despite its name
 bool limitTilt=true, hasCollided=false, canBounce=false, initialized=false;
 std::array<Particle,4> particles{};
 // Source mBaseTrMatrix remains unchanged during both halfsteps. The caller
 // updates this at the actual source transform/animation phase, not here.
 Quaternion baseRotation;
 Vec3 basePosition, particleOrigin, transformedPosition;
};
struct Parameters {
 // gameDynamics.cpp DynamicsParms constructor defaults. The audited source
 // constructs this globally; no read/load/override call was found. Explicit
 // fields retain both primary friction branches for a real audited override.
 bool newFriction=true;
 float staticParameter=140, staticThreshold=10, microCollision=.015f;
 bool frictionDuringResolve=false; // unused in audited resolveCollision
 float elasticity=.3f;
 bool friction=true, frictionTangentVelocity=true, fixedFriction=true;
 float fixedFrictionValue=100;
 bool noRotationEffect=true;
 float rotatingMomentDamp=.05f;
};
struct Trace {
 Vec3 position, velocity;
 float radius=0, restitution=1;
 bool hardIntersect=false;
};
enum class CollisionResult { Accepted, Separating, Invalid };
class ContactReceiver {
public:
 virtual ~ContactReceiver()=default;
 // Actual solver contact point/normal, BEFORE solver velocity/position
 // correction. Separating contacts do not mark a source particle touching.
 virtual bool contact(Vec3 point, Vec3 normal, std::string& error)=0;
};
class TraceProvider {
public:
 virtual ~TraceProvider()=default;
 virtual bool traceMap(Trace&,float dt,ContactReceiver&,std::string& error)=0;
 // Called after traceMap with its mutable result and hardIntersect=false.
 // A caller with no platform manager returns true without contacts/movement.
 virtual bool tracePlatforms(Trace&,float dt,ContactReceiver&,std::string& error)=0;
};
struct StepReport { unsigned acceptedContacts=0, bounceCallbacks=0; };
// Portable f32 source operation order, not a claim of PPC libm/FMA bit identity.
bool initializeFive(State&,Vec3 position,std::string& error);
bool setBaseTransform(State&,Quaternion rotation,Vec3 position,std::string& error);
// Clears force/torque and damps momentum once, then applies source contact
// friction. Caller omits friction when picked/always-carried, and sets source
// force.y=-560 AFTER this function, matching Pellet::update.
bool computeForces(State&,const Parameters&,bool applyFriction,std::string& error);
bool integrate(State&,float dt,std::string& error);
CollisionResult resolveCollision(State&,Vec3 point,Vec3 normal,float restitution,std::string& error);
// One source DynCreature::simulate halfstep; caller performs two at dt/2.
// Null/refusing trace and invalid math leave State/StepReport unchanged.
// Callback-side external mutations cannot be rolled back by this helper.
bool simulateHalfStep(State&,float dt,const Parameters&,TraceProvider*,StepReport&,std::string& error);
}}
