#pragma once
#include "pc_p2_retail_rooms.h"
#include "pc_p2_retail_scene.h"
#include "pc_p2_original_room_height.h"
#include <memory>
namespace p2retail {
class SceneContext;
// Arithmetic adoption storage only. The actual SceneRuntime supplies the
// current installed-map lifetime owner before any source query is allowed.
struct SourceHeightInputs {
 SourceHeightInputs()=default;
 SourceHeightInputs(const SourceHeightInputs&)=delete;
 SourceHeightInputs& operator=(const SourceHeightInputs&)=delete;
 std::vector<p2originalnumber::roomHeight::UnitGrid> units;
 std::vector<p2originalnumber::roomHeight::Room> rooms;
 std::vector<p2originalnumber::roomHeight::Bounds> transformedBounds;
};
bool adoptSourceHeightInputs(const SourceRoomCensus&,const SourceRoomGeometry&,
                            std::unique_ptr<SourceHeightInputs>&,std::string&);
struct SourceHeightResult {
 const SceneContext* owner=nullptr;
 const SourceRoomCensus* inputs=nullptr;
 std::uint64_t nativeSerial=0,selectionRevision=0;
 std::array<float,3> position{},normal{{0,1,0}};
 float minY=0,maxY=0;
 const void* originalTriangle=nullptr;
 unsigned triangleIndex=0;
 int roomIndex=-1;
};
struct SourcePrebirthSupport {
 SourceHeightResult height;
 SourceWaterResult water;
};
}
// Fresh source RoomMapMgr.getCurrTri highest-floor selector. A genuine no-hit
// succeeds with null originalTriangle/heights0; unavailable ownership refuses.
bool pc_p2_retail_scene_source_height(const p2retail::SceneContext&,std::uint64_t capturedNativeSerial,
 std::uint64_t selectionRevision,const std::array<float,3>& position,p2retail::SourceHeightResult&,std::string&);
// Genuine Stage Prepared/Loading before allocation: require an original local
// supporting triangle AND registered source-empty KnownDry. No Body guard is
// needed for an unborn actor. The authored position is never changed/relocated.
bool pc_p2_retail_scene_prebirth_support(const p2retail::SceneContext&,std::uint64_t capturedNativeSerial,
 std::uint64_t selectionRevision,const std::array<float,3>& position,p2retail::SourcePrebirthSupport&,std::string&);
