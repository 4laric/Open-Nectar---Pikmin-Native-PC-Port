#pragma once
#include "pc_p2_retail_scene_input.h"

namespace p2retail {
struct SourceRoomUnit {
 std::string name,archiveMember,archiveSha256,gridBytes,mapcodeBytes;
 std::vector<std::array<std::uint32_t,3>> vertexBits;
 std::vector<std::array<unsigned,3>> triangles;
 std::vector<unsigned char> mapcodes;
 std::array<std::uint32_t,6> sourceBounds{},vertexBounds{};
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
