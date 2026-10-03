#include "pc_p2_original_room_inverse.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cfenv>
#include <limits>
using namespace p2originalnumber::roomInverse;
namespace {
std::uint32_t bits(float x){std::uint32_t b;std::memcpy(&b,&x,4);return b;}
bool same(const Matrix& a,const Matrix& b){for(unsigned i=0;i<12;++i)if(bits(a[i])!=bits(b[i]))return false;return true;}
bool golden(const Matrix& a,const std::uint32_t (&b)[12]){for(unsigned i=0;i<12;++i)if(bits(a[i])!=b[i])return false;return true;}
}
int main(){unsigned checks=0,failures=0;auto check=[&](bool value,const char* name){++checks;if(!value){++failures;std::printf("FAIL %s\n",name);}};std::string error;
 // Independently evaluated exact-Fraction binary32 oracle: every paired/scalar
 // instruction rounded ties-to-even, with signed zero tracked independently;
 // source coefficient integer reciprocal estimate then one source Newton step.
 // No production helper constructs the expected inverse/normal bit values.
 const Matrix source{{1.2f,.3f,-.7f,4.1f,.5f,2.3f,.2f,-1.7f,-.1f,.4f,.9f,3.2f}};
 const std::uint32_t inverseGolden[12]={0x3f82e4e7u,0xbe90b505u,0x3f5bb112u,0xc0ed6b17u,0xbe77514eu,0x3f04de10u,0xbe9b3b34u,0x4035efb1u,0x3e6244f0u,0xbe862ed7u,0x3fabacddu,0xc0b49680u};
 Matrix result{};check(inverse(source,result,error)&&error.empty(),"nontrivial literal paired inverse available");
 check(golden(result,inverseGolden),"nontrivial cofactor/determinant/fres/Newton/translation exact bit golden");
 Matrix alias=source;check(inverse(alias,alias,error)&&same(alias,result),"in-place source inverse preserves actual input read phase");
 Vec3 normal{99,98,97};check(normalTranspose(result,{.2f,-.7f,.3f},normal,error)&&bits(normal.x)==0x3ee13785u&&bits(normal.y)==0xbeff35f2u&&bits(normal.z)==0x3f494641u,"source inverse transpose column FMA golden without normalization");
 Matrix translated=result;translated[3]=100000;translated[7]=-100000;translated[11]=17;Vec3 normal2;
 check(normalTranspose(translated,{.2f,-.7f,.3f},normal2,error)&&bits(normal2.x)==bits(normal.x)&&bits(normal2.y)==bits(normal.y)&&bits(normal2.z)==bits(normal.z),"normal transpose ignores translation");
 const Matrix quarters[4]={{{1,0,0,11,0,1,0,-3,0,0,1,7}},{{0,0,1,11,0,1,0,-3,-1,0,0,7}},{{-1,0,0,11,0,1,0,-3,0,0,-1,7}},{{0,0,-1,11,0,1,0,-3,1,0,0,7}}};
 const std::uint32_t quarterGolden[4][12]={
 {0x3f800000u,0,0,0xc1300000u,0,0x3f800000u,0,0x40400000u,0,0,0x3f800000u,0xc0e00000u},
 {0,0,0xbf800000u,0x40e00000u,0x80000000u,0x3f800000u,0,0x40400000u,0x3f800000u,0x80000000u,0,0xc1300000u},
 {0xbf800000u,0,0,0x41300000u,0,0x3f800000u,0,0x40400000u,0,0,0xbf800000u,0x40e00000u},
 {0,0x80000000u,0x3f800000u,0xc0e00000u,0,0x3f800000u,0x80000000u,0x40400000u,0xbf800000u,0,0,0x41300000u}};
 for(unsigned i=0;i<4;++i)check(inverse(quarters[i],result,error)&&golden(result,quarterGolden[i]),"supplied exact-quarter control including signed zero and translation");
 const Matrix reflected{{-1,0,0,11,0,1,0,-3,0,0,1,7}};
 const std::uint32_t reflectionGolden[12]={0xbf800000u,0x80000000u,0x80000000u,0x41300000u,0x80000000u,0x3f800000u,0x80000000u,0x40400000u,0x80000000u,0x80000000u,0x3f800000u,0xc0e00000u};
 check(inverse(reflected,result,error)&&golden(result,reflectionGolden),"negative determinant source reciprocal/sign/reflection golden");
 const float reciprocalInputs[6]={1,2,3,.75f,10,-3};const std::uint32_t reciprocalGoldens[6]={0x3f7ff800u,0x3efff800u,0x3eaaa800u,0x3faaa800u,0x3dccc800u,0xbeaaa800u};
 float estimate;for(unsigned i=0;i<6;++i)check(sourceFres(reciprocalInputs[i],estimate)&&bits(estimate)==reciprocalGoldens[i],"raw source fres factual reference golden");
 // At each exact mantissa bin boundary, expected bits are emulator coefficient
 // facts; these controls catch table selection/exponent errors independently.
 const std::uint32_t binGolden[32]={0x3f7ff800u,0x3f783800u,0x3f70ea00u,0x3f6a0800u,0x3f638800u,0x3f5d6200u,0x3f579000u,0x3f520800u,0x3f4cc800u,0x3f47ca00u,0x3f430800u,0x3f3e8000u,0x3f3a2c00u,0x3f360800u,0x3f321400u,0x3f2e4a00u,0x3f2aa800u,0x3f272c00u,0x3f23d600u,0x3f209e00u,0x3f1d8800u,0x3f1a9000u,0x3f17ae00u,0x3f14f800u,0x3f124400u,0x3f0fbe00u,0x3f0d3800u,0x3f0ade00u,0x3f088400u,0x3f065000u,0x3f041c00u,0x3f020c00u};
 for(unsigned i=0;i<32;++i)check(sourceFres(1.f+float(i)/32.f,estimate)&&bits(estimate)==binGolden[i],"all source fres mantissa coefficient bins");
 check(sourceFres(1.f+1.f/32768.f,estimate)&&bits(estimate)==0x3f7ff60fu,"odd decrement interpolation uses upward half rounding");
 check(sourceFres(1.f+1023.f/32768.f,estimate)&&bits(estimate)==0x3f7837f0u,"last sub-bin before reciprocal coefficient boundary");
 check(sourceFres(std::ldexp(1.f,-129),estimate)&&bits(estimate)==0x7f7fffffu,"source fres small finite input saturation");
 check(sourceFres(-std::ldexp(1.f,-129),estimate)&&bits(estimate)==0xff7fffffu,"source fres negative saturation sign");
 check(sourceFres(std::ldexp(1.f,-128),estimate)&&bits(estimate)==0x7f7ff800u,"source fres exact small-input boundary");
 check(sourceFres(std::ldexp(1.f,125),estimate)&&bits(estimate)==0x00fff800u,"source fres last large exponent before zero");
 check(sourceFres(std::ldexp(1.f,126),estimate)&&bits(estimate)==0,"source fres large finite input returns zero");
 check(sourceFres(-std::ldexp(1.f,126),estimate)&&bits(estimate)==0x80000000u,"source fres preserves negative zero for large input");
 for(float x:{0.f,-0.f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}){estimate=123;check(!sourceFres(x,estimate)&&estimate==123,"unsupported fres input failure atomic");}
 Matrix saved=result;Matrix singular=quarters[0];singular[4]=singular[0];singular[5]=singular[1];singular[6]=singular[2];
 check(!inverse(singular,result,error)&&!error.empty()&&same(result,saved),"zero determinant failure atomic");
 Matrix tiny=quarters[0];tiny[0]=tiny[5]=tiny[10]=std::ldexp(1.f,-60);
 check(!inverse(tiny,result,error)&&same(result,saved),"source cofactor/determinant underflow to zero refuses");
 Matrix refinement=quarters[0];refinement[0]=refinement[5]=refinement[10]=std::ldexp(1.f,-25);
 check(!inverse(refinement,result,error)&&same(result,saved),"finite determinant with nonfinite reciprocal-square Newton intermediate refuses");
 Matrix overflow=quarters[0];overflow[0]=std::numeric_limits<float>::max();overflow[5]=2;
 check(!inverse(overflow,result,error)&&same(result,saved),"cofactor/determinant overflow refuses atomically");
 Matrix invalid=quarters[0];invalid[7]=std::numeric_limits<float>::quiet_NaN();
 check(!inverse(invalid,result,error)&&same(result,saved),"nonfinite source matrix refuses");
 normal={99,98,97};check(!normalTranspose(saved,{std::numeric_limits<float>::infinity(),0,0},normal,error)&&normal.x==99&&normal.y==98&&normal.z==97,"nonfinite normal refusal preserves output");
 Matrix hugeNormal=quarters[0];hugeNormal[0]=std::numeric_limits<float>::max();
 check(!normalTranspose(hugeNormal,{2,0,0},normal,error)&&normal.x==99,"normal arithmetic overflow failure atomic");
#if defined(__MINGW32__) && !defined(_RC_UP)
 constexpr int upward=0x200; // verified MinGW CRT FE_UPWARD; engine float.h shadows CRT
#else
 constexpr int upward=FE_UPWARD;
#endif
 const int rounding=std::fegetround();if(std::fesetround(upward)==0){estimate=123;check(!sourceFres(3,estimate)&&estimate==123,"fres unsupported rounding environment refuses");check(!inverse(quarters[0],result,error)&&same(result,saved),"inverse unsupported rounding environment refuses atomically");check(!normalTranspose(saved,{0,1,0},normal,error)&&normal.x==99,"transpose unsupported rounding environment refuses atomically");std::fesetround(rounding);}else check(false,"rounding mode control available");
 std::printf("original_room_inverse checks=%u failures=%u native=0 hardware=0 source_owner=0 gameplay=0 save=0\n",checks,failures);return failures?1:0;
}
