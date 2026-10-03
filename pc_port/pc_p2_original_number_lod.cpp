#include "pc_p2_original_number_lod.h"
#include <cmath>
#include <limits>

// Primary: research creatureLOD.cpp:17,39; camera.cpp:103,407,424;
// pelletMgr.cpp:901,2024,3573; sysShapeModel.cpp:81,236; JMath.h:219.
namespace p2originalnumber { namespace lod {
namespace {
bool finite(float v){return std::isfinite(v);}
bool finite(Vec3 v){return finite(v.x)&&finite(v.y)&&finite(v.z);}
float dot(Vec3 a,Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
Vec3 sub(Vec3 a,Vec3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
bool valid(const Sphere& s){return finite(s.position)&&finite(s.radius)&&s.radius>=0;}
bool valid(const CameraSample& s){return finite(s.position)&&finite(s.viewVector)&&finite(dot(s.viewVector,s.viewVector))&&dot(s.viewVector,s.viewVector)>0&&finite(s.fieldOfViewTangent)&&s.fieldOfViewTangent>0&&finite(s.cameraSizeModifier)&&s.cameraSizeModifier<0;}
bool fail(std::string& e,const char* message){e=message;return false;}
}
bool projectionSample(Vec3 position,Vec3 view,float fov,float nearClip,float farClip,CameraSample& out,std::string& e){
 if(!finite(position)||!finite(view)||!finite(fov)||fov<=0||fov>=180||!finite(nearClip)||nearClip<=0||!finite(farClip)||farClip<=nearClip)return fail(e,"Number LOD projection input invalid");
 float radians=((fov*.5f)/180.f)*3.1415927f;
 const unsigned index=unsigned(int(radians*325.9493f))&0x7ffu;
 const double tableRadians=(double(index)*double(6.2831855f))/2048.;
 const float cosine=float(std::cos(tableRadians)),sine=float(std::sin(tableRadians));
 if(sine==0)return fail(e,"Number LOD projection source sine is zero");
 CameraSample sample{position,view,cosine/sine,-(farClip-nearClip)/(farClip*2.f*nearClip)};
 if(!valid(sample))return fail(e,"Number LOD projection produced invalid constants");
 out=sample;e.clear();return true;
}
bool calcScreenSize(const CameraSample& camera,const Sphere& sphere,float& out,std::string& e){
 if(!valid(camera)||!valid(sphere))return fail(e,"Number LOD screen-size input invalid");
 Vec3 delta=sub(sphere.position,camera.position);
 float depth=dot(delta,camera.viewVector),scaledRadius=camera.fieldOfViewTangent*sphere.radius;
 float numerator=camera.cameraSizeModifier*scaledRadius;
 if(!finite(delta)||!finite(depth)||!finite(scaledRadius)||!finite(numerator))return fail(e,"Number LOD screen-size arithmetic overflow");
 // Preserve retail IEEE comparisons: a nonzero sphere on the eye plane has
 // infinite screen size (Near); 0/0 is NaN, whose >threshold tests are false.
 float value=depth==0?(numerator==0?std::numeric_limits<float>::quiet_NaN():std::numeric_limits<float>::infinity()):std::fabs(numerator/depth);
 out=value;e.clear();return true;
}
bool sphereVisible(const CameraSnapshot& camera,const Sphere& sphere,bool& out,std::string& e){
 if(!valid(camera.sample)||!valid(sphere)||camera.planeCount<1||camera.planeCount>6)return fail(e,"Number LOD visibility input/frustum invalid");
 bool visible=true;
 for(unsigned i=0;i<camera.planeCount;++i){const auto& p=camera.planes[i];float magnitude=dot(p.normal,p.normal);
  if(!finite(p.normal)||!finite(p.offset)||!finite(magnitude)||std::fabs(magnitude-1.f)>.001f)return fail(e,"Number LOD plane invalid");
  float distance=dot(sphere.position,p.normal)-p.offset;
  if(!finite(distance))return fail(e,"Number LOD plane-distance overflow");
  if(distance<-sphere.radius)visible=false;
 }
 out=visible;e.clear();return true;
}
bool evaluate(const Sphere& sphere,const Viewport* views,std::size_t count,bool pikiInCell,Result& out,std::string& e){
 if(!valid(sphere)||count>2||(count&&!views))return fail(e,"Number LOD source sphere/viewport roster invalid");
 Result result;std::uint8_t best=Far;bool shouldCull=true;
 for(std::size_t i=0;i<count;++i){
  if(!views[i].viewable)continue;
  if(!views[i].camera)return fail(e,"Number LOD viewable viewport has no actual camera");
  bool visible=false;if(!sphereVisible(*views[i].camera,sphere,visible,e)||!calcScreenSize(views[i].camera->sample,sphere,result.screenSize[i],e))return false;
  result.sampled[i]=true;
  if(visible){shouldCull=false;result.flags=std::uint8_t(result.flags|std::uint8_t(VisibleVP0<<i));}
  float size=result.screenSize[i];std::uint8_t distance=size>.07f?0:(size>.02f?Mid:Far);
  if(distance<best)best=distance;
 }
 // Source clears flags at entry; Result's default Far is for refused callers.
 // Preserve only viewport visibility bits here before adding the chosen tier.
 result.flags=std::uint8_t((result.flags&std::uint8_t(VisibleVP0|VisibleVP1))|best);
 if(!shouldCull)result.flags=std::uint8_t(result.flags|Visible);
 else result.flags=Far;
 if(pikiInCell)result.flags=std::uint8_t(result.flags|PikiInCell);
 out=result;e.clear();return true;
}
void movieActorOverride(Result& result)noexcept{result.flags=std::uint8_t(result.flags|Visible|VisibleVP0|VisibleVP1);}
bool usesRigidFive(const Result& result)noexcept{return (result.flags&std::uint8_t(Mid|Far))<2;}
bool numberSphere(unsigned number,Vec3 center,Sphere& out,std::string& e){
 if(!finite(center)||(number!=1&&number!=5))return fail(e,"Number LOD literal size/actual center invalid");
 // Verified pinned white1/white2 JNT1 bounds and joint sphere radius:
 // 4398d4785dbc740159f3cf304727d8655e89122f3be1cc3aca5259f5ee468f39
 // bc941851d8e5191b77f55b2719ccd80c3ca97f84ee87c779671de7c355dd14cf
 out={center,2.f*(number==1?10.30095386505127f:20.518136978149414f)};e.clear();return true;
}
}}
