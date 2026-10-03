#pragma once
#include "pc_p2_original_number_motion.h"
#include <vector>
#include <string>
namespace p2originalnumber { namespace motion {
struct CaveGrid {
 Vec3 minimum,maximum;unsigned countX=0,countZ=0;float scaleX=0,scaleZ=0;
 std::vector<std::vector<unsigned>> cells; // source [z+x*countZ], original repeats
};
// Literal normal-retail RoomMapMgr::createGlobalCollision + GridDivider::create.
// Inputs MUST be actual combined source collision vertices/triangles in actual
// mRoomMgr order. This pure recipe does not authenticate a scene or make them.
// Refuses the source's undefined zero-cell dimensions instead of inventing one.
bool buildCaveGrid(const std::vector<Vec3>& vertices,const std::vector<std::array<unsigned,3>>& triangles,CaveGrid&,std::string&);
// findTriLists order, truncation/clamping and per-cell repeats. Output unchanged
// on malformed input; querying outside the bbox still selects its edge cell.
bool caveCandidates(const CaveGrid&,Vec3 center,float radius,std::vector<unsigned>&,std::string&);
}}
