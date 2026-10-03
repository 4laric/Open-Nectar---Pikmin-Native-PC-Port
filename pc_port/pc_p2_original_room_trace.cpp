#include "pc_p2_original_room_trace.h"
#include <cmath>
#include <limits>
// Primary: gameMapParts.cpp:2203,3304; geomIntersection.cpp:13;
// geometry.cpp:533; Sys/geometry.h:12,34; GridDivider recipe in number_grid.
namespace p2originalnumber { namespace roomTrace {
namespace {
float add(float a,float b){volatile float r=a+b;return r;}
float sub(float a,float b){volatile float r=a-b;return r;}
float mul(float a,float b){volatile float r=a*b;return r;}
float fused(float a,float b,float c){float out;if(!triangle::sourceFma(a,b,c,out))return std::numeric_limits<float>::quiet_NaN();return out;}
float dot(Vec3 a,Vec3 b){return fused(a.z,b.z,fused(a.x,b.x,mul(a.y,b.y)));}
Vec3 difference(Vec3 a,Vec3 b){return {sub(a.x,b.x),sub(a.y,b.y),sub(a.z,b.z)};}
Vec3 scale(Vec3 a,float f){return {mul(a.x,f),mul(a.y,f),mul(a.z,f)};}
Vec3 sum(Vec3 a,Vec3 b){return {add(a.x,b.x),add(a.y,b.y),add(a.z,b.z)};}
bool finite(Vec3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
bool fail(std::string& e,const char* m){e=m;return false;}
// RoomMap traceMove assembly: separately rounded squared guard, then fused XY
// with separately rounded Z square, then raw estimate. NOT dot(v,v).
bool speed(Vec3 v,float& out){
 return triangle::sourceVectorLength(v,out);
}

}
bool prepare(const std::vector<Vec3>& vertices,const std::vector<TriangleInput>& inputs,Mesh& out,std::string& e){
 if(inputs.empty()||inputs.size()>1048576)return fail(e,"Room source triangle bound invalid");
 std::vector<std::array<unsigned,3>> abc;abc.reserve(inputs.size());Mesh mesh;mesh.triangles.reserve(inputs.size());
 for(unsigned i=0;i<inputs.size();++i){const auto& input=inputs[i];if(!input.original)return fail(e,"Room original triangle identity absent");PreparedTriangle t;
  for(unsigned j=0;j<3;++j){if(input.abc[j]>=vertices.size())return fail(e,"Room source ABC out of range");t.vertices[j]=vertices[input.abc[j]];}
  if(!triangle::build(t.vertices,t.geometry))return fail(e,"Room source triangle arithmetic refused");
  t.identity={input.original,i,input.roomIndex,input.mapCode};mesh.triangles.push_back(t);abc.push_back(input.abc);
 }
 if(!motion::buildCaveGrid(vertices,abc,mesh.grid,e))return false;
 out=std::move(mesh);e.clear();return true;
}
bool traceMap(const Mesh& mesh,Owner& owner,Move& out,float dt,Callback* callback,Report& outReport,std::string& e){
 if(!owner.current(mesh,e))return false;
 Hidden hidden;if(!owner.hidden(mesh,hidden,e))return false;
 if(hidden.enabled&&!hidden.sentinel.original)return fail(e,"Room actual hidden sentinel unavailable");
 if(!owner.current(mesh,e))return false;
 if(!finite(out.sphere.position)||!finite(out.sphere.velocity)||!std::isfinite(out.sphere.radius)||out.sphere.radius<0||!std::isfinite(dt)||dt<0||!std::isfinite(out.sphere.restitution)||out.sphere.restitution<0||!std::isfinite(out.floorThreshold)||!std::isfinite(out.wallThreshold))return fail(e,"Room actual MoveInfo invalid");
 Move move=out;Report report;float velocityLength;if(!speed(move.sphere.velocity,velocityLength))return fail(e,"Room source speed arithmetic refused");
 unsigned count=1;float step=dt;
 while(mul(step,velocityLength)>move.sphere.radius&&count<=4){count*=2;step=mul(step,.5f);}
 report.substeps=count;
 for(unsigned n=0;n<count;++n){
  move.sphere.position=sum(move.sphere.position,scale(move.sphere.velocity,step));
  if(!finite(move.sphere.position))return fail(e,"Room source movement overflow");
  if(!owner.current(mesh,e))return false;
  std::vector<unsigned> candidates;if(!motion::caveCandidates(mesh.grid,move.sphere.position,move.sphere.radius,candidates,e))return false;
  for(const unsigned index:candidates){
   if(!owner.current(mesh,e))return false;
   if(index>=mesh.triangles.size())return fail(e,"Room candidate index invalid");
   const auto tri=mesh.triangles[index];if(!tri.identity.original||tri.identity.index!=index)return fail(e,"Room original triangle identity invalid");
   ++report.candidates;triangle::Contact hit;
   const triangle::Sphere sphere{move.sphere.position,move.sphere.radius};
   const auto mode=move.sphere.hardIntersect?triangle::SweepType::IntersectPlane:triangle::SweepType::InsidePlane;
   const auto result=triangle::sweep(tri.vertices,tri.geometry,sphere,mode,hit);
   if(result==triangle::Result::Invalid)return fail(e,"Room source SphereSweep arithmetic refused");
   if(result==triangle::Result::Miss)continue;
   move.roomIndex=tri.identity.roomIndex;const Vec3 correctionNormal=hit.normal;
   if(callback){if(!callback->contact(tri.identity,hit.point,hit.normal,move,e))return false;if(!owner.current(mesh,e))return false;}
   if(!finite(hit.point)||!finite(hit.normal))return fail(e,"Room callback returned nonfinite contact");
   ++report.contacts;
   if(hit.normal.y>=move.floorThreshold){move.hasFloor=true;move.floor=tri.identity;move.floorNormal=hit.normal;}
   else if(std::fabs(hit.normal.y)<=move.wallThreshold){move.hasWall=true;move.wall=tri.identity;move.wallNormal=hit.normal;}
   const float impact=mul(add(1,move.sphere.restitution),dot(hit.normal,move.sphere.velocity));
   move.sphere.velocity=difference(move.sphere.velocity,scale(hit.normal,impact));
   move.sphere.position=sum(move.sphere.position,scale(correctionNormal,hit.strength));
   if(!finite(move.sphere.velocity)||!finite(move.sphere.position))return fail(e,"Room source contact response overflow");
  }
 }
 if(hidden.enabled&&!move.hasFloor&&sub(move.sphere.position.y,move.sphere.radius)<0){
  if(!owner.current(mesh,e))return false;
  move.sphere.position.y=move.sphere.radius;
  if(move.sphere.velocity.y<0)move.sphere.velocity.y=mul(-move.sphere.velocity.y,sub(move.sphere.restitution,1));
  move.hasFloor=true;move.floor=hidden.sentinel;move.floorNormal={0,1,0};move.unusedNormal={0,1,0};move.baseSpherePosition=move.sphere.position;move.baseSpherePosition.y=sub(move.baseSpherePosition.y,move.sphere.radius);report.hidden=true;
  if(callback){Vec3 up{0,1,0},bottom=move.baseSpherePosition;if(!callback->contact(hidden.sentinel,bottom,up,move,e))return false;if(!owner.current(mesh,e))return false;}
  if(!finite(move.sphere.velocity))return fail(e,"Room hidden response overflow");
 }
 if(!owner.current(mesh,e))return false;
 out=move;outReport=report;e.clear();return true;
}
} }
