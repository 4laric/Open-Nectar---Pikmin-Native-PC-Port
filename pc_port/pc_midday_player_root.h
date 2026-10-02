#pragma once
#include "pc_midday_player_core.h"
#include "pc_midday_demo.h"
#include "pc_midday_result.h"
#include <memory>
namespace pc_midday {
struct PlayerRootStageTag {};
// Pure native-boundary validation. Generic codec raw bytes remain unchanged;
// noncanonical bool storage is refused instead of normalized during native bind.
bool validatePlayerRootCore(const PlayerCoreFields&,const PlayerCoreTopology&,std::string&);
class IsolatedPlayerRoot {
 struct Impl;std::unique_ptr<Impl> impl_;
public:
 IsolatedPlayerRoot();~IsolatedPlayerRoot();
 bool prepare(const PlayerState& compiledContent,const Bytes& core,const Bytes& demo,const Bytes& result,
              const PlayerCoreTopology&,const std::map<uint64_t,Creature*>& stagedActors,
              const RestoreGate&,ConstructorFence&,std::string&);
 // Borrowed partial root. UfoParts/Olimar/light resources MUST still be staged
 // and bound before whole-world verification/publication. No publication API.
 PlayerState* root()const;
 bool heldBy(const ConstructorFence&)const;
};
}
