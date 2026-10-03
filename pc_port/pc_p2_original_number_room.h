#pragma once
#include "pc_p2_original_number_rigid.h"
#include <array>
#include <string>
namespace p2originalnumber { namespace room {
using Vec3=rigid::Vec3;
using Matrix3x4=std::array<float,12>; // source row-major, translation at 3/7/11
// Normal authored makeRoom/makeOneRoom policy, quarterTurn0..3. Source degree
// casts, JMath initializer values, makeTR FMA and paired PSMTXMultVec ordering.
// No native Matrix4f/quantized quadrant substitute or scene/admission grant.
// Outputs unchanged on refusal. Hardware/libm runtime qualification is separate.
bool make(unsigned quarterTurn,float sourceCentreX,float sourceCentreZ,Matrix3x4&,std::string&);
bool transformVertex(const Matrix3x4&,Vec3 originalSourceVertex,Vec3&,std::string&);
}}
