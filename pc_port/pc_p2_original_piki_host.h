#pragma once
#include "pc_p2_original_piki_native_facts.h"
#include "pc_p2_original_piki_animator.h"
#include <string>
class MapMgr;
namespace p2retail {class SceneContext;}
namespace p2original {namespace piki {
// Strong read-only factory query used by actual Piki/ViewPiki dispatch. A
// canonical source-labelled body without this genuine factory is Unavailable,
// never Unowned/P1 fallback. Partial includes native allocations before their
// GenPiki association and every failed/pending source initialization.
enum class NativeHostPhase {Unowned,Partial,Committed,Unavailable};
struct NativeHostRead {
 Handle handle;
 const captain::LoadedScene* scene=nullptr;
 const p2retail::SceneContext* stage=nullptr;
 NativeAnimator* animator=nullptr;
 const PhysicalSource* physical=nullptr;
 MapMgr* map=nullptr;
};
// The singular factory verifies actual pool allocation incarnation, selected
// Stage/map/thread/scene identity and committed native lifetime BEFORE lending
// these pointers. They remain owned through checked runtime/animator/effects/
// physical retirement; they must not be cached across calls or heap reuse.
// Committed is body ownership, not GameWorldActive: dispatch still observes
// genuine canonical World phase and physical facts before actions/animation.
// For Unowned/Partial/Unavailable, output is unchanged. No accepting weak
// definition, external setter, source-ready boolean or duplicate body map.
NativeHostPhase nativeHost(Piki*,NativeHostRead&,std::string&);
} }
