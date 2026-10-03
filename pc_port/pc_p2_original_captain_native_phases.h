#pragma once
#include "pc_p2_original_captain_body_phases.h"
namespace p2retail {class SceneContext;}
namespace p2original {namespace captain {namespace bodyphases {
// Actual stage reset composes the common Navi producer with the selected map
// trace and registered SeaMgr. Retains partial ownership on a failed bind;
// there is no retry replacing a live composition or public authority setter.
bool createNativePhases(const p2retail::SceneContext&,Provider&,SourceSceneTrace&,SourceBank&,std::string&);
bool retireNativePhases(const LoadedScene&,std::string&);
bool cachedNativeWater(const Navi*,bool& hasWater,std::string&);
// Native frame dispatcher runs this before P1 UI gates. Actual GameSystem
// paused/frameTimer and source rate come from the concrete borrowed producer.
bool tickNativePhases(float actualRate,std::string&);
bool nativeAnimationFrame(const Navi*,nativecontrol::AnimationFrame&,std::string&);
}}}
