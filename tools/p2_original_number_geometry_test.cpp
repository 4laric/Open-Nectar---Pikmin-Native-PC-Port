#include "pc_p2_original_number_geometry.h"
#include <cmath>
#include <cstdio>
#include <limits>
using namespace p2originalnumber;
namespace {unsigned checks=0,failures=0;void check(bool b,const char* label){++checks;if(!b){++failures;std::printf("FAIL %s\n",label);}}bool near(float a,float b){return std::fabs(a-b)<0.00001f;}}
int main(){
 float radius=-9;check(terrainRadius(Size::One,radius)&&near(radius,3.8f),"One floor contact uses halfheight, not collider or pickup radius");
 check(terrainRadius(Size::Five,radius)&&near(radius,7.0f),"Five floor contact uses halfheight");
 const auto bad=static_cast<Size>(10);check(!terrainRadius(bad,radius)&&near(radius,7),"unsupported radius leaves output unchanged");
 std::array<float,3> slot{};
 check(carrierSlot(Size::One,0,0,0,false,false,slot)&&near(slot[0],0)&&near(slot[1],-4.8f)&&near(slot[2],10),"One upright initial carrier below center");
 check(carrierSlot(Size::One,1,0,2,true,true,slot)&&near(slot[0],0)&&near(slot[1],8.8f)&&near(slot[2],-12),"inverted pickup uses other face and native four-unit offset");
 check(carrierSlot(Size::Five,-2,1.5707963267948966f,0,true,false,slot)&&near(slot[0],20)&&near(slot[1],-12)&&near(slot[2],0),"stuck carrier retains actual angle");
 for(int i=0;i<10;++i){check(carrierSlot(Size::Five,i,0,0,false,false,slot)&&near(std::hypot(slot[0],slot[2]),20)&&near(slot[1],-8),"all ten Five carrier slots share source ring");}
 const auto before=slot;check(!carrierSlot(Size::Five,10,0,0,false,false,slot)&&slot==before,"out of range carrier does not mutate output");
 check(!carrierSlot(Size::One,0,0,std::numeric_limits<float>::infinity(),false,false,slot)&&slot==before,"nonfinite radial offset refused");
 std::array<Sphere,4> contact;unsigned count=99;
 check(particles(Size::Five,contact,count)&&count==4,"Five has four particles, not eight pairs");
 for(const auto& s:contact)check(near(s.radius,7)&&near(s.center[1],0)&&near(std::hypot(s.center[0],s.center[2]),13),"Five contacts on centered thirteen-unit ring");
 check(near(contact[0].center[0],0)&&near(contact[0].center[2],13),"literal source particle order");
 check(particles(Size::One,contact,count)&&count==0&&near(contact[0].radius,0),"One clears previous Five particle descriptors");
 check(!particles(bad,contact,count)&&count==0,"unsupported particles preserve prior result");
 std::array<Sphere,2> coll;
 check(collider(Size::One,coll)&&near(coll[0].radius,11)&&near(coll[0].center[1],4)&&near(coll[1].radius,10)&&near(coll[1].center[1],5),"One actual root and child collider literals");
 check(collider(Size::Five,coll)&&near(coll[0].radius,24)&&near(coll[0].center[1],12)&&near(coll[1].radius,20)&&near(coll[1].center[1],10),"Five actual root and child collider literals");
 check(!collider(bad,coll)&&near(coll[0].radius,24),"unsupported collider leaves output unchanged");
 std::printf("original_number_geometry checks=%u failures=%u engine=0\n",checks,failures);return failures?1:0;
}
