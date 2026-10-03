#pragma once
#include "pc_p2_original_number_profile.h"
#include <array>
namespace p2originalnumber {
struct Sphere {float radius=0;std::array<float,3> center{};};
// Literal retail contact and carry geometry. Callers must authenticate the
// actual numeric binding; these helpers cannot identify an ordinary Pellet.
bool terrainRadius(Size,float& out)noexcept;
bool carrierSlot(Size,int slot,float stuckAngle,float radialOffset,bool picked,bool inverted,std::array<float,3>& out)noexcept;
bool particles(Size,std::array<Sphere,4>& out,unsigned& count)noexcept;
bool collider(Size,std::array<Sphere,2>& out)noexcept;
}
