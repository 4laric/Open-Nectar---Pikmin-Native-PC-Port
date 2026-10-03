#pragma once
#include <array>
#include <memory>
#include <string>
class Navi;
namespace p2retail {class SceneContext;}
namespace p2original {namespace sourceCamera {class Rig;}namespace captain {class LoadedScene;}}
namespace p2original {namespace captain {namespace camera {
struct ActorPose {std::array<float,3> position{};float face=0;};
// Ordinary source camera update/draw only. Authenticate the exact committed,
// Active body before/after observation. No CPlate offset, controller decoding,
// whistle data or movie-active model translation is inferred here.
bool readCameraPose(const p2retail::SceneContext&,Navi*,ActorPose&,std::string&);
// Only the actual Rig::reset can create this scope. Keep it alive through both
// initial pose/Plate observations and current() immediately before camera state
// publication. It retains the real native phase composition until reset exits.
class CameraResetPoseScope final {
public:
 ~CameraResetPoseScope();
 CameraResetPoseScope(const CameraResetPoseScope&)=delete;
 CameraResetPoseScope& operator=(const CameraResetPoseScope&)=delete;
 bool read(unsigned slot,ActorPose&,std::string&)const;
 bool current(std::string&)const;
private:
 struct Impl;std::unique_ptr<Impl> m;
 explicit CameraResetPoseScope(std::unique_ptr<Impl>);
 static std::unique_ptr<CameraResetPoseScope> begin(const p2retail::SceneContext&,const LoadedScene&,std::string&);
 friend class p2original::sourceCamera::Rig;
};
}}}
