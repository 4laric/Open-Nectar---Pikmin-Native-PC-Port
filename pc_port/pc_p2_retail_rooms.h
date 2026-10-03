#pragma once
#include "pc_p2_retail_scene_input.h"
#include <map>

namespace p2retail {
struct SourceUnitGrid {
 unsigned maxX=0,maxZ=0;
 std::array<std::uint32_t,2> serializedScaleBits{};
 // Source z+x*maxZ storage; exact serialized list order, duplicates retained.
 std::vector<std::vector<unsigned>> cells;
};
struct SourceRoomUnit {
 std::string name,archiveMember,archiveSha256,gridBytes,mapcodeBytes;
 std::vector<std::array<std::uint32_t,3>> vertexBits;
 std::vector<std::array<unsigned,3>> triangles;
 // Original unit TriangleTable::readObject serializes four planes after ABC.
 // Local getCurrTri/height/insideXZ use these bits, never rebuilt combined planes.
 std::vector<std::array<std::uint32_t,16>> sourcePlaneBits;
 std::vector<unsigned char> mapcodes;
 std::array<std::uint32_t,6> sourceBounds{},vertexBounds{};
 SourceUnitGrid sourceGrid;
};
struct SourceRoomInstance {
 unsigned iteration=0,roomIndex=0,unit=0,quarterTurn=0;
 std::array<float,3> translation{};
 float centreX=0,centreZ=0,directionDegrees=0;
};
// Immutable raw source inputs owned by SceneRuntime. Transform arguments are
// adopted development authoring, never a claimed native/source collision matrix.
// Hidden collision, platforms, water and contact authority are not provided.
struct SourceRoomCensus {
 std::string sha256;
 std::vector<SourceRoomUnit> units;
 std::vector<SourceRoomInstance> rooms;
};
bool parseSourceRoomCensus(const SelectedSceneInputs&,SourceRoomCensus&,std::string&);
struct SourceWaterUnit {std::string name,raw;unsigned version=0,count=0;};
// Authenticated original count-zero input. A source SeaMgr/actor cached-water
// lifecycle must still adopt it; this record is never runtime known-dry.
struct SourceWaterInputs {std::string sha256,roomCensusSha256;std::vector<SourceWaterUnit> units;};
bool parseSourceWaterInputs(const SelectedSceneInputs&,const SourceRoomCensus&,SourceWaterInputs&,std::string&);
struct SourceFloorParameters {
 std::string sha256,sourceBytes,roomCensusSha256,waterCensusSha256;
 std::map<std::string,std::string> parameters;
 unsigned definitionIndex=0,firstFloor=0,lastFloor=0,hiddenCollisionValue=0;
 bool hasHiddenCollision=false;
};
bool parseSourceFloorParameters(const SelectedSceneInputs&,const SourceRoomCensus&,const SourceWaterInputs&,SourceFloorParameters&,std::string&);
struct SourceLocalWaypoint {
 std::array<float,4> positionRadius{};
 std::array<int,8> fromLinks{{-1,-1,-1,-1,-1,-1,-1,-1}};
 unsigned fromCount=0;
};
struct SourceRouteUnit {std::string name,raw;std::vector<SourceLocalWaypoint> waypoints;};
struct SourceRoutePoint {
 unsigned createdRoom=0,createdWaypoint=0,fromCount=0;
 float radius=0;
 bool door=false;
 std::array<int,8> fromLinks{{-1,-1,-1,-1,-1,-1,-1,-1}};
 std::vector<unsigned> rooms;
};
// Authenticated construction inputs, NOT positioned live source WayPoints.
// getMinY and makeInvertLinks require the actual source MapMgr query owner.
struct SourceRouteInputs {
 std::string sha256,roomCensusSha256,waterCensusSha256,parametersSha256,poolRaw;
 std::vector<SourceRouteUnit> units;
 std::vector<std::vector<unsigned>> roomIndices;
 std::vector<SourceRoutePoint> points;
};
bool parseSourceRouteInputs(const SelectedSceneInputs&,const SourceRoomCensus&,const SourceWaterInputs&,const SourceFloorParameters&,SourceRouteInputs&,std::string&);
struct SourceRoomMatrix {unsigned roomIndex=0,unit=0;std::array<float,12> matrix{};};
struct SourceRoomTriangle {std::array<unsigned,3> abc{};unsigned roomIndex=0;unsigned char mapcode=0;};
struct SourceRoomGeometry {
 std::string censusSha256;
 std::vector<SourceRoomMatrix> rooms;
 std::vector<std::array<float,3>> vertices;
 std::vector<SourceRoomTriangle> triangles;
 std::array<float,6> expandedVertexBounds{};
};
// Source makeTR/PSMTXMultVec arithmetic, independently qualified portable
// quarter-LUT implementation. Original PPC/libm runtime bit identity remains
// unobserved; no source plane/sphere/grid or physical admission is asserted.
bool adoptSourceRoomGeometry(const SourceRoomCensus&,SourceRoomGeometry&,std::string&);
}
