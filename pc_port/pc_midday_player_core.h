#pragma once
#include "pc_midday_codec.h"
#include "pc_midday_restore.h"
#include <array>
class PlayerState;
namespace pc_midday {
struct PlayerGraph {
 uint16_t start=0,end=0;
 std::vector<std::array<int32_t,3>> entries;
};
struct CourseFlagRecord {uint16_t entries=0;Bytes bits;};
// Named fields only. This does NOT cover PlayerState resource roots, demo/result
// machinery, UfoParts animation/materials or shared scene effects.
struct PlayerCoreFields {
 int32_t sprouted=0,lostBattle=0,leftBehind=0,totalPlucked=0;
 int32_t totalRegisteredParts=0,totalParts=0,currentParts=0,requiredParts=0;
 int32_t totalDead=0,totalBorn=0,living=0;
 uint8_t shipUpgrade=0,shipEffect=0,container=0,displayPiki=0,unused186=0;
 bool extinctionPlayed=false,tutorial=false,naviPilot=false,dayEnd=false,challenge=false;
 uint16_t lastUpdated=0;
 std::array<uint8_t,30> collectedByDay{},partsToNext{};
 std::array<uint8_t,5> stageParts{};
 PlayerGraph hour,day;
 std::array<CourseFlagRecord,5> courses;
};
// Supplied by the reviewed scene/content factory, never inferred by reading
// possibly uninitialized course pointers. Zero generator entries means no read.
struct PlayerCoreTopology {
 uint16_t hourStart=0,hourEnd=0,dayStart=0,dayEnd=0;
 std::array<uint16_t,5> courseEntries{};
};
struct PlayerCoreReadFence {
 bool sceneInitialized=false,agreedReadOnlyFence=false;
 uint64_t tickBefore=0,tickAfter=0;
};
bool validatePlayerCoreTopology(const PlayerCoreFields&,const PlayerCoreTopology&,std::string&);
bool encodePlayerCore(const PlayerCoreFields&,Bytes&,std::string&);
bool decodePlayerCore(const Bytes&,PlayerCoreFields&,std::string&);
bool capturePlayerCore(const PlayerState&,const PlayerCoreTopology&,const PlayerCoreReadFence&,Bytes&,std::string&);
bool preparePlayerCore(PlayerCoreFields&,const Bytes&,const PlayerCoreTopology&,const RestoreGate&,std::string&);
}
