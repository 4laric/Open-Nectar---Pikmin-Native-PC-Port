#include "pc_p2_retail_scene.h"
#include "pc_p2_retail_start.h"
#include "pc_p2_retail_geometry.h"
#include "pc_randomizer.h"
#include "pc_p2_original_pod.h"
#include "pc_p2_retail_cave_native.h"
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
class SceneRuntime final:public FloorIdentityAuthority {
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
 std::vector<BirthIdentity> issuedBirths;
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
 bool releaseMap(std::string& error){
  // Physical transaction release will return to Prepared only after every
  // actual consumer retires. A committed/installing owner cannot be discarded.
  if(context.mPhase!=ScenePhase::Prepared||pc_p2_original_pod_owned()||
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
