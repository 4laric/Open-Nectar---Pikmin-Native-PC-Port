#include "pc_p2_original_number_five_motion.h"
#include "pc_p2_original_number_triangle.h"
#include <cstdint>
#include <cstring>
#include <cmath>
#include <cstdio>
#include <limits>
#include <vector>
using namespace p2originalnumber;
namespace {
bool near(float a,float b,float tolerance=.0001f){return std::fabs(a-b)<=tolerance;}
bool near(rigid::Vec3 a,rigid::Vec3 b){return near(a.x,b.x)&&near(a.y,b.y)&&near(a.z,b.z);}
std::uint32_t bits(float value){std::uint32_t out;std::memcpy(&out,&value,sizeof(out));return out;}
struct Call {bool platform,receiver;rigid::Trace request;float dt;};
// Focused orchestration controls only. This is not an admitted gameplay world
// or a substitute for the native provider's actual triangles/platforms.
struct ControlTraces:fiveMotion::TraceProvider {
 std::vector<Call> calls;
 bool mapProbeFloor=false,platformProbeFloor=false,simpleFloor=false;
 bool centerOverride=false,refuse=false,malformed=false,particleContact=false;
 bool mapWall=false,platformWall=false,ignoreReceiverFailure=false;
 bool* eventSeen=nullptr;bool correctionAfterEvent=false;
 unsigned particleCalls=0;
 bool traceMap(rigid::Trace& t,float dt,rigid::ContactReceiver* receiver,fiveMotion::Contacts& contacts,std::string& e)override{
  calls.push_back({false,receiver!=nullptr,t,dt});
  if(refuse){e="control actual-provider refusal";return false;}
  if(malformed){contacts.floor=true;contacts.floorNormal={0,2,0};return true;}
  t.position={t.position.x+t.velocity.x*dt,t.position.y+t.velocity.y*dt,t.position.z+t.velocity.z*dt};
  if(receiver){++particleCalls;if(particleContact&&particleCalls==1){const bool ok=receiver->contact(t.position,{0,1,0},e);correctionAfterEvent=eventSeen&&*eventSeen;t.position.y+=1;return ignoreReceiverFailure?true:ok;}}
  else if(t.restitution==0){if(mapProbeFloor){contacts.floor=true;contacts.floorNormal={0,1,0};}}
  else {
   if(simpleFloor){contacts.floor=true;contacts.floorNormal={0,1,0};t.position.y=7;t.velocity.y=0;}
   if(centerOverride){t.position={100,999,200};t.velocity={999,999,999};}
   if(mapWall){contacts.wall=true;contacts.wallNormal={1,0,0};}
  }
  return true;
 }
 bool tracePlatforms(rigid::Trace& t,float dt,rigid::ContactReceiver* receiver,fiveMotion::Contacts& contacts,std::string&)override{
  calls.push_back({true,receiver!=nullptr,t,dt});
  if(t.restitution==0&&platformProbeFloor){contacts.floor=true;contacts.floorNormal={0,1,0};}
  if(!receiver&&t.restitution==.5f&&platformWall){contacts.wall=true;contacts.wallNormal={1,0,0};}
  return true;
 }
 unsigned probes()const{unsigned n=0;for(const auto& c:calls)if(!c.platform&&!c.receiver&&c.request.restitution==0)++n;return n;}
};
struct ControlEvents:fiveMotion::Events {
 bool seen=false,refuse=false,particleBeforeFlags=false,simpleBeforeFloorForce=false,mutateVelocity=false;
 unsigned calls=0;
 bool bounce(fiveMotion::State& s,fiveMotion::BouncePhase phase,std::string& error)override{
  seen=true;++calls;
  if(phase==fiveMotion::BouncePhase::AcceptedParticleContact)
   particleBeforeFlags=!s.body.hasCollided&&!s.body.particles[0].touching&&s.body.current.velocity.y>-10;
  else simpleBeforeFloorForce=!s.previousFloor&&s.body.current.velocity.x==10&&s.body.current.position.y==0;
  if(mutateVelocity)s.body.current.velocity={20,12,-8};
  if(refuse){error="control event lifetime refusal";return false;}return true;
 }
};
fiveMotion::State state(){fiveMotion::State s;std::string error;rigid::initializeFive(s.body,{},error);return s;}
}
int main(){
 unsigned checks=0,failures=0;std::string error;
 auto check=[&](bool ok,const char* name){++checks;if(!ok){++failures;std::printf("FAIL %s\n",name);}};
 rigid::Parameters params;lod::Result nearLod;nearLod.flags=lod::Visible|lod::VisibleVP0;
 lod::Result farLod;farLod.flags=lod::Visible|lod::VisibleVP0|lod::Far;
 fiveMotion::Options options;options.sourceLod=&nearLod;
 auto s=state();ControlTraces trace;fiveMotion::Report report;
 s.body.current.velocity={1,0,2};
 check(fiveMotion::update(s,.1f,params,options,&trace,report,error)&&report.rigid&&!report.resting&&report.halfSteps==2,"actual-provided Near selects two source rigid halfsteps");
 check(report.mapCalls==9&&report.platformCalls==9&&trace.calls.size()==18,"eight particle map/platform pairs then final center pair");
 check(near(s.body.current.position,{.1f,-1.4f,.2f})&&near(s.body.current.velocity,{1,-56,2}),"source force and two explicit halfstep positions");
 check(trace.calls[0].request.hardIntersect&&near(trace.calls[0].dt,.05f)&&!trace.calls[1].request.hardIntersect,"particle hard-map soft-platform halfstep order");
 check(!trace.calls[16].receiver&&!trace.calls[16].request.hardIntersect&&trace.calls[16].request.radius==7&&trace.calls[16].request.restitution==.5f&&near(trace.calls[16].dt,.1f),"final center trace source policy");
 check(near(trace.calls[16].request.position,{})&&near(trace.calls[16].request.velocity,{1,-14,2}),"final center starts before simulation with displacement-derived velocity");
 check(near(s.body.basePosition,{})&&near(s.body.particles[0].position.y,0),"base transform held through both halfsteps");
 check(fiveMotion::refreshBaseTransform(s,error)&&near(s.body.basePosition,s.body.current.position)&&near(s.body.particles[0].position.y,-1.4f),"actual later transform phase refreshes particle positions");
 auto corrected=state();corrected.body.current.velocity={1,0,2};ControlTraces correction;correction.centerOverride=true;
 check(fiveMotion::update(corrected,.1f,params,options,&correction,report,error)&&near(corrected.body.current.position,{100,-1.4f,200})&&near(corrected.body.current.velocity,{1,-56,2}),"final center writes XZ only, preserves rigid Y and velocity");
 auto resting=state();resting.body.hasCollided=true;resting.body.current.velocity={5,0,0};resting.body.current.momentum={100,0,0};ControlTraces floorProbe;floorProbe.mapProbeFloor=true;
 check(fiveMotion::update(resting,.1f,params,options,&floorProbe,report,error)&&report.resting&&report.halfSteps==0,"rest eligibility checks momentum after source .05 damping");
 check(report.mapCalls==2&&report.platformCalls==1&&floorProbe.probes()==1,"map-floor resting probe skips its platform pass but final center still runs");
 check(near(floorProbe.calls[0].request.position,{0,-7,0})&&near(floorProbe.calls[0].request.velocity,{0,-560,0})&&floorProbe.calls[0].request.radius==7&&floorProbe.calls[0].request.restitution==0,"rest probe literal center offset/radius/gravity velocity/rest0");
 check(near(resting.body.current.position,{})&&near(resting.body.current.velocity,{5,0,0})&&near(resting.body.current.momentum,{95,0,0})&&resting.body.current.force.y==-560,"rest probe does not copy traced position/velocity or clear source force");
 auto platformRest=state();platformRest.body.hasCollided=true;ControlTraces platformProbe;platformProbe.platformProbeFloor=true;
 check(fiveMotion::update(platformRest,.1f,params,options,&platformProbe,report,error)&&report.resting&&report.mapCalls==2&&report.platformCalls==2,"resting falls back to actual platforms only without map floor");
 auto noRest=state();noRest.body.hasCollided=true;ControlTraces noProbeFloor;
 check(fiveMotion::update(noRest,.1f,params,options,&noProbeFloor,report,error)&&!report.resting&&report.halfSteps==2&&report.mapCalls==10&&report.platformCalls==10,"rest probe without actual floor proceeds to both halfsteps");
 auto boundary=state();boundary.body.hasCollided=true;boundary.body.current.velocity={10,0,0};ControlTraces threshold;threshold.mapProbeFloor=true;
 check(fiveMotion::update(boundary,.1f,params,options,&threshold,report,error)&&threshold.probes()==1&&report.resting&&report.halfSteps==0,"source estimated length of velocity10 is below strict threshold10");
 boundary=state();boundary.body.hasCollided=true;boundary.body.current.momentum={200,0,0};ControlTraces momentumThreshold;momentumThreshold.mapProbeFloor=true;
 check(fiveMotion::update(boundary,.1f,params,options,&momentumThreshold,report,error)&&momentumThreshold.probes()==0,"angular momentum threshold100 is strict after damping");
 boundary=state();boundary.body.hasCollided=true;fiveMotion::Options nonNormal=options;nonNormal.normalState=false;ControlTraces normalThreshold;normalThreshold.mapProbeFloor=true;
 check(fiveMotion::update(boundary,.1f,params,nonNormal,&normalThreshold,report,error)&&normalThreshold.probes()==0,"actual non-Normal state skips sleeping probe");
 auto picked=state();picked.body.hasCollided=true;fiveMotion::Options carrying=options;carrying.picked=true;ControlTraces pickedTrace;pickedTrace.mapProbeFloor=true;
 check(fiveMotion::update(picked,.1f,params,carrying,&pickedTrace,report,error)&&pickedTrace.probes()==0&&near(picked.body.current.velocity.y,-56),"picked rigid Five still receives source gravity and skips resting");
 auto accelerated=state();accelerated.acceleration={3,99,4};fiveMotion::Options flick=options;flick.collisionFlick=true;ControlTraces flickTrace;
 check(fiveMotion::update(accelerated,.1f,params,flick,&flickTrace,report,error)&&near(accelerated.body.current.position,{.3f,-1.4f,.4f}),"source collision flick adds horizontal acceleration before both halfsteps");
 // Independent exact-Fraction binary32 oracle: separately rounded square
 // guard; q=round(roundFMA(x,x,round(y*y))+round(z*z)); estimate coefficient
 // integer interpolation; estimate*q -> RNE f32; inverse, normal components,
 // speed-acceleration and final component products each separately RNE f32.
 // Input (3,-56,4): source speed=0x4260e73d, accel5=0x409ffdd0.
 // These constants do not call production helpers to construct expectations.
 check(bits(accelerated.body.current.velocity.x)==0x402eed4bu&&bits(accelerated.body.current.velocity.y)==0xc24c14d7u&&bits(accelerated.body.current.velocity.z)==0x40693c63u&&near(accelerated.acceleration,{}),"source raw estimate normalize/subtract exact golden f32 components");
 auto unapplied=state();unapplied.acceleration={0,3,0};ControlTraces unappliedTrace;
 check(fiveMotion::update(unapplied,.1f,params,options,&unappliedTrace,report,error)&&bits(unapplied.body.current.velocity.y)==0xc25400a2u&&bits(unapplied.body.current.velocity.x)==0&&bits(unapplied.body.current.velocity.z)==0,"unapplied acceleration Y still participates in source final magnitude subtraction");
 auto greater=state();greater.acceleration={100,0,0};ControlTraces greaterTrace;
 check(fiveMotion::update(greater,.001f,params,options,&greaterTrace,report,error)&&near(greater.body.current.velocity.y,-.56f),"acceleration greater than speed retains original speed, not zero/clamp subtraction");
 auto particle=state();particle.body.current.velocity={0,-10,0};ControlTraces particleTraces;particleTraces.particleContact=true;
 check(fiveMotion::update(particle,.01f,params,options,&particleTraces,report,error)&&report.acceptedParticleContacts==1&&report.particleBounceCallbacks==1,"actual supplied particle callback reaches rigid contact state/report");
 fiveMotion::Options simple=options;simple.sourceLod=&farLod;auto far=state();far.body.current.velocity={10,0,0};far.body.current.momentum={10,20,30};far.body.current.rotatedMomentum={1,2,3};far.body.current.rotation={.70710677f,.70710677f,0,0};far.previousFloor=true;far.body.hasCollided=true;far.body.particles[0].touching=true;far.body.particles[0].collisionNormal={0,1,0};ControlTraces farTrace;farTrace.simpleFloor=true;
 check(fiveMotion::update(far,.1f,params,simple,&farTrace,report,error)&&!report.rigid&&report.halfSteps==0&&report.mapCalls==1&&report.platformCalls==1,"actual Far selects literal simple branch with single map/platform pair");
 check(near(far.body.current.velocity,{0,0,0})&&near(far.body.current.momentum,{})&&near(far.body.current.rotatedMomentum,{}),"simple branch floor damping10 and clears rigid momentum");
 check(near(far.body.current.rotation.w,.70710677f)&&far.body.hasCollided&&far.body.particles[0].touching,"simple fallback preserves quaternion and prior rigid contact history");
 check(far.previousFloor&&!report.firstSimpleBounce,"simple fallback preserves prior floor and does not invent first bounce");
 auto firstFloor=state();ControlTraces firstFloorTrace;firstFloorTrace.simpleFloor=true;
 check(fiveMotion::update(firstFloor,.1f,params,simple,&firstFloorTrace,report,error)&&firstFloor.previousFloor&&report.firstSimpleBounce,"actual first simple floor reports source bounce transition");
 auto pickedFar=state();pickedFar.body.current.position={0,11,0};pickedFar.previousFloor=true;fiveMotion::Options pickedSimple=simple;pickedSimple.picked=true;ControlTraces pickedSimpleTrace;
 check(fiveMotion::update(pickedFar,.1f,params,pickedSimple,&pickedSimpleTrace,report,error)&&pickedSimpleTrace.calls[0].request.position.y==7&&pickedSimpleTrace.calls[0].request.velocity.y==0&&pickedFar.body.current.position.y==11,"picked simple traces bodyY minus4, gates gravity on prior floor, restores4");
 auto disabled=state();fiveMotion::Options dynamicsDisabled=options;dynamicsDisabled.disableDynamics=true;ControlTraces disabledTrace;
 check(fiveMotion::update(disabled,.1f,params,dynamicsDisabled,&disabledTrace,report,error)&&!report.rigid,"actual source disableDynamics selects simple despite Near");
 auto wall=state();wall.wallTimer=99;wall.body.current.velocity={-1,0,0};ControlTraces walls;walls.mapWall=true;
 check(fiveMotion::update(wall,.1f,params,carrying,&walls,report,error)&&wall.wallTimer==101,"rigid wall timer uses pretrace displacement and literal +2 when99");
 auto simpleWall=state();simpleWall.previousFloor=true;simpleWall.body.current.velocity={-1,0,0};ControlTraces platformWallOnly;platformWallOnly.platformWall=true;
 check(fiveMotion::update(simpleWall,.1f,params,pickedSimple,&platformWallOnly,report,error)&&simpleWall.wallTimer==0&&report.movementContacts.wall,"simple wall timer runs before platform wall contacts");
 auto rigidFloorState=state();rigidFloorState.previousFloor=true;ControlTraces noCenterFloor;
 check(fiveMotion::update(rigidFloorState,.1f,params,options,&noCenterFloor,report,error)&&rigidFloorState.previousFloor,"rigid branch does not replace source previousFloor with final center contacts");
 auto refused=state();refused.body.current.velocity={1,2,3};auto before=refused;fiveMotion::Report beforeReport;beforeReport.mapCalls=99;report=beforeReport;ControlTraces refusal;refusal.refuse=true;
 check(!fiveMotion::update(refused,.1f,params,options,&refusal,report,error)&&near(refused.body.current.position,before.body.current.position)&&near(refused.body.current.velocity,before.body.current.velocity)&&report.mapCalls==99,"actual provider refusal preserves state/report atomically");
 ControlTraces malformed;malformed.malformed=true;check(!fiveMotion::update(refused,.1f,params,options,&malformed,report,error)&&near(refused.body.current.position,before.body.current.position),"invalid actual contact result refused atomically");
 check(!fiveMotion::update(refused,.1f,params,options,nullptr,report,error),"missing actual provider refused");
 fiveMotion::Options missing=options;missing.sourceLod=nullptr;check(!fiveMotion::update(refused,.1f,params,missing,&trace,report,error),"missing actual source LOD refused");
 for(float dt:{0.f,-.1f,std::numeric_limits<float>::quiet_NaN()})check(!fiveMotion::update(refused,dt,params,options,&trace,report,error),"unsupported timestep refused");
 fiveMotion::Options eventOptions=options;eventOptions.requireGameplayEvents=true;
 auto eventState=state();ControlTraces eventTrace;
 check(!fiveMotion::update(eventState,.01f,params,eventOptions,&eventTrace,report,error)&&eventTrace.calls.empty(),"required gameplay events missing refuses before actual traces");
 ControlEvents particleEvents;eventOptions.events=&particleEvents;eventState.body.current.velocity={0,-10,0};eventTrace.particleContact=true;eventTrace.eventSeen=&particleEvents.seen;
 check(fiveMotion::update(eventState,.01f,params,eventOptions,&eventTrace,report,error)&&particleEvents.calls==1&&report.executedBounceEvents==1,"accepted particle bounce actually executes once");
 check(particleEvents.particleBeforeFlags&&eventTrace.correctionAfterEvent,"particle event after impulse but before touching flags and solver correction");
 ControlEvents simpleEvents;fiveMotion::Options simpleEventsOptions=simple;simpleEventsOptions.events=&simpleEvents;simpleEventsOptions.requireGameplayEvents=true;
 auto simpleEventState=state();simpleEventState.body.current.velocity={10,0,0};ControlTraces simpleEventTrace;simpleEventTrace.simpleFloor=true;
 check(fiveMotion::update(simpleEventState,.1f,params,simpleEventsOptions,&simpleEventTrace,report,error)&&simpleEvents.simpleBeforeFloorForce&&report.executedBounceEvents==1&&simpleEventState.body.current.velocity.x==0,"simple actual bounce before floor assignment/forces with live traced velocity");
 simpleEvents.mutateVelocity=true;simpleEventState=state();simpleEventState.body.current.velocity={10,0,0};simpleEventTrace=ControlTraces{};simpleEventTrace.simpleFloor=true;
 check(fiveMotion::update(simpleEventState,.05f,params,simpleEventsOptions,&simpleEventTrace,report,error)&&simpleEvents.simpleBeforeFloorForce&&simpleEventState.body.current.velocity.x==10&&simpleEventState.body.current.velocity.y==12&&simpleEventState.body.current.velocity.z==-4,"first-floor bounce velocity mutation feeds source floor forces and survives commit");
 simpleEvents.mutateVelocity=false;
 auto eventRefusal=state();eventRefusal.body.current.velocity={0,-10,0};ControlEvents badEvent;badEvent.refuse=true;eventOptions.events=&badEvent;ControlTraces failedEventTrace;failedEventTrace.particleContact=true;report.mapCalls=999;
 check(!fiveMotion::update(eventRefusal,.01f,params,eventOptions,&failedEventTrace,report,error)&&eventRefusal.body.current.velocity.y==-10&&!eventRefusal.body.hasCollided&&report.mapCalls==999,"particle lifetime event refusal preserves state/report");
 failedEventTrace=ControlTraces{};failedEventTrace.particleContact=true;failedEventTrace.ignoreReceiverFailure=true;
 check(!fiveMotion::update(eventRefusal,.01f,params,eventOptions,&failedEventTrace,report,error)&&eventRefusal.body.current.velocity.y==-10,"provider ignoring event refusal still refuses atomically");
 simpleEvents.refuse=true;simpleEventState=state();simpleEventState.body.current.velocity={10,0,0};simpleEventTrace=ControlTraces{};simpleEventTrace.simpleFloor=true;
 check(!fiveMotion::update(simpleEventState,.1f,params,simpleEventsOptions,&simpleEventTrace,report,error)&&!simpleEventState.previousFloor&&simpleEventState.body.current.velocity.x==10,"simple lifetime event refusal precedes floor commit");
 // Exact-Fraction oracle gives raw length10 = 0x411ffdd0, not float10.
 float literalLength=123;
 check(triangle::sourceVectorLength({10,0,0},literalLength)&&bits(literalLength)==0x411ffdd0u&&literalLength<10,"direct source velocity10 estimate threshold golden");
 check(triangle::sourceVectorLength({100,0,0},literalLength)&&bits(literalLength)==0x42c805ddu&&literalLength>100,"direct source momentum100 estimate threshold golden");
 check(triangle::sourceVectorLength({3,-56,4},literalLength)&&bits(literalLength)==0x4260e73du,"direct source mixed vector guarded FMA length golden");
 check(triangle::sourceVectorLength({},literalLength)&&bits(literalLength)==0,"zero vector guard returns zero");
 for(const rigid::Vec3 input:{rigid::Vec3{std::numeric_limits<float>::quiet_NaN(),0,0},rigid::Vec3{0,std::numeric_limits<float>::infinity(),0},rigid::Vec3{std::numeric_limits<float>::max(),0,0}}){
  literalLength=123;check(!triangle::sourceVectorLength(input,literalLength)&&literalLength==123,"nonfinite/overflow source vector length refusal preserves output");
 }
 std::printf("original_number_five_motion checks=%u failures=%u actual_world=0 native=0 gameplay=0 save=0\n",checks,failures);
 return failures?1:0;
}
