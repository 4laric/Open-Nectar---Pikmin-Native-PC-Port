#pragma once
#include "pc_p2_original_captain_body_borrower.h"
namespace p2original {namespace captain {namespace bodyphases {
// Actual selected scene geometry is borrowed, never rebuilt from native Shape.
// The stage retains this child until the common phase owner is retired.
std::unique_ptr<SourceSceneTrace> createNativeTrace(const p2retail::SceneContext&,std::string&);
}}}
// Scene's actual RouteMgr writer authenticates before mutation and again after
// any callback. Only NativeTrace's genuine post-map room receiver supplies it.
bool pc_p2_original_captain_room_visit_current(const p2retail::SceneContext&,
 std::uint64_t serial,std::uint64_t revision,const Navi*,int room,std::string&);
