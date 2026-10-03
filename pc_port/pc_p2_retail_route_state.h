#pragma once
#include "pc_p2_retail_rooms.h"
#include "pc_p2_original_room_height.h"
#include <memory>
class Navi;
namespace p2retail {
class SceneContext;
struct SourceWayPoint {
 std::array<float,3> position{};
 float radius=0;
 unsigned char flags=0;
 unsigned fromCount=0,toCount=0;
 std::array<int,8> from{{-1,-1,-1,-1,-1,-1,-1,-1}},to{{-1,-1,-1,-1,-1,-1,-1,-1}};
 std::vector<unsigned> rooms;
};
struct SourceRouteState {
 std::vector<SourceWayPoint> points;
 std::vector<bool> visited;
};
class SourceRouteMap:public p2originalnumber::roomHeight::HeightProvider {
public:
 virtual bool current(std::string&)=0;
};
// Pure construction behavior: first-created positions/radii, source ground,
// makeInvertLinks order and startup setCloseAll. Actual Scene owns the provider
// and storage; this function alone never admits a runtime graph/room action.
bool adoptSourceRouteState(const SourceRoomCensus&,const SourceRoomGeometry&,
 const SourceRouteInputs&,SourceRouteMap&,std::unique_ptr<SourceRouteState>&,std::string&);
// Pure bit/state behavior only; the Scene writer must authenticate the actual
// Root trace-room phase before this helper. No caller boolean grants a visit.
bool openSourceRoom(SourceRouteState&,unsigned roomIndex,std::string&);
}
// Current Scene-owned grounded source graph, distinct from routes.ini/P1.
// Read-only view; no native body phase, visited mutation or SAVE grant.
const p2retail::SourceRouteState* pc_p2_retail_scene_source_routes(const p2retail::SceneContext&,
 std::uint64_t capturedNativeSerial,std::uint64_t selectionRevision) noexcept;
// Strong Root-owned private NativeTrace room callback phase, never a default
// accepting stub. Genuine actor/trace/room/FSM/scene lifetime must be current.
bool pc_p2_original_captain_room_visit_current(const p2retail::SceneContext&,std::uint64_t,
 std::uint64_t,const Navi*,int roomIndex,std::string&);
bool pc_p2_retail_scene_visit_room(const p2retail::SceneContext&,std::uint64_t capturedNativeSerial,
 std::uint64_t selectionRevision,Navi*,int roomIndex,std::string&);
