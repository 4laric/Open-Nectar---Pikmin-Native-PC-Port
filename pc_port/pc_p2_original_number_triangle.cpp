#include "pc_p2_original_number_triangle.h"
#include <cfenv>
#include <cmath>
#include <cstdint>
#include <limits>

namespace p2originalnumber { namespace triangle {
namespace {
static_assert(std::numeric_limits<float>::is_iec559 &&
              std::numeric_limits<float>::digits == 24 && sizeof(float) == 4,
              "Source arithmetic requires IEEE binary32");
static_assert(std::numeric_limits<double>::is_iec559 &&
              std::numeric_limits<double>::digits == 53 && sizeof(double) == 8,
              "Source estimate requires IEEE binary64");

// Hardware estimate coefficient facts, independently expressed as signed
// integer interpolation in a frexp significand interval. Reference table:
// https://github.com/dolphin-emu/dolphin/blob/
// 221d396b3acb9e0d1a815cbf7613a9f7032cc14a/Source/Core/Common/FloatUtils.cpp
// No Dolphin implementation code is used here. Table agreement is emulator
// reference evidence; these helpers do not claim a hardware capture comparison.
struct Coefficient { std::int32_t intercept, slope; };
constexpr Coefficient coefficients[32] = {
 {0x1a7e800,-0x568},{0x17cb800,-0x4f3},{0x1552800,-0x48d},{0x130c000,-0x435},
 {0x10f2000,-0x3e7},{0x0eff000,-0x3a2},{0x0d2e000,-0x365},{0x0b7c000,-0x32e},
 {0x09e5000,-0x2fc},{0x0867000,-0x2d0},{0x06ff000,-0x2a8},{0x05ab800,-0x283},
 {0x046a000,-0x261},{0x0339800,-0x243},{0x0218800,-0x226},{0x0105800,-0x20b},
 {0x3ffa000,-0x7a4},{0x3c29000,-0x700},{0x38aa000,-0x670},{0x3572000,-0x5f2},
 {0x3279000,-0x584},{0x2fb7000,-0x524},{0x2d26000,-0x4cc},{0x2ac0000,-0x47e},
 {0x2881000,-0x43a},{0x2665000,-0x3fa},{0x2468000,-0x3c2},{0x2287000,-0x38e},
 {0x20c1000,-0x35e},{0x1f12000,-0x332},{0x1d79000,-0x30a},{0x1bf4000,-0x2e6}
};
bool environment() noexcept {
 // Reject FTZ/DAZ as well as directed rounding, rather than silently changing
 // the process environment. Volatile operands prevent constant folding.
 volatile float tiny = std::numeric_limits<float>::denorm_min();
 volatile float one = 1.0f;
#if defined(__MINGW32__) && !defined(_RC_NEAR)
 // Engine Dolphin/float.h shadows CRT float.h. MinGW's FE_TONEAREST expands
 // to _RC_NEAR, whose verified CRT value is zero; do not alter global headers.
 constexpr int nearest = 0;
#else
 constexpr int nearest = FE_TONEAREST;
#endif
 return std::fegetround() == nearest && tiny * one > 0.0f;
}
float add(float a, float b) noexcept { volatile float r = a + b; return r; }
float sub(float a, float b) noexcept { volatile float r = a - b; return r; }
float mul(float a, float b) noexcept { volatile float r = a * b; return r; }
float divide(float a, float b) noexcept { volatile float r = a / b; return r; }
double add64(double a, double b) noexcept { volatile double r=a+b; return r; }
double sub64(double a, double b) noexcept { volatile double r=a-b; return r; }
float fused(float a, float b, float c) noexcept {
 // Two binary32 operands have an exact binary64 product (48 significant bits,
 // exponent range safely inside binary64). TwoSum retains the exact addition
 // residual, avoiding the host MinGW fmaf rounding error. Correct a binary64
 // midpoint before the final binary32 rounding to avoid double rounding.
 volatile double product = static_cast<double>(a)*static_cast<double>(b);
 const double sum = add64(product,c);
 if (!std::isfinite(sum)) return static_cast<float>(sum);
 const double virtualC = sub64(sum,product);
 const double residual = add64(sub64(product,sub64(sum,virtualC)),sub64(c,virtualC));
 volatile float rounded = static_cast<float>(sum);
 float result = rounded;
 // The nearest/even overflow midpoint rounds to infinity, but a nonzero exact
 // residual toward zero makes the correct binary32 result the largest finite
 // value. Handle this before the finite-neighbor midpoint correction below.
 constexpr double overflowMidpoint = 0x1.ffffffp127;
 if (std::isinf(result) && std::fabs(sum) == overflowMidpoint &&
     ((sum > 0.0 && residual < 0.0) || (sum < 0.0 && residual > 0.0)))
  result = std::copysign(std::numeric_limits<float>::max(),result);
 if (residual != 0.0 && std::isfinite(result)) {
  const float neighbor = std::nextafter(result,residual > 0.0 ?
       std::numeric_limits<float>::infinity() : -std::numeric_limits<float>::infinity());
  if (std::isfinite(neighbor) && sum ==
      (static_cast<double>(result)+static_cast<double>(neighbor))*0.5)
   result = neighbor;
 }
 return result;
}
bool finite(Vec3 v) noexcept {
 return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}
bool finite(const Vertices& v) noexcept {
 return finite(v[0]) && finite(v[1]) && finite(v[2]);
}
Vec3 subtract(Vec3 a, Vec3 b) noexcept {
 return {sub(a.x,b.x),sub(a.y,b.y),sub(a.z,b.z)};
}
Vec3 cross(Vec3 a, Vec3 b) noexcept {
 return {fused(a.y,b.z,-mul(a.z,b.y)),
         fused(a.z,b.x,-mul(a.x,b.z)),
         fused(a.x,b.y,-mul(a.y,b.x))};
}
float dot(Vec3 a, Vec3 b) noexcept {
 return fused(a.z,b.z,fused(a.x,b.x,mul(a.y,b.y)));
}
bool normalize(Vec3& v) noexcept {
 // makePlanes @80417D80: three separately rounded squares, x+y then z.
 const float squared = add(add(mul(v.x,v.x),mul(v.y,v.y)),mul(v.z,v.z));
 float length;
 if (!sourceSqrt(squared,length)) return false;
 if (length > 0.0f) {
  const float inverse = divide(1.0f,length);
  v = {mul(v.x,inverse),mul(v.y,inverse),mul(v.z,inverse)};
 }
 return finite(v);
}
}
bool rawReciprocalSqrt(float input, double& output) noexcept {
 if (!environment() || !std::isfinite(input) || !(input > 0.0f)) return false;
 int exponent;
 const double fraction = std::frexp(static_cast<double>(input), &exponent);
 const int power = exponent - 1;
 // Even powers use the [1,2) reciprocal-root coefficient half; odd powers
 // use [2,4). Every operation below is exact in binary64 for a binary32 input.
 const unsigned index = static_cast<unsigned>((fraction * 2.0 - 1.0) * 32768.0);
 const unsigned half = power % 2 == 0 ? 16u : 0u;
 const auto& c = coefficients[half + (index >> 11)];
 const std::int32_t interpolated = c.intercept + c.slope * static_cast<int>(index & 2047u);
 const int floorHalfPower = power / 2 - (power < 0 && power % 2 != 0 ? 1 : 0);
 const double result = std::ldexp(1.0 + static_cast<double>(interpolated) / 67108864.0,
                                  -floorHalfPower - 1);
 if (!std::isfinite(result)) return false;
 output = result;
 return true;
}
bool sourceSqrt(float input, float& output) noexcept {
 if (!environment() || !std::isfinite(input)) return false;
 float result = input;
 if (input > 0.0f) {
  double estimate;
  if (!rawReciprocalSqrt(input,estimate)) return false;
  // Estimate has at most 27 significant bits; input has 24. Their product
  // fits binary64 exactly, followed by the source fmuls binary32 rounding.
  volatile float rounded = static_cast<float>(estimate * static_cast<double>(input));
  result = rounded;
 }
 if (!std::isfinite(result)) return false;
 output = result;
 return true;
}
bool sourceFma(float a, float b, float c, float& output) noexcept {
 if (!environment() || !std::isfinite(a) || !std::isfinite(b) || !std::isfinite(c))
  return false;
 const float result = fused(a,b,c);
 if (!std::isfinite(result)) return false;
 output = result;
 return true;
}
bool sourceVectorLength(Vec3 v, float& output) noexcept {
 if (!environment() || !finite(v)) return false;
 const float xx=mul(v.x,v.x), yy=mul(v.y,v.y), zz=mul(v.z,v.z);
 const float guard=add(zz,add(xx,yy));
 if (!std::isfinite(guard)) return false;
 float candidate=0.0f;
 if (guard>0.0f && !sourceSqrt(add(zz,fused(v.x,v.x,yy)),candidate)) return false;
 output=candidate;return true;
}
bool makePlanes(const Vertices& v, Plane& face, std::array<Plane,3>& edges) noexcept {
 if (!environment() || !finite(v)) return false;
 Plane candidate;
 candidate.normal = cross(subtract(v[2],v[0]),subtract(v[1],v[0]));
 if (!normalize(candidate.normal)) return false;
 candidate.offset = dot(candidate.normal,v[0]);
 if (!std::isfinite(candidate.offset)) return false;
 std::array<Plane,3> edgeCandidates;
 for (unsigned i=0; i<3; ++i) {
  auto& edge = edgeCandidates[i];
  edge.normal = cross(subtract(v[i],v[(i+1)%3]),candidate.normal);
  if (!normalize(edge.normal)) return false;
  edge.offset = dot(edge.normal,v[i]);
  if (!std::isfinite(edge.offset)) return false;
 }
 face = candidate;
 edges = edgeCandidates;
 return true;
}
bool createSphere(const Vertices& v, Sphere& output) noexcept {
 if (!environment() || !finite(v)) return false;
 Sphere candidate;
 // Source constant lbl_80520314 = binary32 0x3eaaaaab.
 constexpr float third = 0x1.555556p-2f;
 candidate.center = {mul(add(add(v[0].x,v[1].x),v[2].x),third),
                     mul(add(add(v[0].y,v[1].y),v[2].y),third),
                     mul(add(add(v[0].z,v[1].z),v[2].z),third)};
 if (!finite(candidate.center)) return false;
 for (const auto& vertex : v) {
  const Vec3 d = subtract(vertex,candidate.center);
  // createSphere @80416A44: rounded y*y, then fused x*x and z*z.
  const float squared = dot(d,d);
  float distance;
  if (!sourceSqrt(squared,distance)) return false;
  if (distance > candidate.radius) candidate.radius = distance;
 }
 output = candidate;
 return true;
}
bool build(const Vertices& v, Geometry& output) noexcept {
 Geometry candidate;
 if (!makePlanes(v,candidate.face,candidate.edges) || !createSphere(v,candidate.sphere))
  return false;
 output = candidate;
 return true;
}
namespace {
bool validSphere(const Sphere& sphere) noexcept {
 return finite(sphere.center) && std::isfinite(sphere.radius) && sphere.radius >= 0.0f;
}
bool validPlane(const Plane& plane) noexcept {
 return finite(plane.normal) && std::isfinite(plane.offset);
}
bool normaliseContact(Vec3& v, bool fusedSquares, float& length) noexcept {
 const float squared = fusedSquares ? dot(v,v) :
     add(add(mul(v.x,v.x),mul(v.y,v.y)),mul(v.z,v.z));
 if (!sourceSqrt(squared,length)) return false;
 if (length > 0.0f) {
  const float inverse = divide(1.0f,length);
  v = {mul(v.x,inverse),mul(v.y,inverse),mul(v.z,inverse)};
 } else {
  length = 0.0f; // Source normalise's nonpositive return value.
 }
 return finite(v);
}
bool edgeResult(const Sphere& sphere, Vec3 normal, float length,
                float parameter, EdgeContact& candidate) noexcept {
 candidate.normal = length == 0.0f ? Vec3{} : normal;
 candidate.strength = sub(sphere.radius,length);
 candidate.parameter = parameter;
 return finite(candidate.normal) && std::isfinite(candidate.strength) &&
        std::isfinite(candidate.parameter);
}
bool contactResult(const Sphere& sphere, Vec3 normal, float strength,
                   ContactKind kind, unsigned index, Contact& candidate) noexcept {
 candidate.normal = normal;
 candidate.strength = strength;
 candidate.kind = kind;
 candidate.edgeIndex = index;
 candidate.point = subtract(sphere.center,
        {mul(normal.x,sphere.radius),mul(normal.y,sphere.radius),mul(normal.z,sphere.radius)});
 return finite(candidate.point) && finite(candidate.normal) && std::isfinite(strength);
}
}
Result intersectEdge(const Sphere& sphere, Vec3 start, Vec3 end, EdgeContact& output) noexcept {
 if (!environment() || !validSphere(sphere) || !finite(start) || !finite(end))
  return Result::Invalid;
 Vec3 direction = subtract(end,start);
 float edgeLength;
 // @80416574: rounded y square, then x and z fused squares.
 if (!normaliseContact(direction,true,edgeLength)) return Result::Invalid;
 const Vec3 startSeparation = subtract(sphere.center,start);
 const float parameter = dot(startSeparation,direction);
 if (!std::isfinite(parameter)) return Result::Invalid;
 EdgeContact candidate;
 if (parameter < 0.0f || parameter > edgeLength) {
  // Source always tests start first, then end; it does not select the nearest
  // endpoint from the parameter. Acceptance is <=, unlike the interior <.
  for (unsigned i=0; i<2; ++i) {
   const Vec3 endpoint = i == 0 ? start : end;
   const Vec3 separation = subtract(endpoint,sphere.center);
   float distance;
   if (!sourceSqrt(dot(separation,separation),distance)) return Result::Invalid;
   if (distance <= sphere.radius) {
    Vec3 normal = subtract(sphere.center,endpoint);
    float normalLength;
    // @80416688 /80416794: stored Vector3 normalisation uses three rounded
    // squares and two additions, despite fused endpoint acceptance above.
    if (!normaliseContact(normal,false,normalLength) ||
        !edgeResult(sphere,normal,normalLength,static_cast<float>(i),candidate))
     return Result::Invalid;
    output = candidate;
    return Result::Hit;
   }
  }
  return Result::Miss;
 }
 const Vec3 projection = {mul(direction.x,parameter),mul(direction.y,parameter),
                          mul(direction.z,parameter)};
 Vec3 perpendicular = subtract(startSeparation,projection);
 float distance;
 // @80416838: interior normalisation again uses fused squares.
 if (!normaliseContact(perpendicular,true,distance)) return Result::Invalid;
 if (!(distance < sphere.radius)) return Result::Miss;
 if (!edgeResult(sphere,perpendicular,distance,parameter,candidate)) return Result::Invalid;
 output = candidate;
 return Result::Hit;
}
Result sweep(const Vertices& v, const Geometry& geometry, const Sphere& sphere,
             SweepType type, Contact& output) noexcept {
 if (!environment() || !finite(v) || !validSphere(sphere) ||
     !validPlane(geometry.face)) return Result::Invalid;
 for (const auto& plane : geometry.edges)
  if (!validPlane(plane)) return Result::Invalid;
 if (type != SweepType::InsidePlane && type != SweepType::IntersectPlane)
  return Result::Invalid;
 const float distance = sub(dot(sphere.center,geometry.face.normal),geometry.face.offset);
 if (!std::isfinite(distance)) return Result::Invalid;
 if (type == SweepType::InsidePlane) {
  if (std::fabs(distance) > sphere.radius) return Result::Miss;
 } else {
  const float lower = sub(-sphere.radius,5.0f);
  if (!std::isfinite(lower)) return Result::Invalid;
  if (distance > sphere.radius || distance < lower) return Result::Miss;
 }
 bool inside = true;
 for (const auto& plane : geometry.edges) {
  const float edgeDistance = sub(dot(sphere.center,plane.normal),plane.offset);
  if (!std::isfinite(edgeDistance)) return Result::Invalid;
  if (edgeDistance > 0.0f) inside = false;
 }
 Contact candidate;
 if (inside) {
  if (!contactResult(sphere,geometry.face.normal,sub(sphere.radius,distance),
                     ContactKind::Face,3,candidate)) return Result::Invalid;
  output = candidate;
  return Result::Hit;
 }
 for (unsigned i=0; i<3; ++i) {
  EdgeContact edge;
  const Result result = intersectEdge(sphere,v[i],v[(i+1)%3],edge);
  if (result == Result::Invalid) return result;
  if (result == Result::Hit) {
   if (!contactResult(sphere,edge.normal,edge.strength,ContactKind::Edge,i,candidate))
    return Result::Invalid;
   output = candidate;
   return Result::Hit;
  }
 }
 return Result::Miss;
}
} }
