#pragma once
#include <array>
#include <string>
class Navi;
namespace p2retail {class SceneContext;}
namespace p2original {namespace captain {namespace camera {
struct ActorPose {std::array<float,3> position{};float face=0;};
// Ordinary source camera update/draw only. Authenticate the exact committed,
// Active body before/after observation. No CPlate offset, controller decoding,
// whistle data or movie-active model translation is inferred here.
bool readCameraPose(const p2retail::SceneContext&,Navi*,ActorPose&,std::string&);
}}}
