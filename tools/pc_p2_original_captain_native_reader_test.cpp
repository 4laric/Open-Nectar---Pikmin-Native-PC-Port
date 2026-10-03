// Actual reader/cstick/control TUs with engineering source-bank/body doubles.
// These assertions do not qualify a concrete source world or gameplay.
#include "pc_p2_original_captain_native_reader.h"
#include "Navi.h"
#include "NaviState.h"
#include "Kontroller.h"
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <limits>
using namespace p2original::captain;
namespace pk=p2original::piki;namespace nr=p2original::captain::nativereader;
namespace {
int checks=0;void check(bool x){++checks;if(!x){std::cerr<<"Failed check "<<checks<<'\n';std::exit(1);}}
struct Typed final:NaviState,State {
 StateId id=StateId::Walk;
 const NaviState* nativeState()const override{return this;}StateId sourceStateId()const override{return id;}
 bool sourceAlive(const Navi&)const override{return true;}bool sourceInvincible()const override{return false;}
 std::optional<std::uint8_t> actorInvincibleFrames(const Navi&)const override{return 0;}
 bool canEnterSourceDead(const Navi&)const override{return false;}void enterSourceDead(Navi&)override{}void sourceDamageFeedback(Navi&)override{}
};
Navi a,b;Typed typed,other;Kontroller controller;
struct Scene final:LoadedScene {std::string campaign="engineering",fingerprint="bank",catalog="source";std::uint64_t epoch=1;
 const std::string& selectedCampaign()const override{return campaign;}const std::string& selectedFingerprint()const override{return fingerprint;}const std::string& sourceCatalog()const override{return catalog;}
 MoviePlayer* moviePlayer()const override{return nullptr;}std::uint64_t incarnation()const override{return epoch;}Navi* captainAt(unsigned s)const override{return s==0?&a:s==1?&b:nullptr;}
} scene;
struct WorldDouble final:World {Phase phase_=Phase::Loading;Demo demo_=Demo::Inactive;
 const std::string& selectedCampaign()const override{return scene.campaign;}const std::string& selectedFingerprint()const override{return scene.fingerprint;}const std::string& sourceCatalog()const override{return scene.catalog;}
 std::uint64_t incarnation()const override{return scene.epoch;}Navi* captainAt(unsigned s)const override{return scene.captainAt(s);}Phase phase()const override{return phase_;}Demo demo()const override{return demo_;}
} world;
SourceBank bank;SourceBank* selectedBank=&bank;const LoadedScene* selectedScene=&scene;bool lifetime=true,alive=true,bankValid=true;
std::string bytes;std::optional<float> timer;std::int8_t disband=0;std::uint64_t motionGeneration=1;
pk::Plate* ownedPlate=nullptr;pk::PlateState plateState;std::function<void()> jointQuery;
pk::Ownership bodyOwnership;unsigned retained=0;
nativecontrol::Request request(){nativecontrol::Request r;r.hasController=true;r.demo=world.demo_;r.camera.side={1,0,0};r.camera.up={0,1,0};r.camera.view={0,0,1};r.subStick={0,.075f};return r;}
struct Driver final:nr::PlateDriver {
 nr::Reader& reader;pk::Plate plate;std::vector<cstick::CommandKind> commands;std::function<void()> event;bool accepted=true;
 explicit Driver(nr::Reader& r):reader(r),plate(r.plateSource()){ownedPlate=&plate;}
 pk::Plate& storage()const override{return const_cast<pk::Plate&>(plate);}
 bool execute(Navi* n,const cstick::Command& c,std::string& e)override{
  commands.push_back(c.kind);
  if(c.kind==cstick::CommandKind::SetPos||c.kind==cstick::CommandKind::SetPosGray){pk::PlatePose pose;check(reader.readPose(n,pose,e));check(pose.scale==c.scale);check(pose.angle==c.angle);}
  if(event)event();
  return accepted;
 }
 bool afterRefresh(Navi*,cstick::AfterRefresh& out,std::string&)const override{out={};out.maxPositionOffset={0,0,0};return true;}
};
void fresh(){++scene.epoch;selectedScene=&scene;selectedBank=&bank;world.phase_=Phase::Loading;world.demo_=Demo::Inactive;lifetime=true;alive=true;bankValid=true;motionGeneration=1;timer.reset();disband=0;ownedPlate=nullptr;jointQuery={};bodyOwnership={};retained=0;a.current=&typed;b.current=&typed;typed.id=StateId::Walk;a.mKontroller=&controller;b.mKontroller=nullptr;a.mSRT.t={1,2,3};a.mVelocity={4,5,6};a.mFaceDirection=0;plateState={};plateState.maxPositionOffset={7,8,9};}
void init(nr::Reader& r,Driver& d){std::string e;check(r.initializeAfterBodyReset(e));check(r.bindPlate(d,e));}
void active(nr::Reader& r){std::string e;nr::Plan p;check(r.prepareCStick(&a,request(),p,e));check(p.resetsSceneAnimationTimer());check(r.commitCStick(p,e));}
}
const LoadedScene* pc_p2_original_captain_loaded_scene(){return selectedScene;}
const World* pc_p2_original_captain_world(){return &world;}
SourceBank* pc_p2_original_captain_source_bank(){return selectedBank;}
bool pc_p2_original_captain_actor_lifetime(const Navi* n,bool& out){if(!lifetime||(n!=&a&&n!=&b))return false;out=alive;return true;}
bool pc_p2_original_captain_actor_timers(const Navi*,PcOriginalCaptainTimers& out){if(!lifetime)return false;out.disbandDisable=disband;return true;}
namespace p2original {namespace captain {
struct SourceBank::Impl {};SourceBank::SourceBank():m(new Impl){}SourceBank::~SourceBank()=default;
bool SourceBank::sourceBytes(SourceResource,std::string& out,std::string&)const{out=bytes;return bankValid;}
bool SourceBank::parameters(SourceParameters& out,std::string&)const{out.neutralStick=.1f;return bankValid;}
bool SourceBank::state(const Navi*,MotionState& out,std::string&)const{out.generation=motionGeneration;return bankValid;}
bool SourceBank::jointWorld(Navi*,unsigned joint,std::array<float,12>& out,std::string&){check(joint==10);out={1,0,0,10,0,1,0,20,0,0,1,30};if(jointQuery)jointQuery();return bankValid;}
namespace nativecontrol {std::optional<float> sceneAnimationTimer(const Navi*){return timer;}}
}}
namespace p2original {namespace piki {
Plate* nativePlate(const captain::LoadedScene& s,std::string&){return &s==selectedScene?ownedPlate:nullptr;}
bool Plate::state(Navi*,PlateState& out,std::string&)const{out=plateState;return true;}
unsigned Plate::retainedSlots()const noexcept{return retained;}
bool readOwnership(Ownership& out,std::string&){out=bodyOwnership;return true;}
}}
int main(int argc,char** argv){
 check(argc==2);std::ifstream file(argv[1],std::ios::binary);bytes.assign(std::istreambuf_iterator<char>(file),{});check(!bytes.empty());std::string e;
 {fresh();nr::Reader r;pk::CaptainFrame f;f.face=77;check(!r.frame(&a,f,e));check(f.face==77);check(r.initializeAfterBodyReset(e));check(&r.plateSource()==static_cast<pk::PlateSource*>(&r));check(r.naviParameterBytes()==bytes);pk::PlateParameters parms;check(r.readParameters(&a,parms,e));check(parms.startingOffset==17.5f&&parms.lengthLimit==130&&parms.maxPositionSize==6);pk::PlatePose pose;pose.scale=88;check(!r.readPose(&a,pose,e));check(pose.scale==88);Driver d(r);check(r.bindPlate(d,e));check(!r.frame(&a,f,e));check(f.face==77);nr::Plan neutral;auto req=request();req.subStick={0,0};check(!r.prepareCStick(&a,req,neutral,e));check(d.commands.empty());}
 {fresh();nr::Reader r;Driver d(r);init(r,d);active(r);check((d.commands==std::vector<cstick::CommandKind>{cstick::CommandKind::Refresh,cstick::CommandKind::SetPos}));pk::CaptainFrame f;check(r.frame(&a,f,e));check(f.rhnd.x==13&&f.rhnd.y==20&&f.rhnd.z==30);check(f.plateOffset.x==7&&f.velocity.x==4&&f.position.x==1);check(f.command&&f.cstickNeutral&&!f.carryingPellet&&f.formationable&&f.sceneAnimationTimer==0);check(r.initializeAfterBodyReset(e));check(r.frame(&a,f,e)&&f.command);timer=9;world.phase_=Phase::GameWorldActive;disband=60;alive=false;typed.id=StateId::ThrowWait;check(r.frame(&a,f,e));check(!f.formationable&&!f.alive&&f.throwWait&&!f.throwing&&f.sceneAnimationTimer==9);typed.id=StateId::Throw;check(r.frame(&a,f,e)&&f.throwing&&!f.throwWait);typed.id=StateId::Follow;check(r.frame(&a,f,e)&&f.follow);typed.id=StateId::Pellet;f.face=77;check(!r.frame(&a,f,e)&&f.face==77);}
 {fresh();nr::Reader r;Driver d(r);init(r,d);nr::Plan p;check(r.prepareCStick(&a,request(),p,e));d.event=[&]{selectedBank=nullptr;};check(!r.commitCStick(p,e));check(d.commands.size()==1);selectedBank=&bank;pk::CaptainFrame f;check(r.frame(&a,f,e)&&f.command);check(!r.commitCStick(p,e));}
 {fresh();nr::Reader r;Driver d(r);init(r,d);nr::Plan p;check(r.prepareCStick(&a,request(),p,e));d.event=[&]{std::string nested;check(!r.commitCStick(p,nested));};check(!r.commitCStick(p,e));check(d.commands.size()==1);}
 {fresh();nr::Reader r;Driver d(r);init(r,d);nr::Plan p;check(r.prepareCStick(&a,request(),p,e));d.event=[&]{check(!r.initializeAfterBodyReset(e));};check(!r.commitCStick(p,e));check(d.commands.size()==1);d.event={};active(r);}
 {fresh();nr::Reader r;Driver d(r);init(r,d);auto req=request();req.subStick.z=.1f;nr::Plan p;check(r.prepareCStick(&a,req,p,e));check(r.commitCStick(p,e));pk::CaptainFrame f;check(r.frame(&a,f,e)&&f.command&&f.cstickNeutral);req.subStick.z=.1001f;check(r.prepareCStick(&a,req,p,e));check(r.commitCStick(p,e));check(r.frame(&a,f,e)&&!f.cstickNeutral);for(int i=0;i<40;++i)active(r);pk::PlatePose pose;check(r.readPose(&a,pose,e)&&pose.scale==3);}
 {fresh();nr::Reader r;Driver d(r);init(r,d);active(r);nr::Plan neutral;auto req=request();req.subStick={0,0};check(r.prepareCStick(&a,req,neutral,e));auto before=d.commands.size();check(!r.commitCStick(neutral,e));check(d.commands.size()==before+2);pk::CaptainFrame f;check(r.frame(&a,f,e)&&!f.command&&f.cstickNeutral);}
 {fresh();nr::Reader r;Driver d(r);init(r,d);nr::Plan p;check(r.prepareCStick(&a,request(),p,e));++motionGeneration;check(!r.commitCStick(p,e));check(d.commands.empty());}
 {fresh();nr::Reader r;Driver d(r);init(r,d);active(r);pk::CaptainFrame f;f.face=77;jointQuery=[&]{a.current=&other;};check(!r.frame(&a,f,e));check(f.face==77);jointQuery={};a.current=&typed;timer.reset();world.phase_=Phase::GameWorldActive;check(!r.frame(&a,f,e)&&f.face==77);world.phase_=Phase::Loading;world.demo_=Demo::Playing;check(!r.frame(&a,f,e)&&f.face==77);}
 {fresh();nr::Reader r;Driver d(r);init(r,d);active(r);pk::CaptainFrame f;f.face=77;jointQuery=[&]{++motionGeneration;};check(!r.frame(&a,f,e)&&f.face==77);jointQuery={};ownedPlate=nullptr;check(!r.frame(&a,f,e)&&f.face==77);}
 {fresh();nr::Reader r;Driver d(r);init(r,d);nr::Plan p;auto req=request();req.hasController=false;check(!r.prepareCStick(&a,req,p,e));req=request();req.subStick.x=std::numeric_limits<float>::quiet_NaN();check(!r.prepareCStick(&a,req,p,e));check(d.commands.empty());pk::Handle h;check(!r.setFormed(h,&a,e));}
 {fresh();nr::Reader r;Driver d(r);init(r,d);pk::CaptainFrame f;f.face=77;pk::PlatePose pose;pk::PlateParameters parms;nr::Plan plan;check(!r.frame(nullptr,f,e)&&f.face==77);check(!r.readPose(nullptr,pose,e));check(!r.readParameters(nullptr,parms,e));check(!r.prepareCStick(nullptr,request(),plan,e));scene.campaign+="changed";check(!r.readParameters(&a,parms,e));scene.campaign="engineering";++scene.epoch;check(!r.initializeAfterBodyReset(e));}
 {fresh();nr::Reader r;Driver d(r);init(r,d);active(r);nr::Plan old;check(r.prepareCStick(&a,request(),old,e));check(!r.canRetire(e));world.phase_=Phase::Inactive;retained=1;check(!r.canRetire(e));retained=0;bodyOwnership.entries=1;check(!r.canRetire(e));bodyOwnership={};ownedPlate=nullptr;lifetime=false;selectedBank=nullptr;pk::PlateParameters parms;check(r.readParameters(&a,parms,e));check(r.canRetire(e));check(r.retireAfterBodyConsumers(e));check(r.naviParameterBytes().empty());check(!r.commitCStick(old,e));world.phase_=Phase::Loading;selectedBank=&bank;lifetime=true;check(r.initializeAfterBodyReset(e));check(r.bindPlate(d,e)==false);ownedPlate=&d.plate;check(r.bindPlate(d,e));check(!r.commitCStick(old,e));}
 std::cout<<checks<<" concrete captain reader engineering checks PASS\n";
}
