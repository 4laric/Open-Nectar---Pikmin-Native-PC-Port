#include "pc_p2_original_number_room.h"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
using namespace p2originalnumber::room;
namespace {std::uint32_t bits(float v){std::uint32_t x;std::memcpy(&x,&v,4);return x;}}
int main(){
 unsigned checks=0;auto check=[&](bool ok){++checks;assert(ok);};Matrix3x4 m;std::string error;Vec3 out{99,99,99};
 check(make(0,0,0,m,error));check(m[0]==1&&m[5]==1&&m[10]==1&&bits(m[8])==0x80000000);
 check(transformVertex(m,{1,2,3},out,error)&&out.x==1&&out.y==2&&out.z==3);
 check(make(0,0,3,m,error)&&m[11]==510);
 check(transformVertex(m,{1,2,3},out,error)&&out.x==1&&out.y==2&&out.z==513);
 check(make(2,0,6,m,error)&&m[11]==1020&&m[0]==-1&&bits(m[2])==0x33bbbd2e&&bits(m[8])==0xb3bbbd2e);
 check(transformVertex(m,{0,0,1000000},out,error)&&out.x>0.08f&&out.x<0.09f&&out.z==-998980);
 check(make(1,0,0,m,error)&&bits(m[0])==0xb33bbd2e&&m[2]==-1&&m[8]==1);
 check(make(3,0,0,m,error)&&bits(m[0])==0x340ccde3&&m[2]==1&&m[8]==-1);
 const auto original=m;
 check(!make(4,0,0,m,error)&&m==original);
 check(!make(0,std::numeric_limits<float>::infinity(),0,m,error)&&m==original);
 check(!make(0,std::numeric_limits<float>::max(),0,m,error)&&m==original);
 const auto originalVertex=out;
 check(!transformVertex(m,{std::numeric_limits<float>::quiet_NaN(),0,0},out,error)&&out.x==originalVertex.x);
 // Paired-lane order preserves the small middle product instead of losing it
 // in a left-associated sum (1e20 + 1 - 1e20 + 0).
 Matrix3x4 paired{{1,1,1,0,0,1,0,0,0,0,1,0}};
 check(transformVertex(paired,{1e20f,1,-1e20f},out,error)&&out.x==1);
 std::printf("Original number source room transform: %u controls passed\n",checks);
}
