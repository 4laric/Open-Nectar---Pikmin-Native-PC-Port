#pragma once
#include "pc_midday_player_resource_stage.h"
#include "pc_midday_player_root.h"
namespace pc_midday {
struct PlayerReplayStage {std::array<bool,30> replay{};bool preload=false;};
// Resource owner and root must be retained together; destroy the native root
// before resource allocations, all while the exact constructor fence is held.
// On false DISCARD the private transaction. No live global/ledger installation.
bool bindPlayerResources(IsolatedPlayerRoot&,IsolatedPlayerResources&,
 const ActorBytes&,const std::array<ActorBytes,2>& permanentEffects,
 LogicalResolver& partsResolver,LogicalResolver& lightResolver,LogicalResolver& glowResolver,
 ConstructorFence&,double clockNow,PlayerReplayStage&,std::string&);
}
