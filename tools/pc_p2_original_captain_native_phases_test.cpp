#include "pc_p2_original_captain_native_phases.h"
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
class MapMgr {}; // Physical pointer holder only; real query TU is a strong double.
namespace p2retail {
struct SourceRoomGeometry {};struct SourceWaterInputs {};
class SceneRuntime {public:static std::unique_ptr<SceneContext> make(MapMgr& map,const std::string& campaign,const std::string& session,unsigned serial){auto c=std::unique_ptr<SceneContext>(new SceneContext);c->mMap=&map;c->mCampaign=campaign;c->mSession=session;c->mRevision=4;c->mSnapshot.scene.serial=serial;return c;}};
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
struct WorldOwner:World {
 Phase value=Phase::Loading;bool wrong=false;
 const std::string& selectedCampaign()const override{return scene.c;}const std::string& selectedFingerprint()const override{return scene.f;}const std::string& sourceCatalog()const override{return scene.catalog;}
 std::uint64_t incarnation()const override{return scene.epoch;}Phase phase()const override{return value;}Demo demo()const override{return Demo::Inactive;}Navi* captainAt(unsigned i)const override{return wrong?nullptr:scene.captainAt(i);}
} world;
const LoadedScene* sceneProvider=&scene;const World* worldProvider=&world;SourceBank* bankProvider=nullptr;bp::Owner* body=nullptr;
bool lifetime=true,fsmTransition=false;bp::Facts observation;int animationCount=0,selectorCount=0,timerCount=0,execCount=0;int floorKey=0,wallKey=0;unsigned slip=0;bool floorOut=false,wallOut=false,platformFloor=false,below=false,expire=false,nested=false,retireDenied=false,flagChild=false;float randomDraw=1;bp::Vec3 traceNormal{0,1,0};
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
 bool map(Navi&,bp::TraceInfo& out,float rate,std::string&)override{events.push_back("map");out.sphere.center.x+=out.velocity.x*rate;out.sphere.center.y+=out.velocity.y*rate;out.sphere.center.z+=out.velocity.z*rate;out.floor=floorOut?bp::FloorHandle{&floorKey,::scene.epoch}:bp::FloorHandle{};out.floorNormal=traceNormal;out.wall=wallOut?bp::FloorHandle{&wallKey,::scene.epoch}:bp::FloorHandle{};out.wallNormal={1,0,0};return true;}
 bool platforms(Navi&,bp::TraceInfo& out,float,std::string&)override{events.push_back("platform");if(platformFloor)out.floor={&floorKey,::scene.epoch};return true;}
 bool constrain(Navi&,bp::Sphere& out,std::string&)override{events.push_back("constrain");out.center.x=99;return true;}
 bool room(Navi&,int,std::string&)override{events.push_back("room");return true;}
} trace;
void frame(){observation={};observation.deltaTime=.01f;observation.gravity=100;observation.mapPresent=true;observation.movieMotion=false;observation.movieActor=false;observation.movieExtra=false;observation.gameFrozen=false;observation.movieActive=false;observation.naviManagerFlag1=false;observation.stuck=false;observation.targetCollision=false;observation.platformsPresent=false;observation.hiddenCollision=false;observation.inWater=false;observation.rushBoots=false;observation.gamePaused=false;observation.frameTimer=1;observation.managerSlotOpen=true;}
}
const LoadedScene* pc_p2_original_captain_loaded_scene(){return sceneProvider;}const World* pc_p2_original_captain_world(){return worldProvider;}SourceBank* pc_p2_original_captain_source_bank(){return bankProvider;}

bool pc_p2_original_captain_actor_lifetime(const Navi*,bool& out){if(!lifetime)return false;out=true;return true;}
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
const p2retail::SceneContext* prepared=nullptr;int queries=0;bool queryAvailable=true,queryWrong=false,queryExpire=false,queryRetire=false,queryRetireDenied=false;float expectedWaterY=0;
p2retail::SourceRoomGeometry geometry;p2retail::SourceWaterInputs inputs;
}
const p2retail::SceneContext* pc_p2_retail_scene_prepared()noexcept{return prepared;}
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
 check(bp::createNativePhases(*context,provider,trace,bank,error),"real native composition initializes both actual body and water owners");body=pc_p2_original_captain_body_phase_owner(&a);check(body&&body==pc_p2_original_captain_body_phase_owner(&b),"genuine source composition owns both actors");bp::Fields fields;check(body->readFields(&a,fields,error)&&!fields.bounding&&!fields.previous,"source cached center deliberately unknown before simulation");auto firstBirth=fields.initializationSerial;
 bool wet=true;check(bp::cachedNativeWater(&a,wet,error)&&!wet&&queries==0,"genuine init cached null without fresh map query");check(!bp::createNativePhases(*context,provider,trace,bank,error),"live composition cannot be overwritten");world.value=Phase::GameWorldActive;
 events.clear();check(!bp::body_animation(&a,error)&&queries==0&&events==std::vector<std::string>{"cellLOD","clocks"},"unknown cached sphere refuses exactly at animation water point");
 observation.naviManagerFlag1=true;a.mSRT.t.set(3,7,5);events.clear();check(bp::body_simulation(&a,.01f,error)&&events.empty(),"actual manager flag simulation writes bounding without trace");check(body->readFields(&a,fields,error)&&fields.bounding&&fields.bounding->center.y==7&&fields.bounding->radius==8.5f,"actual cached bound from source simulation");observation.naviManagerFlag1=false;expectedWaterY=7;a.mVelocity.set(0,5,0);events.clear();
 check(bp::body_animation(&a,error)&&queries==1,"real composition animation calls actual Scene query once");check(events==std::vector<std::string>{"cellLOD","clocks","findWater","geometry","cursor"}&&a.mVelocity.y==4,"literal water point precedes gravity and geometry");check(bp::cachedNativeWater(&a,wet,error)&&!wet&&queries==1,"cached water never performs fresh find");
 queryRetire=true;events.clear();check(bp::body_animation(&a,error)&&queryRetireDenied,"in-flight real composition retirement refuses without destruction");queryRetire=false;check(body==pc_p2_original_captain_body_phase_owner(&a),"callback retirement leaves actual owner live");
 queryAvailable=false;events.clear();float prior=a.mVelocity.y;check(!bp::body_animation(&a,error)&&a.mVelocity.y==prior&&events.back()=="findWater","missing actual query stops gravity/geometry");queryAvailable=true;queryWrong=true;events.clear();check(!bp::body_animation(&a,error)&&events.back()=="findWater","wrong query selection identity refuses");queryWrong=false;
 queryExpire=true;events.clear();check(!bp::body_animation(&a,error)&&events.back()=="findWater","scene replacement during query stops body suffix");queryExpire=false;--scene.epoch;wet=true;check(bp::cachedNativeWater(&a,wet,error)&&!wet,"failed candidate never overwrites genuine cached null");
 world.wrong=true;wet=true;check(!bp::cachedNativeWater(&a,wet,error)&&wet,"canonical World roster mismatch leaves cached output unchanged");world.wrong=false;prepared=nullptr;check(!bp::body_animation(&a,error),"missing actual SceneContext revokes phase authority");prepared=context.get();lifetime=false;check(!bp::body_animation(&a,error),"missing actual actor lifetime refuses native phase");lifetime=true;
 world.value=Phase::GameWorldActive;check(!bp::retireNativePhases(scene,error),"active retirement refuses");world.value=Phase::Inactive;check(bp::retireNativePhases(scene,error),"exact inactive composition retirement drains actual child owners");check(!pc_p2_original_captain_body_phase_owner(&a),"retired composition revokes owner");wet=true;check(!bp::cachedNativeWater(&a,wet,error)&&wet,"missing composition leaves output untouched");body=nullptr;
 world.value=Phase::Loading;check(bp::createNativePhases(*context,provider,trace,bank,error),"actual replacement composition");body=pc_p2_original_captain_body_phase_owner(&a);check(body->readFields(&a,fields,error)&&fields.initializationSerial>firstBirth&&!fields.bounding,"replacement owner birth defeats scene-slot ABA without invented sphere");world.value=Phase::Inactive;check(bp::retireNativePhases(scene,error),"replacement inactive cleanup");body=nullptr;
 std::cout<<checks<<" actual native composition/body/water TU controls PASS (engineering doubles; no gameplay claim)\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
