#pragma once
#include "pc_p2_retail_cave_plan.h"
#include <cstdint>
#include <string>

class StageInfo;
class MapMgr;
class RouteMgr;

namespace p2retail {
enum class ScenePhase { Prepared, Installing, Committed, Releasing };
class SceneRuntime;
struct SourceRoomGeometry;
struct SourceWaterInputs;
enum class SourceWaterState { Unavailable,KnownDry };
struct SourceWaterResult {
 SourceWaterState state=SourceWaterState::Unavailable;
 const SourceRoomGeometry* geometry=nullptr;
 const SourceWaterInputs* inputs=nullptr;
 std::uint64_t nativeSerial=0,selectionRevision=0;
 unsigned registeredRooms=0;
};

// Borrowed from the actual selected-floor owner. Callers cannot construct or
// replace a context. Prepared facts admit resource checks, never live gameplay.
// Keep this object alive through partial release; revoke the live getter before
// any native manager, stage heap, actor address or scene serial can be reused.
class SceneContext final {
public:
    SceneContext(const SceneContext&)=delete;
    SceneContext& operator=(const SceneContext&)=delete;
    StageInfo* stage() const noexcept { return mStage; }
    MapMgr* map() const noexcept { return mMap; }
    RouteMgr* routes() const noexcept { return mRoutes; }
    ScenePhase phase() const noexcept { return mPhase; }
    const Snapshot& snapshot() const noexcept { return mSnapshot; }
    const FloorPlan& plan() const noexcept { return mPlan; }
    const std::string& campaignSha256() const noexcept { return mCampaign; }
    const std::string& sessionSha256() const noexcept { return mSession; }
    std::uint64_t selectionRevision() const noexcept { return mRevision; }
    const std::string& planRole() const noexcept { return mPlanRole; }
    const std::string& geometryRole() const noexcept { return mGeometryRole; }
    const std::string& routesRole() const noexcept { return mRoutesRole; }
    const std::string& geometryBytes() const noexcept { return mGeometryBytes; }
    const std::string& routesBytes() const noexcept { return mRoutesBytes; }
private:
    friend class SceneRuntime;
    SceneContext()=default;
    StageInfo* mStage=nullptr;
    MapMgr* mMap=nullptr;
    RouteMgr* mRoutes=nullptr;
    ScenePhase mPhase=ScenePhase::Prepared;
    Snapshot mSnapshot;
    FloorPlan mPlan;
    std::string mCampaign,mSession,mPlanRole,mGeometryRole,mRoutesRole;
    std::string mGeometryBytes,mRoutesBytes;
    std::uint64_t mRevision=0;
};
}

// Both lookups recheck the real selected session/revision and owned stage.
// The prepared lookup survives committed-authority revocation during cleanup.
// The live lookup additionally requires the committed actual NativeFloor census.
const p2retail::SceneContext* pc_p2_retail_scene_prepared() noexcept;
const p2retail::SceneContext* pc_p2_retail_scene_committed() noexcept;
// A committed scene alone is not a running World: the source World lifecycle
// owner supplies the actual game/movie/pause boundary before this can be true.
bool pc_p2_retail_scene_game_active() noexcept;
// Typed live consumers must match the actual scene, serial and authenticated
// selection before asking about activity. Installation or reunion alone never
// grants activity; movie, pause, UI and day-end exclusions remain source-owned.
bool pc_p2_retail_scene_current_activity(const p2retail::SceneIdentity&,
    std::uint64_t nativeSerial,std::uint64_t selectionRevision) noexcept;
// Scene930's actual registered count-zero SeaMgr query. Unavailable is an
// error, never an inferred no-map/dry fact. Caller owns its actual body phase.
bool pc_p2_retail_scene_find_water(const p2retail::SceneContext&,std::uint64_t nativeSerial,
    std::uint64_t selectionRevision,const std::array<float,3>& position,
    p2retail::SourceWaterResult&,std::string& error);
