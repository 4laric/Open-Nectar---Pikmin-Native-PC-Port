#include "pc_p2_original_captain_body_phases.h"
#include "Navi.h"
#include "NaviState.h"
#include <fstream>
#include <iterator>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <cmath>
#include <algorithm>
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
bp::Owner* pc_p2_original_captain_body_phase_owner(const Navi*){return body;}
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
int main(int argc,char** argv){try{
 check(argc==2,"actual private Navi parameters input");std::ifstream file(argv[1],std::ios::binary);bytes.assign(std::istreambuf_iterator<char>(file),{});SourceBank bank;bankProvider=&bank;NaviStateMachine machine;machine.registerState(&typed);machine.registerState(&second);a.mStateMachine=&machine;b.mStateMachine=&machine;a.current=&typed;b.current=&typed;a.mSRT.t.set(10,0,20);frame();
 auto owner=bp::Owner::create(provider,trace,bank,error);check(bool(owner),"actual source parameter composition");body=owner.get();check(owner->initializeAfterBodyReset(&a,error)&&owner->initializeAfterBodyReset(&b,error),"genuine Loading actor birth");check(!owner->initializeAfterBodyReset(&a,error),"duplicate birth refused");bp::Fields fields;check(owner->readFields(&a,fields,error)&&fields.initializationSerial&&fields.fpFlags==0&&fields.floor.triangle==nullptr&&fields.floorNormal.y==1&&fields.boundingRadius==8.5f&&!fields.bounding&&!fields.previous,"literal cold fields and unknown cached sphere/previous preserved");
 check(!bp::body_animation(&a,error)&&animationCount==0,"Loading body phase fenced");world.value=Phase::GameWorldActive;check(bp::setMoveRotation(&a,false,error),"genuine source FP flag event");bool disabled=false;check(bp::readFlag(&a,1,disabled,error)&&disabled,"actual owned FP bit read");check(bp::setMoveRotation(&a,true,error),"actual flag reset");
 a.current=&forged;check(!bp::body_update(&a,error)&&timerCount==0,"unregistered forged typed pointer fenced");a.current=&typed;
 check(bp::body_simulation(&a,0,error),"actual no-floor simulation establishes source cached sphere");check(owner->readFields(&a,fields,error)&&fields.bounding&&fields.bounding->center.x==10&&fields.bounding->radius==8.5f,"actual simulation defines cached sphere");events.clear();a.mVelocity.set(0,5,0);check(bp::body_animation(&a,error),"positive ordinary source animation");check(events==std::vector<std::string>{"cellLOD","clocks","water","geometry","cursor"},"source clocks/previous/water/gravity/model choreography");check(a.mVelocity.y==4&&animationCount==1,"source gravity after water");
 floorOut=true;wallOut=true;observation.platformsPresent=true;events.clear();check(bp::body_simulation(&a,.1f,error),"actual map/platform floor movement");check(events==std::vector<std::string>{"map","bounce","wall","platform","wall","selector","hell"},"bounce before floor assignment then both source wall callback points");check(owner->readFields(&a,fields,error)&&fields.floor.triangle==&floorKey&&fields.fakeBounce.triangle==&floorKey,"actual floor/fakeBounce retained separately");
 a.mTargetVelocity.set(100,0,0);a.mVelocity.set(0,0,0);events.clear();check(bp::body_animation(&a,error)&&near(a.mVelocity.x,10)&&near(a.mVelocity.y,-1),"literal .1 acceleration blending and gravity");check(near(a.mFaceDirection,.1256637f),"literal horizontal target rotation");
 check(bp::setMoveRotation(&a,false,error),"source move rotation disable event");float face=a.mFaceDirection;check(bp::body_animation(&a,error)&&a.mFaceDirection==face,"owned FP flag suppresses rotation independently native flags");
 traceNormal={.70710678f,.70710678f,0};slip=2;observation.platformsPresent=false;wallOut=false;check(bp::body_simulation(&a,0,error),"actual slope trace retained");a.mTargetVelocity.set(0,0,0);a.mVelocity.set(0,0,0);check(bp::body_animation(&a,error)&&near(a.mVelocity.x,1.767767f)&&near(a.mVelocity.y,-2.767767f),"source steep slide factor2.5 from actual floor normal");observation.rushBoots=true;a.mVelocity.set(0,0,0);check(bp::body_animation(&a,error)&&near(a.mVelocity.x,2.828427f)&&near(a.mVelocity.y,-3.828427f),"actual RushBoots steep factor4 once");observation.rushBoots=false;slip=0;traceNormal={0,1,0};check(bp::body_simulation(&a,0,error),"flat source floor restored");
 wallOut=true;events.clear();check(bp::body_simulation(&a,0,error)&&events==std::vector<std::string>{"map","wall","wall","selector","hell"},"literal second map-wall callback also occurs when PlatMgr absent");wallOut=false;
 randomDraw=0;a.mVelocity.set(100,0,0);events.clear();check(bp::body_simulation(&a,.1f,error)&&std::find(events.begin(),events.end(),"walkEffect")!=events.end(),"literal Navi terrain walk effect branch uses source RNG");randomDraw=1;
 observation.hiddenCollision=true;events.clear();check(bp::body_simulation(&a,.1f,error)&&a.mSRT.t.x==99,"actual hidden constraint callback");observation.hiddenCollision=false;
 observation.naviManagerFlag1=true;a.mVelocity.set(1,2,3);events.clear();int selected=selectorCount;check(bp::body_simulation(&a,.1f,error)&&a.mVelocity.x==0&&events.empty()&&selectorCount==selected,"actual manager movie freeze early return");observation.naviManagerFlag1=false;
 observation.stuck=true;events.clear();check(!bp::body_simulation(&a,.1f,error)&&events.empty(),"missing actual stuck producer explicitly refuses");observation.stuck=false;
 observation.movieMotion=true;events.clear();selected=animationCount;check(!bp::body_animation(&a,error)&&animationCount==selected&&events.empty(),"unsupported movie-motion cannot impersonate common clocks");observation.movieMotion=false;
 observation.mapPresent.reset();check(!bp::body_animation(&a,error),"missing map presence authority refuses");observation.mapPresent=true;
 events.clear();fsmTransition=true;check(bp::body_update(&a,error)&&a.current==&second&&execCount==1&&events.back()=="plate","actual typed FSM exec precedes real plate update and legitimate transition retained");check(events.front()=="iframe"&&events[1]=="event0"&&events[5]=="partyTimers"&&events[6]=="event4"&&events[7]=="menus","source mandatory update callbacks ordering");fsmTransition=false;flagChild=true;check(bp::body_update(&a,error),"literal source State.exec nested moveRotation event permitted");flagChild=false;
 frame();floorOut=false;wallOut=false;check(bp::body_simulation(&b,0,error),"second actual body establishes cached sphere");events.clear();check(owner->managerAnimation(error),"actual manager per-slot animation");auto firstPlate=std::find(events.begin(),events.end(),"plate"),firstClocks=std::find(events.begin(),events.end(),"clocks");check(firstPlate<firstClocks&&std::count(events.begin(),events.end(),"iframe")==2&&std::count(events.begin(),events.end(),"clocks")==2,"each source update precedes its animation");check(owner->readFields(&a,fields,error)&&fields.faceDirectionOffset&&*fields.faceDirectionOffset==a.mFaceDirection,"source manager captures prior face offset");events.clear();check(owner->managerSimulation(0,error)&&events.back()=="psm"&&std::count(events.begin(),events.end(),"map")==2,"both open source simulations precede genuine manager suffix");observation.naviManagerFlag1=true;events.clear();check(owner->managerAnimation(error)&&events.empty(),"actual manager flag excludes nonmovie actors");observation.naviManagerFlag1=false;observation.managerSlotOpen.reset();check(!owner->managerAnimation(error),"unknown genuine manager slot refuses");observation.managerSlotOpen=true;
 nested=true;events.clear();check(!bp::body_animation(&a,error)&&events==std::vector<std::string>{"cellLOD","clocks","water"},"rejected nested phase invalidates outer before gravity/model");nested=false;
 expire=true;events.clear();check(!bp::body_animation(&a,error)&&events==std::vector<std::string>{"cellLOD","clocks","water"},"water retirement callback stops gravity/geometry publication");expire=false;check(retireDenied&&owner->canRetire(error),"in-flight retirement refuses then drains callback owner");owner->forget(&a);check(!owner->readFields(&a,fields,error),"forgotten actor has no invented source fields");
 world.value=Phase::Loading;check(owner->initializeAfterBodyReset(&a,error),"genuine reinitialization after retirement event");bp::Fields reborn;check(owner->readFields(&a,reborn,error)&&reborn.initializationSerial!=fields.initializationSerial,"private initialization generation changes");world.value=Phase::GameWorldActive;
 world.wrong=true;check(!bp::body_update(&a,error),"canonical roster slot mismatch");world.wrong=false;lifetime=false;check(!bp::body_update(&a,error),"missing actual CF lifetime");lifetime=true;++scene.epoch;check(!bp::body_update(&a,error),"scene incarnation expiration");--scene.epoch;
 auto oldSerial=reborn.initializationSerial;owner->forget(&a);owner->forget(&b);body=nullptr;owner.reset();world.value=Phase::Loading;owner=bp::Owner::create(provider,trace,bank,error);check(bool(owner),"actual replacement composition accepts same canonical scene");body=owner.get();check(owner->initializeAfterBodyReset(&a,error)&&owner->readFields(&a,reborn,error)&&reborn.initializationSerial>oldSerial,"private birth serial process-monotonic across owner recreation");
 body=nullptr;disabled=true;check(!bp::readFlag(&a,1,disabled,error)&&disabled,"missing strong source owner leaves flag output intact");
 std::cout<<checks<<" actual body phase TU controls PASS (engineering doubles; no gameplay claim)\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
