#pragma once
#include "pc_midday_actor_archive.h"
class ViewPiki;class RouteMgr;class CollInfo;
namespace pc_midday {
// Disposable stage only, immediately after new ViewPiki(validatedProps).
// constructorRoute is the actual staged routeMgr used during construction;
// caller must keep it unchanged throughout construction/preparation/binding.
// canonicalCollision is scene-owned new CollInfo(4), still count-zero. This
// bridge observes its native descriptor, not allocator extents or ownership.
// No allocation/animation init/FSM transition/global registration is performed.
// Full base/subtype/collider bind and scene closure must follow before publish.
// In particular the restored collider shape must equal the selected
// PikiShapeObject::mShape; its separate payload is not consumed by this bridge.
// The stage allocation ledger retains ownership of all constructor allocations;
// deleting ViewPiki alone does not free that graph. Shared resources are borrowed.
bool prepare_view_piki(ViewPiki&,const ActorBytes& base,const ActorBytes& subtype,
 LogicalResolver&,RouteMgr* constructorRoute,CollInfo& canonicalCollision,std::string&);
}
