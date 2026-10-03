#include "pc_p2_original_captain_camera_pose.h"
#include "pc_p2_original_captain_body_borrower.h"
#include "Navi.h"
#include <cmath>
namespace p2original {namespace captain {namespace camera {
namespace {bool fail(std::string& error,const char* text){error=text;return false;}}
bool readCameraPose(const p2retail::SceneContext& context,Navi* actor,ActorPose& out,std::string& error){
 bodyphases::BodyBorrowerGuard guard;
 if(!bodyphases::BodyBorrowerGuard::capture(context,actor,guard,error))return false;
 const auto* world=pc_p2_original_captain_world();
 if(!world)return fail(error,"source camera World borrower disappeared");
 const Demo demo=world->demo();
 // Source Navi::getPosition uses model translation with MVP_IsActive. Until
 // that actual movie producer exists, body SRT is valid only outside movies.
 if((demo!=Demo::Inactive&&demo!=Demo::Absent)||!guard.current(error))
  return fail(error,"source camera pose requires ordinary current movie/body authority");
 ActorPose next{{actor->mSRT.t.x,actor->mSRT.t.y,actor->mSRT.t.z},actor->mFaceDirection};
 if(!std::isfinite(next.position[0])||!std::isfinite(next.position[1])||!std::isfinite(next.position[2])||!std::isfinite(next.face))
  return fail(error,"source camera actor pose is nonfinite");
 if(!guard.current(error))return false;
 if(pc_p2_original_captain_world()!=world||world->demo()!=demo||!guard.current(error))
  return fail(error,"source camera actor observation expired");
 out=next;error.clear();return true;
}
}}}
