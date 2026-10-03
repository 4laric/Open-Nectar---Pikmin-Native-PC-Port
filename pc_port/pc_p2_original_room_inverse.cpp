#include "pc_p2_original_room_inverse.h"
#include <cmath>
#include <cstdint>
#include <limits>
// Primary native/pikmin2-research/src/Dolphin/mtx/mtx.c:277 PSMTXInverse
// (actual GPVE01 rev0 800EA41C); include/Matrixf.h:404 multTranspose.
namespace p2originalnumber { namespace roomInverse {
namespace {
struct Coefficient {std::int32_t intercept,decrement;};
// Reciprocal-estimate coefficient facts from primary emulator reference:
// https://github.com/dolphin-emu/dolphin/blob/
// 221d396b3acb9e0d1a815cbf7613a9f7032cc14a/Source/Core/Common/FloatUtils.cpp
// Independently expressed interpolation on normalized frexp intervals.
// No emulator implementation code is copied. No Gekko capture/FPSCR claim.
constexpr Coefficient coefficient[32]={
 {0x7ff800,0x3e1},{0x783800,0x3a7},{0x70ea00,0x371},{0x6a0800,0x340},
 {0x638800,0x313},{0x5d6200,0x2ea},{0x579000,0x2c4},{0x520800,0x2a0},
 {0x4cc800,0x27f},{0x47ca00,0x261},{0x430800,0x245},{0x3e8000,0x22a},
 {0x3a2c00,0x212},{0x360800,0x1fb},{0x321400,0x1e5},{0x2e4a00,0x1d1},
 {0x2aa800,0x1be},{0x272c00,0x1ac},{0x23d600,0x19b},{0x209e00,0x18b},
 {0x1d8800,0x17c},{0x1a9000,0x16e},{0x17ae00,0x15b},{0x14f800,0x15b},
 {0x124400,0x143},{0x0fbe00,0x143},{0x0d3800,0x12d},{0x0ade00,0x12d},
 {0x088400,0x11a},{0x065000,0x11a},{0x041c00,0x108},{0x020c00,0x106}
};
bool environment(){float ignored;return triangle::sourceFma(0,0,0,ignored);}
float add(float a,float b){volatile float r=a+b;return r;}
float sub(float a,float b){volatile float r=a-b;return r;}
float mul(float a,float b){volatile float r=a*b;return r;}
bool finite(const Matrix& m){for(float x:m)if(!std::isfinite(x))return false;return true;}
bool fail(std::string& error,const char* message){error=message;return false;}
struct Pair {float a,b;};
// Literal paired lanes, all operations checked before caller output writes.
struct Lanes {
 bool ok=true;
 float fused(float a,float b,float c){float out;if(!triangle::sourceFma(a,b,c,out)){ok=false;return 0;}return out;}
 Pair checked(Pair p){if(!std::isfinite(p.a)||!std::isfinite(p.b))ok=false;return p;}
 Pair product(Pair a,Pair b){return checked({mul(a.a,b.a),mul(a.b,b.b)});}
 Pair sum(Pair a,Pair b){return checked({add(a.a,b.a),add(a.b,b.b)});}
 Pair difference(Pair a,Pair b){return checked({sub(a.a,b.a),sub(a.b,b.b)});}
 Pair madd(Pair a,Pair b,Pair c){return checked({fused(a.a,b.a,c.a),fused(a.b,b.b,c.b)});}
 Pair msub(Pair a,Pair b,Pair c){return checked({fused(a.a,b.a,-c.a),fused(a.b,b.b,-c.b)});}
 Pair nmsub(Pair a,Pair b,Pair c){Pair p=msub(a,b,c);return {-p.a,-p.b};}
 Pair nmadd(Pair a,Pair b,Pair c){Pair p=madd(a,b,c);return {-p.a,-p.b};}
 Pair scalar0(Pair a,Pair b){return product(a,{b.a,b.a});}
};
}
bool sourceFres(float input,float& output) noexcept {
 if(!environment()||!std::isfinite(input)||input==0)return false;
 int exponent;const double significand=std::frexp(std::fabs(static_cast<double>(input)),&exponent);
 const int power=exponent-1;float result;
 if(power < -128)result=std::numeric_limits<float>::max();
 else if(power >=126)result=0;
 else{
  const unsigned bin=static_cast<unsigned>((significand*2.0-1.0)*32768.0);
  const auto c=coefficient[bin/1024];
  const std::int32_t interpolation=c.intercept-(c.decrement*static_cast<std::int32_t>(bin%1024)+1)/2;
  // Exact dyadic binary32 mantissa/exponent construction; no reciprocal divide.
  const double value=std::ldexp(1.0+static_cast<double>(interpolation)/8388608.0,-power-1);
  result=static_cast<float>(value);
 }
 if(!std::isfinite(result))return false;
 output=std::copysign(result,input);return true;
}
bool inverse(const Matrix& src,Matrix& output,std::string& error){
 if(!environment()||!finite(src))return fail(error,"source inverse environment/matrix unavailable");
 Lanes math;
 Pair p0{src[0],1},p1{src[1],src[2]},p2{src[4],1},p3{src[5],src[6]},p4{src[8],1},p5{src[9],src[10]};
 Pair p6{p1.b,p0.a},p7{p3.b,p2.a};
 Pair p11=math.product(p3,p6),p13=math.product(p5,p7),p8{p5.b,p4.a};
 p11=math.msub(p1,p7,p11);
 Pair p12=math.product(p1,p8);p13=math.msub(p3,p8,p13);
 Pair p10=math.product(p3,p4);p12=math.msub(p5,p6,p12);
 Pair p9=math.product(p0,p5);p8=math.product(p1,p2);
 p6=math.difference(p6,p6);p10=math.msub(p2,p5,p10);
 p7=math.product(p0,p13);p9=math.msub(p1,p4,p9);
 p7=math.madd(p2,p12,p7);p8=math.msub(p0,p3,p8);p7=math.madd(p4,p11,p7);
 if(!math.ok)return fail(error,"source inverse cofactor/determinant arithmetic refused");
 if(p7.a==p6.a)return fail(error,"source inverse determinant zero");
 float estimate;if(!sourceFres(p7.a,estimate))return fail(error,"source inverse fres refused");
 p0={estimate,estimate};p6=math.sum(p0,p0);p5=math.product(p0,p0);
 p0=math.nmsub(p7,p5,p6); // negate AFTER fused determinant*r*r - 2r
 p1={src[3],src[3]};p13=math.scalar0(p13,p0);
 p2={src[7],src[7]};p12=math.scalar0(p12,p0);
 p3={src[11],src[11]};p11=math.scalar0(p11,p0);
 p5={p13.a,p12.a};p10=math.scalar0(p10,p0);
 p4={p13.b,p12.b};p9=math.scalar0(p9,p0);
 Matrix out{};out[0]=p5.a;out[1]=p5.b;p6=math.product(p13,p1);
 out[4]=p4.a;out[5]=p4.b;p8=math.scalar0(p8,p0);
 p6=math.madd(p12,p2,p6);out[8]=p10.a;p6=math.nmadd(p11,p3,p6);
 out[9]=p9.a;p7=math.product(p10,p1);p5={p11.a,p6.a};out[10]=p8.a;
 p4={p11.b,p6.b};out[2]=p5.a;out[3]=p5.b;p7=math.madd(p9,p2,p7);
 out[6]=p4.a;out[7]=p4.b;p7=math.nmadd(p8,p3,p7);out[11]=p7.a;
 if(!math.ok||!finite(out))return fail(error,"source inverse refinement/translation arithmetic refused");
 output=out;error.clear();return true;
}
bool normalTranspose(const Matrix& inverse,Vec3 n,Vec3& output,std::string& error){
 if(!environment()||!finite(inverse)||!std::isfinite(n.x)||!std::isfinite(n.y)||!std::isfinite(n.z))return fail(error,"source normal transpose input unavailable");
 Lanes math;Vec3 out;float* value[3]={&out.x,&out.y,&out.z};
 for(unsigned c=0;c<3;++c)*value[c]=math.fused(n.z,inverse[8+c],math.fused(n.x,inverse[c],mul(n.y,inverse[4+c])));
 if(!math.ok)return fail(error,"source normal transpose arithmetic refused");
 output=out;error.clear();return true;
}
} }
