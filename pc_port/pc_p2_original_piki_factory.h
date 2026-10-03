#pragma once
#include "pc_p2_original_piki_physical_bootstrap.h"
#include "pc_p2_original_piki_host.h"
#include "pc_p2_retail_scene.h"
#include "pc_p2_original_game_system.h"
#include "pc_p2_original_system_clock.h"
#include "pc_p2_original_system_selected.h"
#include <array>
class StageInfo;
namespace p2original {namespace piki {
class NativeEffects;class Plate;
// The continuation owner for actual allocatePool results. It is process
// retained before the first host write; a failed initialization retains the
// physical root and every attempted source consumer for checked cleanup.
// The engineered starting twenty preflight authenticates all selected rows and
// genuine source support before the first allocation. It does not publish the
// Stage/World; its actual composer must still observe every resource owner.
class NativeBodyFactory final {
public:
 static NativeBodyFactory& instance();
 NativeBodyFactory(const NativeBodyFactory&)=delete;
 NativeBodyFactory& operator=(const NativeBodyFactory&)=delete;
 bool initializeAllocation(const p2retail::SceneContext&,PoolTicket,std::string&);
 bool createStartingTwenty(const p2retail::SceneContext&,std::string&);
 // Actual owned source section initialization BEFORE Captain reset. Reads both
 // authenticated SYSTEM resources, invokes source GameSystem::init and the
 // literal BaseGameSection System::setFrameRate(2) event. No World activation.
 bool prepareSourceSection(const p2retail::SceneContext&,std::string&);
 bool sourceGameSystemState(GameSystemState&,std::string&)const;
 bool sourceDeltaTime(float&,std::string&)const;
 bool sourceParameters(SelectedSystemParameters&,std::string&)const;
 bool retireSourceSection(std::string&);
 bool sourceSectionOwned()const noexcept;
 struct BodyRead {
  PoolTicket allocation;Handle handle;OriginalPikiBody source;
  bool committed=false;std::uint8_t sourceCreatureFlags=0;
  Vector3f position{0.0f,0.0f,0.0f};
  bool positionKnown=false;
 };
 // All retained roots, including partial/unassociated and inactive records.
 // Exact Stage/thread and actual allocation/lifetime are checked, never
 // inferred from an Active-only roster or a caller census.
 bool inventory(std::array<BodyRead,20>&,std::size_t&,std::string&)const;
 // Read-only constructor callback view, including the associated partial root
 // before Runtime commits. It grants no action or World phase and performs no
 // external producer callback; exact lifetime is rechecked before publication.
 bool bodyRead(Handle,BodyRead&,std::string&)const;
 NativeHostPhase host(Piki*,NativeHostRead&,std::string&)const;
 bool retireBodies(std::string&);
 bool owned()const noexcept;
private:
 NativeBodyFactory()=default;
 GameSystem sourceSystem;
 SystemClock sourceClock;
 SelectedSystemParameters systemParameters;
 const p2retail::SceneContext* systemStage=nullptr;
 StageInfo* systemStageInfo=nullptr;MapMgr* systemMap=nullptr;RouteMgr* systemRoutes=nullptr;
 std::uint64_t systemSerial=0,systemRevision=0;
 std::string systemCampaign,systemSession,systemLayout,systemVisit;
 bool systemInitialized=false,clockBound=false,clockInitialized=false;
 mutable bool systemBusy=false,systemReentered=false;
 class SystemOperation;
 bool exactSystem(bool cleanup,std::string&)const;
 struct Entry {
  PoolTicket ticket;NativePhysicalBootstrap physical;
  OriginalPikiBody source;Handle handle;
  bool reserved=false,association=false,animatorAttempt=false,runtimeAttempt=false,committed=false;
  bool physicalWritten=false;
  std::uint8_t creatureFlags=0;
 };
 std::array<Entry,20> entries;
 const p2retail::SceneContext* stage=nullptr;
 const captain::LoadedScene* scene=nullptr;
 const captain::World* world=nullptr;
 NativeAnimator* animator=nullptr;PhysicalSource* physical=nullptr;
 NativeEffects* effects=nullptr;Plate* plate=nullptr;
 bool consumersRetired=false;
 std::uint64_t serial=0,revision=0,incarnation=0;
 SceneBinding binding;MapMgr* map=nullptr;StageInfo* stageInfo=nullptr;
 std::string campaign,session;
 mutable captain::Phase operationPhase=captain::Phase::Inactive;
 mutable p2retail::ScenePhase operationStagePhase=p2retail::ScenePhase::Prepared;
 mutable bool busy=false,reentered=false;
 class Operation;
 bool exact(std::string&)const;
 bool valid(const Entry&,std::string&)const;
 bool initializeImpl(const p2retail::SceneContext&,PoolTicket,std::string&);
};
}}
