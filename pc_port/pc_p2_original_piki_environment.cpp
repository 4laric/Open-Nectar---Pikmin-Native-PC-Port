#include "pc_p2_original_piki_factory.h"
#include "pc_randomizer.h"
#include <exception>
namespace p2original {namespace piki {
namespace {bool fail(std::string& e,const char* message){e=message;return false;}}
class NativeBodyFactory::SystemOperation {
 const NativeBodyFactory& owner;bool entered=false;
public:
 explicit SystemOperation(const NativeBodyFactory& value):owner(value){
  if(owner.systemBusy)owner.systemReentered=true;
  else {owner.systemBusy=true;owner.systemReentered=false;entered=true;}
 }
 ~SystemOperation(){if(entered)owner.systemBusy=false;}
 bool ok()const noexcept{return entered&&!owner.systemReentered;}
};
bool NativeBodyFactory::sourceSectionOwned()const noexcept{return systemStage!=nullptr;}
bool NativeBodyFactory::exactSystem(bool cleanup,std::string& e)const{
 // Compare the borrowed pointer before any dereference. The actual Scene owner
 // must preserve this Stage through successful source-section retirement.
 if(!systemStage||pc_p2_retail_scene_prepared()!=systemStage||!systemStage->ownsCurrentThread()
  ||systemReentered)return fail(e,"source environment lost actual Stage/thread");
 const auto phase=systemStage->phase();
 if((phase!=p2retail::ScenePhase::Prepared&&phase!=p2retail::ScenePhase::Installing
   &&phase!=p2retail::ScenePhase::Committed&&!(cleanup&&phase==p2retail::ScenePhase::Releasing))
  ||systemStage->stage()!=systemStageInfo||systemStage->map()!=systemMap||systemStage->routes()!=systemRoutes
  ||systemStage->nativeSerial()!=systemSerial||systemStage->selectionRevision()!=systemRevision
  ||systemStage->campaignSha256()!=systemCampaign||systemStage->sessionSha256()!=systemSession
  ||systemStage->plan().layoutSha256!=systemLayout)
  return fail(e,"source environment Stage identity changed");
 const auto& identity=systemStage->snapshot().scene;
 if(identity.seed!=systemSession||identity.visit!=systemVisit||identity.layoutSha256!=systemLayout
  ||identity.serial!=systemSerial)return fail(e,"source environment full scene identity changed");
 // These selected getters are real immutable SDK reads, not caller grants.
 if(!pc_randomizer_original_session()||pc_randomizer_original_campaign()!=systemCampaign
  ||pc_randomizer_session_fingerprint()!=systemSession
  ||pc_randomizer_original_selection_revision()!=systemRevision
  ||pc_p2_retail_scene_prepared()!=systemStage||!systemStage->ownsCurrentThread()
  ||systemStage->phase()!=phase||systemStage->nativeSerial()!=systemSerial
  ||systemStage->selectionRevision()!=systemRevision||systemStage->stage()!=systemStageInfo
  ||systemStage->map()!=systemMap||systemStage->routes()!=systemRoutes
  ||systemStage->campaignSha256()!=systemCampaign||systemStage->sessionSha256()!=systemSession
  ||systemStage->plan().layoutSha256!=systemLayout||systemStage->snapshot().scene.seed!=systemSession
  ||systemStage->snapshot().scene.visit!=systemVisit||systemStage->snapshot().scene.layoutSha256!=systemLayout
  ||systemStage->snapshot().scene.serial!=systemSerial||systemReentered)
  return fail(e,"source environment selection changed during observation");
 return true;
}
bool NativeBodyFactory::prepareSourceSection(const p2retail::SceneContext& ctx,std::string& e){
 // Creating-thread refusal precedes mutable bookkeeping in all source owners.
 if(pc_p2_retail_scene_prepared()!=&ctx||!ctx.ownsCurrentThread()
  ||ctx.phase()!=p2retail::ScenePhase::Prepared)return fail(e,"source environment requires actual Prepared Stage");
 SystemOperation operation(*this);
 if(!operation.ok())return fail(e,"source environment initialization reentered");
 try{
  if(systemStage){
   if(systemStage!=&ctx||!exactSystem(false,e))return fail(e,"source environment retains another Stage");
  }else{
   if(owned()||poolOwned())return fail(e,"source environment must initialize before body allocation");
   SelectedSystemParameters selected;
   if(!readSelectedSystemParameters(ctx,selected,e)||!operation.ok())return false;
   const auto campaign=ctx.campaignSha256(),session=ctx.sessionSha256(),layout=ctx.plan().layoutSha256;
   const auto identity=ctx.snapshot().scene;
   const auto serial=ctx.nativeSerial(),revision=ctx.selectionRevision();
   auto* info=ctx.stage();auto* map=ctx.map();auto* routes=ctx.routes();
   if(pc_p2_retail_scene_prepared()!=&ctx||!ctx.ownsCurrentThread()
    ||ctx.phase()!=p2retail::ScenePhase::Prepared)return fail(e,"source environment changed after SYSTEM read");
   // Every allocating copy precedes the retained pointer and source events.
   auto nextCampaign=campaign,nextSession=session,nextLayout=layout,nextVisit=identity.visit;
   systemCampaign=std::move(nextCampaign);systemSession=std::move(nextSession);
   systemLayout=std::move(nextLayout);systemVisit=std::move(nextVisit);
   systemParameters=selected;systemSerial=serial;systemRevision=revision;
   systemStageInfo=info;systemMap=map;systemRoutes=routes;systemStage=&ctx;
  }
  if(!exactSystem(false,e)||!operation.ok())return false;
  // Preserve each completed event across a later refusal. No retry reinitializes
  // an already initialized GameSystem or loses a bound clock owner.
  if(!clockBound){if(!sourceClock.sourceStageOwned(ctx,e))return false;clockBound=true;}
  if(!exactSystem(false,e)||!operation.ok())return false;
  if(!systemInitialized){if(!sourceSystem.sourceInit(ctx,e))return false;systemInitialized=true;}
  if(!exactSystem(false,e)||!operation.ok())return false;
  if(!clockInitialized){if(!sourceClock.sourceBaseGameSectionInit(e))return false;clockInitialized=true;}
  if(!exactSystem(false,e)||!operation.ok())return false;
  e.clear();return true;
 }catch(const std::exception& x){e=std::string("source environment initialization exception: ")+x.what();return false;}
}
bool NativeBodyFactory::sourceGameSystemState(GameSystemState& out,std::string& e)const{
 if(!systemStage||pc_p2_retail_scene_prepared()!=systemStage||!systemStage->ownsCurrentThread())
  return fail(e,"source GameSystem owner unavailable");
 SystemOperation operation(*this);GameSystemState value;
 if(!operation.ok()||!exactSystem(false,e)||!systemInitialized||!clockInitialized
  ||!sourceSystem.readState(value,e)||!exactSystem(false,e)||!operation.ok())return false;
 out=value;e.clear();return true;
}
bool NativeBodyFactory::sourceDeltaTime(float& out,std::string& e)const{
 if(!systemStage||pc_p2_retail_scene_prepared()!=systemStage||!systemStage->ownsCurrentThread())
  return fail(e,"source System clock owner unavailable");
 SystemOperation operation(*this);float value=0;
 if(!operation.ok()||!exactSystem(false,e)||!systemInitialized||!clockInitialized
  ||!sourceClock.readDeltaTime(value,e)||!exactSystem(false,e)||!operation.ok())return false;
 out=value;e.clear();return true;
}
bool NativeBodyFactory::sourceParameters(SelectedSystemParameters& out,std::string& e)const{
 if(!systemStage||pc_p2_retail_scene_prepared()!=systemStage||!systemStage->ownsCurrentThread())
  return fail(e,"source SYSTEM resource owner unavailable");
 SystemOperation operation(*this);
 if(!operation.ok()||!exactSystem(false,e)||!systemInitialized||!clockInitialized||!operation.ok())return false;
 out=systemParameters;e.clear();return true;
}
bool NativeBodyFactory::retireSourceSection(std::string& e){
 if(!systemStage){e.clear();return true;}
 if(pc_p2_retail_scene_prepared()!=systemStage||!systemStage->ownsCurrentThread())
  return fail(e,"source environment retirement lost Stage/thread");
 SystemOperation operation(*this);
 if(!operation.ok()||!exactSystem(true,e)||owned()||poolOwned())
  return fail(e,"source environment retirement requires independently empty body owners");
 const auto* current=pc_p2_original_captain_world();
 if(current&&current->phase()==captain::Phase::GameWorldActive)return fail(e,"source environment cannot retire active World");
 if(!operation.ok()||!exactSystem(true,e))return false;
 if(systemInitialized){if(!sourceSystem.sourceRelease(e))return false;systemInitialized=false;}
 if(!exactSystem(true,e)||!operation.ok())return false;
 if(clockBound){if(!sourceClock.sourceStageReleased(e))return false;clockBound=false;clockInitialized=false;}
 if(!exactSystem(true,e)||!operation.ok())return false;
 systemStage=nullptr;systemStageInfo=nullptr;systemMap=nullptr;systemRoutes=nullptr;
 systemSerial=0;systemRevision=0;systemCampaign.clear();systemSession.clear();systemLayout.clear();systemVisit.clear();
 systemParameters={};e.clear();return true;
}
}}
