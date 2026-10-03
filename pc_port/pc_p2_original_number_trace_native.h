#pragma once
#include "pc_p2_original_number_motion.h"
class Pellet;class CollTriInfo;class Shape;struct DynCollShape;
struct PcOriginalNumberContacts {
 CollTriInfo* floor=nullptr;CollTriInfo* wall=nullptr;
 Shape* floorModel=nullptr;DynCollShape* floorPlatform=nullptr;
 p2originalnumber::rigid::Vec3 floorNormal{},wallNormal{};
};
// Borrow the actual selected scene geometry. Source-owner composition must
// authenticate this scene before installing the factory; no geometry is minted.
// Map advances the source sphere (<=16 substeps); platform pass resolves at the
// resulting position WITHOUT advancing again. Contact callbacks precede response.
bool pc_p2_original_number_trace_map(Pellet*,p2originalnumber::rigid::Trace&,float,
 p2originalnumber::rigid::ContactReceiver*,PcOriginalNumberContacts&,std::string&);
bool pc_p2_original_number_trace_platforms(Pellet*,p2originalnumber::rigid::Trace&,
 p2originalnumber::rigid::ContactReceiver*,PcOriginalNumberContacts&,std::string&);
