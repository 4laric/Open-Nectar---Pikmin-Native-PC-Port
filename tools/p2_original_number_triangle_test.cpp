#include "pc_p2_original_number_triangle.h"
#include <cfenv>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>

namespace t = p2originalnumber::triangle;
namespace {
#if defined(__MINGW32__) && !defined(_RC_NEAR)
// Verified MinGW CRT rounding constants; engine Dolphin/float.h shadows them.
constexpr int nearestRounding=0,downwardRounding=0x100;
#else
constexpr int nearestRounding=FE_TONEAREST,downwardRounding=FE_DOWNWARD;
#endif
int checks = 0, failures = 0;
void check(bool passed, const char* label) {
 ++checks;
 if (!passed) { ++failures; std::cerr << "FAIL: " << label << '\n'; }
}
float fromBits(std::uint32_t bits) { float f; std::memcpy(&f,&bits,sizeof(f)); return f; }
std::uint32_t bits(float f) { std::uint32_t b; std::memcpy(&b,&f,sizeof(b)); return b; }
std::uint64_t bits(double d) { std::uint64_t b; std::memcpy(&b,&d,sizeof(b)); return b; }
struct EstimateControl { std::uint32_t input; std::uint64_t estimate; std::uint32_t product; };
// Literal controls at both ends of every estimate coefficient interval, in both
// exponent parities. Generated from the pinned coefficient facts with integer
// significand construction, independently of production frexp/ldexp indexing.
// They establish agreement with that emulator reference, not a hardware run.
constexpr EstimateControl estimates[] = {
 {0x40000000u,0x3fe69fa000000000ull,0x3fb4fd00u},
 {0x4007ff00u,0x3fe5f2b5a0000000ull,0x3fba8da9u},
 {0x40080000u,0x3fe5f2e000000000ull,0x3fba9070u},
 {0x400fff00u,0x3fe55493cc000000ull,0x3fbff7ddu},
 {0x40100000u,0x3fe554a000000000ull,0x3fbff9a0u},
 {0x4017ff00u,0x3fe4c31234000000ull,0x3fc53be1u},
 {0x40180000u,0x3fe4c30000000000ull,0x3fc53c80u},
 {0x401fff00u,0x3fe43c70d4000000ull,0x3fca5b25u},
 {0x40200000u,0x3fe43c8000000000ull,0x3fca5d00u},
 {0x4027ff00u,0x3fe3bfaf9c000000ull,0x3fcf5b78u},
 {0x40280000u,0x3fe3bfc000000000ull,0x3fcf5d60u},
 {0x402fff00u,0x3fe34b8e88000000ull,0x3fd43debu},
 {0x40300000u,0x3fe34b8000000000ull,0x3fd43e80u},
 {0x4037ff00u,0x3fe2deed94000000ull,0x3fd9027eu},
 {0x40380000u,0x3fe2df0000000000ull,0x3fd90480u},
 {0x403fff00u,0x3fe2794cb8000000ull,0x3fddae71u},
 {0x40400000u,0x3fe2794000000000ull,0x3fddaf00u},
 {0x4047ff00u,0x3fe219cbf0000000ull,0x3fe24154u},
 {0x40480000u,0x3fe219c000000000ull,0x3fe241e0u},
 {0x404fff00u,0x3fe1bfcb40000000ull,0x3fe6bc36u},
 {0x40500000u,0x3fe1bfc000000000ull,0x3fe6bcc0u},
 {0x4057ff00u,0x3fe16acaa0000000ull,0x3feb2099u},
 {0x40580000u,0x3fe16ae000000000ull,0x3feb22d0u},
 {0x405fff00u,0x3fe11a8a0c000000ull,0x3fef727bu},
 {0x40600000u,0x3fe11a8000000000ull,0x3fef7300u},
 {0x4067ff00u,0x3fe0ce6984000000ull,0x3ff3afedu},
 {0x40680000u,0x3fe0ce6000000000ull,0x3ff3b070u},
 {0x406fff00u,0x3fe086090c000000ull,0x3ff7d97fu},
 {0x40700000u,0x3fe0862000000000ull,0x3ff7dbe0u},
 {0x4077ff00u,0x3fe0416898000000ull,0x3ffbf4d1u},
 {0x40780000u,0x3fe0416000000000ull,0x3ffbf550u},
 {0x407fff00u,0x3fe000082c000000ull,0x3fffff83u},
 {0x3f800000u,0x3feffe8000000000ull,0x3f7ff400u},
 {0x3f87ff00u,0x3fef0a1e90000000ull,0x3f83ea0au},
 {0x3f880000u,0x3fef0a4000000000ull,0x3f83eb90u},
 {0x3f8fff00u,0x3fee2a5c00000000ull,0x3f87bdadu},
 {0x3f900000u,0x3fee2a8000000000ull,0x3f87bf40u},
 {0x3f97ff00u,0x3fed5c99c0000000ull,0x3f8b76efu},
 {0x3f980000u,0x3fed5c8000000000ull,0x3f8b7760u},
 {0x3f9fff00u,0x3fec9e57c8000000ull,0x3f8f16d2u},
 {0x3fa00000u,0x3fec9e4000000000ull,0x3f8f1740u},
 {0x3fa7ff00u,0x3febedd610000000ull,0x3f929fc4u},
 {0x3fa80000u,0x3febedc000000000ull,0x3f92a030u},
 {0x3fafff00u,0x3feb495490000000ull,0x3f961277u},
 {0x3fb00000u,0x3feb498000000000ull,0x3f961440u},
 {0x3fb7ff00u,0x3feab01330000000ull,0x3f997399u},
 {0x3fb80000u,0x3feab00000000000ull,0x3f997400u},
 {0x3fbfff00u,0x3fea2051f8000000ull,0x3f9cc11bu},
 {0x3fc00000u,0x3fea204000000000ull,0x3f9cc180u},
 {0x3fc7ff00u,0x3fe99910e8000000ull,0x3f9ffbddu},
 {0x3fc80000u,0x3fe9994000000000ull,0x3f9ffdd0u},
 {0x3fcfff00u,0x3fe91a0fe8000000ull,0x3fa3289fu},
 {0x3fd00000u,0x3fe91a0000000000ull,0x3fa32900u},
 {0x3fd7ff00u,0x3fe8a1cf08000000ull,0x3fa64370u},
 {0x3fd80000u,0x3fe8a1c000000000ull,0x3fa643d0u},
 {0x3fdfff00u,0x3fe8300e38000000ull,0x3fa94fa2u},
 {0x3fe00000u,0x3fe8304000000000ull,0x3fa951c0u},
 {0x3fe7ff00u,0x3fe7c48d78000000ull,0x3fac5044u},
 {0x3fe80000u,0x3fe7c48000000000ull,0x3fac50a0u},
 {0x3fefff00u,0x3fe75e4cc8000000ull,0x3faf4285u},
 {0x3ff00000u,0x3fe75e4000000000ull,0x3faf42e0u},
 {0x3ff7ff00u,0x3fe6fd0c28000000ull,0x3fb22866u},
 {0x3ff80000u,0x3fe6fd0000000000ull,0x3fb228c0u},
 {0x3fffff00u,0x3fe6a04b98000000ull,0x3fb501a8u}
};
std::array<std::uint32_t,20> flatten(const t::Geometry& g) {
 std::array<std::uint32_t,20> result{};
 unsigned i=0;
 const auto plane = [&](const t::Plane& p) {
  result[i++]=bits(p.normal.x); result[i++]=bits(p.normal.y);
  result[i++]=bits(p.normal.z); result[i++]=bits(p.offset);
 };
 plane(g.face); for (const auto& p : g.edges) plane(p);
 result[i++]=bits(g.sphere.center.x); result[i++]=bits(g.sphere.center.y);
 result[i++]=bits(g.sphere.center.z); result[i]=bits(g.sphere.radius);
 return result;
}
bool sameBits(const t::Geometry& g, const std::array<std::uint32_t,20>& expected) {
 const auto actual=flatten(g);
 if (actual==expected) return true;
 for (unsigned i=0; i<actual.size(); ++i)
  if (actual[i]!=expected[i])
   std::cerr << "lane " << i << ": actual 0x" << std::hex << actual[i]
             << ", expected 0x" << expected[i] << std::dec << '\n';
 return false;
}
// Independent exact-rational instruction oracle controls: round each specified
// binary32 operation to nearest/even; FMA rounds only after the exact sum. The
// oracle uses integer significand coefficient lookup, not production arithmetic.
constexpr std::array<std::uint32_t,20> flatExpected{{
 0,0x3f800600u,0,0,0,0,0xbf8005bcu,0,0x3f350c87u,0x80000000u,0x3f350c87u,
 0x40350c87u,0xbf8005bcu,0,0,0,0x3faaaaabu,0,0x3faaaaabu,0x403ed33au}};
constexpr std::array<std::uint32_t,20> asymmetricExpected{{
 0xbf0c1dcdu,0x3ec90328u,0xbf3d43b2u,0xc0f936edu,0x3d803a53u,
 0xbf5c874fu,0xbf00f988u,0xbf4fab2eu,0x3eef38a0u,0x3f603fddu,
 0x3df44622u,0x4123615fu,0xbf4816cbu,0xbf0e3639u,0x3e913899u,
 0x404a9726u,0x40900000u,0x4062aaabu,0x41115556u,0x4184e60bu}};
constexpr std::array<std::uint32_t,20> precisionExpected{{
 0x3f2bcbe4u,0x3eca2deau,0xbf20a48au,0xc320623eu,0x3f318739u,
 0xbf222ec9u,0x3eaf9751u,0x42ae89f2u,0xbefb281cu,0x3f5efdf3u,
 0x3cc0e14eu,0x41064c75u,0x3e2a6403u,0xbf67b1e9u,0xbec87e1bu,
 0xc2ca7cf5u,0xbee55556u,0x3ff0b53eu,0x43802556u,0x40377d97u}};
}
int main() {
 check(std::fegetround()==nearestRounding,"nearest rounding environment");
 for (const auto& c : estimates) {
  double estimate=0; float product=0;
  check(t::rawReciprocalSqrt(fromBits(c.input),estimate) && bits(estimate)==c.estimate,
        "raw estimate interval control");
  check(t::sourceSqrt(fromBits(c.input),product) && bits(product)==c.product,
        "source estimate product control");
 }
 // Negative exponents, parity boundaries, and float subnormal normalization.
 double estimate=0;
 check(t::rawReciprocalSqrt(0.25f,estimate) && bits(estimate)==0x3ffffe8000000000ull,
       "negative even exponent estimate");
 check(t::rawReciprocalSqrt(0.5f,estimate) && bits(estimate)==0x3ff69fa000000000ull,
       "negative odd exponent estimate");
 check(t::rawReciprocalSqrt(std::numeric_limits<float>::denorm_min(),estimate) &&
       bits(estimate)==0x44969fa000000000ull,"minimum subnormal estimate");
 check(t::rawReciprocalSqrt(std::numeric_limits<float>::min(),estimate) &&
       bits(estimate)==0x43dffe8000000000ull,"minimum normal estimate");
 float value=0;
 const float tiny=std::numeric_limits<float>::denorm_min();
 check(t::sourceFma(15,fromBits(0xbf3d43b2),-fromBits(0x409d0a77),value) &&
       bits(value)==0xc17ff4b2u,"host fmaf regression exact product sum");
 check(t::sourceFma(1.5f,fromBits(0x3f800001),0,value) && bits(value)==0x3fc00002u,
       "fused exact midpoint ties to even upper");
 check(t::sourceFma(1.5f,fromBits(0x3f800001),-tiny,value) && bits(value)==0x3fc00001u,
       "midpoint tiny negative residual avoids double rounding");
 check(t::sourceFma(1.5f,fromBits(0x3f800003),tiny,value) && bits(value)==0x3fc00005u,
       "midpoint tiny positive residual avoids double rounding");
 check(t::sourceFma(-1.5f,fromBits(0x3f800001),tiny,value) && bits(value)==0xbfc00001u,
       "negative midpoint positive residual");
 check(t::sourceFma(-1.5f,fromBits(0x3f800003),-tiny,value) && bits(value)==0xbfc00005u,
       "negative midpoint negative residual");
 check(t::sourceFma(-0.0f,2,-0.0f,value) && bits(value)==0x80000000u,
       "fused same sign negative zero");
 check(t::sourceFma(-0.0f,2,0.0f,value) && bits(value)==0,"fused opposite zero signs");
 check(t::sourceFma(-tiny,0.5f,-0.0f,value) && bits(value)==0x80000000u,
       "fused negative underflow midpoint");
 const float maximum=std::numeric_limits<float>::max();
 check(t::sourceFma(maximum,2,-maximum,value) && value==maximum,
       "fused product overflow cancellation remains finite");
 check(t::sourceSqrt(1,value) && value != std::sqrt(1.0f),"raw source sqrt is not libm sqrt");
 check(t::sourceSqrt(-0.0f,value) && bits(value)==0x80000000u,"negative zero preserved");
 check(t::sourceSqrt(0.0f,value) && bits(value)==0,"positive zero preserved");
 check(t::sourceSqrt(-9,value) && value==-9,"source nonpositive branch");
 const t::Vertices flat{{{0,0,0},{4,0,0},{0,0,4}}};
 const t::Vertices asymmetric{{{1.25f,-3.5f,7.75f},{17.125f,2.625f,-0.75f},
                               {-4.875f,11.5f,20.25f}}};
 const t::Vertices precision{{{0.03125f,fromBits(0x3f800001),256.25f},
                             {0.875f,fromBits(0x40490fdb),258.5f},
                             {-2.25f,fromBits(0x3fc00001),254.125f}}};
 t::Geometry g;
 check(t::build(flat,g) && sameBits(g,flatExpected),"flat source face edges sphere exact bits");
 check(g.face.normal.y>1,"source estimate normalization retained");
 check(t::build(asymmetric,g) && sameBits(g,asymmetricExpected),"asymmetric instruction order");
 check(t::build(precision,g) && sameBits(g,precisionExpected),"precision-sensitive fused order");
 t::Plane face; std::array<t::Plane,3> edges;
 t::Sphere sphere;
 check(t::makePlanes(precision,face,edges) && t::createSphere(precision,sphere),"independent helpers");
 t::Geometry split{face,edges,sphere};
 check(sameBits(split,precisionExpected),"split matches combined operation");
 t::Vertices reversed=flat; std::swap(reversed[1],reversed[2]);
 check(t::build(reversed,g) && g.face.normal.y<0,"source CA cross BA winding");
 const t::Vertices coincident{{{7,-3,2},{7,-3,2},{7,-3,2}}};
 check(t::build(coincident,g) && g.face.normal.x==0 && g.face.normal.y==0 &&
       g.face.normal.z==0 && g.sphere.radius==0,"finite degenerate source result");
 const t::Vertices ordered{{{16777216,0,0},{-16777216,0,0},{1,0,0}}};
 check(t::createSphere(ordered,sphere) && bits(sphere.center.x)==0x3eaaaaabu,
       "centroid A plus B then C rounding");
 const t::Vertices negativeZero{{{-0.0f,-0.0f,-0.0f},{-0.0f,-0.0f,-0.0f},
                                 {-0.0f,-0.0f,-0.0f}}};
 check(t::createSphere(negativeZero,sphere) && bits(sphere.center.x)==0x80000000u &&
       bits(sphere.radius)==0,"centroid signed zero radius positive zero");
 value=123; estimate=456;
 const float nan=std::numeric_limits<float>::quiet_NaN();
 const float inf=std::numeric_limits<float>::infinity();
 for (float invalid : {nan,inf,-inf}) {
  check(!t::sourceSqrt(invalid,value) && value==123,"invalid sqrt failure atomic");
  check(!t::rawReciprocalSqrt(invalid,estimate) && estimate==456,"invalid estimate failure atomic");
  check(!t::sourceFma(invalid,1,0,value) && value==123,"invalid fused input atomic");
 }
 check(!t::sourceFma(maximum,2,0,value) && value==123,"fused result overflow atomic");
 for (float invalid : {-1.0f,-0.0f,0.0f})
  check(!t::rawReciprocalSqrt(invalid,estimate) && estimate==456,"nonpositive raw estimate rejected");
 check(t::build(asymmetric,g),"prepare sentinel geometry");
 const auto sentinel=flatten(g);
 auto bad=flat; bad[1].x=nan;
 check(!t::build(bad,g) && flatten(g)==sentinel,"invalid vertex combined atomic");
 const t::Vertices overflow{{{0,0,0},{1e30f,0,0},{0,0,1e30f}}};
 check(!t::build(overflow,g) && flatten(g)==sentinel,"finite cross overflow combined atomic");
 face=g.face; edges=g.edges; sphere=g.sphere;
 check(!t::makePlanes(overflow,face,edges) && flatten({face,edges,sphere})==sentinel,
       "plane outputs unchanged after overflow");
 const t::Vertices sphereOverflow{{{1e30f,0,0},{-1e30f,0,0},{0,0,0}}};
 check(!t::createSphere(sphereOverflow,sphere) && flatten({face,edges,sphere})==sentinel,
       "sphere distance overflow atomic");
 const int rounding=std::fegetround();
 check(std::fesetround(downwardRounding)==0,"set directed rounding control");
 check(!t::sourceSqrt(2,value) && value==123,"directed rounding sqrt refusal");
 check(!t::sourceFma(1,2,3,value) && value==123,"directed rounding fused refusal");
 check(!t::rawReciprocalSqrt(2,estimate) && estimate==456,"directed rounding estimate refusal");
 check(!t::build(flat,g) && flatten(g)==sentinel,"directed rounding geometry refusal");
 check(std::fesetround(rounding)==0,"restore rounding control");
 std::cout << "Triangle controls: " << checks << ", failures: " << failures
           << "; engine=0, no hardware/gameplay qualification\n";
 return failures ? 1 : 0;
}
