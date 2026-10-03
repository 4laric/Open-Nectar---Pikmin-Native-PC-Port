#include "pc_p2_original_number_rigid.h"
#include <cmath>
#include <cstdio>
#include <limits>
#include <vector>
#include <cstdint>
#include <cstring>
#include <cfenv>
#if defined(__MINGW32__) && !defined(_RC_NEAR)
// Engine Dolphin/float.h shadows CRT float.h in the native include graph.
constexpr int upwardRounding=0x200;
#else
constexpr int upwardRounding=FE_UPWARD;
#endif
using namespace p2originalnumber::rigid;
namespace {
std::uint32_t bits(float value){std::uint32_t out;std::memcpy(&out,&value,sizeof(out));return out;}
bool bitVec(Vec3 v,std::uint32_t x,std::uint32_t y,std::uint32_t z){return bits(v.x)==x&&bits(v.y)==y&&bits(v.z)==z;}
bool near(float a,float b,float tolerance=.0001f){return std::fabs(a-b)<=tolerance;}
bool near(Vec3 a,Vec3 b){return near(a.x,b.x)&&near(a.y,b.y)&&near(a.z,b.z);}
float dot(Vec3 a,Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
float quatNorm(Quaternion q){return q.w*q.w+q.x*q.x+q.y*q.y+q.z*q.z;}
struct EmptyTrace:TraceProvider {
 std::vector<unsigned> order;
 std::vector<Trace> requests;
 bool traceMap(Trace& t,float,ContactReceiver&,std::string&)override{order.push_back(0);requests.push_back(t);t.position={999,999,999};t.velocity={100,200,300};return true;}
 bool tracePlatforms(Trace& t,float,ContactReceiver&,std::string&)override{order.push_back(1);requests.push_back(t);return true;}
};
struct ContactTrace:TraceProvider {
 unsigned index=0;bool separating=false,refuse=false,invalid=false;
 bool traceMap(Trace& t,float,ContactReceiver& receiver,std::string& e)override{
  ++index;if(index!=1)return true;
  if(invalid)return receiver.contact(t.position,{0,2,0},e);
  if(refuse){e="actual trace unavailable";return false;}
  return receiver.contact(t.position,{0,separating?-1.f:1.f,0},e);
 }
 bool tracePlatforms(Trace&,float,ContactReceiver&,std::string&)override{return true;}
};
struct BadTrace:TraceProvider {
 unsigned mode=0;
 bool traceMap(Trace& t,float,ContactReceiver& receiver,std::string& e)override{
  if(mode==0){receiver.contact(t.position,{0,2,0},e);return true;}
  if(mode==1)t.velocity.x=std::numeric_limits<float>::quiet_NaN();
  if(mode==2)t.radius=0;
  if(mode==3)t.hardIntersect=false;
  return true;
 }
 bool tracePlatforms(Trace& t,float,ContactReceiver&,std::string&)override{if(mode==4)t.hardIntersect=true;return true;}
};
}
int main(){
 unsigned checks=0,failures=0;
 auto check=[&](bool ok,const char* name){++checks;if(!ok){++failures;std::printf("FAIL %s\n",name);}};
 std::string error;Parameters parameters;State state;
 check(initializeFive(state,{10,20,30},error)&&error.empty(),"literal initialization");
 check(near(state.current.position,{10,20,30})&&near(state.previous.position,{10,20,30}),"both source configs initialized");
 check(state.timeStep==1&&state.limitTilt&&state.initialized&&!state.hasCollided,"source initial policy");
 check(near(state.transformation[0],.0029083333f)&&near(state.transformation[4],.005f)&&near(state.transformation[8],.0029083333f),"source Five inertia scaling, not reciprocal");
 for(unsigned i=0;i<4;++i){const auto& p=state.particles[i];check(p.radius==7&&!p.touching&&near(dot(p.local,p.local),169),"four radius7 ring13 particles");}
 check(near(state.particles[0].local,{0,0,13})&&near(state.particles[1].local,{13,0,0}),"source local particle order");
 check(parameters.newFriction&&parameters.staticParameter==140&&parameters.staticThreshold==10&&parameters.elasticity==.3f&&parameters.fixedFrictionValue==100&&parameters.rotatingMomentDamp==.05f,"literal global dynamics defaults");
 State invalid;check(!integrate(invalid,.1f,error),"uninitialized state refused");
 State before=state;check(!initializeFive(state,{std::numeric_limits<float>::infinity(),0,0},error)&&near(state.current.position,before.current.position),"invalid initial position preserves state");
 check(!setBaseTransform(state,{0,0,0,0},{},error)&&near(state.basePosition,before.basePosition),"zero base quaternion refused atomically");
 check(!integrate(state,-.1f,error)&&near(state.current.position,before.current.position),"negative timestep refused");
 check(!integrate(state,std::numeric_limits<float>::quiet_NaN(),error),"nonfinite timestep refused");
 State free;check(initializeFive(free,{},error),"free state init");free.limitTilt=false;free.current.velocity={3,4,5};free.current.momentum={10,20,30};
 check(integrate(free,.1f,error),"free integrate");
 check(near(free.current.position,{.3f,.4f,.5f})&&near(free.current.velocity,{3,4,5}),"free linear motion conserved");
 check(near(free.current.momentum,{10,20,30})&&bits(free.current.rotation.w)==0x3f8004f8u&&bits(free.current.rotation.x)==0x3abea123u&&bits(free.current.rotation.y)==0x3ba3dd67u&&bits(free.current.rotation.z)==0x3b8ef8dbu,"torque-free momentum conserved; source raw-estimate quaternion golden");
 check(near(free.previous.position,{})&&near(quatNorm(free.previous.rotation),1),"integration remembers pre-step position/orientation");
 State force;initializeFive(force,{},error);force.limitTilt=false;force.current.velocity={1,2,3};force.current.force={0,-560,0};force.current.torque={1,2,3};
 check(integrate(force,.05f,error)&&integrate(force,.05f,error),"two source halfsteps");
 check(near(force.current.position,{.1f,-1.2f,.3f})&&near(force.current.velocity,{1,-54,3}),"halfsteps use old velocity for position before force");
 check(near(force.current.momentum,{.1f,.2f,.3f}),"halfstep torque accumulation");
 State center;initializeFive(center,{},error);center.current.velocity={0,-10,0};
 check(resolveCollision(center,{}, {0,1,0},.3f,error)==CollisionResult::Accepted&&near(center.current.velocity,{0,3,0}),"known centered restitution impulse");
 check(near(center.current.momentum,{}),"center impulse has no torque");
 before=center;check(resolveCollision(center,{}, {0,1,0},.3f,error)==CollisionResult::Separating&&near(center.current.velocity,before.current.velocity),"separating impulse refused without mutation");
 check(resolveCollision(center,{}, {0,2,0},.3f,error)==CollisionResult::Invalid&&near(center.current.velocity,before.current.velocity),"nonunit contact normal refused");
 State rest;initializeFive(rest,{},error);check(resolveCollision(rest,{}, {0,1,0},.3f,error)==CollisionResult::Accepted&&near(rest.current.velocity,{}),"zero contact accepted with zero impulse");
 State off;initializeFive(off,{},error);off.limitTilt=false;integrate(off,0,error);off.current.velocity={0,-10,0};
 const float coefficient=off.transformation[8],expectedImpulse=13.f/(1.f+169.f*coefficient);
 check(resolveCollision(off,{13,0,0},{0,1,0},.3f,error)==CollisionResult::Accepted,"offcenter impulse accepted");
 check(near(off.current.velocity.y,-10+expectedImpulse)&&near(off.current.momentum.z,13*expectedImpulse),"analytic offcenter impulse denominator and torque");
 check(near(off.current.velocity.y+off.current.rotatedMomentum.z*13,3),"contact restitution includes angular velocity");
 const float kinetic=dot(off.current.velocity,off.current.velocity)+dot(off.current.momentum,off.current.rotatedMomentum);
 check(kinetic<100&&kinetic>=0,"inelastic contact reduces total kinetic energy");
 State friction;initializeFive(friction,{},error);friction.current.velocity={5,0,0};friction.current.momentum={100,20,30};
 for(auto& p:friction.particles){p.touching=true;p.collisionNormal={0,1,0};}
 check(computeForces(friction,parameters,true,error)&&bitVec(friction.current.force,0xc40c0637u,0,0),"four new-friction static contacts");
 check(near(friction.current.momentum,{95,19,28.5f})&&near(friction.current.torque,{}),"once-per-update source momentum damping and no new-friction torque");
 friction.current.velocity={20,0,0};check(computeForces(friction,parameters,true,error)&&bitVec(friction.current.force,0xc3c802bcu,0,0),"four new-friction kinetic contacts");
 check(computeForces(friction,parameters,false,error)&&near(friction.current.force,{}),"carried skips contact friction but retains damping");
 Parameters old=parameters;old.newFriction=false;old.rotatingMomentDamp=0;old.fixedFriction=false;old.noRotationEffect=false;
 State legacy;initializeFive(legacy,{},error);legacy.current.velocity={5,0,0};legacy.particles[0].touching=true;legacy.particles[0].collisionNormal={0,1,0};
 check(computeForces(legacy,old,true,error)&&bitVec(legacy.current.force,0xbe66698cu,0,0)&&bits(legacy.current.torque.y)==0xc03b35c2u,"audited old-friction override branch: fraction, literal Five .9 and torque");
 old.frictionTangentVelocity=false;check(computeForces(legacy,old,true,error)&&near(legacy.current.force,{-1.125f,0,0}),"old-friction unnormalized tangent override");
 old.friction=false;check(computeForces(legacy,old,true,error)&&near(legacy.current.force,{}),"old-friction disable override");
 Parameters bad=parameters;bad.elasticity=2;before=legacy;check(!computeForces(legacy,bad,true,error)&&near(legacy.current.force,before.current.force),"unsupported parameter range refused atomically");
 State simulation;initializeFive(simulation,{10,20,30},error);simulation.current.velocity={1,2,3};simulation.current.force={0,-560,0};EmptyTrace empty;StepReport report{99,99};
 check(simulateHalfStep(simulation,.05f,parameters,&empty,report,error),"real caller interface with no contacts");
 check(report.acceptedContacts==0&&report.bounceCallbacks==0&&!simulation.hasCollided,"no contacts produces no bounce/touch claim");
 check(near(simulation.current.position,{10.05f,20.1f,30.15f})&&near(simulation.current.velocity,{1,-26,3}),"halfstep rigid state independent of traced sphere writeback");
 check(empty.order==std::vector<unsigned>({0,1,0,1,0,1,0,1}),"each particle traces map then platform");
 for(unsigned i=0;i<4;++i){check(empty.requests[2*i].hardIntersect&&!empty.requests[2*i+1].hardIntersect&&empty.requests[2*i].restitution==1,"source hard map then soft platform policy");check(near(simulation.particles[i].position.x,10+simulation.particles[i].local.x)&&near(simulation.particles[i].position.y,20),"particle positions use source prior base transform");}
 check(bits(empty.requests[0].radius)==0x4104f49eu,"particle radius includes post-integration speed expansion");
 before=simulation;report={99,99};check(!simulateHalfStep(simulation,.05f,parameters,nullptr,report,error)&&near(simulation.current.position,before.current.position)&&report.acceptedContacts==99,"null trace refuses without partial state/report");
 State cap;initializeFive(cap,{},error);cap.current.velocity={10000,0,0};EmptyTrace capped;check(simulateHalfStep(cap,.1f,parameters,&capped,report,error)&&capped.requests[0].radius==57,"source dynamic radius expansion capped at50");
 State contacts;initializeFive(contacts,{},error);contacts.limitTilt=false;contacts.current.velocity={0,-10,0};ContactTrace contact;
 check(simulateHalfStep(contacts,.01f,parameters,&contact,report,error)&&report.acceptedContacts==1&&report.bounceCallbacks==1&&contacts.particles[0].touching&&contacts.hasCollided,"actual trace callback marks accepted source particle and first bounce");
 contacts.current.velocity={0,-10,0};contact.index=0;check(simulateHalfStep(contacts,.01f,parameters,&contact,report,error)&&report.acceptedContacts==1&&report.bounceCallbacks==0,"previous collision suppresses next-halfstep bounce callback");
 State separation;initializeFive(separation,{},error);separation.current.velocity={0,-10,0};ContactTrace separate;separate.separating=true;
 check(simulateHalfStep(separation,.01f,parameters,&separate,report,error)&&report.acceptedContacts==0&&!separation.particles[0].touching&&!separation.hasCollided,"separating actual contact does not mark touching");
 State refusal;initializeFive(refusal,{},error);refusal.current.velocity={0,-10,0};before=refusal;ContactTrace refusing;refusing.refuse=true;report={99,99};
 check(!simulateHalfStep(refusal,.1f,parameters,&refusing,report,error)&&near(refusal.current.position,before.current.position)&&report.acceptedContacts==99,"provider refusal rolls back halfstep and report");
 ContactTrace malformed;malformed.invalid=true;check(!simulateHalfStep(refusal,.1f,parameters,&malformed,report,error)&&near(refusal.current.position,before.current.position),"invalid actual callback rolls back halfstep");
 for(unsigned i=0;i<5;++i){BadTrace badTrace;badTrace.mode=i;check(!simulateHalfStep(refusal,.1f,parameters,&badTrace,report,error)&&near(refusal.current.position,before.current.position)&&report.acceptedContacts==99,"invalid/swallowed actual contact or mutated trace policy refused atomically");}
 State base;initializeFive(base,{},error);check(setBaseTransform(base,{1,0,0,0},{30,40,50},error)&&near(base.particles[0].position,{30,40,63}),"explicit source transform phase updates particle world positions");
 State zeroContacts;initializeFive(zeroContacts,{},error);zeroContacts.current.velocity={0,0,0};ContactTrace zeroTrace;
 check(simulateHalfStep(zeroContacts,0,parameters,&zeroTrace,report,error)&&report.acceptedContacts==1&&zeroContacts.particles[0].touching,"zero-velocity source callback still accepted as touching");
 const float half=std::sqrt(.5f);State tilt;initializeFive(tilt,{},error);tilt.current.rotation={half,half,0,0};tilt.current.momentum={100,0,0};
 check(integrate(tilt,.1f,error)&&near(tilt.current.rotation.w,half)&&near(tilt.current.rotation.x,half)&&tilt.current.momentum.x<-800,"literal worsening-tilt branch adds corrective momentum and rejects low candidate");
 // Goldens independently evaluated using exact rational operations with
 // IEEE binary32 ties-to-even after EACH source scalar/paired instruction.
 // Raw estimate uses reference coefficient integer interpolation, no production
 // math calls construct expectations. These controls do not qualify the host
 // JMath sin/cos table, actual physics contacts, body ownership or gameplay.
 State arithmetic;initializeFive(arithmetic,{},error);arithmetic.limitTilt=false;
 arithmetic.current.rotation={.9f,.1f,.2f,.3f};arithmetic.current.momentum={2,-3,5};
 check(integrate(arithmetic,0,error),"nontrivial source rotation/inertia arithmetic control");
 const std::uint32_t matrixGolden[9]={0x3b5bea30u,0xba58470bu,0xb99b58b8u,0xba58470au,0x3b89427cu,0x3a0cbec2u,0xb99b58b7u,0x3a0cbec1u,0x3b49081eu};
 bool matrixMatch=true;for(unsigned i=0;i<9;++i)if(bits(arithmetic.current.rotatedTransform[i])!=matrixGolden[i])matrixMatch=false;
 check(matrixMatch,"source paired matrix concat exact rational golden; asymmetry preserved");
 check(bitVec(arithmetic.current.rotatedMomentum,0x3bfc791au,0xbc3cf0ffu,0x3c5730d6u),"source paired matrix/vector instruction order golden");
 check(bits(arithmetic.current.rotation.w)==0x3f6c5f71u&&bits(arithmetic.current.rotation.x)==0x3dd21bf3u&&bits(arithmetic.current.rotation.y)==0x3e521bf3u&&bits(arithmetic.current.rotation.z)==0x3e9d94f7u,"source quaternion NONFUSED square sum/raw normalize golden");
 State translated;initializeFive(translated,{},error);translated.particles[0].local={3,-2,5};
 check(setBaseTransform(translated,{.9f,.1f,.2f,.3f},{1.3f,-2.7f,4.1f},error)&&bitVec(translated.particles[0].position,0x40d3d70au,0xc0370a3eu,0x40e33334u),"source paired matrix translation lane order golden");
 State impulse;initializeFive(impulse,{},error);impulse.current.velocity={3,-7,5};impulse.current.rotatedMomentum={-2,4,3};impulse.current.rotatedTransform={{.02f,.003f,-.004f,.005f,.03f,.006f,-.007f,.008f,.04f}};
 check(resolveCollision(impulse,{.3f,.7f,-.2f},{.6f,.8f,0},.3f,error)==CollisionResult::Accepted&&bitVec(impulse.current.velocity,0x40dff2c0u,0xbfd59c04u,0x40a00000u),"source fused dot/cross collision velocity exact rational golden");
 check(bitVec(impulse.current.momentum,0x3f887a66u,0xbf4cb799u,0xbf9989b3u)&&bitVec(impulse.current.rotatedMomentum,0x3cc25847u,0xbcd3d065u,0xbd7d4dabu),"source impulse torque and paired transformed momentum golden");
 State inverseControl;initializeFive(inverseControl,{},error);inverseControl.current.rotation={.68f,.72f,.11f,.05f};inverseControl.current.momentum={100,0,0};
 check(integrate(inverseControl,.1f,error)&&bitVec(inverseControl.current.momentum,0xc45fb1a9u,0,0x42b5a2b1u),"source fused inverse norm/one reciprocal/products tilt correction golden");
 // Environment refusal must never commit a partially integrated state.
 const int savedRound=std::fegetround();State rounded=arithmetic;before=rounded;
 if(std::fesetround(upwardRounding)==0){check(!integrate(rounded,.1f,error)&&!error.empty()&&bitVec(rounded.current.position,bits(before.current.position.x),bits(before.current.position.y),bits(before.current.position.z)),"refused arithmetic environment preserves state and reports error");std::fesetround(savedRound);}
 else check(false,"rounding control available");
 std::printf("original_number_rigid checks=%u failures=%u native=0 gameplay=0 lod=0 save=0\n",checks,failures);
 return failures?1:0;
}
