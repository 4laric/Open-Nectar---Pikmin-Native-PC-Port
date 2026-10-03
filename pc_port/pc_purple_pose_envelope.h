#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
// Fixture-only captured-pose admission reserve. No future animation sweep claim.
class PcPurplePoseEnvelope {
public:
 struct Part {unsigned id=0;std::uintptr_t key=0;float horizontal=0,low=0,high=0,radius=0,lowX=0,highX=0,lowZ=0,highZ=0;};
 bool observe(std::uintptr_t owner,std::uintptr_t key,unsigned id,float x,float y,float z,float radius,float yaw=0) {
  if(failed_||!owner||!key||!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z)||!std::isfinite(radius)||radius<=0||!std::isfinite(yaw))return fail();
  if(owner_&&owner_!=owner)return fail();
  owner_=owner;
  const float horizontal=std::hypot(x,z),localX=std::cos(yaw)*x-std::sin(yaw)*z,localZ=std::sin(yaw)*x+std::cos(yaw)*z;
  if(!std::isfinite(horizontal)||!std::isfinite(localX)||!std::isfinite(localZ))return fail();
  for(unsigned i=0;i<count_;++i)if(parts_[i].id==id){
   Part& p=parts_[i];if(p.key!=key)return fail();
   p.horizontal=std::max(p.horizontal,horizontal);p.low=std::min(p.low,y);p.high=std::max(p.high,y);p.radius=std::max(p.radius,radius);
   p.lowX=std::min(p.lowX,localX);p.highX=std::max(p.highX,localX);p.lowZ=std::min(p.lowZ,localZ);p.highZ=std::max(p.highZ,localZ);return true;
  }
  if(count_==parts_.size())return fail();
  parts_[count_++]={id,key,horizontal,y,y,radius,localX,localX,localZ,localZ};return true;
 }
 const Part* find(unsigned id,std::uintptr_t key) const {
  if(failed_)return nullptr;
  for(unsigned i=0;i<count_;++i)if(parts_[i].id==id&&parts_[i].key==key)return &parts_[i];
  return nullptr;
 }
 static bool projectedRadius(const Part& p,float relativeY,float otherRadius,float& radius) {
  if(!std::isfinite(relativeY)||!std::isfinite(otherRadius)||otherRadius<=0
   ||!std::isfinite(p.horizontal)||p.horizontal<0||!std::isfinite(p.low)||!std::isfinite(p.high)||p.low>p.high
   ||!std::isfinite(p.radius)||p.radius<=0||!std::isfinite(p.lowX)||!std::isfinite(p.highX)||p.lowX>p.highX
   ||!std::isfinite(p.lowZ)||!std::isfinite(p.highZ)||p.lowZ>p.highZ)return false;
  const float dy=std::max(0.f,std::max(p.low-relativeY,relativeY-p.high)-.1f),sum=p.radius+otherRadius+1.f;
  if(!std::isfinite(dy)||!std::isfinite(sum))return false;
  const float spread=std::hypot((p.highX-p.lowX)*.5f,(p.highZ-p.lowZ)*.5f);
  const float centre=std::hypot((p.highX+p.lowX)*.5f,(p.highZ+p.lowZ)*.5f);
  // Three yaw samples at -pi/8,0,+pi/8. Any intermediate offset lies
  // within this chord reserve of a sample. Captured animation box only.
  const float chord=2.f*centre*std::sin(3.141592654f/32.f);
  radius=dy>=sum?0.f:std::sqrt(sum*sum-dy*dy)+spread+chord;return std::isfinite(radius);
 }
 static bool rotatedOffset(const Part& p,float yaw,float& x,float& z){
  if(!std::isfinite(yaw)||!std::isfinite(p.lowX)||!std::isfinite(p.highX)||!std::isfinite(p.lowZ)||!std::isfinite(p.highZ))return false;
  const float lx=(p.lowX+p.highX)*.5f,lz=(p.lowZ+p.highZ)*.5f;
  x=std::cos(yaw)*lx+std::sin(yaw)*lz;z=-std::sin(yaw)*lx+std::cos(yaw)*lz;
  return std::isfinite(x)&&std::isfinite(z);
 }
private:
 bool fail(){failed_=true;return false;}
 std::array<Part,32> parts_{};unsigned count_=0;std::uintptr_t owner_=0;bool failed_=false;
};

constexpr float PcPurplePulseYawHalfArc=3.141592654f/8.f;
inline bool pcPurplePulseYawEligible(float yaw,float targetX,float targetZ,float cursorX,float cursorZ,float cursorSpeed,float faceAdjust,float maximumDt){
 if(!std::isfinite(yaw)||!std::isfinite(targetX)||!std::isfinite(targetZ)||!std::isfinite(cursorX)||!std::isfinite(cursorZ)
  ||!std::isfinite(cursorSpeed)||cursorSpeed<0||!std::isfinite(faceAdjust)||faceAdjust<0||!std::isfinite(maximumDt)
  ||maximumDt<=0||maximumDt>1.f/30.f)return false;
 const float factor=faceAdjust*maximumDt*10.f,target=std::hypot(targetX,targetZ),cursor=std::hypot(cursorX,cursorZ),step=cursorSpeed*maximumDt;
 if(!std::isfinite(factor)||factor>1||!std::isfinite(target)||target<=0||!std::isfinite(cursor)||cursor<=0||!std::isfinite(step)||step>=cursor)return false;
 const float movement=std::fabs(std::remainder(std::atan2(targetX,targetZ)-yaw,6.283185307f));
 const float neutral=std::fabs(std::remainder(std::atan2(cursorX,cursorZ)-yaw,6.283185307f))+std::asin(step/cursor);
 // Creature::moveRotation interpolates with factor[0,1]; the native
 // neutral cursor turn uses0.2. Both stay in this qualified shortest arc.
 // Tangent clamping cannot increase the one-tick cursor displacement.
 return std::isfinite(movement)&&std::isfinite(neutral)&&movement<=PcPurplePulseYawHalfArc&&neutral<=PcPurplePulseYawHalfArc;
}
