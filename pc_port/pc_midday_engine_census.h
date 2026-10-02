#pragma once
#include "pc_midday_capture.h"
class ObjectMgr;
struct WorldClock;
namespace pc_midday {
// One bounded manager traversal; caller supplies every manager/dynamic inventory.
// All getters are reviewed iteration queries. No birth/save/FSM/update calls.
bool observeManager(ObjectMgr*, Family, std::vector<Observation>&, std::string&);
AdapterOutput observeClock(const WorldClock&);
// These are deliberately incomplete previews, not completed save adapters.
AdapterOutput rejectUnimplementedEngineState(const Observation&);
std::vector<Globals> observeRequiredGlobals(const WorldClock&, const BirthLedger&);
}
