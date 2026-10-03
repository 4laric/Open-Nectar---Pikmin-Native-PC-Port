#pragma once
#include "pc_p2_original_number_lod.h"
#include "pc_p2_original_number_motion.h"

namespace p2originalnumber { namespace fiveMotion {
using Vec3=rigid::Vec3;
struct Contacts {
 // Actual solver results, including map/hidden/platform floor semantics.
 // Do not infer floor presence from a callback normal or model position.
 bool floor=false,wall=false;
 Vec3 floorNormal,wallNormal;
};
class TraceProvider {
public:
 virtual ~TraceProvider()=default;
 // Update the SAME contacts result through map then platforms, just as
 // source MoveInfo. Null receiver means no rigid collision callback.
 virtual bool traceMap(rigid::Trace&,float dt,rigid::ContactReceiver*,Contacts&,std::string& error)=0;
 virtual bool tracePlatforms(rigid::Trace&,float dt,rigid::ContactReceiver*,Contacts&,std::string& error)=0;
};
struct State {
 rigid::State body;
 Vec3 acceleration;
 bool previousFloor=false; // source mFloorTriangle; rigid branch retains it
 unsigned wallTimer=0;
};
enum class BouncePhase { FirstSimpleFloor, AcceptedParticleContact };
class Events {
public:
 virtual ~Events()=default;
 // Called at the literal source phase. State is the pending motion snapshot,
 // not committed native state. Return false after any lifetime/error refusal.
 // Particle bounce follows resolveCollision and PRECEDES touching/hasCollided
 // assignment and solver sphere correction; simple bounce precedes floor
 // assignment/floor forces. Provider owns the actual triangle association.
 virtual bool bounce(const State&,BouncePhase,std::string& error)=0;
};
struct Options {
 // Supplied by actual source camera/section adapter, not computed here.
 const lod::Result* sourceLod=nullptr;
 Events* events=nullptr;
 bool requireGameplayEvents=false; // actual wrapper must opt in; controls may omit
 bool disableDynamics=false,picked=false,alwaysCarried=false;
 bool normalState=true,collisionFlick=false;
 unsigned pelletFlag=0;
};
struct Report {
 bool rigid=false,resting=false,firstSimpleBounce=false;
 unsigned halfSteps=0,acceptedParticleContacts=0,particleBounceCallbacks=0;
 unsigned executedBounceEvents=0; // counts actual successful Events invocations
 unsigned mapCalls=0,platformCalls=0;
 Contacts movementContacts;
};
// Direct Vector3 lengths/dots use the audited source primitive; dependent
// rigid integration arithmetic and original hardware identity remain separate.
// Source Pellet::update Five motion only (after actual carry/FSM/LOD).
// Positive finite dt required. State/report unchanged on refusal; caller's
// trace-side external mutations cannot be rolled back. Neither callbacks nor
// tests qualify actual world geometry, camera, birth, gameplay or SAVE.
bool update(State&,float dt,const rigid::Parameters&,const Options&,TraceProvider*,Report&,std::string& error);
// Actual post-update transform/animation phase: held base transform during
// both halfsteps is then replaced by current source body quaternion/position.
// Does not alter the rigid state orientation using the carry BCK animation.
bool refreshBaseTransform(State&,std::string& error);
}}
