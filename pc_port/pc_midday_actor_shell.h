#pragma once
#include "pc_midday_actor_archive.h"
#include <memory>
class ViewPiki;
namespace pc_midday {
class ConstructorFence;
// Factory prerequisite ONLY: exact native root objects with no nested owned
// allocations, no manager lookup/swap and no gameplay initialization. The
// schema/catalog must describe the complete saved actor before construction.
// Output must be empty; failure preserves it. Properties are borrowed canonical
// resources, kept alive by the scene. Destroy roots before releasing the fence.
//
// These shells cannot enter manager pools (including free reusable slots), run
// gameplay, or bind the whole actor family. A separate typed allocation owner
// must prepare EVERY action/state/effect/path/collision/controller subobject
// first, including abort cleanup and failure injection. No such full factory
// is implied by this API. unique_ptr owns the root only.
bool create_navi_shell(const ActorBytes& base, LogicalResolver&, ConstructorFence&,
                       std::unique_ptr<Navi>& output, std::string&);
bool create_view_piki_shell(const ActorBytes& base, const ActorBytes& subtype,
                            LogicalResolver&, ConstructorFence&,
                            std::unique_ptr<ViewPiki>& output, std::string&);
}
