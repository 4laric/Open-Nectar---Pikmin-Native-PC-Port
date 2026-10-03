#include "pc_p2_retail_scene.h"
#include "pc_p2_retail_start.h"
#include "pc_p2_retail_geometry.h"
#include "pc_randomizer.h"
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

namespace p2retail {namespace {
std::uint64_t nextSerial=1;
struct HeapScope {int previous;HeapScope():previous(gsys->setHeap(SYSHEAP_App)){}~HeapScope(){gsys->setHeap(previous);}};
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
class SceneRuntime final {
 SceneContext context;
 SelectedSceneInputs selected;
 SourceStart start;
 StageInfo stage;
 StageInfo* previousStage=nullptr;
 Shape* shape=nullptr;
 bool installed=false;
public:
 SceneRuntime(SelectedSceneInputs input,SourceStart source):selected(std::move(input)),start(std::move(source)){}
 bool install(MapMgr* map,std::string& error){
  if(!gsys||!map||map!=mapMgr||map->mMapModel||installed||!nextSerial||nextSerial==std::numeric_limits<std::uint64_t>::max()){
   error="retail scene map ownership/order";return false;
  }
  if(!selectedCurrent()){error="retail scene selection changed before map installation";return false;}
  HeapScope heap;ModelStream stream(selected.bytes[1]);shape=new Shape;ShapeScope shapeScope(shape);
  const auto modelRole=sceneInputRole(selected.selection,SceneInput::Geometry);
  shape->mName=StdSystem::stringDup(modelRole.c_str());shape->read(stream);
  // The source-validated MOD embeds exactly the retained selected INI. Its
  // routes were decoded by Shape::read; initIni would reopen an ambient file.
  shape->resolveTextureNames();shape->initialise();shape->optimize();
  if(shape->mVertexCount<=0||shape->mTriCount<=0||shape->mJointCount!=1||!shape->mRouteGroup.mChild){
   error="retail scene actual parsed model lacks collision/routes";return false;
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
  if(!cave||flowCont.mIsVersusMode||!selectedCurrent()){
   error="retail scene actual story/source selection differs";return false;
  }
  auto* routes=new RouteMgr;routes->construct(map);
  if(!routes->getWayPoint('test',0)){error="retail scene actual native route graph missing";return false;}
  previousStage=flowCont.mCurrentStage;
  stage.mStageName=StdSystem::stringDup((selected.plan.cave+" floor "+std::to_string(selected.plan.floor)).c_str());
  stage.mFileName=StdSystem::stringDup(developmentFloorRole);
  stage.mStageID=STAGE_TESTMAP;stage.mStageIndex=STAGE_TESTMAP;
  stage.mChalStageID=CHALSTAGE_NOT;stage.mHasInitialised=FALSE;
  context.mCampaign=selected.campaign;context.mSession=selected.session;context.mRevision=selected.revision;
  context.mPlan=selected.plan;context.mPlanRole=sceneInputRole(selected.selection,SceneInput::Plan);
  context.mGeometryRole=modelRole;context.mRoutesRole=sceneInputRole(selected.selection,SceneInput::Routes);
  context.mGeometryBytes=selected.bytes[1];context.mRoutesBytes=selected.bytes[2];
  SceneIdentity identity{selected.session,"development:"+selected.plan.cave+":floor"+std::to_string(selected.plan.floor)+":native"+std::to_string(nextSerial),selected.plan.layoutSha256,nextSerial++};
  context.mSnapshot={cave->cave,cave->source,cave->sourceSha256,cave->catalogSha256,selected.plan.floor,cave->maxFloor,identity,true,true};
  context.mStage=&stage;context.mMap=map;context.mRoutes=routes;
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
};
namespace {std::unique_ptr<SceneRuntime> runtime;}
const SceneContext* preparedScene()noexcept{return runtime?runtime->prepared():nullptr;}
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
