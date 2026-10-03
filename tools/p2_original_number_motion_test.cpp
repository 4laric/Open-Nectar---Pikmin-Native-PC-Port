#include "pc_p2_original_number_motion.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>
using namespace p2originalnumber::motion;
namespace { unsigned checks=0;void check(bool ok){++checks;assert(ok);}bool close(float a,float b){return std::fabs(a-b)<0.0001f;} }
int main(){
 Vec3 v{1,2,3},out{99,99,99};
 check(beginSimple(v,0.1f,false,false,true,out)&&close(out.y,-54));
 check(beginSimple(v,0.1f,true,false,true,out)&&out.y==2);
 check(beginSimple(v,0.1f,true,false,false,out)&&close(out.y,-54));
 check(beginSimple(v,0.1f,false,true,true,out)&&out.y==2);
 check(beginSimple(v,0.1f,false,true,false,out)&&close(out.y,-54));
 check(!beginSimple(v,-1,false,false,false,out));
 // Post-trace flat friction removes only the tangent, at 10*dt.
 check(finishSimple({10,5,-10},{0,1,0},0.05f,false,false,out)&&close(out.x,5)&&close(out.y,5)&&close(out.z,-5));
 // The slope's tangential gravity cancellation occurs after contact response.
 check(finishSimple({0,0,0},{0.6f,0.8f,0},0.1f,false,false,out)&&close(out.x,-26.88f)&&close(out.y,20.16f));
 check(finishSimple(v,{0,1,0},0.1f,true,false,out)&&out.x==1&&out.y==2&&out.z==3);
 check(finishSimple(v,{0,1,0},0.1f,false,true,out)&&out.x==1&&out.y==2&&out.z==3);
 check(restitution({0,-10,0},{0,1,0},0.5f,out)&&out.y==5);
 // Source contact response also reverses a separating velocity.
 check(restitution({0,10,0},{0,1,0},0.5f,out)&&out.y==-5);
 unsigned count=0;float step=0;
 check(stepCount(1,3.8f,3.8f,count,step)&&count==1&&step==1);
 check(stepCount(1,3.81f,3.8f,count,step)&&count==2&&step==0.5f);
 check(stepCount(1,100000,3.8f,count,step)&&count==16&&step==0.0625f);
 check(stepCount(1,100000,3.8f,count,step,8)&&count==8&&step==0.125f);
 check(!stepCount(1,10,3.8f,count,step,32));
 check(stepCount(0,10,3.8f,count,step)&&count==1&&step==0);
 check(!stepCount(1,10,0,count,step));
 const Triangle floor{{Vec3{0,0,0},Vec3{10,0,0},Vec3{0,0,10}},Vec3{0,1,0},0};
 Contact hit;
 check(intersect(floor,{2,3,2},3.8f,true,hit)==Intersection::Hit&&close(hit.overlap,0.8f)&&hit.normal.y==1&&close(hit.point.y,-0.8f));
 check(intersect(floor,{2,-8.8f,2},3.8f,true,hit)==Intersection::Hit);
 check(intersect(floor,{2,-8.81f,2},3.8f,true,hit)==Intersection::Miss);
 check(intersect(floor,{2,-4,2},3.8f,false,hit)==Intersection::Miss);
 check(intersect(floor,{2,3.8f,2},3.8f,false,hit)==Intersection::Hit&&hit.overlap==0);
 check(intersect(floor,{2,3.81f,2},3.8f,false,hit)==Intersection::Miss);
 // Outside triangle: ordered edge, endpoint, and strict side tangency.
 check(intersect(floor,{2,0,-1},2,false,hit)==Intersection::Hit&&hit.normal.z==-1&&hit.overlap==1);
 check(intersect(floor,{-1,0,-1},std::sqrt(2.0f),false,hit)==Intersection::Hit&&close(hit.overlap,0));
 check(intersect(floor,{2,0,-2},2,false,hit)==Intersection::Miss);
 Contact unchanged=hit;
 check(intersect(floor,{std::numeric_limits<float>::infinity(),0,0},2,false,hit)==Intersection::Invalid&&hit.overlap==unchanged.overlap);
 check(intersect(floor,{2,0,2},0,false,hit)==Intersection::Invalid);
 p2originalnumber::rigid::Trace hidden;hidden.position={2,-100,3};hidden.velocity={1,-10,2};hidden.radius=3.8f;hidden.restitution=0.5f;bool applied=false;
 check(hiddenFloor(hidden,true,false,hit,applied)&&applied&&hidden.position.y==3.8f&&hidden.velocity.y==-5&&hit.point.y==0&&hit.normal.y==1);
 hidden.position.y=-100;hidden.velocity.y=10;
 check(hiddenFloor(hidden,true,false,hit,applied)&&applied&&hidden.velocity.y==10);
 hidden.position.y=-100;
 check(hiddenFloor(hidden,false,false,hit,applied)&&!applied&&hidden.position.y==-100);
 check(hiddenFloor(hidden,true,true,hit,applied)&&!applied&&hidden.position.y==-100);
 std::printf("Original number source motion: %u controls passed\n",checks);
}
