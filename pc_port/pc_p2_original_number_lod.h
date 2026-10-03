#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

// Literal source Number sphere LOD math, separate from native activation.
// No fabricated viewport roster, forced near distance, physics or SAVE proof.
namespace p2originalnumber { namespace lod {
struct Vec3 { float x=0,y=0,z=0; };
struct Sphere { Vec3 position;float radius=0; };
struct Plane { Vec3 normal;float offset=0; };
struct CameraSample {
 Vec3 position,viewVector;
 float fieldOfViewTangent=0,cameraSizeModifier=0;
};
struct CameraSnapshot {
 CameraSample sample;
 std::array<Plane,6> planes{};
 unsigned planeCount=0;
};
struct Viewport {
 // Caller supplies actual viewable state and camera snapshot from the actual
 // simulation viewport roster. No assumed count or camera fallback here.
 bool viewable=false;
 const CameraSnapshot* camera=nullptr;
};
enum Flag : std::uint8_t { Mid=1,Far=2,Visible=4,PikiInCell=8,VisibleVP0=16,VisibleVP1=32 };
struct Result {
 std::uint8_t flags=Far;
 std::array<float,2> screenSize{};
 std::array<bool,2> sampled{};
};
// Source JMath table indexing and updateScreenConstants expression from
// actual projection values. Host libm is not a PPC bit-equivalence claim.
bool projectionSample(Vec3 position,Vec3 viewVector,float fovDegrees,float nearClip,float farClip,CameraSample&,std::string& error);
bool calcScreenSize(const CameraSample&,const Sphere&,float& out,std::string& error);
bool sphereVisible(const CameraSnapshot&,const Sphere&,bool& out,std::string& error);
bool evaluate(const Sphere&,const Viewport*,std::size_t count,bool pikiInCell,Result&,std::string& error);
// Pellet::update's actual movie-actor override occurs AFTER updateLOD.
void movieActorOverride(Result&) noexcept;
bool usesRigidFive(const Result&) noexcept;
// Verified original direct Number getLODSphere radii: twice the model rough
// radius, clamped to JNT1 joint bounding radius. Center comes from the actual
// current source CollTree bounding sphere; this API does not invent a center.
bool numberSphere(unsigned number,Vec3 actualCollTreeCenter,Sphere&,std::string& error);

// Thin reader for the actual native Camera public fields (include/Camera.h).
// Raw planes intentionally avoid isPointVisible's netplay sim-pass forced
// visibility policy. This captures real camera data but does NOT establish
// that the caller chose source-equivalent, current simulation viewports.
template<class NativeCamera>
bool snapshotNativeCamera(const NativeCamera* camera,CameraSnapshot& out,std::string& error){
 if(!camera){error="Number LOD actual camera unavailable";return false;}
 if(camera->mActivePlaneCount<1||camera->mActivePlaneCount>6){error="Number LOD actual camera frustum unavailable";return false;}
 CameraSnapshot snapshot;
 Vec3 position{camera->mPosition.x,camera->mPosition.y,camera->mPosition.z};
 Vec3 view{-camera->mViewZAxis.x,-camera->mViewZAxis.y,-camera->mViewZAxis.z};
 if(!projectionSample(position,view,camera->mFov,camera->mNear,camera->mFar,snapshot.sample,error))return false;
 snapshot.planeCount=unsigned(camera->mActivePlaneCount);
 for(unsigned i=0;i<snapshot.planeCount;++i){
  if(!camera->mPlanePointers[i]){error="Number LOD actual camera plane unavailable";return false;}
  const auto& p=camera->mPlanePointers[i]->mPlane;
  snapshot.planes[i]={{p.mNormal.x,p.mNormal.y,p.mNormal.z},p.mOffset};
 }
 // Validate all copied planes without an actual sphere-dependent claim.
 bool ignored=false;
 if(!sphereVisible(snapshot,{position,0},ignored,error))return false;
 out=snapshot;error.clear();return true;
}
}}
