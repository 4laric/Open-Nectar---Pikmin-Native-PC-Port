#pragma once
#include "pc_p2_original_number_triangle.h"
#include <array>
#include <string>
namespace p2originalnumber { namespace roomInverse {
using Matrix=std::array<float,12>; // row-major source 3x4 affine matrix
using Vec3=rigid::Vec3;
// Finite nonzero binary32 source fres estimate. Coefficient facts have an
// independently expressed emulator reference, not a hardware capture claim.
// Preserves source small-input saturation / large-input signed zero.
// IEEE nearest/gradual-underflow required; all failures preserve output.
bool sourceFres(float input,float& output) noexcept;
// Literal PSMTXInverse paired lane schedule (source 800EA41C). Refuses singular,
// invalid environment/nonfinite operands/intermediates instead of producing
// unsupported nonfinite matrices. No ideal reciprocal/inverse substitution.
// Pure math only: no matrix owner, roster, source body, query or live grant.
bool inverse(const Matrix&,Matrix&,std::string& error);
// Matrixf::multTranspose scalar column order; NO translation or normalization.
// Caller supplies the actual source inverse. This API grants no ownership.
bool normalTranspose(const Matrix& actualSourceInverse,Vec3 normal,Vec3&,std::string& error);
} }
