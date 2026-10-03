#pragma once
#include "pc_p2_original_number_room.h"
namespace p2originalnumber { namespace bodyYaw {
using Vec3=room::Vec3;
using Matrix=room::Matrix3x4;
// Pure original FakePiki/Matrixf arithmetic for unit scale and (0,face,0).
// The actual animator/body owner authenticates scale, mode and lifetime before
// calling and revalidates afterward. No matrix or source body authority here.
bool bodySRT(float sourceFace,Vec3 sourceBodyPosition,Matrix&,std::string&);
// Translation column of original SDK paired PSMTXConcat(body,root-local matrix).
// Root rotation/scale do not contribute to that column; the authenticated dense
// root supplies its local translation. This does not decode animation.
bool worldRoot(float sourceFace,Vec3 sourceBodyPosition,Vec3 sourceRootLocal,
               Vec3& atomicWorldOutput,std::string&);
} }
