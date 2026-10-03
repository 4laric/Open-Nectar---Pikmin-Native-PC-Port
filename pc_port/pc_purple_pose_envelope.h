#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
// Fixture-only captured-pose admission reserve. No future animation sweep claim.
class PcPurplePoseEnvelope {
public:
 struct Part {unsigned id=0;std::uintptr_t key=0;float horizontal=0,low=0,high=0,radius=0;};
 bool observe(std::uintptr_t owner,std::uintptr_t key,unsigned id,float x,float y,float z,float radius) {
  if(failed_||!owner||!key||!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z)||!std::isfinite(radius)||radius<=0)return fail();
  if(owner_&&owner_!=owner)return fail();
  owner_=owner;
  const float horizontal=std::hypot(x,z);if(!std::isfinite(horizontal))return fail();
  for(unsigned i=0;i<count_;++i)if(parts_[i].id==id){
   Part& p=parts_[i];if(p.key!=key)return fail();
   p.horizontal=std::max(p.horizontal,horizontal);p.low=std::min(p.low,y);p.high=std::max(p.high,y);p.radius=std::max(p.radius,radius);return true;
  }
  if(count_==parts_.size())return fail();
  parts_[count_++]={id,key,horizontal,y,y,radius};return true;
 }
 const Part* find(unsigned id,std::uintptr_t key) const {
  if(failed_)return nullptr;
  for(unsigned i=0;i<count_;++i)if(parts_[i].id==id&&parts_[i].key==key)return &parts_[i];
  return nullptr;
 }
 static bool projectedRadius(const Part& p,float relativeY,float otherRadius,float& radius) {
  if(!std::isfinite(relativeY)||!std::isfinite(otherRadius)||otherRadius<=0
   ||!std::isfinite(p.horizontal)||p.horizontal<0||!std::isfinite(p.low)||!std::isfinite(p.high)||p.low>p.high
   ||!std::isfinite(p.radius)||p.radius<=0)return false;
  const float dy=std::max(0.f,std::max(p.low-relativeY,relativeY-p.high)-.1f),sum=p.radius+otherRadius+1.f;
  if(!std::isfinite(dy)||!std::isfinite(sum))return false;
  radius=dy>=sum?0.f:std::sqrt(sum*sum-dy*dy)+p.horizontal;return std::isfinite(radius);
 }
private:
 bool fail(){failed_=true;return false;}
 std::array<Part,32> parts_{};unsigned count_=0;std::uintptr_t owner_=0;bool failed_=false;
};
