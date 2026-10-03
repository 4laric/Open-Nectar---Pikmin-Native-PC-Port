#include "pc_p2_original_captain_native_phases.h"
#include "pc_p2_original_captain_body_borrower.h"
#include "pc_p2_original_captain_camera_pose.h"
#include "pc_p2_original_captain_native_trace.h"
#include "pc_p2_retail_rooms.h"
#include "pc_p2_retail_scene.h"
#include "Navi.h"
#include "NaviState.h"
#include <fstream>
#include <iterator>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <cmath>
#include <algorithm>
#include <limits>
class MapMgr {}; // Physical pointer holder only; real query TU is a strong double.
namespace p2original {namespace sourceCamera {
// Friend Rig double exercises the ACTUAL private scope/composition/body TUs.
// This is not the selected camera Rig or a real Stage reset execution claim.
class Rig {public:template<class Callback>static bool resetControl(const p2retail::SceneContext& c,const captain::LoadedScene& s,Callback&& callback,std::string& e){
 auto scope=captain::camera::CameraResetPoseScope::begin(c,s,e);return scope&&callback(*scope);
}};
}}
namespace p2retail {

class SceneRuntime {public:static void revision(SceneContext& c,unsigned value){c.mRevision=value;}static std::unique_ptr<SceneContext> make(MapMgr& map,const std::string& campaign,const std::string& session,unsigned serial){auto c=std::unique_ptr<SceneContext>(new SceneContext);c->mMap=&map;c->mCampaign=campaign;c->mSession=session;c->mRevision=4;c->mSnapshot.scene.serial=serial;return c;}};
}
using namespace p2original::captain;namespace bp=bodyphases;
namespace {
// Strong world/geometry/trace/nativecontrol doubles isolate the ACTUAL backend
// TU. They establish mechanical/callback controls only, not source trace SDK,
// rendering or ordinary gameplay acceptance.
Navi a,b;std::string bytes,error;std::vector<std::string> events;unsigned checks=0;
void check(bool yes,const char* text){++checks;if(!yes)throw std::runtime_error(std::string(text)+": "+error);}
bool near(float x,float y){return std::fabs(x-y)<.0001f;}
struct Scene:LoadedScene {
 std::string c="body-campaign",f="body-session",catalog="body-catalog";unsigned epoch=1;
 const std::string& selectedCampaign()const override{return c;}const std::string& selectedFingerprint()const override{return f;}const std::string& sourceCatalog()const override{return catalog;}
 MoviePlayer* moviePlayer()const override{return nullptr;}std::uint64_t incarnation()const override{return epoch;}Navi* captainAt(unsigned i)const override{return i==0?&a:i==1?&b:nullptr;}
} scene;
unsigned cameraDemoReads=0,cameraDemoExpireAt=0;bool cameraDemoChangesState=false;
struct WorldOwner:World {
 Phase value=Phase::Loading;bool wrong=false;Demo movie=Demo::Inactive;
 const std::string& selectedCampaign()const override{return scene.c;}const std::string& selectedFingerprint()const override{return scene.f;}const std::string& sourceCatalog()const override{return scene.catalog;}
 std::uint64_t incarnation()const override{return scene.epoch;}Phase phase()const override{return value;}Demo demo()const override{if(cameraDemoExpireAt&&++cameraDemoReads==cameraDemoExpireAt){if(cameraDemoChangesState)a.current=b.current;else ++scene.epoch;}return movie;}Navi* captainAt(unsigned i)const override{return wrong?nullptr:scene.captainAt(i);}
} world;
const LoadedScene* sceneProvider=&scene;const World* worldProvider=&world;SourceBank* bankProvider=nullptr;bp::Owner* body=nullptr;
bool lifetime=true,actorAlive=true,fsmTransition=false;bp::Facts observation;int animationCount=0,selectorCount=0,timerCount=0,execCount=0;int floorKey=0,wallKey=0;unsigned slip=0;bool floorOut=false,wallOut=false,platformFloor=false,below=false,expire=false,nested=false,retireDenied=false,flagChild=false;int traceRoom=-1,observedRoom=-1;bool roomRefuses=false,roomInvalidates=false,roomRebirth=false;bp::SourceSceneTrace* wrongTrace=nullptr;float randomDraw=1;bp::Vec3 traceNormal{0,1,0};
struct Typed:NaviState,State {
 StateId id;Typed(StateId value):NaviState(48+int(value)),id(value){}
 const NaviState* nativeState()const override{return this;}StateId sourceStateId()const override{return id;}bool sourceAlive(const Navi&)const override{return true;}bool sourceInvincible()const override{return false;}
 std::optional<std::uint8_t> actorInvincibleFrames(const Navi&)const override{return 0;}bool canEnterSourceDead(const Navi&)const override{return false;}void enterSourceDead(Navi&)override{}void sourceDamageFeedback(Navi&)override{}
 void exec(Navi*)override;
} typed(StateId::Walk),second(StateId::Gather),forged(StateId::Walk);
void Typed::exec(Navi* n){events.push_back("exec");++execCount;if(flagChild&&!bp::setMoveRotation(n,false,error))throw std::runtime_error("literal child flag event refused");if(fsmTransition)n->current=&second;}
struct Provider:bp::Provider {
 const LoadedScene& scene()const override{return ::scene;}
 bool facts(const Navi&,bp::Facts& out,std::string&)const override{out=observation;return true;}
 bool cellLOD(Navi&,float far,float close,std::string&)override{events.push_back("cellLOD");return near(far,.01f)&&near(close,.009f);}
 bool animationKey(Navi&,Animator,Listener,int,std::string&)override{return true;}
 bool water(Navi& n,const bp::Sphere& sphere,std::string&)override{events.push_back("water");bp::Fields fields;if(!body->readFields(&n,fields,error)||!fields.previous||fields.previous->x!=n.mSRT.t.x||sphere.radius!=8.5f)return false;if(expire){retireDenied=!body->canRetire(error);body->forget(&n);}if(nested){std::string ignored;bp::body_update(&n,ignored);}return true;}
 bool geometry(Navi&,bool,std::string&)override{events.push_back("geometry");return true;}bool cursor(Navi&,std::string&)override{events.push_back("cursor");return true;}
 bool event(Navi&,bp::UpdateEvent event,std::string&)override{events.push_back("event"+std::to_string(int(event)));return true;}
 bool menus(Navi&,bool& out,std::string&)override{events.push_back("menus");out=false;return true;}bool plateUpdate(Navi&,std::string&)override{events.push_back("plate");return true;}
 bool bounce(Navi& n,bp::FloorHandle,std::string&)override{events.push_back("bounce");bp::Fields fields;return body->readFields(&n,fields,error)&&!fields.floor.triangle;}
 bool wall(Navi&,bp::Vec3,std::string&)override{events.push_back("wall");return true;}bool random(float& out,std::string&)override{events.push_back("random");out=randomDraw;return true;}
 bool walkEffect(Navi&,bool,std::string&)override{events.push_back("walkEffect");return true;}bool belowMap(const Navi&,bool& out,std::string&)const override{events.push_back("hell");out=below;return true;}
 bool recoverBelowMap(Navi&,std::string&)override{events.push_back("recover");return true;}
 bool managerSimulationEnded(std::string&)override{events.push_back("psm");return true;}
} provider;
struct Trace:bp::SourceSceneTrace {
 const LoadedScene& scene()const override{return ::scene;}
 bool floor(bp::FloorHandle handle,bp::FloorFacts& out,std::string&)const override{if(handle.incarnation!=::scene.epoch)return false;out={slip,8,traceNormal};return true;}
 bool map(Navi&,bp::TraceInfo& out,float rate,std::string&)override{events.push_back("map");out.sphere.center.x+=out.velocity.x*rate;out.sphere.center.y+=out.velocity.y*rate;out.sphere.center.z+=out.velocity.z*rate;out.floor=floorOut?bp::FloorHandle{&floorKey,::scene.epoch}:bp::FloorHandle{};out.floorNormal=traceNormal;out.wall=wallOut?bp::FloorHandle{&wallKey,::scene.epoch}:bp::FloorHandle{};out.wallNormal={1,0,0};out.roomIndex=traceRoom;return true;}
 bool platforms(Navi&,bp::TraceInfo& out,float,std::string&)override{events.push_back("platform");if(platformFloor)out.floor={&floorKey,::scene.epoch};return true;}
 bool constrain(Navi&,bp::Sphere& out,std::string&)override{events.push_back("constrain");out.center.x=99;return true;}
 bool room(Navi& n,int expected,std::string&)override{events.push_back("room");bp::Fields f;if(!body->readFields(&n,f,error))return false;observedRoom=f.roomIndex;
 check(body->roomVisitCurrent(&n,*this,expected,error),"exact Owner room callback guard is active after published index");
 check(!body->roomVisitCurrent(&b,*this,expected,error),"room phase rejects other captain");check(!body->roomVisitCurrent(&n,*this,expected+1,error),"room phase rejects wrong room");
 if(wrongTrace)check(!body->roomVisitCurrent(&n,*wrongTrace,expected,error),"room phase rejects another trace");
 auto* old=n.current;n.current=&second;check(!body->roomVisitCurrent(&n,*this,expected,error),"room phase rejects current FSM mutation");n.current=old;
 if(roomRebirth){auto birth=f.initializationSerial;world.value=Phase::Loading;check(!body->initializeAfterBodyReset(&n,error),"room callback cannot replace actual body birth during retained operation");world.value=Phase::GameWorldActive;check(body->readFields(&n,f,error)&&f.initializationSerial==birth&&!body->roomVisitCurrent(&n,*this,expected,error),"rejected birth mutation preserves fields and revokes room continuation");return false;}
 if(roomInvalidates){body->forget(&n);check(!body->roomVisitCurrent(&n,*this,expected,error),"attempted body retirement invalidates in-flight room generation");return false;}
 return !roomRefuses&&observedRoom==expected;}
} trace;
void frame(){observation={};observation.deltaTime=.01f;observation.gravity=100;observation.mapPresent=true;observation.movieMotion=false;observation.movieActor=false;observation.movieExtra=false;observation.gameFrozen=false;observation.movieActive=false;observation.naviManagerFlag1=false;observation.stuck=false;observation.targetCollision=false;observation.platformsPresent=false;observation.hiddenCollision=false;observation.inWater=false;observation.rushBoots=false;observation.gamePaused=false;observation.frameTimer=1;observation.managerSlotOpen=true;}
}
const LoadedScene* pc_p2_original_captain_loaded_scene(){return sceneProvider;}const World* pc_p2_original_captain_world(){return worldProvider;}SourceBank* pc_p2_original_captain_source_bank(){return bankProvider;}

bool pc_p2_original_captain_actor_lifetime(const Navi*,bool& out){if(!lifetime)return false;out=actorAlive;return true;}
void pc_p2_original_captain_invincibility_update(Navi*){events.push_back("iframe");++timerCount;}
void pc_p2_original_captain_party_timers_update(Navi*){events.push_back("partyTimers");}
namespace p2original {namespace captain {
struct SourceBank::Impl{};SourceBank::SourceBank():m(new Impl){}SourceBank::~SourceBank()=default;
bool SourceBank::sourceBytes(SourceResource,std::string& out,std::string&)const{out=bytes;return true;}bool SourceBank::state(const Navi*,MotionState& out,std::string&)const{out={};return true;}
namespace nativecontrol {
bool advanceAnimation(Navi*,const std::function<bool(Animator,Listener,int)>&,std::string&){events.push_back("clocks");++animationCount;return true;}
bool selectWalkAnimation(Navi*,std::string&){events.push_back("selector");++selectorCount;return true;}
}
}}
namespace {
const p2retail::SceneContext* prepared=nullptr;bool loadingFloorCommitted=false;int queries=0;bool queryAvailable=true,queryWrong=false,queryExpire=false,queryRetire=false,queryRetireDenied=false;float expectedWaterY=0;
// Immutable wide selected geometry exercises the REAL numeric grid and trace;
// these authored triangles are engineering fixtures, not imported source assets.
const p2retail::SourceRoomGeometry geometry=[](){p2retail::SourceRoomGeometry g;g.vertices={{{-1000,0,-1000}},{{-1000,0,1000}},{{1000,0,-1000}},{{1000,0,1000}}};g.triangles={{{0,2,1},0,0x28},{{2,3,1},0,0x28}};return g;}();
p2retail::SourceWaterInputs inputs;
const p2retail::SourceFloorParameters floorParameters{};
const p2retail::SourceFloorParameters hiddenParameters=[](){p2retail::SourceFloorParameters p;p.hasHiddenCollision=true;return p;}();
const p2retail::SourceRoomGeometry* geometryOwner=&geometry;
const p2retail::SourceFloorParameters* parameterOwner=&floorParameters;
unsigned geometryQueries=0,expireGeometryAt=0;unsigned sceneVisitCalls=0,sceneWrites=0,laterWrites=0;bool writerExpires=false;float writerExpectedVelocity=0;

}
const p2retail::SceneContext* pc_p2_retail_scene_prepared()noexcept{return prepared;}
const p2retail::SceneContext* pc_p2_retail_scene_committed()noexcept{return world.value==Phase::GameWorldActive||(world.value==Phase::Loading&&loadingFloorCommitted)?prepared:nullptr;}
const p2retail::SourceRoomGeometry* pc_p2_retail_scene_source_geometry(const p2retail::SceneContext& c,std::uint64_t serial,std::uint64_t revision)noexcept{
 ++geometryQueries;if(&c!=prepared||serial!=scene.epoch||revision!=4)return nullptr;
 if(expireGeometryAt==geometryQueries)++scene.epoch;
 return geometryOwner;
}
const p2retail::SourceFloorParameters* pc_p2_retail_scene_floor_parameters(const p2retail::SceneContext& c,std::uint64_t serial,std::uint64_t revision)noexcept{return &c==prepared&&serial==scene.epoch&&revision==4?parameterOwner:nullptr;}
bool pc_p2_original_captain_room_visit_current(const p2retail::SceneContext&,std::uint64_t,std::uint64_t,const Navi*,int,std::string&);
bool pc_p2_retail_scene_visit_room(const p2retail::SceneContext& c,std::uint64_t serial,std::uint64_t revision,Navi* n,int room,std::string& e){
 ++sceneVisitCalls;
 if(!pc_p2_original_captain_room_visit_current(c,serial,revision,n,room,e))return false;
 bp::Fields f;if(!body->readFields(n,f,e)||f.roomIndex!=room||n->mVelocity.y!=writerExpectedVelocity){e="writer observed room="+std::to_string(f.roomIndex)+" velocityY="+std::to_string(n->mVelocity.y);return false;}
 check(!pc_p2_original_captain_room_visit_current(c,serial,revision,&b,room,e),"Scene writer rejects wrong actor");check(!pc_p2_original_captain_room_visit_current(c,serial,revision,n,room+1,e),"Scene writer rejects wrong room");check(!pc_p2_original_captain_room_visit_current(c,serial,revision+1,n,room,e),"Scene writer rejects wrong selected revision");
 events.push_back("sceneVisit");++sceneWrites;
 if(writerExpires)lifetime=false;
 if(!pc_p2_original_captain_room_visit_current(c,serial,revision,n,room,e))return false;
 ++laterWrites;return true;
}
bool pc_p2_retail_scene_find_water(const p2retail::SceneContext& c,std::uint64_t serial,std::uint64_t revision,const std::array<float,3>& position,p2retail::SourceWaterResult& out,std::string& e){
 ++queries;events.push_back("findWater");if(!queryAvailable){e="engineering Scene930 query unavailable";return false;}
 if(&c!=prepared||serial!=scene.epoch||revision!=4||position[1]!=expectedWaterY)throw std::runtime_error("exact cached sphere/query ownership");
 if(queryRetire){world.value=Phase::Inactive;queryRetireDenied=!bp::retireNativePhases(scene,e);world.value=Phase::GameWorldActive;}
 if(queryExpire)++scene.epoch;
 out={p2retail::SourceWaterState::KnownDry,&geometry,&inputs,serial,queryWrong?revision+1:revision,1};return true;
}
int main(int argc,char** argv){try{
 check(argc==2,"actual private Navi parameter argument");std::ifstream file(argv[1],std::ios::binary);bytes.assign(std::istreambuf_iterator<char>(file),{});SourceBank bank;bankProvider=&bank;NaviStateMachine machine;machine.registerState(&typed);machine.registerState(&second);a.mStateMachine=&machine;b.mStateMachine=&machine;a.current=&typed;b.current=&typed;frame();MapMgr map;auto context=p2retail::SceneRuntime::make(map,scene.c,scene.f,scene.epoch);prepared=context.get();
 world.value=Phase::GameWorldActive;check(!bp::createNativePhases(*context,provider,trace,bank,error),"actual composition requires Loading");world.value=Phase::Loading;
 geometryOwner=nullptr;check(!bp::createNativeTrace(*context,error),"actual trace requires selected geometry owner");geometryOwner=&geometry;
 parameterOwner=nullptr;check(!bp::createNativeTrace(*context,error),"actual trace requires selected floor parameter owner");parameterOwner=&floorParameters;
 auto nativeTrace=bp::createNativeTrace(*context,error);wrongTrace=nativeTrace.get();check(bool(nativeTrace),"Loading builds actual numeric trace from immutable wide selected geometry");
 parameterOwner=&hiddenParameters;auto hiddenTrace=bp::createNativeTrace(*context,error);check(bool(hiddenTrace),"actual hidden selected parameters bind separately");parameterOwner=&floorParameters;
 auto resetControl=[&](auto callback){return p2original::sourceCamera::Rig::resetControl(*context,scene,callback,error);};
 check(!resetControl([](auto&){return true;}),"Loading camera initialization cannot precede actual phase composition");
 check(bp::createNativePhases(*context,provider,trace,bank,error),"real native composition initializes both actual body and water owners");body=pc_p2_original_captain_body_phase_owner(&a);check(body&&body==pc_p2_original_captain_body_phase_owner(&b),"genuine source composition owns both actors");bp::Fields fields;check(body->readFields(&a,fields,error)&&!fields.bounding&&!fields.previous,"source cached center deliberately unknown before simulation");auto firstBirth=fields.initializationSerial;
 check(!resetControl([](auto&){return true;}),"Loading camera initialization requires actual committed physical floor");loadingFloorCommitted=true;
 check(resetControl([&](auto& scope){camera::ActorPose initial;check(scope.read(0,initial,error)&&initial.position[0]==a.mSRT.t.x&&initial.face==a.mFaceDirection,"private reset scope observes first actual initialized Loading pose");check(scope.read(1,initial,error)&&initial.position[0]==b.mSRT.t.x,"private reset scope observes actual partner Loading pose");check(!scope.read(2,initial,error),"reset scope rejects non-roster camera slot");check(!body->canRetire(error),"private body hold blocks retirement through entire camera reset");world.value=Phase::Inactive;check(!bp::retireNativePhases(scene,error),"composition read hold blocks native retirement during camera callback");world.value=Phase::Loading;return scope.current(error);}),"actual Loading body scope closes normally without animation/Plate prerequisites");
 check(resetControl([&](auto& scope){check(!resetControl([](auto&){return true;}),"nested actual camera reset scope refuses");return !scope.current(error);}),"nested reset invalidates outer initialization publication");
 check(resetControl([&](auto& scope){body->forget(&b);camera::ActorPose initial{{81,82,83},84};check(!scope.read(0,initial,error)&&initial.position[0]==81&&initial.face==84,"partner forget during camera reset blocks first pose publication");return !scope.current(error);}),"private body hold preserves storage and revokes forgotten-partner scope");check(body->readFields(&b,fields,error),"callback forget cannot remove held partner storage");
 check(resetControl([&](auto& scope){check(!body->initializeAfterBodyReset(&a,error),"camera reset hold refuses body cold reinitialization");return !scope.current(error);}),"attempted camera callback reinitialization revokes outer scope");
 check(resetControl([&](auto& scope){camera::ActorPose initial{{81,82,83},84};world.movie=Demo::Playing;check(!scope.read(0,initial,error)&&initial.face==84,"Loading camera movie-active pose refuses without output");world.movie=Demo::Inactive;world.value=Phase::GameWorldActive;check(!scope.current(error),"reset scope cannot continue after World activation");world.value=Phase::Loading;return true;}),"private reset scope owns only exact Loading observation");
 check(resetControl([&](auto& scope){camera::ActorPose initial{{81,82,83},84};a.current=&second;check(!scope.read(0,initial,error)&&initial.face==84,"reset scope refuses actual FSM transition before camera publication");a.current=&typed;return true;}),"initial camera observations retain exact captured FSM identity");
 check(resetControl([&](auto& scope){camera::ActorPose initial{{81,82,83},84};cameraDemoReads=0;cameraDemoExpireAt=2;check(!scope.read(0,initial,error)&&initial.face==84,"Loading camera post-movie callback expiry refuses atomic publication");--scene.epoch;cameraDemoExpireAt=0;return true;}),"Loading reset callback expiry control");
 check(resetControl([&](auto& scope){camera::ActorPose initial{{81,82,83},84};actorAlive=false;check(scope.read(0,initial,error),"known CF-dead Loading body remains an actual initial camera target");actorAlive=true;initial={{81,82,83},84};lifetime=false;check(!scope.read(0,initial,error)&&initial.face==84,"unknown Loading body lifetime refuses initial camera publication");lifetime=true;const float face=a.mFaceDirection;a.mFaceDirection=std::numeric_limits<float>::infinity();check(!scope.read(0,initial,error)&&initial.face==84,"nonfinite Loading face refuses initial camera publication");a.mFaceDirection=face;return scope.current(error);}),"Loading camera finite/lifetime controls preserve output and release scope");
 world.wrong=true;check(!resetControl([](auto&){return true;}),"Loading reset cannot admit changed actual World roster");world.wrong=false;
 check(resetControl([&](auto& scope){return scope.current(error);}),"closed failure scopes release private body/composition holds for a fresh reset");loadingFloorCommitted=false;
 bool wet=true;check(bp::cachedNativeWater(&a,wet,error)&&!wet&&queries==0,"genuine init cached null without fresh map query");check(!bp::createNativePhases(*context,provider,trace,bank,error),"live composition cannot be overwritten");
 camera::ActorPose pose{{91,92,93},94};auto unchangedPose=[&](){return pose.position==std::array<float,3>{91,92,93}&&pose.face==94;};
 check(!camera::readCameraPose(*context,&a,pose,error)&&unchangedPose(),"ordinary camera pose cannot bypass Loading bootstrap scope");
 world.value=Phase::GameWorldActive;a.mSRT.t.set(11,12,13);a.mFaceDirection=.7f;
 check(camera::readCameraPose(*context,&a,pose,error)&&pose.position==std::array<float,3>{11,12,13}&&pose.face==.7f,"actual camera pose independent of unknown Plate offset/previous/bounding");
 b.mSRT.t.set(21,22,23);b.mFaceDirection=1.1f;check(camera::readCameraPose(*context,&b,pose,error)&&pose.position[0]==21&&pose.face==1.1f,"camera reads exact second source roster body");
 pose={{91,92,93},94};actorAlive=false;check(camera::readCameraPose(*context,&a,pose,error)&&pose.position[0]==11,"known CF-dead source body remains a valid camera target");actorAlive=true;pose={{91,92,93},94};
 world.movie=Demo::Playing;check(!camera::readCameraPose(*context,&a,pose,error)&&unchangedPose(),"movie-active camera pose cannot substitute body SRT for source model translation");world.movie=Demo::Unknown;check(!camera::readCameraPose(*context,&a,pose,error)&&unchangedPose(),"unknown source movie authority leaves camera pose unchanged");world.movie=Demo::Inactive;
 a.mFaceDirection=std::numeric_limits<float>::quiet_NaN();check(!camera::readCameraPose(*context,&a,pose,error)&&unchangedPose(),"nonfinite source face refuses camera publication");a.mFaceDirection=.7f;
 a.mSRT.t.y=std::numeric_limits<float>::infinity();check(!camera::readCameraPose(*context,&a,pose,error)&&unchangedPose(),"nonfinite source position refuses camera publication");a.mSRT.t.y=12;
 lifetime=false;check(!camera::readCameraPose(*context,&a,pose,error)&&unchangedPose(),"expired source body lifetime refuses camera publication");lifetime=true;
 prepared=nullptr;check(!camera::readCameraPose(*context,&a,pose,error)&&unchangedPose(),"missing committed source scene refuses camera pose");prepared=context.get();
 world.wrong=true;check(!camera::readCameraPose(*context,&a,pose,error)&&unchangedPose(),"source roster replacement refuses camera pose");world.wrong=false;
 cameraDemoReads=0;cameraDemoExpireAt=1;check(!camera::readCameraPose(*context,&a,pose,error)&&unchangedPose(),"movie observation expiry refuses before actor reads");--scene.epoch;
 cameraDemoReads=0;cameraDemoExpireAt=2;check(!camera::readCameraPose(*context,&a,pose,error)&&unchangedPose(),"post-observation source identity expiry refuses pose publication");--scene.epoch;cameraDemoExpireAt=0;
 b.current=&second;cameraDemoChangesState=true;cameraDemoReads=0;cameraDemoExpireAt=2;check(!camera::readCameraPose(*context,&a,pose,error)&&unchangedPose(),"post-observation actual FSM change refuses camera publication");a.current=&typed;b.current=&typed;cameraDemoChangesState=false;cameraDemoExpireAt=0;
 Navi foreignCameraActor;check(!camera::readCameraPose(*context,&foreignCameraActor,pose,error)&&unchangedPose(),"foreign actor cannot borrow source camera pose");
 events.clear();check(!bp::body_animation(&a,error)&&queries==0&&events==std::vector<std::string>{"cellLOD","clocks"},"unknown cached sphere refuses exactly at animation water point");
 observation.naviManagerFlag1=true;a.mSRT.t.set(3,7,5);events.clear();check(bp::body_simulation(&a,.01f,error)&&events.empty(),"actual manager flag simulation writes bounding without trace");check(body->readFields(&a,fields,error)&&fields.bounding&&fields.bounding->center.y==7&&fields.bounding->radius==8.5f,"actual cached bound from source simulation");observation.naviManagerFlag1=false;expectedWaterY=7;a.mVelocity.set(0,5,0);events.clear();
 check(bp::body_animation(&a,error)&&queries==1,"real composition animation calls actual Scene query once");check(events==std::vector<std::string>{"cellLOD","clocks","findWater","geometry","cursor"}&&a.mVelocity.y==4,"literal water point precedes gravity and geometry");check(bp::cachedNativeWater(&a,wet,error)&&!wet&&queries==1,"cached water never performs fresh find");
 queryRetire=true;events.clear();check(bp::body_animation(&a,error)&&queryRetireDenied,"in-flight real composition retirement refuses without destruction");queryRetire=false;check(body==pc_p2_original_captain_body_phase_owner(&a),"callback retirement leaves actual owner live");
 queryAvailable=false;events.clear();float prior=a.mVelocity.y;check(!bp::body_animation(&a,error)&&a.mVelocity.y==prior&&events.back()=="findWater","missing actual query stops gravity/geometry");queryAvailable=true;queryWrong=true;events.clear();check(!bp::body_animation(&a,error)&&events.back()=="findWater","wrong query selection identity refuses");queryWrong=false;
 queryExpire=true;events.clear();check(!bp::body_animation(&a,error)&&events.back()=="findWater","scene replacement during query stops body suffix");queryExpire=false;--scene.epoch;wet=true;check(bp::cachedNativeWater(&a,wet,error)&&!wet,"failed candidate never overwrites genuine cached null");
 world.wrong=true;wet=true;check(!bp::cachedNativeWater(&a,wet,error)&&wet,"canonical World roster mismatch leaves cached output unchanged");world.wrong=false;prepared=nullptr;check(!bp::body_animation(&a,error),"missing actual SceneContext revokes phase authority");prepared=context.get();lifetime=false;check(!bp::body_animation(&a,error),"missing actual actor lifetime refuses native phase");lifetime=true;
 observation.naviManagerFlag1=true;b.mSRT.t.set(4,7,5);check(bp::body_simulation(&b,.01f,error),"second actual source slot establishes its own cached bound");observation.naviManagerFlag1=false;
 observation.gamePaused=true;events.clear();const auto beforePause=queries;
 check(bp::tickNativePhases(.01f,error)&&events.empty()&&queries==beforePause,"actual source paused manager does not dispatch any actor phase");
 check(!bp::tickNativePhases(.01f,error)&&queries==beforePause,"same source GameSystem frame cannot dispatch twice");
 observation.gamePaused=false;observation.frameTimer=2;a.mFaceDirection=.3f;events.clear();
 check(bp::tickNativePhases(.01f,error)&&queries==beforePause+2&&events.back()=="psm","actual manager updates and animates both open bodies before simulation suffix");
 auto clock=std::find(events.begin(),events.end(),"clocks"),execute=std::find(events.begin(),events.end(),"exec"),mapEvent=std::find(events.begin(),events.end(),"map"),queryEvent=std::find(events.begin(),events.end(),"findWater");
 check(execute<clock&&clock<queryEvent&&queryEvent<mapEvent,"literal source manager update before animation before simulation");
 nativecontrol::AnimationFrame observed;check(bp::nativeAnimationFrame(&a,observed,error)&&observed.displacementKnown&&observed.faceDirectionOffset==.3f&&observed.deltaTime==.01f,"native control observations borrow actual source phase fields and GameSystem facts");
 check(!body->roomVisitCurrent(&a,trace,0,error),"Owner room guard refuses outside actual simulation callback");
 traceRoom=9;check(bp::body_simulation(&a,0,error)&&observedRoom==9,"source roomIndex published before actual Room callback");traceRoom=10;roomRefuses=true;check(!bp::body_simulation(&a,0,error)&&observedRoom==10&&body->readFields(&a,fields,error)&&fields.roomIndex==10,"Room refusal retains literal source partial roomIndex write");traceRoom=11;roomRefuses=false;roomInvalidates=true;events.clear();check(!bp::body_simulation(&a,0,error)&&events.back()=="room","room callback generation expiry stops post-room simulation suffix");roomInvalidates=false;roomRebirth=true;traceRoom=12;events.clear();check(!bp::body_simulation(&a,0,error)&&events.back()=="room","attempted callback reinitialization stops source suffix");roomRebirth=false;traceRoom=-1;roomRefuses=false;
 // Trace calls execute production NativeTrace and Numeric411 against actual
 // composition/BodyBorrowerGuard; only geometry/session providers are doubles.
 check(!bp::createNativeTrace(*context,error),"active native trace construction refuses");
 bp::TraceInfo info;info.sphere={{0,5,0},8.5f};info.velocity={0,-10,0};info.roomIndex=-1;
 check(nativeTrace->map(a,info,.1f,error)&&info.floor.triangle&&info.floor.incarnation==scene.epoch&&info.floorNormal.y>.99f,"real numeric trace selects genuine source table floor identity");
 bp::FloorFacts ground;check(nativeTrace->floor(info.floor,ground,error)&&ground.slip==2&&ground.contents==8&&ground.planeNormal.y>.99f,"actual mapcode slip and contents decoded from selected original triangle");
 bp::FloorFacts unchanged{99,99,{9,9,9}};int foreignTriangle=0;check(!nativeTrace->floor({&foreignTriangle,scene.epoch},unchanged,error)&&unchanged.slip==99,"foreign native floor identity refuses without writing facts");check(!nativeTrace->floor({info.floor.triangle,scene.epoch+1},unchanged,error)&&unchanged.slip==99,"source floor wrong incarnation refuses");
 auto same=[](const bp::TraceInfo& x,const bp::TraceInfo& y){auto v=[](bp::Vec3 a,bp::Vec3 b){return a.x==b.x&&a.y==b.y&&a.z==b.z;};return v(x.sphere.center,y.sphere.center)&&x.sphere.radius==y.sphere.radius&&v(x.velocity,y.velocity)&&x.floor.triangle==y.floor.triangle&&x.floor.incarnation==y.floor.incarnation&&x.wall.triangle==y.wall.triangle&&x.wall.incarnation==y.wall.incarnation&&v(x.floorNormal,y.floorNormal)&&v(x.wallNormal,y.wallNormal)&&x.traceRadius==y.traceRadius&&x.roomIndex==y.roomIndex;};
 bp::TraceInfo before=info;world.value=Phase::Loading;check(!nativeTrace->map(a,info,.1f,error)&&same(info,before),"trace requires actual Active body borrower and keeps output");world.value=Phase::GameWorldActive;
 actorAlive=false;check(nativeTrace->map(a,info,0,error),"genuine known CF-dead body remains physically traceable");actorAlive=true;
 before=info;++scene.epoch;check(!nativeTrace->map(a,info,.1f,error)&&same(info,before),"expired source scene refuses atomically");--scene.epoch;
 p2retail::SceneRuntime::revision(*context,5);check(!nativeTrace->map(a,info,.1f,error)&&same(info,before),"selected revision expiry leaves all TraceInfo unchanged");check(!nativeTrace->floor(info.floor,unchanged,error)&&unchanged.slip==99,"selected revision expiry leaves floor facts unchanged");p2retail::SceneRuntime::revision(*context,4);
 prepared=nullptr;check(!nativeTrace->map(a,info,.1f,error)&&same(info,before),"missing selected context refuses atomically");prepared=context.get();
 geometryOwner=nullptr;check(!nativeTrace->map(a,info,.1f,error)&&same(info,before),"replaced selected geometry refuses atomically");geometryOwner=&geometry;
 expireGeometryAt=geometryQueries+2;check(!nativeTrace->map(a,info,.1f,error)&&same(info,before),"source geometry callback expiry refuses numeric publication");expireGeometryAt=0;--scene.epoch;
 parameterOwner=&hiddenParameters;check(!hiddenTrace->map(a,info,.1f,error)&&same(info,before),"actual hidden collision flag requires unavailable sentinel and refuses");bp::Sphere clamp=info.sphere;check(!hiddenTrace->constrain(a,clamp,error)&&clamp.center.y==info.sphere.center.y,"actual hidden clamp producer unavailable without fabricated bounds");parameterOwner=&floorParameters;
 check(!nativeTrace->platforms(a,info,.1f,error)&&same(info,before),"missing original PlatMgr refuses without output");check(!nativeTrace->room(a,0,error)&&sceneVisitCalls==0,"direct map plus room cannot invoke Scene writer outside Owner simulation");check(!pc_p2_original_captain_room_visit_current(*context,scene.epoch,4,&a,0,error),"Scene room writer receiver absent outside actual trace callback");
 bp::BodyBorrowerGuard borrower;check(!borrower.current(error),"default borrowed trace guard has no source authority");
 check(bp::BodyBorrowerGuard::capture(*context,&a,borrower,error)&&borrower.current(error),"trace guard borrows actual committed scene/body generation");
 a.current=&second;check(!borrower.current(error),"source trace callback changing real FSM revokes borrower");a.current=&typed;
 lifetime=false;check(!borrower.current(error),"unknown actual source lifetime revokes borrower");lifetime=true;
 world.wrong=true;check(!borrower.current(error),"source trace World roster replacement revokes borrower");world.wrong=false;
 observation.frameTimer=3;queryAvailable=false;events.clear();
 check(!bp::tickNativePhases(.01f,error),"source frame refuses unavailable registered SeaMgr query");const auto failureQueries=queries;
 check(!bp::tickNativePhases(.01f,error)&&queries==failureQueries,"refused source frame cannot replay callback effects");queryAvailable=true;
 world.value=Phase::GameWorldActive;check(!bp::retireNativePhases(scene,error),"active retirement refuses");world.value=Phase::Inactive;check(bp::retireNativePhases(scene,error),"exact inactive composition retirement drains actual child owners");check(!pc_p2_original_captain_body_phase_owner(&a),"retired composition revokes owner");wet=true;check(!bp::cachedNativeWater(&a,wet,error)&&wet,"missing composition leaves output untouched");body=nullptr;
 world.value=Phase::Loading;check(bp::createNativePhases(*context,provider,trace,bank,error),"actual replacement composition");body=pc_p2_original_captain_body_phase_owner(&a);check(body->readFields(&a,fields,error)&&fields.initializationSerial>firstBirth&&!fields.bounding,"replacement owner birth defeats scene-slot ABA without invented sphere");world.value=Phase::Inactive;check(bp::retireNativePhases(scene,error),"replacement inactive cleanup");body=nullptr;
 world.value=Phase::Loading;check(bp::createNativePhases(*context,provider,*nativeTrace,bank,error),"actual phase composition borrows production NativeTrace");body=pc_p2_original_captain_body_phase_owner(&a);world.value=Phase::GameWorldActive;frame();a.mSRT.t.set(0,0,0);observation.naviManagerFlag1=true;check(bp::body_simulation(&a,0,error),"genuine simulation establishes native trace body's cached sphere");observation.naviManagerFlag1=false;expectedWaterY=0;check(bp::body_animation(&a,error),"actual native trace body previous position established by animation");a.mVelocity.set(0,-10,0);bp::TraceInfo predicted;predicted.sphere={{a.mSRT.t.x,a.mSRT.t.y+8.5f,a.mSRT.t.z},8.5f};predicted.velocity={0,-10,0};check(nativeTrace->map(a,predicted,.1f,error),"reference real trace computes source response before room publication test");writerExpectedVelocity=predicted.velocity.y;events.clear();auto writesBefore=sceneWrites;
 check(bp::body_simulation(&a,.1f,error)&&sceneWrites==writesBefore+1&&laterWrites==sceneWrites,"real NativeTrace room writer admitted through actual Owner map simulation");auto visit=std::find(events.begin(),events.end(),"sceneVisit"),bounce=std::find(events.begin(),events.end(),"bounce");check(visit<bounce&&std::find(events.begin(),events.end(),"selector")>visit,"Scene room write occurs after velocity/index publication before bounce and selector");
 writerExpires=true;a.mVelocity.set(0,-10,0);predicted={};predicted.sphere={{a.mSRT.t.x,a.mSRT.t.y+8.5f,a.mSRT.t.z},8.5f};predicted.velocity={0,-10,0};check(nativeTrace->map(a,predicted,.1f,error),"reference real trace for expiry branch");writerExpectedVelocity=predicted.velocity.y;events.clear();auto laterBefore=laterWrites;check(!bp::body_simulation(&a,.1f,error)&&sceneWrites==writesBefore+2&&laterWrites==laterBefore&&events.back()=="sceneVisit","Scene callback lifetime expiry blocks later writer mutation and body suffix");writerExpires=false;lifetime=true;
 check(!nativeTrace->room(a,0,error)&&sceneVisitCalls==sceneWrites,"receiver closes after callback failure; direct room cannot replay writer");world.value=Phase::Inactive;check(bp::retireNativePhases(scene,error),"actual NativeTrace composition cleanup");body=nullptr;
 std::cout<<checks<<" actual native composition/body/water TU controls PASS (engineering doubles; no gameplay claim)\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

