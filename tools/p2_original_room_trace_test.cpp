#include "pc_p2_original_room_trace.h"
#include <cstdio>
#include <cmath>
#include <limits>
using namespace p2originalnumber;
namespace {
struct ControlOwner:roomTrace::Owner {
 bool live=true,hiddenAvailable=true,enabled=false,sentinelAvailable=true;int sentinel=2;unsigned reads=0;
 bool current(const roomTrace::Mesh&,std::string& e)override{++reads;if(!live){e="control owner stale body/scene";return false;}return true;}
 bool hidden(const roomTrace::Mesh&,roomTrace::Hidden& h,std::string& e)override{if(!hiddenAvailable){e="control source hidden flag unavailable";return false;}h.enabled=enabled;h.sentinel.original=sentinelAvailable?&sentinel:nullptr;return true;}
};
struct ControlCallback:roomTrace::Callback {
 std::vector<unsigned> indices;
 ControlOwner& owner;bool invalidate=false,refuse=false,mutate=false;unsigned calls=0;bool firstBeforeFloor=false,hiddenAfterFloor=false;int expectedRoom=37;const void* expected=nullptr;
 explicit ControlCallback(ControlOwner& o):owner(o){}
 bool contact(const roomTrace::ContactIdentity& id,rigid::Vec3& point,rigid::Vec3& normal,const roomTrace::Move& move,std::string& e)override{
  ++calls;indices.push_back(id.index);if(calls==1)firstBeforeFloor=!move.hasFloor&&move.roomIndex==expectedRoom&&id.original==expected&&id.mapCode==19&&point.y<move.sphere.position.y&&move.sphere.velocity.y<0;
  hiddenAfterFloor=move.hasFloor&&move.floor.original==&owner.sentinel&&move.sphere.position.y==move.sphere.radius&&move.sphere.velocity.y<0&&normal.y==1&&point.y==0;
  if(mutate)normal={1,0,0};
  if(invalidate)owner.live=false;
  if(refuse){e="control callback refuses";return false;}return true;
 }
};
bool near(float a,float b){return std::fabs(a-b)<.01f;}
}
int main(){unsigned checks=0,failures=0;auto check=[&](bool ok,const char* name){++checks;if(!ok){++failures;std::printf("FAIL %s\n",name);}};
 std::string error;int original=1;
 const std::vector<rigid::Vec3> vertices{{-100,0,-100},{100,0,-100},{0,0,100}};
 const std::vector<roomTrace::TriangleInput> inputs{{{{0,1,2}},&original,37,19}};
 roomTrace::Mesh mesh;check(roomTrace::prepare(vertices,inputs,mesh,error),"pure source mesh preparation");
 check(mesh.grid.countX==3&&mesh.grid.countZ==3&&mesh.triangles[0].geometry.face.normal.y>0,"literal CaveGrid and CA cross BA plane");
 roomTrace::Move move;move.sphere={{0,6,0},{0,-10,0},7,.5f,false};ControlOwner owner;ControlCallback cb(owner);cb.expected=&original;roomTrace::Report report;
 check(roomTrace::traceMap(mesh,owner,move,.1f,&cb,report,error)&&report.contacts>0,"ordinary source floor trace");
 check(cb.firstBeforeFloor&&move.hasFloor&&move.floor.original==&original&&move.floor.index==0&&move.floor.roomIndex==37&&move.floor.mapCode==19,"original triangle metadata and callback before floor/response");
 check(near(move.sphere.position.y,7)&&move.sphere.velocity.y>0&&move.mapCode==0,"floor response/correction and unchanged MoveInfo map code");
 roomTrace::Mesh repeatMesh;const std::vector<rigid::Vec3> collinear{{-100,0,-100},{100,0,100},{0,0,0}};
 check(roomTrace::prepare(collinear,inputs,repeatMesh,error),"source finite degenerate planes retained");
 roomTrace::Move repeated;repeated.sphere={{-110.f+220.f/3.f,0,-110.f+220.f/3.f},{0,0,0},20,0,false};ControlOwner repeatsOwner;roomTrace::Report repeatedReport;
 check(roomTrace::traceMap(repeatMesh,repeatsOwner,repeated,0,nullptr,repeatedReport,error)&&repeatedReport.contacts>=2,"boundary grid preserves repeated finite-degenerate contacts");
 roomTrace::Move rapid;rapid.sphere={{0,50,0},{0,1000,0},0,0,false};ControlOwner rapidOwner;
 check(roomTrace::traceMap(mesh,rapidOwner,rapid,1,nullptr,report,error)&&report.substeps==8&&rapid.sphere.position.y==1050,"Room max8 even with source zero-radius");
 roomTrace::Move refused;refused.sphere={{0,6,0},{0,-10,0},7,.5f,false};auto before=refused;ControlOwner stale;stale.live=false;report.contacts=999;
 check(!roomTrace::traceMap(mesh,stale,refused,.1f,nullptr,report,error)&&refused.sphere.position.y==before.sphere.position.y&&report.contacts==999,"stale owner refuses before mesh/trace effects");
 ControlOwner invalidated;ControlCallback dies(invalidated);dies.expected=&original;dies.invalidate=true;
 check(!roomTrace::traceMap(mesh,invalidated,refused,.1f,&dies,report,error)&&dies.calls==1&&!refused.hasFloor&&refused.sphere.velocity.y==-10&&report.contacts==999,"owner invalidation after callback refuses before floor/correction and next mesh read");
 ControlOwner callbackOwner;ControlCallback refuses(callbackOwner);refuses.refuse=true;
 check(!roomTrace::traceMap(mesh,callbackOwner,refused,.1f,&refuses,report,error)&&!refused.hasFloor,"callback refusal propagates atomically");
 ControlOwner absent;absent.hiddenAvailable=false;ControlCallback notCalled(absent);
 check(!roomTrace::traceMap(mesh,absent,refused,.1f,&notCalled,report,error)&&notCalled.calls==0,"unavailable actual hidden flag refuses before any contacts");
 roomTrace::Move hidden;hidden.sphere={{500,-3,500},{0,-2,0},7,.5f,false};ControlOwner hiddenOwner;hiddenOwner.enabled=true;ControlCallback hiddenCb(hiddenOwner);
 check(roomTrace::traceMap(mesh,hiddenOwner,hidden,.1f,&hiddenCb,report,error)&&report.hidden&&hidden.hasFloor&&hidden.floor.original==&hiddenOwner.sentinel,"actual-owned hidden sentinel floor");
 check(hiddenCb.hiddenAfterFloor&&hidden.baseSpherePosition.y==0&&hidden.sphere.velocity.y==-1,"literal hidden callback AFTER pop/response/floor and unusual restitution");
 roomTrace::Move noHidden;noHidden.sphere={{500,-3,500},{0,-2,0},7,.5f,false};ControlOwner noHiddenOwner;
 check(roomTrace::traceMap(mesh,noHiddenOwner,noHidden,.1f,nullptr,report,error)&&!noHidden.hasFloor&&!report.hidden&&noHidden.sphere.position.y<0,"actual false hidden flag does not invent floor");
 auto hard=before;hard.sphere.position.y=-4;hard.sphere.velocity={};hard.sphere.hardIntersect=true;ControlOwner hardOwner;
 check(roomTrace::traceMap(mesh,hardOwner,hard,0,nullptr,report,error)&&hard.hasFloor,"hard Plane branch permits source negative-distance band");
 auto soft=before;soft.sphere.position.y=-8;soft.sphere.velocity={};ControlOwner softOwner;
 check(roomTrace::traceMap(mesh,softOwner,soft,0,nullptr,report,error)&&!soft.hasFloor,"soft absolute-distance rejection");
 auto malformed=mesh;malformed.grid.cells.assign(9,std::vector<unsigned>{99});ControlOwner badMesh;
 check(!roomTrace::traceMap(malformed,badMesh,refused,.1f,nullptr,report,error)&&!refused.hasFloor,"malformed borrowed candidate refuses");
 auto thresholds=before;thresholds.floorThreshold=2;thresholds.wallThreshold=2;ControlOwner thresholdOwner;
 check(roomTrace::traceMap(mesh,thresholdOwner,thresholds,.1f,nullptr,report,error)&&!thresholds.hasFloor&&thresholds.hasWall,"actual MoveInfo thresholds drive floor/wall classification");
 auto badInputs=inputs;badInputs[0].abc[2]=999;roomTrace::Mesh output=mesh;
 check(!roomTrace::prepare(vertices,badInputs,output,error)&&output.triangles[0].identity.original==&original,"preparation out-of-range failure atomic");
 for(float value:{-1.f,std::numeric_limits<float>::quiet_NaN()}){auto invalid=before;invalid.sphere.radius=value;ControlOwner o;check(!roomTrace::traceMap(mesh,o,invalid,.1f,nullptr,report,error),"negative/nonfinite radius refused");}

 ControlOwner noSentinel;noSentinel.enabled=true;noSentinel.sentinelAvailable=false;ControlCallback sentinelCb(noSentinel);
 check(!roomTrace::traceMap(mesh,noSentinel,refused,.1f,&sentinelCb,report,error)&&sentinelCb.calls==0,"enabled hidden without actual sentinel refuses before effects");
 roomTrace::Move hiddenStale;hiddenStale.sphere={{500,-3,500},{0,-2,0},7,.5f,false};ControlOwner hiddenDies;hiddenDies.enabled=true;ControlCallback hiddenInvalid(hiddenDies);hiddenInvalid.invalidate=true;
 check(!roomTrace::traceMap(mesh,hiddenDies,hiddenStale,.1f,&hiddenInvalid,report,error)&&hiddenInvalid.calls==1&&!hiddenStale.hasFloor&&hiddenStale.sphere.position.y==-3,"hidden post-callback lifetime refusal rolls back pending state");
 int originalSecond=3;auto orderedInputs=inputs;orderedInputs.push_back({{{0,1,2}},&originalSecond,42,23});roomTrace::Mesh orderedMesh;
 check(roomTrace::prepare(collinear,orderedInputs,orderedMesh,error),"ordered source mesh retains distinct original objects");
 auto orderedMove=repeated;ControlOwner orderedOwner;ControlCallback orderedCb(orderedOwner);
 check(roomTrace::traceMap(orderedMesh,orderedOwner,orderedMove,0,&orderedCb,report,error)&&orderedCb.indices.size()>=4,"repeated actual candidates invoke repeated ordered contacts");
 bool ordered=true;for(unsigned i=0;i<orderedCb.indices.size();++i)if(orderedCb.indices[i]!=(i%2))ordered=false;
 check(ordered&&orderedMove.roomIndex==42,"original ABC order repeats per cell without dedup or resort");
 auto mutableMove=before;ControlOwner mutationOwner;ControlCallback mutation(mutationOwner);mutation.mutate=true;
 check(roomTrace::traceMap(mesh,mutationOwner,mutableMove,.1f,&mutation,report,error)&&mutableMove.hasWall&&!mutableMove.hasFloor&&mutableMove.sphere.position.y>6&&mutableMove.sphere.velocity.y==-10,"callback normal mutation affects classification/response but correction retains pre-callback normal");
 std::printf("original_room_trace checks=%u failures=%u actual_scene=0 gameplay=0 platforms=0 save=0\n",checks,failures);return failures?1:0;
}
