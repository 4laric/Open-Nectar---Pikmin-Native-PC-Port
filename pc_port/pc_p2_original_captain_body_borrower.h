#pragma once
#include "pc_p2_original_captain_native_phases.h"
namespace p2retail {class SceneContext;}
namespace p2original {namespace captain {namespace bodyphases {
// Concrete read-only Navi borrower for original RoomMap consumers. It does
// not own another actor, mesh or scene and does not authorize platform tracing.
// Capture at the real body simulation call; validate before borrowed mesh reads
// and after callbacks. A known CF-dead source body still has a valid lifetime.
class BodyBorrowerGuard {
public:
 static bool capture(const p2retail::SceneContext&,Navi*,BodyBorrowerGuard&,std::string&);
 bool current(std::string&)const;
private:
 const p2retail::SceneContext* context_=nullptr;const LoadedScene* scene_=nullptr;
 const World* world_=nullptr;SourceBank* bank_=nullptr;Owner* phases_=nullptr;
 Navi* actor_=nullptr;const void* state_=nullptr;unsigned slot_=2;
 std::uint64_t serial_=0,revision_=0,birth_=0,motionGeneration_=0;
 std::string campaign_,session_,catalog_;
};
}}}
