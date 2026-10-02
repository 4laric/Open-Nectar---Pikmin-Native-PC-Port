#pragma once
#include "pc_midday_projectile.h"
namespace pc_midday {
// Scene-global records: exactly one per checkpoint, never per firing actor.
bool kabuto_projectile_fields(ActorArchive&);
bool kabuto_projectile_schema(const ActorFields&,std::vector<FieldSchema>&,std::string&);
bool longlegs_projectile_fields(ActorArchive&);
bool longlegs_projectile_schema(const ActorFields&,std::vector<FieldSchema>&,std::string&);
}
