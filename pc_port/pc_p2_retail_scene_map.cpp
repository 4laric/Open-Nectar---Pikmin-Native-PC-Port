#include "pc_p2_retail_scene.h"
#include "pc_p2_retail_start.h"
#include "pc_p2_retail_geometry.h"
#include "pc_randomizer.h"
#include "pc_p2_original_pod.h"
#include "pc_p2_retail_cave_native.h"
#include "pc_p2_retail_scene_bodies.h"
#include "pc_p2_retail_exit.h"
#include "pc_p2_original_pod_floor.h"
#include "pc_p2_retail_treasure_cargo.h"
#include "pc_p2_retail_treasure_policy.h"
#include "pc_p2_campaign_treasure_config.h"
#include "pc_p2_original_captain_damage.h"
#include "pc_campaign_ui_observer.h"
#include "NaviMgr.h"
#include "gameflow.h"
#include "zen/ogResult.h"
#include "pc_p2_surface_water.h"
#include "pc_p2_surface_topology.h"
#include "OnePlayerSection.h"
#include "FlowController.h"
#include "MapMgr.h"
#include "MapCode.h"
#include "Collision.h"
#include "Route.h"
#include "Shape.h"
#include "Stream.h"
#include "system.h"
#include <memory>
#include <limits>
#include <cstdio>
#include <cstdlib>
#include <array>
#include <chrono>
#include <random>

// Actual common captain owner can retain an unpublished Loading bank before
// the complete source body roster is admitted. It still pins this map context.
bool pc_p2_original_captain_scene_owned(const p2retail::SceneContext&) noexcept;

namespace p2retail {namespace {
std::uint64_t nextSerial=1;
std::uint64_t nextActivation=1;
// A fresh process incarnation prevents native serial/activation counters from
// aliasing a previous run. This nonce identifies a visit; it authenticates no
// SAVE, selected input, actor or World state.
const std::string& processIncarnation(){
 static const std::string nonce=[](){
  std::random_device random;
  const auto clock=std::uint64_t(std::chrono::high_resolution_clock::now().time_since_epoch().count());
  std::seed_seq seed{random(),random(),random(),random(),unsigned(clock),unsigned(clock>>32)};
  std::mt19937_64 generator(seed);std::string value;char part[17];
  for(unsigned i=0;i<4;++i){std::snprintf(part,sizeof(part),"%016llx",(unsigned long long)generator());value+=part;}
  return value;
 }();
 return nonce;
}
// BaseShape has no destructor for its nested arrays/registered textures. These
// two fixed source profiles own a bounded Sys-heap model bank for this process;
// scene retirement releases routes/stage/actors, never a borrowed texture.
struct ModelCache {
 enum Phase {Empty,Loading,Ready,Failed} phase=Empty;
 const void* system=nullptr;
 Shape* shape=nullptr;
 std::string name;
};
std::array<ModelCache,2> modelCache;
struct HeapScope {int previous;explicit HeapScope(int heap=SYSHEAP_App):previous(gsys->setHeap(heap)){}~HeapScope(){gsys->setHeap(previous);}};
struct ShapeScope {
 Shape* previous;immut char* first;immut char* second;
 explicit ShapeScope(Shape* shape):previous(gsys->mCurrentShape),first(gsys->mTextureBase1),second(gsys->mTextureBase2){gsys->mCurrentShape=shape;gsys->setTextureBase("","");}
 ~ShapeScope(){gsys->mCurrentShape=previous;gsys->setTextureBase(first,second);}
};
class ModelStream final:public RamStream {
public:
 explicit ModelStream(std::string& bytes):RamStream(bytes.data(),int(bytes.size())){}
 void read(void* destination,int size)override{
  if(size<0||mPosition<0||size>mLength-mPosition){std::fputs("P2_RETAIL_MAP selected model bounds\n",stderr);std::abort();}
  RamStream::read(destination,size);
 }
};
bool dryGround(MapMgr& map,float x,float z,float ceiling,float& y){
 auto* triangle=map.getStaticGroundBelow(x,z,ceiling,y);
 if(!triangle||!std::isfinite(y)||triangle->mTriangle.mNormal.y<=0)return false;
 const auto attribute=MapCode::getAttribute(triangle);
 return attribute!=ATTR_Water&&attribute!=ATTR_Hole;
}
}
// Actual owner of the selected scene's stage/map/route resources. The separate
// NativeFloor transaction must complete before any committed/activity lookup.
class SceneRuntime final:public FloorIdentityAuthority,public SceneOps {
 SceneContext context;
 SelectedSceneInputs selected;
 SourceStart start;
 StageInfo stage;
 std::string stageName,stageFile;
 StageInfo* previousStage=nullptr;
 Shape* shape=nullptr;
 MapMgr* ownedMap=nullptr;
 RouteMgr* ownedRoutes=nullptr;
 bool installed=false;
 std::vector<BirthIdentity> issuedBirths,retiredBirths;
 std::unique_ptr<NativeFloor> floorOwner;
 FloorSession floorSession;
 p2originalpod::FloorLifecycle pod;
 p2treasure::Catalog treasureCatalog;
 bool bootAttempted=false,exitPrepared=false,cargoPrepared=false,podCommitted=false;
 bool consumersClaimed=false;
 bool fail(std::string& error,const char* message)const{error=message;return false;}
 bool same(const Snapshot& floor)const{return p2originalpod::sameFloor(floor,context.mSnapshot);}
 bool origin(const ContentRow& row,const BirthIdentity& birth,const Snapshot& floor,std::string& error)const{
  const auto* cave=descriptor(context.mSnapshot.cave);const auto* definitionRow=cave?definition(*cave,floor.floor):nullptr;
  BirthIdentity expected;
  if(!same(floor)||!definitionRow||birth.row>=definitionRow->rows.size()||
     &row!=&definitionRow->rows[birth.row]||!expectedBirth(*cave,floor.floor,floor.scene,birth.row,birth.ordinal,expected,error)||!(birth==expected))
   return fail(error,"retail scene literal independent birth differs");
  return true;
 }
 p2originalpod::ContextProvider provider(){return [this](const SceneIdentity& identity,Snapshot& out){
  if(!owns(identity))return false;
  out=context.mSnapshot;return true;
 };}
public:
 SceneRuntime(SelectedSceneInputs input,SourceStart source):selected(std::move(input)),start(std::move(source)){}
 bool install(MapMgr* map,std::string& error){
  if(!gsys||!map||map!=mapMgr||map->mMapModel||installed||!nextSerial||nextSerial==std::numeric_limits<std::uint64_t>::max()){
   error="retail scene map ownership/order";return false;
  }
  if(!selectedCurrent()){error="retail scene selection changed before map installation";return false;}
  ownedMap=map;
  HeapScope heap;
  const auto modelRole=sceneInputRole(selected.selection,SceneInput::Geometry);
  auto& cached=modelCache[selected.selection.floor-1];
  if(cached.phase==ModelCache::Ready){
   if(cached.system!=gsys){error="retail scene model bank belongs to another native system";return false;}
   shape=cached.shape;
  }else{
   if(cached.phase!=ModelCache::Empty){error="retail scene model bank initialization failed or reentered";return false;}
   cached.phase=ModelCache::Loading;cached.system=gsys;
   cached.name="p2-retail-map-"+selected.plan.geometrySha256;
   HeapScope sysHeap(SYSHEAP_Sys);
   cached.shape=new Shape;shape=cached.shape;shape->mName=cached.name.c_str();
   {ShapeScope shapeScope(shape);ModelStream stream(selected.bytes[1]);shape->read(stream);
    // Every source-profile texture is embedded in these admitted MOD bytes.
    // No ambient/path-cache texture-name resolution is needed.
    shape->initialise();shape->optimize();}
   if(shape->mVertexCount<=0||shape->mTriCount<=0||shape->mJointCount!=1||!shape->mRouteGroup.mChild){
    cached.phase=ModelCache::Failed;error="retail scene actual parsed model lacks collision/routes";return false;
   }
   cached.phase=ModelCache::Ready;
  }
  // The source-validated MOD embeds exactly the retained selected INI. Its
  // routes were decoded by Shape::read; ambient light/platform imports are
  // unnecessary for this route-only INI.
  for(auto* group=static_cast<RouteGroup*>(shape->mRouteGroup.mChild);group;group=static_cast<RouteGroup*>(group->mNext)){
   for(auto* point=static_cast<RoutePoint*>(group->mPointListRoot.mChild);point;point=static_cast<RoutePoint*>(point->mNext))point->mIsOpen=1;
  }
  map->initShape(shape,true);
  float ground=0;
  if(!dryGround(*map,float(start.mapStart[0]),float(start.mapStart[2]),float(start.mapStart[1]),ground)){
   error="retail scene original map-start lacks actual dry ground";return false;
  }
  context.mStartBase={float(start.mapStart[0]),ground+float(SourceStart::groundOffset),float(start.mapStart[2])};
  for(unsigned i=0;i<2;++i){float offsetGround=0;
   if(!dryGround(*map,context.mStartBase[0]+float(SourceStart::captainX[i]),context.mStartBase[2]+float(SourceStart::captainZ[i]),context.mStartBase[1],offsetGround)||offsetGround>context.mStartBase[1]){
    error="retail scene original captain offset lacks actual dry footing";return false;
   }
  }
  // Original RoomMapMgr has no CourseInfo/demo matrix: getMapRotation returns
  // zero. The authored Pod angle is unrelated to captain facing.
  context.mMapYaw=0;context.mStartsGrounded=true;
  const auto* cave=descriptor(selected.plan.cave);
  const auto* definitionRow=cave?definition(*cave,selected.plan.floor):nullptr;
  if(!definitionRow||flowCont.mIsVersusMode||!selectedCurrent()){
   error="retail scene actual story/source selection differs";return false;
  }
  ownedRoutes=new RouteMgr;auto* routes=ownedRoutes;context.mRoutes=routes;routes->construct(map);
  if(!routes->getWayPoint('test',0)){error="retail scene actual native route graph missing";return false;}
  previousStage=flowCont.mCurrentStage;
  stageName=selected.plan.cave+" floor "+std::to_string(selected.plan.floor);stageFile=developmentFloorRole;
  stage.mStageName=stageName.c_str();stage.mFileName=stageFile.c_str();
  stage.mStageID=STAGE_TESTMAP;stage.mStageIndex=STAGE_TESTMAP;
  stage.mChalStageID=CHALSTAGE_NOT;stage.mHasInitialised=FALSE;
  context.mCampaign=selected.campaign;context.mSession=selected.session;context.mRevision=selected.revision;
  context.mPlan=selected.plan;context.mPlanRole=sceneInputRole(selected.selection,SceneInput::Plan);
  context.mGeometryRole=modelRole;context.mRoutesRole=sceneInputRole(selected.selection,SceneInput::Routes);
  context.mGeometryBytes=selected.bytes[1];context.mRoutesBytes=selected.bytes[2];
  SceneIdentity identity{selected.session,"development:"+processIncarnation()+":"+selected.plan.cave+":floor"+std::to_string(selected.plan.floor)+":native"+std::to_string(nextSerial),selected.plan.layoutSha256,nextSerial++};
  context.mSnapshot={cave->cave,cave->source,cave->sourceSha256,cave->catalogSha256,selected.plan.floor,cave->maxFloor,identity,true,true};
  context.mStage=&stage;context.mMap=map;context.mRoutes=routes;
  // Reserve a complete independent source census before any native provider
  // sees a birth request. A new process gets a fresh development visit; cold
  // restore requires the separate authenticated SAVE ledger, never this path.
  unsigned birthCount=0;
  for(const auto& row:definitionRow->rows){
   if(row.weight()||row.minimum()>10000-birthCount){error="retail scene source census bounds/weighted selection";return false;}
   birthCount+=row.minimum();
  }
  if(!nextActivation||birthCount>std::numeric_limits<std::uint64_t>::max()-nextActivation){error="retail scene source activation exhaustion";return false;}
  issuedBirths.reserve(birthCount);
  for(unsigned row=0;row<definitionRow->rows.size();++row)for(unsigned ordinal=0;ordinal<definitionRow->rows[row].minimum();++ordinal)
   issuedBirths.push_back({row,ordinal,identity.serial,instanceKey(*cave,selected.plan.floor,definitionRow->rows[row],ordinal),nextActivation++});
  flowCont.mCurrentStage=&stage;routeMgr=routes;installed=true;
  std::printf("P2_RETAIL_MAP_INSTALLED cave=%s floor=%u native_serial=%llu revision=%llu actual_ground=%.6f captain_base_y=%.6f map_yaw=%.6f world_active=0\n",
   cave->cave.c_str(),selected.plan.floor,(unsigned long long)identity.serial,(unsigned long long)selected.revision,ground,context.mStartBase[1],context.mMapYaw);
  error.clear();return true;
 }
 bool selectedCurrent()const{
  return pc_randomizer_original_session()&&pc_randomizer_original_selection_revision()==selected.revision&&
   pc_randomizer_original_campaign()==selected.campaign&&pc_randomizer_session_fingerprint()==selected.session;
 }
 const SceneContext* prepared()const noexcept{
  // Nonallocating incarnation/mode checks: the revision belongs to the real
  // retained selection; campaign/session were captured and checked at install.
  return installed&&pc_randomizer_original_session()&&pc_randomizer_original_selection_revision()==context.mRevision&&
   context.mSnapshot.scene.serial&&flowCont.mCurrentStage==context.mStage&&mapMgr==context.mMap&&
   mapMgr->mMapModel==shape&&routeMgr==context.mRoutes&&!flowCont.mIsVersusMode?&context:nullptr;
 }
 bool expectedBirth(const CaveDescriptor& supplied,unsigned floor,const SceneIdentity& scene,
                    unsigned row,unsigned ordinal,BirthIdentity& out,std::string& error)const override{
  const auto* actual=descriptor(context.mSnapshot.cave);
  if(!prepared()||!actual||!(scene==context.mSnapshot.scene)||floor!=context.mSnapshot.floor||
   supplied.cave!=actual->cave||supplied.source!=actual->source||supplied.sourceSha256!=actual->sourceSha256||
   supplied.catalogSha256!=actual->catalogSha256||supplied.maxFloor!=actual->maxFloor){
   error="retail scene independent census identity";return false;
  }
  for(const auto& birth:issuedBirths)if(birth.row==row&&birth.ordinal==ordinal){out=birth;error.clear();return true;}
  error="retail scene birth absent from reserved source census";return false;
 }
 const FloorIdentityAuthority* births()const noexcept{return prepared()?this:nullptr;}
 bool owns(const SceneIdentity& identity)const noexcept override{return prepared()&&identity==context.mSnapshot.scene;}
 bool mode(const SceneIdentity& identity,bool& story,bool& inCave,std::string& error)const override{
  if(!owns(identity))return fail(error,"retail scene actual story mode unavailable");
  story=context.mSnapshot.story;inCave=context.mSnapshot.inCave;error.clear();return true;
 }
 bool preflight(const FloorPlan& plan,const Snapshot& snapshot,std::string& error)override{
  if(!prepared()||context.mPhase!=ScenePhase::Prepared||!same(snapshot)||plan.authenticatedBytes!=context.mPlan.authenticatedBytes||
     !context.mStartsGrounded||!pc_p2_retail_scene_bodies_admitted(context,error))return fail(error,"retail scene physical preflight ownership");
  consumersClaimed=true;
  std::string catalogBytes;
  if(!pc_randomizer_original_input("p2-treasure-catalog.txt",catalogBytes,error)||catalogBytes.size()>32768||
     p2treasureplacements::hash(catalogBytes)!=p2treasure::RetailDigest||!selectedCurrent())return fail(error,"retail scene selected treasury catalog");
  std::istringstream catalogText(catalogBytes);
  if(!treasureCatalog.read(catalogText)||!p2treasurestate::catalog_valid(treasureCatalog))return fail(error,"retail scene treasury catalog structure");
  if(!pc_p2_retail_exit_preflight(context,error))return false;
  exitPrepared=true;
  p2originalpod::Resources resources;resources.input=pc_randomizer_original_input;
  if(!pod.prepare(context.mPlan,context.mSnapshot,*this,provider(),resources,error))return false;
  p2retailcargo::Config cargo;cargo.floor=context.mSnapshot;cargo.context=provider();cargo.births=this;
  cargo.placement=[this](const SceneIdentity& identity,unsigned row,unsigned ordinal,Vector3f& out,float& yaw,std::string& e){
   Placement actual;if(!floorOwner||!floorOwner->placement(identity,row,ordinal,actual,e))return false;
   out.set(actual.x,actual.y,actual.z);yaw=actual.yawDegrees*0.01745329251994329577f;return true;
  };
  if(!pc_p2_retail_treasure_cargo_preflight(cargo,error))return false;
  cargoPrepared=true;error.clear();return true;
 }
 bool begin(const FloorPlan& plan,const Snapshot& snapshot,std::string& error)override{
  if(!prepared()||context.mPhase!=ScenePhase::Prepared||!same(snapshot)||plan.authenticatedBytes!=context.mPlan.authenticatedBytes||
     !pod.prepared()||!exitPrepared||!cargoPrepared)return fail(error,"retail scene physical begin order");
  context.mPhase=ScenePhase::Installing;
  return pod.birth(error)&&pc_p2_retail_exit_birth(context,error);
 }
 bool prior(const ContentRow& row,const BirthIdentity& birth,const Snapshot& snapshot,LiveBinding&,bool& absent,std::string& error)override{
  if(context.mPhase!=ScenePhase::Installing||!origin(row,birth,snapshot,error))return false;
  for(const auto& terminal:retiredBirths)if(terminal==birth)return fail(error,"retail fresh scene cannot rebirth a retired native instance");
  // Only this newly minted development visit is admitted. Cold SAVE disposition
  // requires its separate authenticated ledger, never a fabricated live receipt.
  absent=false;error.clear();return true;
 }
 bool cargo(const Placement& placement,const ContentRow& row,const BirthIdentity& birth,const Snapshot& snapshot,LiveBinding& out,std::string& error)override{
  if(context.mPhase!=ScenePhase::Installing||!origin(row,birth,snapshot,error)||placement.instance!=birth.instance||row.kind!="loose_treasure")return false;
  const auto* entry=treasureCatalog.find(row.catalogId);
  if(!entry)return fail(error,"retail scene literal treasure catalog entry absent");
  LiveBinding next;next.identity=birth;
  if(p2treasurestate::state.seen(treasureCatalog,entry->id)){
   if(!pc_p2_retail_treasure_cargo_absent(birth,entry->id,error))return false;
   next.state=BindingState::ConsumedTreasure;next.receipt=entry->id;
  }else{
   Pellet* actor=nullptr;if(!pc_p2_retail_treasure_cargo_birth(birth,actor,error)||!actor)return false;
   next.actor=actor;
  }
  out=std::move(next);error.clear();return true;
 }
 bool suppressed(const ContentRow&,const BirthIdentity&,const Snapshot&,LiveBinding&,std::string& error)override{
  return fail(error,"retail source population suppression requires actual source body owner");
 }
 bool absent(const ContentRow& row,const BirthIdentity& birth,const Snapshot& snapshot,const LiveBinding& binding)const override{
  std::string error;
  return origin(row,birth,snapshot,error)&&binding.identity==birth&&!binding.actor&&binding.state==BindingState::ConsumedTreasure&&
   row.kind=="loose_treasure"&&binding.receipt==row.catalogId&&pc_p2_retail_treasure_cargo_absent(birth,binding.receipt,error);
 }
 bool commit(const Snapshot& snapshot,std::string& error)override{
  if(context.mPhase!=ScenePhase::Installing||!same(snapshot)||!owns(snapshot.scene)||!pc_p2_retail_exit_owned(context)||
     !pc_p2_retail_scene_bodies_admitted(context,error))return fail(error,"retail scene commit actual census owner differs");
  // Exit commit remains safely releasable if the receiver refuses its commit.
  if(!pc_p2_retail_exit_commit(context,error)||!pod.commit(snapshot,error))return false;
  podCommitted=true;context.mPhase=ScenePhase::Committed;error.clear();return true;
 }
 bool canRelease(std::string& error)const override{
  if(!prepared())return fail(error,"retail scene cleanup lost actual map owner");
  if(context.mPhase!=ScenePhase::Releasing){
   if(cargoPrepared&&podCommitted&&!pc_p2_retail_treasure_cargo_can_release_collected(error))return false;
   if(!podCommitted&&pc_p2_original_pod_owned()&&!pc_p2_original_pod_can_abort_prepared(context.mSnapshot.scene))return fail(error,"retail scene prepared receiver rollback unavailable");
   if(exitPrepared&&!pc_p2_retail_exit_can_release(context,error))return false;
   if(consumersClaimed&&!pc_p2_retail_scene_bodies_can_retire(context,error))return false;
  }
  error.clear();return true;
 }
 bool release(std::string& error)override{
  if(!canRelease(error))return false;
  context.mPhase=ScenePhase::Releasing;
  if(cargoPrepared){
   auto teardown=[this](std::string& e){return pod.release(e);};
   const bool released=podCommitted?pc_p2_retail_treasure_cargo_release_collected(teardown,error):pc_p2_retail_treasure_cargo_abort_prepared(teardown,error);
   if(!released)return false;
   cargoPrepared=false;podCommitted=false;
  }else if(pod.prepared()){
   if(!pod.release(error))return false;
  }else if(pc_p2_original_pod_owned()){
   if(!pc_p2_original_pod_abort_prepared(context.mSnapshot.scene,error))return false;
  }
  if(exitPrepared){if(!pc_p2_retail_exit_release(context,error))return false;exitPrepared=false;}
  if(consumersClaimed&&!pc_p2_retail_scene_bodies_retired(context)){
   if(!pc_p2_retail_scene_bodies_retire(context,error))return false;
   if(!pc_p2_retail_scene_bodies_retired(context))return fail(error,"retail scene source body retirement incomplete");
  }
  // Keep Releasing until NativeFloor has also retired every enemy/generator.
  error.clear();return true;
 }
 bool retired(const BirthIdentity& birth,const Snapshot& snapshot,std::string& error)override{
  if(!same(snapshot)||context.mPhase!=ScenePhase::Committed||!owns(snapshot.scene))return fail(error,"retail scene native retirement outside actual live floor");
  for(const auto& issued:issuedBirths)if(issued==birth){
   for(const auto& prior:retiredBirths)if(prior==birth){error.clear();return true;}
   retiredBirths.push_back(birth);error.clear();return true;
  }
  return fail(error,"retail scene native retirement missing issued birth");
 }
 bool boot(std::string& error){
  if(bootAttempted||!prepared()||context.mPhase!=ScenePhase::Prepared||!pc_p2_retail_scene_bodies_admitted(context,error))return fail(error,"retail scene boot requires actual fresh source bodies");
  bootAttempted=true;consumersClaimed=true;
  floorOwner=std::make_unique<NativeFloor>(context.mPlan,*this);
  if(!floorSession.activate(context.mSnapshot.cave,context.mSnapshot.floor,context.mSnapshot.scene,true,*this,*floorOwner,error))return false;
  if(!committed())return fail(error,"retail scene whole physical commit unreadable");
  std::printf("P2_RETAIL_FLOOR_COMMITTED cave=%s floor=%u native_serial=%llu source_births=%u world_active=0\n",context.mSnapshot.cave.c_str(),context.mSnapshot.floor,(unsigned long long)context.nativeSerial(),unsigned(issuedBirths.size()));
  error.clear();return true;
 }
 const SceneContext* committed()const noexcept{
  return prepared()&&context.mPhase==ScenePhase::Committed&&floorOwner&&floorOwner->current(context.mSnapshot.scene,context.nativeSerial())?&context:nullptr;
 }
 bool releaseFloor(std::string& error){
  if(!prepared())return fail(error,"retail scene floor release requires retained actual context");
  if(floorOwner){if(!floorSession.unload(error))return false;}
  // Unsupported/preflight-refused floors can own baseline bodies without the
  // NativeFloor ever reaching SceneOps::preflight. Retire those real consumers
  // through their strong owner even when FloorSession already rolled back.
  if((pod.prepared()||exitPrepared||cargoPrepared||(consumersClaimed&&!pc_p2_retail_scene_bodies_retired(context)))&&!release(error))return false;
  if(pc_p2_retail_cave_native_scene_owned(context.mSnapshot.scene)||pc_p2_original_pod_owned()||
     (consumersClaimed&&!pc_p2_retail_scene_bodies_retired(context)))return fail(error,"retail scene physical/body consumers remain owned");
  context.mPhase=ScenePhase::Prepared;error.clear();return true;
 }
 bool releaseMap(std::string& error){
  // Physical transaction release will return to Prepared only after every
  // actual consumer retires. A committed/installing owner cannot be discarded.
  if(context.mPhase!=ScenePhase::Prepared||pc_p2_original_pod_owned()||exitPrepared||
     pc_p2_original_captain_scene_owned(context)||pc_p2_retail_scene_bodies_owned(context)||(consumersClaimed&&!pc_p2_retail_scene_bodies_retired(context))||
     pc_p2_retail_cave_native_scene_owned(context.mSnapshot.scene)){
   error="retail map teardown requires retired physical floor/Pod consumers";return false;
  }
  installed=false;context.mStartsGrounded=false;context.mSnapshot.scene.serial=0;
  if(flowCont.mCurrentStage==&stage)flowCont.mCurrentStage=previousStage;
  if(routeMgr==ownedRoutes)routeMgr=nullptr;
  if(ownedMap&&mapMgr==ownedMap&&ownedMap->mMapModel==shape)ownedMap->mMapModel=nullptr;
  context.mStage=nullptr;context.mMap=nullptr;context.mRoutes=nullptr;
  if(ownedRoutes){ownedRoutes->disposeOwned();delete ownedRoutes;ownedRoutes=nullptr;}
  ownedMap=nullptr;shape=nullptr;issuedBirths.clear();
  pc_p2_surface_water_reset();pc_p2_surface_topology_reset();
  error.clear();return true;
 }
};
namespace {std::unique_ptr<SceneRuntime> runtime;}
const SceneContext* preparedScene()noexcept{return runtime?runtime->prepared():nullptr;}
const FloorIdentityAuthority* sceneBirths()noexcept{return runtime?runtime->births():nullptr;}
const SceneContext* committedScene()noexcept{return runtime?runtime->committed():nullptr;}
bool bootScene(std::string& error){if(!runtime){error="retail scene boot without actual map owner";return false;}return runtime->boot(error);}
bool canReleaseScene(std::string& error){if(!runtime){error.clear();return true;}return runtime->canRelease(error);}
bool releaseScene(std::string& error){if(!runtime){error.clear();return true;}return runtime->releaseFloor(error);}
bool releaseSceneMap(std::string& error){if(!runtime){error.clear();return true;}if(!runtime->releaseMap(error))return false;runtime.reset();return true;}
bool installSceneMap(MapMgr* map,bool& handled,std::string& error){
 if(runtime){error="retail scene previous map owner remains installed";return false;}
 SelectedSceneInputs inputs;bool selected=false;
 if(!pc_p2_retail_scene_selection(inputs,selected,error))return false;
 if(!selected){handled=false;return true;}
 SourceStart start;GeometryFacts geometry;
 if(!parseSourceStart(inputs,start,error)||!parseRetailGeometry(inputs,geometry,error))return false;
 runtime=std::make_unique<SceneRuntime>(std::move(inputs),std::move(start));
 // Keep partial resource ownership on refusal. The caller must terminate or
 // run the owning scene's cleanup; it cannot reuse this map as a surface.
 if(!runtime->install(map,error))return false;
 handled=true;return true;
}
}
const p2retail::SceneContext* pc_p2_retail_scene_prepared()noexcept{return p2retail::preparedScene();}
bool pc_p2_retail_scene_install_map(MapMgr* map,bool& handled,std::string& error){return p2retail::installSceneMap(map,handled,error);}
const p2retail::FloorIdentityAuthority* pc_p2_retail_scene_births()noexcept{return p2retail::sceneBirths();}
bool pc_p2_retail_scene_release_map(std::string& error){return p2retail::releaseSceneMap(error);}

const p2retail::SceneContext* pc_p2_retail_scene_committed()noexcept{return p2retail::committedScene();}
bool pc_p2_retail_scene_boot(std::string& error){return p2retail::bootScene(error);}
bool pc_p2_retail_scene_can_release(std::string& error){return p2retail::canReleaseScene(error);}
bool pc_p2_retail_scene_release(std::string& error){return p2retail::releaseScene(error);}
bool pc_p2_retail_scene_current_activity(const p2retail::SceneIdentity& identity,std::uint64_t serial,std::uint64_t revision)noexcept{
 const auto* scene=pc_p2_retail_scene_committed();
 const auto* world=pc_p2_original_captain_world();const auto* loaded=pc_p2_original_captain_loaded_scene();
 if(!scene||!(scene->snapshot().scene==identity)||scene->nativeSerial()!=serial||scene->selectionRevision()!=revision||
    !pc_p2_retail_scene_bodies_owned(*scene)||!world||!loaded||!naviMgr||naviMgr->getNaviCount()!=2||
    world->phase()!=p2original::captain::Phase::GameWorldActive||world->incarnation()!=serial||loaded->incarnation()!=serial||
    world->selectedCampaign()!=scene->campaignSha256()||loaded->selectedCampaign()!=scene->campaignSha256()||
    world->selectedFingerprint()!=scene->sessionSha256()||loaded->selectedFingerprint()!=scene->sessionSha256()||
    world->sourceCatalog()!=identity.layoutSha256||loaded->sourceCatalog()!=identity.layoutSha256||
    (world->demo()!=p2original::captain::Demo::Absent&&world->demo()!=p2original::captain::Demo::Inactive))return false;
 for(unsigned i=0;i<2;++i)if(!world->captainAt(i)||world->captainAt(i)!=loaded->captainAt(i)||world->captainAt(i)!=naviMgr->getNavi(int(i)))return false;
 if(world->captainAt(0)==world->captainAt(1)||gameflow.mPauseAll||gameflow.mIsDayEndActive||gameflow.mIsDayEndTriggered||flowCont.mIsDayEndSeqStarted)return false;
 const auto pause=pc_pause_observe();const auto ui=pc_save_ui_observe();
 return !pause.available&&ui.available&&ui.resultState==zen::ogScrResultMgr::Status_NULL;
}
bool pc_p2_retail_scene_game_active()noexcept{
 const auto* scene=pc_p2_retail_scene_committed();
 return scene&&pc_p2_retail_scene_current_activity(scene->snapshot().scene,scene->nativeSerial(),scene->selectionRevision());
}
