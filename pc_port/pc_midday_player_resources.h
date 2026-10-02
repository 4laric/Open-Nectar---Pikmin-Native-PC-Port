#pragma once
#include "pc_midday_actor_archive.h"
#include "pc_midday_player_core.h"
#include <array>
namespace pc_midday {
// One resource record for PlayerState's course-owned roots. Native allocation
// membership, content provenance and listener ownership remain resolver duties.
// UfoParts elements and light effects need separate canonical resource payloads.
bool player_resources_schema(const ActorFields&,std::vector<FieldSchema>&,std::string&);
bool validatePlayerResources(const ActorBytes&,const LogicalResolver&,std::string&);
bool capturePlayerResources(PlayerState&,LogicalResolver&,double,const PlayerCoreReadFence&,ActorBytes&,std::string&);
// Read-only native traversal. Never Apply this visitor on a live or staged world:
// the owned PlayerState/resource factory must provide a separate reviewed bind.
bool player_resources_fields(PlayerState&,const std::array<bool,30>&,bool,ActorArchive&);
}
// bbftReplayedParts has TU-local storage; export exact values without callbacks.
struct PcMiddayPlayerReplayAccess {
 static void read(std::array<bool,30>&);
};
