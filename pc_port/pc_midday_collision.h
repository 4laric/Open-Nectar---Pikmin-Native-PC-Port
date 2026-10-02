#pragma once
#include "pc_midday_actor_archive.h"
class CollInfo; class CollPart;
namespace pc_midday {
// Factory stage only: caller owns stable typed storage and registers each part
// before resolving any actor references. No geometry/AI callbacks are invoked.
bool collision_bind_storage(CollInfo&,CollPart*,u32*,u16);
bool collision_fields(CollInfo&,ActorArchive&);
bool collision_schema(const ActorFields&,const std::string& prefix,std::vector<FieldSchema>&,std::string&);
}
