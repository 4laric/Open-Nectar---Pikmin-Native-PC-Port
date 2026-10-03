#include "pc_p2_original_number_geometry.h"
#include <cmath>
namespace p2originalnumber {
namespace {constexpr float tau=6.2831853071795864769f;}
bool terrainRadius(Size size,float& out)noexcept{
 const auto* p=profile(size);if(!p)return false;
 out=p->height*0.5f;return true;
}
bool carrierSlot(Size size,int slot,float stuck,float offset,bool picked,bool inverted,std::array<float,3>& out)noexcept{
 const auto* p=profile(size);
 if(!p||!std::isfinite(stuck)||!std::isfinite(offset)||(slot!=-2&&(slot<0||unsigned(slot)>=p->carryMax)))return false;
 const float radius=p->pickRadius+offset;if(!std::isfinite(radius)||radius<0)return false;
 const float angle=slot==-2?stuck:(tau/float(p->carryMax))*float(slot);
 const float vertical=p->height*0.5f+1.0f+(picked?4.0f:0.0f);
 const std::array<float,3> result{radius*std::sin(angle),inverted?vertical:-vertical,radius*std::cos(angle)};
 out=result;return true;
}
bool particles(Size size,std::array<Sphere,4>& out,unsigned& count)noexcept{
 const auto* p=profile(size);if(!p)return false;
 std::array<Sphere,4> result{};
 // Retail setupParticles_simple: four height/2 spheres on p_radius-height/2.
 // One's "never" policy has no particles, including in a reused pool slot.
 if(p->particleCount){
  if(p->particleCount!=result.size())return false;
  const float half=p->height*0.5f,ring=p->pickRadius-half;
  for(unsigned i=0;i<result.size();++i){const float angle=(tau/float(result.size()))*float(i);
   result[i]={half,{ring*std::sin(angle),0.0f,ring*std::cos(angle)}};
  }
 }
 out=result;count=p->particleCount;return true;
}
bool collider(Size size,std::array<Sphere,2>& out)noexcept{
 // Authenticated pellet1coll.txt/pellet2coll.txt root and pickup child spheres.
 std::array<Sphere,2> result;
 if(size==Size::One)result={Sphere{11,{0,4,0}},Sphere{10,{0,5,0}}};
 else if(size==Size::Five)result={Sphere{24,{0,12,0}},Sphere{20,{0,10,0}}};
 else return false;
 out=result;return true;
}
}
