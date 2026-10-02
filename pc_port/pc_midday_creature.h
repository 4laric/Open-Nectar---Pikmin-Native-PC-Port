#pragma once
#include "pc_midday_actor_archive.h"
class Creature;
namespace pc_midday {
// Initialized actor tick boundaries only. Refs bind onto a disposable scene;
// caller reconciles SmartPtr counts and global manager membership afterward.
// Presentation/audio objects and EventTalker roots are scene-owned, not serialized here.
bool creature_fields(Creature&,ActorArchive&);
bool creature_schema(const ActorFields&,std::vector<FieldSchema>&,std::string&);
bool capture_creature(Creature&,LogicalResolver&,double,ActorBytes&,std::string&);
// expectedObjectType must come from the independently validated scene factory,
// never by echoing the untrusted payload discriminator.
bool validate_creature(const ActorBytes&,const LogicalResolver&,int expectedObjectType,std::string&);
bool bind_creature(Creature&,const ActorBytes&,LogicalResolver&,double,std::string&);
}
