#pragma once
#include <string>
class Navi;
class Piki;
namespace pc_midday {
class AllocationOwner;
struct NaviAncillaryConfig {
    int collisionCapacity;
    int plateCapacity;
    int controllerPort;
    float plateStartOffset;
    float plateLengthLimit;
    float plateMaxPosSize;
};
struct PikiAncillaryConfig { int collisionCapacity; int pathCapacity; };
// Allocation only. Caller supplies validated scene/profile capacities and plate
// parameters; no global settings, route manager, files or co-op inference.
// Requires fresh null ancillary roots. Successful pointers borrow from owner;
// destroy/discard the complete staging transaction on failure (no retry).
// Does NOT bind saved controller state, collision resources, particle graphs,
// animator resources, or publish a reusable actor/pool slot.
bool allocate_navi_ancillary(Navi&, const NaviAncillaryConfig&, AllocationOwner&, std::string&);
bool allocate_piki_ancillary(Piki&, const PikiAncillaryConfig&, AllocationOwner&, std::string&);
}
