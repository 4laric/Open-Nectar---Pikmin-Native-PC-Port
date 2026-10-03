#pragma once
#include "pc_p2_retail_cave_plan.h"
#include <cstdint>
#include <string>
#include <array>

class StageInfo;
class MapMgr;
class RouteMgr;
namespace p2original {struct InstanceIdentity;}

namespace p2retail {
enum class ScenePhase { Prepared, Installing, Committed, Releasing };
class SceneRuntime;
struct SourceRoomCensus;
struct SourceWaterInputs;
struct SourceRoomGeometry;
struct SourceFloorParameters;
struct SourceRouteInputs;

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
    std::uint64_t nativeSerial() const noexcept { return mSnapshot.scene.serial; }
    bool startsGrounded() const noexcept { return mStartsGrounded; }
    bool ownsCurrentThread() const noexcept;
    const std::array<float,3>& captainStartBase() const noexcept { return mStartBase; }
    float mapYaw() const noexcept { return mMapYaw; }
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
    std::uint64_t mThreadToken=0;
    std::array<float,3> mStartBase{};
    float mMapYaw=0;
    bool mStartsGrounded=false;
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

// Called before ambient stage-file/map loading, on the actual newly constructed
// global MapMgr. Absent optional selection leaves handled=false. A selected
// refusal never falls back to a surface map. Installation is resource ownership,
// not a committed physical floor or running World.
bool pc_p2_retail_scene_install_map(MapMgr*,bool& handled,std::string& error);
// Independently reserved from the actual selected source definition AFTER real
// Stage/map installation, BEFORE native actor allocation. Not a SAVE/card proof.
const p2retail::FloorIdentityAuthority* pc_p2_retail_scene_births() noexcept;
// Borrowed immutable raw room census adopted by this exact current owner.
// Version-1 selection has no census. Releasing/replaced owners refuse. This
// getter grants no matrix, collision, hiddenCollision, Plat or water authority.
const p2retail::SourceRoomCensus* pc_p2_retail_scene_rooms(const p2retail::SceneContext&,
    std::uint64_t nativeSerial,std::uint64_t selectionRevision) noexcept;
// Authenticated raw WaterBox inputs only, never a current water/known-dry query.
const p2retail::SourceWaterInputs* pc_p2_retail_scene_water_inputs(const p2retail::SceneContext&,
    std::uint64_t nativeSerial,std::uint64_t selectionRevision) noexcept;
const p2retail::SourceRoomGeometry* pc_p2_retail_scene_source_geometry(const p2retail::SceneContext&,
    std::uint64_t nativeSerial,std::uint64_t selectionRevision) noexcept;
const p2retail::SourceFloorParameters* pc_p2_retail_scene_floor_parameters(const p2retail::SceneContext&,
    std::uint64_t nativeSerial,std::uint64_t selectionRevision) noexcept;
// Immutable v4 construction inputs only. No positioned live RouteMgr,
// ground/inverse-link result, openRoom callback or room-visited grant.
const p2retail::SourceRouteInputs* pc_p2_retail_scene_source_route_inputs(const p2retail::SceneContext&,
    std::uint64_t nativeSerial,std::uint64_t selectionRevision) noexcept;
namespace p2retail {
enum class SourceWaterState { Unavailable,KnownDry };
struct SourceWaterResult {
 SourceWaterState state=SourceWaterState::Unavailable;
 const SourceRoomGeometry* geometry=nullptr;
 const SourceWaterInputs* inputs=nullptr;
 std::uint64_t nativeSerial=0,selectionRevision=0;
 unsigned registeredRooms=0;
};
}
// Genuine selected count-zero SeaMgr query only after source-unit registration.
// False means unavailable, never known-dry or the source no-map condition.
// Body owner must update its cached water at the actual doAnimation/checkWater
// phase and validate canonical body lifetime; this query grants no phase.
bool pc_p2_retail_scene_find_water(const p2retail::SceneContext&,std::uint64_t nativeSerial,
    std::uint64_t selectionRevision,const std::array<float,3>& position,
    p2retail::SourceWaterResult&,std::string& error);
// Read-only retained actual parent incarnation, including natural retirement.
// Requires this committed selected scene; grants no activity or SAVE authority.
// The full native binding fingerprint remains layoutSha256, while Snapshot
// separately retains immutable definition/catalog hashes. Refusal changes no
// output. No fabricated history or live-parent lookup is used.
bool pc_p2_retail_scene_known_source_birth(const p2original::InstanceIdentity&,
    p2retail::BirthIdentity&,p2retail::Snapshot&,std::string& error);
// Before App heap or map reuse, after actual floor/Pod/body/World teardown.
// Resource-only refusal leaves the owned context intact for cleanup retry.
bool pc_p2_retail_scene_release_map(std::string& error);

// Actual GameCore dispatch: source body ownership must already exist. Floor
// commit precedes LoadedScene/World publication and grants no active gameplay.
bool pc_p2_retail_scene_boot(std::string& error);
// Readiness refuses unfinished cargo/receiver/body graphs before any revoke.
bool pc_p2_retail_scene_can_release(std::string& error);
// Retains the prepared map/stage through partial physical and body retirement.
bool pc_p2_retail_scene_release(std::string& error);
