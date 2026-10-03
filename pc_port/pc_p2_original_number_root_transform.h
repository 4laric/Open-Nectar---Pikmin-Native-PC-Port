#pragma once
#include "pc_p2_original_piki_animator.h"
#include "pc_p2_original_number_yaw.h"
namespace p2originalnumber { namespace bodyYaw {
// Concrete adapter for the parent's frozen original Piki animator interface.
// Its guarded collisionRoot supplies authenticated genuine body/root inputs
// and rechecks their ownership/lifetime after this pure arithmetic callback.
// This header is composed only alongside that actual animator/runtime provider.
class SourceRootTransform final : public p2original::piki::SourceRootWorldTransform {
public:
 bool transform(float face,Vector3f position,Vector3f local,Vector3f& output,
                std::string& error)const override {
  Vec3 candidate;
  if(!worldRoot(face,{position.x,position.y,position.z},{local.x,local.y,local.z},candidate,error))return false;
  output=Vector3f(candidate.x,candidate.y,candidate.z);
  return true;
 }
};
} }
