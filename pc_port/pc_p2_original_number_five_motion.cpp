#include "pc_p2_original_number_five_motion.h"
#include "pc_p2_original_number_triangle.h"
#include <cmath>
#include <limits>

// Primary research pelletMgr.cpp:2048-2240,3409-3426,3434-3473.
// Particle halfstep/contact/math primary is in number_rigid.cpp references.
namespace p2originalnumber { namespace fiveMotion {
namespace {
Vec3 add(Vec3 a,Vec3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
Vec3 sub(Vec3 a,Vec3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
Vec3 mul(Vec3 a,float b){return {a.x*b,a.y*b,a.z*b};}
float fused(float a,float b,float c){float out;if(!triangle::sourceFma(a,b,c,out))return std::numeric_limits<float>::quiet_NaN();return out;}
float dot(Vec3 a,Vec3 b){return fused(a.z,b.z,fused(a.x,b.x,a.y*b.y));}
float length(Vec3 a){float out;if(!triangle::sourceVectorLength(a,out))return std::numeric_limits<float>::quiet_NaN();return out;}
bool finite(Vec3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
bool fail(std::string& e,const char* message){e=message;return false;}
bool normal(Vec3 n){return finite(n)&&std::isfinite(dot(n,n))&&std::fabs(dot(n,n)-1.f)<=.001f;}
bool valid(const Contacts& c){return (!c.floor||normal(c.floorNormal))&&(!c.wall||normal(c.wallNormal));}
void wallTimer(State& s,bool picked,const Contacts& contacts,Vec3 direction){
 if(!picked){s.wallTimer=0;return;}
 bool blocked=contacts.wall;
 if(blocked&&dot(direction,contacts.wallNormal)>.5f)blocked=false;
 if(blocked){if(s.wallTimer<100)s.wallTimer+=2;}
 else if(s.wallTimer)s.wallTimer--;
}
bool trace(TraceProvider& provider,bool platform,rigid::Trace& request,float dt,
 rigid::ContactReceiver* receiver,Contacts& contacts,Report& report,std::string& e){
 if(!finite(request.position)||!finite(request.velocity)||!std::isfinite(request.radius)||request.radius<=0||!std::isfinite(request.restitution))return fail(e,"Five trace input invalid");
 const float radius=request.radius,restitution=request.restitution;const bool hard=request.hardIntersect;
 if(platform){++report.platformCalls;if(!provider.tracePlatforms(request,dt,receiver,contacts,e))return false;}
 else{++report.mapCalls;if(!provider.traceMap(request,dt,receiver,contacts,e))return false;}
 if(!finite(request.position)||!finite(request.velocity)||request.radius!=radius||request.restitution!=restitution||request.hardIntersect!=hard||!valid(contacts))return fail(e,"Five actual trace returned invalid state/policy/contacts");
 return true;
}
bool bounce(State& state,const Options& options,BouncePhase phase,Report& report,std::string& e){
 if(options.events){if(!options.events->bounce(state,phase,e))return false;++report.executedBounceEvents;}
 return true;
}
// Same literal DynCreature::simulate loop as the pure rigid helper, composed
// here because its report-only receiver cannot execute a source-phase event.
// Math/integration/impulse stay in the shared audited rigid primitives.
bool halfStep(State& state,float dt,const rigid::Parameters& params,const Options& options,
 TraceProvider& provider,Report& report,std::string& e){
 auto& body=state.body;body.canBounce=body.hasCollided;body.hasCollided=false;
 rigid::State transformed=body;
 if(!rigid::setBaseTransform(transformed,body.baseRotation,body.basePosition,e))return false;
 body.transformedPosition=transformed.transformedPosition;
 if(!rigid::integrate(body,dt,e))return false;
 class Receiver final:public rigid::ContactReceiver {
  State& state;rigid::Particle& particle;const rigid::Parameters& params;
  const Options& options;Report& report;
 public:
  bool faulted=false;
  Receiver(State& s,rigid::Particle& p,const rigid::Parameters& par,const Options& o,Report& r)
   :state(s),particle(p),params(par),options(o),report(r){}
  bool contact(Vec3 point,Vec3 normal,std::string& e)override{
   const auto result=rigid::resolveCollision(state.body,point,normal,params.elasticity,e);
   if(result==rigid::CollisionResult::Invalid){faulted=true;return false;}
   if(result==rigid::CollisionResult::Accepted){
    if(!state.body.canBounce){
     ++report.particleBounceCallbacks;
     if(!bounce(state,options,BouncePhase::AcceptedParticleContact,report,e)){faulted=true;return false;}
    }
    state.body.hasCollided=true;particle.touching=true;particle.collisionNormal=normal;
    ++report.acceptedParticleContacts;
   }
   return true;
  }
 };
 for(unsigned i=0;i<body.particles.size();++i){
  auto& particle=body.particles[i];particle.position=transformed.particles[i].position;
  const Vec3 sep=sub(particle.position,body.transformedPosition),omega=body.current.rotatedMomentum;
  const Vec3 angular{fused(omega.y,sep.z,-(omega.z*sep.y)),fused(omega.z,sep.x,-(omega.x*sep.z)),fused(omega.x,sep.y,-(omega.y*sep.x))};
  const Vec3 velocity=add(angular,body.current.velocity);
  float extra=dt*length(velocity);if(extra>50)extra=50;
  particle.touching=false;
  rigid::Trace request{particle.position,velocity,particle.radius+extra,1,true};
  Receiver receiver(state,particle,params,options,report);Contacts contacts;
  if(!trace(provider,false,request,dt,&receiver,contacts,report,e))return false;
  if(receiver.faulted)return fail(e,"Five map ignored event/contact refusal");
  request.hardIntersect=false;
  if(!trace(provider,true,request,dt,&receiver,contacts,report,e))return false;
  if(receiver.faulted)return fail(e,"Five platform ignored event/contact refusal");
 }
 return true;
}
}
bool update(State& out,float dt,const rigid::Parameters& params,const Options& options,TraceProvider* provider,Report& outReport,std::string& e){
 if((options.requireGameplayEvents&&!options.events)||!provider||!options.sourceLod||!std::isfinite(dt)||dt<=0||!out.body.initialized||!finite(out.acceleration)||out.wallTimer>101)return fail(e,"Five actual update input/LOD/trace unavailable");
 State s=out;Report report;
 report.rigid=!options.disableDynamics&&lod::usesRigidFive(*options.sourceLod);
 // Validate the full existing source rigid state without changing its base
 // transform or performing an integration in the simple/resting branches.
 rigid::State checked=s.body;
 if(!rigid::setBaseTransform(checked,checked.baseRotation,checked.basePosition,e))return false;
 if(!report.rigid){
  auto& c=s.body.current;c.momentum={};c.rotatedMomentum={};
  Vec3 velocity;
  if(!motion::beginSimple(c.velocity,dt,options.picked,options.alwaysCarried,s.previousFloor,velocity))return fail(e,"Five simple gravity produced invalid state");
  s.acceleration.y=0;
  if(options.collisionFlick&&options.pelletFlag!=1&&!options.picked&&!options.alwaysCarried)velocity=add(velocity,s.acceleration);
  s.acceleration={};
  rigid::Trace request{c.position,velocity,7,.5f,false};
  if(options.picked)request.position.y-=4;
  Contacts contacts;
  if(!trace(*provider,false,request,dt,nullptr,contacts,report,e))return false;
  // Retail simple wall timer uses map result BEFORE platform trace.
  wallTimer(s,options.picked,contacts,request.velocity);
  if(!trace(*provider,true,request,dt,nullptr,contacts,report,e))return false;
  c.velocity=request.velocity; // source velocity pointer is live before bounce
  if(contacts.floor){
   report.firstSimpleBounce=!s.previousFloor;
   if(report.firstSimpleBounce&&!bounce(s,options,BouncePhase::FirstSimpleFloor,report,e))return false;
   // Retail re-reads *velocityPtr after bounceCallback, before floor forces.
   request.velocity=c.velocity;
   if(!motion::finishSimple(request.velocity,contacts.floorNormal,dt,options.picked,options.alwaysCarried,request.velocity))return fail(e,"Five simple floor force produced invalid state");
  }
  s.previousFloor=contacts.floor;
  if(options.picked)request.position.y+=4;
  c.position=request.position;c.velocity=request.velocity;report.movementContacts=contacts;
 }else{
  if(!rigid::computeForces(s.body,params,!options.picked&&!options.alwaysCarried,e))return false;
  auto& c=s.body.current;c.force.y=-motion::gravity;
  float speed=length(c.velocity),momentum=length(c.momentum);
  if(!std::isfinite(speed)||!std::isfinite(momentum))return fail(e,"Five resting magnitudes overflow");
  bool simulate=true;
  if(options.normalState&&s.body.hasCollided&&!options.picked&&speed<10&&momentum<100&&!options.alwaysCarried){
   rigid::Trace probe{c.position,{0,-motion::gravity,0},7,0,false};probe.position.y-=7;
   Contacts contacts;
   if(!trace(*provider,false,probe,dt,nullptr,contacts,report,e))return false;
   if(!contacts.floor&&!trace(*provider,true,probe,dt,nullptr,contacts,report,e))return false;
   if(contacts.floor){simulate=false;report.resting=true;}
  }
  const Vec3 previousPosition=c.position;
  if(simulate){
   if(options.collisionFlick&&!options.picked&&!options.alwaysCarried){s.acceleration.y=0;c.velocity=add(c.velocity,s.acceleration);}
   for(unsigned i=0;i<2;++i){
    if(!halfStep(s,dt/2.f,params,options,*provider,report,e))return false;
    ++report.halfSteps;
   }
  }
  const float frames=1.f/dt;const Vec3 derivedVelocity=mul(sub(c.position,previousPosition),frames);
  if(!finite(derivedVelocity))return fail(e,"Five final center velocity overflow");
  rigid::Trace center{previousPosition,derivedVelocity,7,.5f,false};Contacts contacts;
  if(!trace(*provider,false,center,dt,nullptr,contacts,report,e)||!trace(*provider,true,center,dt,nullptr,contacts,report,e))return false;
  // Source uses ORIGINAL derived velocity for wall timer; trace may change it.
  wallTimer(s,options.picked,contacts,derivedVelocity);
  c.position.x=center.position.x;c.position.z=center.position.z;
  // Intentionally retain rigid Y and velocity instead of copying center trace.
  speed=length(c.velocity);const float acceleration=length(s.acceleration);
  if(!std::isfinite(speed)||!std::isfinite(acceleration))return fail(e,"Five final velocity magnitude overflow");
  Vec3 direction=c.velocity;if(speed>0)direction=mul(direction,1.f/speed);
  c.velocity=mul(direction,speed>acceleration?speed-acceleration:speed);
  s.acceleration={};report.movementContacts=contacts;
 }
 if(!finite(s.body.current.position)||!finite(s.body.current.velocity))return fail(e,"Five update produced invalid state");
 out=s;outReport=report;e.clear();return true;
}
bool refreshBaseTransform(State& out,std::string& e){
 // In Number setup the source particle-origin average is retained; source
 // updateTrMatrix/doAnimation sets translation explicitly to body position.
 return rigid::setBaseTransform(out.body,out.body.current.rotation,out.body.current.position,e);
}
}}
