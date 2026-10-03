#pragma once
#include "pc_p2_original_piki_runtime.h"
#include <memory>
class Shape;
class Graphics;
namespace p2original { namespace piki {
// The singular selected-resource owner implements this view. It must retain
// every returned immutable Shape through animator retirement. current checks
// the actual StageContext, native serial and selected revision, including
// inactive cleanup. No disk/cache fallback or resource-readiness flag exists.
class AnimationBank {
public:
 virtual ~AnimationBank()=default;
 virtual const captain::LoadedScene& scene()const=0;
 virtual bool current(std::string&)const=0;
 virtual Shape* shape(const std::string& exactSelectedRole)const=0;
};
struct AnimatedPose {
 Shape* shape=nullptr;
 Shape* happaShapes[3]={};
 float happa[12]={};
 float sourceFrame=0;
 unsigned sampledFrame=0;
};
// Literal SysShape::Animator clocks/keys over authenticated RGB source clips.
// Rendering exposes the bank's nearest preceding authored sampled pose; the
// 12-pose bank is a sparse visual approximation, never a dense J3D pose claim.
// No P1 Pani animator is created, queried or changed.
class NativeAnimator final {
public:
 explicit NativeAnimator(AnimationBank&);
 ~NativeAnimator();
 NativeAnimator(const NativeAnimator&)=delete;
 NativeAnimator& operator=(const NativeAnimator&)=delete;
 // Before bodies: authenticate selected RGB bank/registry/BCA bytes and all
 // actual Shape roles. Atomically installs the requested complete RGB bank.
 bool prepare(unsigned sourceSpecies,std::string&);
 // Exact committed canonical physical association; Loading supports bootstrap.
 bool attach(Handle,std::string&);
 bool supports(Handle,Motion,std::string&)const;
 bool start(Handle,Motion,std::string&);
 bool advance(Handle,float seconds,std::string&);
 bool status(Handle,Motion&,float& rate,bool& completed,std::string&)const;
 bool speed(Handle,float,std::string&);
 bool finish(Handle,std::string&);
 bool loopStart(Handle,std::string&);
 bool pose(Handle,AnimatedPose&,std::string&)const;
 // Source draw branch, before ordinary ViewPiki::refresh/Pani. The source
 // actor dispatcher owns alive/movie eligibility. Native matrix/draw calls
 // consume the actual selected pose/happa Shapes, never P1 model instances.
 bool draw(Handle,Graphics&,std::string&)const;
 bool canRetire(std::string&)const;
 bool retire(std::string&);
 std::size_t retainedBodies()const noexcept;
 bool owned()const noexcept;
private:
 struct Impl;
 std::unique_ptr<Impl> impl;
};
} }
