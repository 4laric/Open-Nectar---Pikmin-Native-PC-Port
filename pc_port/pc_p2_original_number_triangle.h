#pragma once
#include "pc_p2_original_number_rigid.h"
#include <array>

// Pure source arithmetic only: no scene admission, native allocation or gameplay
// qualification. Requires IEEE binary32/binary64, round-to-nearest and gradual
// underflow. The estimate coefficients have an emulator reference; no physical
// Gekko comparison or original floating-point status-register qualification yet.
namespace p2originalnumber { namespace triangle {
using Vec3 = rigid::Vec3;
using Vertices = std::array<Vec3, 3>;
struct Plane { Vec3 normal; float offset = 0; };
struct Sphere { Vec3 center; float radius = 0; };
struct Geometry { Plane face; std::array<Plane, 3> edges; Sphere sphere; };

// Positive finite binary32 operands only. Returns the raw double frsqrte estimate
// without Newton refinement. Nonpositive operands are handled by sourceSqrt.
bool rawReciprocalSqrt(float input, double& output) noexcept;
// Mirrors pikmin2_sqrtf: positive x -> rounded estimate*x; finite x<=0 -> x,
// including negative zero. This is deliberately not the correctly rounded sqrt.
bool sourceSqrt(float input, float& output) noexcept;
// Correctly rounded binary32 a*b+c, independently of host fmaf. Used for the
// source fmadds/fmsubs instruction boundaries. Finite inputs and result only.
bool sourceFma(float a, float b, float c, float& output) noexcept;
// CA cross BA winding; edge order AB, BC, CA. Plane offsets use dot(normal,
// A/B/C). Finite degenerates retain the source zero-length normalization result.
bool makePlanes(const Vertices&, Plane& face, std::array<Plane, 3>& edges) noexcept;
// Rounded centroid (A+B)+C, then largest source-estimate vertex distance.
bool createSphere(const Vertices&, Sphere&) noexcept;
// All public failures leave every output unchanged. No dynamic allocation.
bool build(const Vertices&, Geometry&) noexcept;
} }
