#pragma once
#include "pc_p2_original_captain_body_borrower.h"
namespace p2original {namespace captain {namespace bodyphases {
// Actual selected scene geometry is borrowed, never rebuilt from native Shape.
// The stage retains this child until the common phase owner is retired.
std::unique_ptr<SourceSceneTrace> createNativeTrace(const p2retail::SceneContext&,std::string&);
}}}
