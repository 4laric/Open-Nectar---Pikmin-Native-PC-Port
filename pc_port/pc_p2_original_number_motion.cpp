#include "pc_p2_original_number_motion.h"
#include <cmath>
namespace p2originalnumber { namespace motion {
namespace {
Vec3 sub(Vec3 a,Vec3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
Vec3 mul(Vec3 a,float b){return {a.x*b,a.y*b,a.z*b};}
float dot(Vec3 a,Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
Vec3 cross(Vec3 a,Vec3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
bool finite(Vec3 a){return std::isfinite(a.x)&&std::isfinite(a.y)&&std::isfinite(a.z);}
float normalize(Vec3& a){float len=std::sqrt(dot(a,a));if(len>0)a=mul(a,1.0f/len);else a={};return len;}
bool edge(Vec3 a,Vec3 b,Vec3 center,float radius,Contact& hit){
 Vec3 direction=sub(b,a);const float length=normalize(direction);
 Vec3 separation=sub(center,a);const float t=dot(separation,direction);
 if(t<0.0f||t>length){
  for(const auto endpoint:{a,b}){
   Vec3 normal=sub(center,endpoint);float distance=std::sqrt(dot(normal,normal));
   if(distance<=radius){normalize(normal);hit={normal,sub(center,mul(normal,radius)),radius-distance};return true;}
  }
  return false;
 }
 Vec3 normal=sub(separation,mul(direction,t));const float distance=normalize(normal);
 if(distance<radius){hit={normal,sub(center,mul(normal,radius)),radius-distance};return true;}
 return false;
}
}
Intersection intersect(const Triangle& t,Vec3 center,float radius,bool hard,Contact& out)noexcept{
 if(!finite(center)||!finite(t.normal)||!std::isfinite(t.offset)||!std::isfinite(radius)||radius<0)return Intersection::Invalid;
 for(const auto v:t.vertices)if(!finite(v))return Intersection::Invalid;
 const float distance=dot(t.normal,center)-t.offset;
 if(!std::isfinite(distance))return Intersection::Invalid;
 if(hard){if(distance>radius||distance<(-radius-5.0f))return Intersection::Miss;}
 else if(std::fabs(distance)>radius)return Intersection::Miss;
 bool inside=true;
 for(unsigned i=0;i<3;++i){
  Vec3 outward=cross(sub(t.vertices[i],t.vertices[(i+1)%3]),t.normal);normalize(outward);
  if(dot(outward,center)-dot(outward,t.vertices[i])>0.0f)inside=false;
 }
 Contact candidate;
 if(inside)candidate={t.normal,sub(center,mul(t.normal,radius)),radius-distance};
 else{
  bool found=false;
  for(unsigned i=0;i<3;++i)if(edge(t.vertices[i],t.vertices[(i+1)%3],center,radius,candidate)){found=true;break;}
  if(!found)return Intersection::Miss;
 }
 if(!finite(candidate.normal)||!finite(candidate.point)||!std::isfinite(candidate.overlap))return Intersection::Invalid;
 out=candidate;return Intersection::Hit;
}
bool restitution(Vec3 v,Vec3 n,float factor,Vec3& out)noexcept{
 if(!finite(v)||!finite(n)||!std::isfinite(factor)||factor<0)return false;
 // Source response is unconditional, including separating intersections.
 Vec3 candidate=sub(v,mul(n,(1.0f+factor)*dot(n,v)));
 if(!finite(candidate))return false;
 out=candidate;return true;
}
bool stepCount(float dt,float speed,float radius,unsigned& count,float& step,unsigned maximum)noexcept{
 if(!std::isfinite(dt)||dt<0||!std::isfinite(speed)||speed<0||!std::isfinite(radius)||radius<0||(maximum!=8&&maximum!=16))return false;
 unsigned next=1;float length=dt;
 // Strict >; source room/surface families cap at eight/sixteen respectively.
 while(length*speed>radius&&next<maximum){next*=2;length*=0.5f;}
 if(!std::isfinite(length*speed))return false;
 count=next;step=length;return true;
}
bool beginSimple(Vec3 v,float dt,bool picked,bool always,bool floor,Vec3& out)noexcept{
 if(!finite(v)||!std::isfinite(dt)||dt<0)return false;
 if((!always&&!picked)||!floor)v.y=-((dt*gravity)-v.y);
 if(!finite(v))return false;
 out=v;return true;
}
bool finishSimple(Vec3 v,Vec3 n,float dt,bool picked,bool always,Vec3& out)noexcept{
 if(!finite(v)||!finite(n)||!std::isfinite(dt)||dt<0)return false;
 if(!picked&&!always){
  const Vec3 tangent=sub(v,mul(n,dot(v,n)));
  v=sub(v,mul(mul(tangent,dt),10.0f));
  const Vec3 fall{0,-(gravity*dt),0};
  const Vec3 projected=sub(fall,mul(n,dot(fall,n)));
  v.x+=-projected.x;v.y+=-projected.y;v.z+=-projected.z;
 }
 if(!finite(v))return false;
 out=v;return true;
}
bool hiddenFloor(rigid::Trace& trace,bool hidden,bool floor,Contact& out,bool& applied)noexcept{
 if(!finite(trace.position)||!finite(trace.velocity)||!std::isfinite(trace.radius)||trace.radius<0||
    !std::isfinite(trace.restitution)||trace.restitution<0)return false;
 if(!hidden||floor||!(trace.position.y-trace.radius<0)){applied=false;return true;}
 auto candidate=trace;candidate.position.y=trace.radius;
 if(candidate.velocity.y<0)candidate.velocity.y=-candidate.velocity.y*(candidate.restitution-1.0f);
 if(!finite(candidate.velocity))return false;
 Contact contact;contact.normal={0,1,0};contact.point=candidate.position;contact.point.y-=candidate.radius;
 contact.overlap=candidate.position.y-trace.position.y;
 if(!finite(contact.point)||!std::isfinite(contact.overlap))return false;
 trace=candidate;out=contact;applied=true;return true;
}
}}
