#include "pc_p2_original_piki_factory.h"
#include "pc_p2_original_piki_composer.h"
#include "pc_p2_retail_scene.h"
#include "pc_p2_retail_starting_piki.h"
#include "pc_p2_retail_height.h"
#include "Piki.h"
#include <cmath>
#include <exception>
namespace p2original {namespace piki {
// Declared by the checked composition borrow; no replacement animator owner.
NativeAnimator* nativeAnimator(const captain::LoadedScene&,std::string&);
NativeEffects* nativeEffects(const captain::LoadedScene&,std::string&);
namespace {bool fail(std::string& e,const char* text){e=text;return false;}}
class NativeBodyFactory::Operation {
 NativeBodyFactory& owner;
public:
 bool entered=false;
 explicit Operation(const NativeBodyFactory& value):owner(const_cast<NativeBodyFactory&>(value)){
  if(owner.busy)owner.reentered=true;
  else {owner.busy=true;owner.reentered=false;entered=true;
   const auto* w=pc_p2_original_captain_world();owner.operationPhase=w?w->phase():captain::Phase::Inactive;}
  if(entered&&owner.stage&&pc_p2_retail_scene_prepared()==owner.stage)owner.operationStagePhase=owner.stage->phase();
 }
 ~Operation(){if(entered)owner.busy=false;}
};
NativeBodyFactory& NativeBodyFactory::instance(){static auto* owner=new NativeBodyFactory;return *owner;}
bool NativeBodyFactory::owned()const noexcept{for(const auto& e:entries)if(e.reserved)return true;return stage!=nullptr;}
bool NativeBodyFactory::exact(std::string& e)const {
 if(reentered||!stage||pc_p2_retail_scene_prepared()!=stage
  ||pc_p2_original_captain_loaded_scene()!=scene||pc_p2_original_captain_world()!=world
  ||!stage->ownsCurrentThread()||stage->nativeSerial()!=serial||stage->selectionRevision()!=revision
  ||stage->stage()!=stageInfo||stage->map()!=map||stage->campaignSha256()!=campaign||stage->sessionSha256()!=session
  ||!scene||!world||scene->incarnation()!=incarnation||world->incarnation()!=incarnation
  ||(busy&&(world->phase()!=operationPhase||stage->phase()!=operationStagePhase))||!nativeSceneCurrent(binding,true,e))
  return fail(e,"body factory lost retained Stage/scene/thread");
 e.clear();return true;
}
bool NativeBodyFactory::valid(const Entry& entry,std::string& e)const {
 if(!exact(e)||!entry.reserved||!poolCurrent(entry.ticket))return fail(e,"body factory allocation is stale");
 if(entry.association&&!pc_p2_original_piki_body_current(entry.ticket.body,entry.handle.lifetime))
  return fail(e,"body factory canonical association is stale");
 if(entry.association){
  OriginalPikiBodyHandle current;
  if(!pc_p2_original_piki_body_handle(entry.ticket.body,current)
   ||current.nativeLifetime!=entry.handle.lifetime
   ||current.body.origin.sourceKey!=entry.source.origin.sourceKey
   ||current.body.origin.recordUid!=entry.source.origin.recordUid
   ||current.body.origin.attempt!=entry.source.origin.attempt
   ||current.body.origin.activation!=entry.source.origin.activation
   ||current.body.origin.catalogFingerprint!=entry.source.origin.catalogFingerprint
   ||current.body.state.species!=entry.source.state.species)
   return fail(e,"body factory immutable ancestry changed");
 }
 if(!exact(e)||!poolCurrent(entry.ticket)
  ||(entry.association&&!pc_p2_original_piki_body_current(entry.ticket.body,entry.handle.lifetime)))
  return fail(e,"body factory changed after canonical ancestry observation");
 e.clear();return true;
}
bool NativeBodyFactory::initializeAllocation(const p2retail::SceneContext& ctx,PoolTicket ticket,std::string& e){
 Operation op(*this);
 if(!op.entered)return fail(e,"body factory initialization reentered");
 return initializeImpl(ctx,ticket,e);
}
bool NativeBodyFactory::initializeImpl(const p2retail::SceneContext& ctx,PoolTicket ticket,std::string& e){
 if(pc_p2_retail_scene_prepared()!=&ctx||!ctx.ownsCurrentThread()
  ||ctx.phase()!=p2retail::ScenePhase::Prepared||!poolCurrent(ticket))
  return fail(e,"body factory requires a real prepared allocation");
 auto* loaded=pc_p2_original_captain_loaded_scene();auto* currentWorld=pc_p2_original_captain_world();
 if(!loaded||!currentWorld||currentWorld->phase()!=captain::Phase::Loading)
  return fail(e,"body factory requires canonical Loading ownership");
 if(stage&&(stage!=&ctx||scene!=loaded||world!=currentWorld))return fail(e,"body factory retains another scene");
 operationStagePhase=ctx.phase();
 SceneBinding captured;if(!nativeSceneBinding(captured,false,e)||captured.scene!=loaded)return false;
 const auto capturedSerial=ctx.nativeSerial(),capturedRevision=ctx.selectionRevision(),capturedIncarnation=loaded->incarnation();
 auto* capturedMap=ctx.map();auto* capturedStage=ctx.stage();
 const auto capturedCampaign=ctx.campaignSha256(),capturedSession=ctx.sessionSha256();
 const auto unchanged=[&](){return pc_p2_retail_scene_prepared()==&ctx&&ctx.ownsCurrentThread()
  &&ctx.phase()==p2retail::ScenePhase::Prepared
  &&pc_p2_original_captain_loaded_scene()==loaded&&pc_p2_original_captain_world()==currentWorld
  &&ctx.nativeSerial()==capturedSerial&&ctx.selectionRevision()==capturedRevision
  &&ctx.map()==capturedMap&&ctx.stage()==capturedStage&&ctx.campaignSha256()==capturedCampaign&&ctx.sessionSha256()==capturedSession
  &&loaded->incarnation()==capturedIncarnation&&currentWorld->phase()==captain::Phase::Loading
  &&nativeSceneCurrent(captured,false,e)&&poolCurrent(ticket)&&!reentered;};
 Entry* target=nullptr;
 for(auto& row:entries){
  if(row.reserved&&row.ticket.body==ticket.body){
   if(row.ticket.allocation!=ticket.allocation||row.physical.owned()||row.association
    ||row.animatorAttempt||row.runtimeAttempt||row.committed)
    return fail(e,"body factory already initialized or mismatched this allocation");
   target=&row;break;
  }
  if(!row.reserved&&!target)target=&row;
 }
 if(!target)return fail(e,"body factory retained census is full");
 OriginalPikiBody source;
 if(!poolSource(ticket,source,e)||!unchanged())return false;
 // Real delegates must exist before accepting this allocation continuation.
 auto* anim=nativeAnimator(*loaded,e);if(!anim||!unchanged())return false;
 auto* facts=nativePhysicalSource(*loaded,e);if(!facts||!unchanged())return false;
 auto* fx=nativeEffects(*loaded,e);if(!fx||!unchanged())return false;
 auto* slots=nativePlate(*loaded,e);if(!slots||!unchanged())return false;
 if(pc_p2_retail_scene_prepared()!=&ctx||!ctx.ownsCurrentThread()
  ||pc_p2_original_captain_loaded_scene()!=loaded||pc_p2_original_captain_world()!=currentWorld
  ||currentWorld->phase()!=captain::Phase::Loading||!poolCurrent(ticket)||reentered)
  return fail(e,"body factory preparation callback changed authority");
 if(stage&&(consumersRetired||animator!=anim||physical!=facts||effects!=fx||plate!=slots))return fail(e,"body factory producer changed or cleanup began");
 // Fallible strings before ownership writes; once retained, no exception may
 // discard an allocated root or an attempted animator/runtime registration.
 binding=captured;campaign=capturedCampaign;session=capturedSession;
 target->source=source;target->ticket=ticket;target->reserved=true;
 stage=&ctx;scene=loaded;world=currentWorld;serial=capturedSerial;revision=capturedRevision;
 incarnation=capturedIncarnation;map=capturedMap;stageInfo=capturedStage;
 animator=anim;physical=facts;effects=fx;plate=slots;
 try {
  if(!target->physical.initialize(ctx,ticket,e)||!valid(*target,e))return false;
  target->physicalWritten=true;
  // Literal source Creature::init flags: IsAtari|IsAlive|IsCollisionFlick.
  // These are source-owned fields, never reconstructed from native HP/FSM.
  target->creatureFlags=7;
  if(!pc_p2_original_piki_body_associate_birth(ticket.body,source))return fail(e,"source body association refused; root retained");
  target->association=true; // BEFORE any fallible canonical ancestry copy.
  std::uint64_t actualLifetime=0;
  if(!pc_p2_original_piki_body_lifetime(ticket.body,actualLifetime))return fail(e,"successful source association omitted lifetime");
  target->handle={ticket.body,actualLifetime};
  if(!valid(*target,e)||!poolFinishPhysicalInitialization(ticket,target->physical,e))return false;
  target->animatorAttempt=true;
  if(!animator->attach(target->handle,e)||!valid(*target,e))return false;
  target->runtimeAttempt=true;
  if(!initialize(ticket.body,e)||!valid(*target,e))return false;
  target->committed=true;e.clear();return true;
 }catch(...){return fail(e,"source body initialization threw; partial consumers retained");}
}
bool NativeBodyFactory::createStartingTwenty(const p2retail::SceneContext& ctx,std::string& e){
 Operation op(*this);
 if(!op.entered||owned()||poolOwned()||pc_p2_retail_scene_prepared()!=&ctx||!ctx.ownsCurrentThread()
  ||ctx.phase()!=p2retail::ScenePhase::Prepared)
  return fail(e,"starting roster requires fresh actual prepared ownership");
 SceneBinding captured;if(!nativeSceneBinding(captured,false,e))return false;
 const auto* w=pc_p2_original_captain_world();
 if(!w||w->phase()!=captain::Phase::Loading)return fail(e,"starting roster requires canonical Loading");
 const auto s=ctx.nativeSerial(),r=ctx.selectionRevision();auto* selectedMap=ctx.map();auto* selectedStage=ctx.stage();
 const auto campaignSha=ctx.campaignSha256(),sessionSha=ctx.sessionSha256();
 operationStagePhase=ctx.phase();
 const auto unchanged=[&](){return !reentered&&pc_p2_retail_scene_prepared()==&ctx&&ctx.ownsCurrentThread()
  &&ctx.nativeSerial()==s&&ctx.selectionRevision()==r&&ctx.map()==selectedMap&&ctx.stage()==selectedStage
  &&ctx.campaignSha256()==campaignSha&&ctx.sessionSha256()==sessionSha&&ctx.phase()==p2retail::ScenePhase::Prepared
  &&pc_p2_original_captain_world()==w&&w->phase()==captain::Phase::Loading&&nativeSceneCurrent(captured,false,e);};
 p2retail::StartingPikiInputs inputs;
 if(!p2retail::selectedStartingPiki(ctx,inputs,e)||!unchanged())return false;
 if(inputs.manifest.campaign!=campaignSha||inputs.manifest.rows.size()!=20)
  return fail(e,"selected starting roster is not the exact engineered twenty");
 std::vector<OriginalPikiSource> catalog;catalog.reserve(20);
 std::array<OriginalPikiBody,20> sources;
 // Complete support and source identity preflight BEFORE any native birth.
 for(std::size_t i=0;i<20;++i){const auto& row=inputs.manifest.rows[i];
  if(row.spawn.count!=1||row.spawn.species!=1||row.spawn.wildParameter!=0)
   return fail(e,"unsupported selected starting member");
  p2retail::SourcePrebirthSupport support;
  if(!pc_p2_retail_scene_prebirth_support(ctx,s,r,row.spawn.position,support,e)||!unchanged())return false;
  if(support.height.owner!=&ctx||support.height.nativeSerial!=s||support.height.selectionRevision!=r
   ||!support.height.originalTriangle||support.water.nativeSerial!=s||support.water.selectionRevision!=r
   ||support.water.state!=p2retail::SourceWaterState::KnownDry)
   return fail(e,"starting member lacks exact source dry support");
  catalog.push_back({row.sourceKey,row.spawn.uid,1,1});
  sources[i].origin={row.sourceKey,row.spawn.uid,0,s,inputs.manifest.catalog};sources[i].state={1,false,false};
 }
 // Install only the genuinely selected development catalog. This slice does
 // not merge unrelated floor GenPiki catalogs or impersonate acquisition.
 const auto& existing=pc_p2_original_piki_catalog_fingerprint();
 if(!existing.empty()&&existing!=inputs.manifest.catalog)return fail(e,"another original Piki catalog remains selected");
 if(!pc_p2_original_piki_origin_install(inputs.manifest.catalog,catalog,e)||!unchanged())return false;
 for(const auto& source:sources)if(!pc_p2_original_piki_body_birth_admit(source))return fail(e,"starting member is already consumed");
 // Composition has already constructed all genuine selected models and
 // producers. No new zero-effects/task delegate is installed here.
 if(!nativeServices(*captured.scene,e)||!unchanged())return false;
 auto* anim=nativeAnimator(*captured.scene,e);if(!anim||!unchanged())return false;
 auto* facts=nativePhysicalSource(*captured.scene,e);if(!facts||!unchanged())return false;
 auto* fx=nativeEffects(*captured.scene,e);if(!fx||!unchanged())return false;
 auto* slots=nativePlate(*captured.scene,e);if(!slots||!unchanged())return false;
 // Reserve all fallible source storage and retain the immutable scene before
 // entering the actual allocator. A thrown post-birth callback cannot hide a
 // root from this independent partial census.
 for(std::size_t i=0;i<20;++i)entries[i].source=sources[i];
 binding=captured;campaign=campaignSha;session=sessionSha;
 stage=&ctx;scene=captured.scene;world=w;serial=s;revision=r;incarnation=captured.incarnation;
 map=selectedMap;stageInfo=selectedStage;animator=anim;physical=facts;effects=fx;plate=slots;
 for(std::size_t i=0;i<20;++i){const auto& source=sources[i];auto& retained=entries[i];
  retained.reserved=true;
  PoolAllocation allocated;
  try{allocated=allocatePool(source,e);}
  catch(...){
   // Pool's fallible immutable source copy precedes birth. If no actual Pool
   // owner exists for this row, remove only this empty reservation. Earlier
   // roots remain retained. Any unknown postbirth exception is conservative:
   // retain the empty marker as well as Pool's independent actual census.
   std::array<PoolOwner,20> actual;const auto count=poolOwners(actual);
   bool unknown=false;
   for(std::size_t n=0;n<count;++n){bool known=false;
    for(const auto& row:entries)if(row.reserved&&row.ticket.body==actual[n].ticket.body
     &&row.ticket.allocation==actual[n].ticket.allocation)known=true;
    if(!known)unknown=true;
   }
   if(!unknown)retained.reserved=false;
   return fail(e,"starting allocation threw; earlier/unknown physical owners retained");
  }
  retained.ticket=allocated.ticket;
  if(!allocated.ticket.body)retained.reserved=false;
  if(allocated.result!=PoolAllocationResult::Retained||!allocated.ticket.body)return fail(e,"starting allocation refused or capacity exhausted");
  // Even a post-allocation error retains the real Pool owner. Continue only
  // with a valid incarnation; no pointer-derived repair token is invented.
  if(!e.empty()||!unchanged())return false;
  if(!initializeImpl(ctx,allocated.ticket,e))return false;
 }
 if(!exact(e))return false;e.clear();return true;
}
bool NativeBodyFactory::inventory(std::array<BodyRead,20>& out,std::size_t& count,std::string& e)const {
 Operation op(*this);if(!op.entered||!exact(e))return false;
 std::array<BodyRead,20> result;std::size_t n=0;
 for(const auto& row:entries)if(row.reserved){if(!valid(row,e))return false;
  BodyRead read{row.ticket,row.handle,row.source,row.committed,row.creatureFlags};
  if(row.physicalWritten){read.position=row.ticket.body->mSRT.t;read.positionKnown=true;}
  if(!std::isfinite(read.position.x)||!std::isfinite(read.position.y)||!std::isfinite(read.position.z)||!valid(row,e))return false;
  result[n++]=std::move(read);
 }
 if(!exact(e))return false;
 // A later ancestry observation may retire an earlier row through another
 // owner. Recheck the ENTIRE census using only nonallocating actual lifetime
 // and raw allocation queries after all fallible ancestry reads have finished.
 for(const auto& row:entries)if(row.reserved){
  if(!poolCurrent(row.ticket)||(row.association&&!pc_p2_original_piki_body_current(row.ticket.body,row.handle.lifetime)))
   return fail(e,"body factory inventory changed during another row observation");
 }
 out=std::move(result);count=n;e.clear();return true;
}
bool NativeBodyFactory::bodyRead(Handle handle,BodyRead& out,std::string& e)const {
 if(!exact(e))return false;
 for(const auto& row:entries)if(row.reserved&&row.association
  &&row.handle.body==handle.body&&row.handle.lifetime==handle.lifetime){
  if(!valid(row,e))return false;
  BodyRead result{row.ticket,row.handle,row.source,row.committed,row.creatureFlags};
  if(!row.physicalWritten)return fail(e,"associated source position was not initialized");
  result.position=row.ticket.body->mSRT.t;
  result.positionKnown=true;
  if(!std::isfinite(result.position.x)||!std::isfinite(result.position.y)||!std::isfinite(result.position.z))return fail(e,"nonfinite owned source position");
  if(!valid(row,e))return false;out=std::move(result);e.clear();return true;
 }
 return fail(e,"body factory has no retained canonical lifetime");
}
NativeHostPhase NativeBodyFactory::host(Piki* body,NativeHostRead& out,std::string& e)const {
 // Read-only host dispatch may nest in a constructor callback. It observes
 // Partial until the actual Runtime/animator commit, never grants P1 fallback.
 for(const auto& row:entries)if(row.reserved&&row.ticket.body==body){
  if(!valid(row,e))return NativeHostPhase::Unavailable;
  if(!row.committed)return NativeHostPhase::Partial;
  NativeHostRead result;result.handle=row.handle;result.scene=scene;result.stage=stage;
  result.animator=animator;result.physical=physical;result.map=stage->map();
  if(!valid(row,e))return NativeHostPhase::Unavailable;out=result;e.clear();return NativeHostPhase::Committed;
 }
 PcP2SourceBody labelled;
 if(pc_p2_source_body_query(body,labelled)!=PcP2SourceBodyKind::None){e="labelled body lacks actual factory ownership";return NativeHostPhase::Unavailable;}
 e.clear();return NativeHostPhase::Unowned;
}
bool NativeBodyFactory::retireBodies(std::string& e){
 Operation op(*this);if(!op.entered)return fail(e,"body factory cleanup reentered");
 if(!owned()){e.clear();return true;}
 if(!exact(e)||world->phase()==captain::Phase::GameWorldActive)
  return fail(e,"body factory cleanup requires retained nonactive source scene");
 auto* fx=effects;if(!fx||!plate||!animator)return fail(e,"body factory lost retained consumer owners");
 if(!consumersRetired){
 for(const auto& row:entries)if(row.reserved&&!valid(row,e))return false;
 // Actual held-Captain references must already be released by its owner.
 // Runtime inspects its complete independent slot/effect census before writes.
 if(!canRetireScene(e)||!exact(e))return false;
 if(!retireScene(e)||!exact(e))return false;
 Ownership runtimeOwners;if(!readOwnership(runtimeOwners,e)||!exact(e))return false;
 if(runtimeOwners.entries||runtimeOwners.inFlightOwnerOperations
  ||plate->retainedSlots()||plate->retainedListeners())
  return fail(e,"body factory cleanup still has runtime/Plate consumers");
 if(!fx->canRetire(e)||!exact(e)||!fx->retire(e)||!exact(e))return false;
 NativeEffects::Ownership effectOwners;
 if(!fx->ownership(effectOwners,e)||!exact(e))return false;
 if(effectOwners.freeContexts||effectOwners.throwContexts||effectOwners.voiceObjects||effectOwners.inFlightOperations)
  return fail(e,"body factory cleanup retains independent native effect owners");
 // Animator's real whole-owner barrier covers failed attaches as well as
 // committed consumers. No absence of an Active roster attests this cleanup.
 if(!animator->canRetire(e)||!exact(e)||!animator->retire(e)||!exact(e))return false;
 if(animator->retainedBodies()||!plate->reset(e)||!exact(e))return false;
 consumersRetired=true;
 }
 for(auto& row:entries)if(row.reserved){
  if(!exact(e))return false;
  if(row.association&&!pc_p2_original_piki_body_current(row.ticket.body,row.handle.lifetime))return fail(e,"body retirement association changed");
  row.committed=false;row.animatorAttempt=row.runtimeAttempt=false;
  // Only after all genuine source consumer owners report empty.
  if(row.association){pc_p2_original_piki_origin_forget(row.ticket.body);row.association=false;row.handle={};}
  if(!exact(e))return false;
  if(row.physical.owned()){
   if(!poolReleasePhysical(row.ticket,row.physical,e)||!exact(e))return false;
  }else if(!poolReleaseAllocation(row.ticket,e)||!exact(e))return false;
  row.ticket={};row.source={};row.creatureFlags=0;row.reserved=false;row.physicalWritten=false;
 }
 stage=nullptr;scene=nullptr;world=nullptr;animator=nullptr;physical=nullptr;effects=nullptr;plate=nullptr;consumersRetired=false;
 serial=revision=incarnation=0;binding={};map=nullptr;stageInfo=nullptr;campaign.clear();session.clear();e.clear();return true;
}
NativeHostPhase nativeHost(Piki* body,NativeHostRead& out,std::string& e){
 return NativeBodyFactory::instance().host(body,out,e);
}
}}
