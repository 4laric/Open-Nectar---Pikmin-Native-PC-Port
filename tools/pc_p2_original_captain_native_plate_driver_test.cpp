// Actual Root driver + Reader/cstick/control TUs. All SDK/Body/Bank/CF doubles
// below are engineering controls; they provide no production implementation.
#include "pc_p2_original_captain_native_plate_driver.h"
#include "pc_p2_original_captain_native_reader.h"
#include "pc_p2_original_piki_native_facts.h"
#include "Navi.h"
#include "NaviState.h"
#include "Kontroller.h"
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
class Piki {};
using namespace p2original::captain;
namespace pk=p2original::piki;namespace nr=p2original::captain::nativereader;
namespace {
int checks=0;void verify(bool ok,int line){++checks;if(!ok){std::cerr<<"Failed check "<<checks<<" at line "<<line<<'\n';std::exit(1);}}
#define check(value) verify((value),__LINE__)
Navi a,b;Kontroller controller;Piki sdk0,sdk1,otherCaptain,freeBody;
struct Typed final:NaviState,State {
 const NaviState* nativeState()const override{return this;}StateId sourceStateId()const override{return StateId::Walk;}
 bool sourceAlive(const Navi&)const override{return true;}bool sourceInvincible()const override{return false;}
 std::optional<std::uint8_t> actorInvincibleFrames(const Navi&)const override{return 0;}
 bool canEnterSourceDead(const Navi&)const override{return false;}void enterSourceDead(Navi&)override{}void sourceDamageFeedback(Navi&)override{}
} typed,replaced;
struct Scene final:LoadedScene {std::string campaign="engineering",fingerprint="source-bank",catalog="sdk";std::uint64_t serial=1;
 const std::string& selectedCampaign()const override{return campaign;}const std::string& selectedFingerprint()const override{return fingerprint;}const std::string& sourceCatalog()const override{return catalog;}
 std::uint64_t incarnation()const override{return serial;}MoviePlayer* moviePlayer()const override{return nullptr;}
 Navi* captainAt(unsigned slot)const override{return slot==0?&a:slot==1?&b:nullptr;}
} scene;
struct WorldDouble final:World {Phase mode=Phase::Loading;
 const std::string& selectedCampaign()const override{return scene.campaign;}const std::string& selectedFingerprint()const override{return scene.fingerprint;}const std::string& sourceCatalog()const override{return scene.catalog;}
 std::uint64_t incarnation()const override{return scene.serial;}Navi* captainAt(unsigned slot)const override{return scene.captainAt(slot);}
 Phase phase()const override{return mode;}Demo demo()const override{return Demo::Inactive;}
} world;
const LoadedScene* canonical=&scene;SourceBank bank;SourceBank* actualBank=&bank;
std::string parameterBytes;std::optional<float> timer;bool actualTransaction=true,bankBound=true,captainLifetime=true,physicalAvailable=true,censusAvailable=true,poseSupported=true;
pk::Plate* actualPlate=nullptr;pk::PlateState state;pk::PlatePose lastPose;std::vector<pk::Frame> sdkRoster;std::vector<pk::Handle> physicalReads;
std::vector<std::string> events;int refreshCount=-1;float refreshStrength=-1;unsigned censusReads=0;
std::function<void()> plateCallback,physicalCallback;
nr::Reader& reader(){return nr::instance();}
struct Physical final:pk::PhysicalSource {
 const LoadedScene& scene()const override{return ::scene;}
 bool readPhysical(pk::Handle h,pk::PhysicalFacts& out,std::string& e)const override {
  const bool live=(h.body==&sdk0&&h.lifetime==1)||(h.body==&sdk1&&h.lifetime==2)||(h.body==&otherCaptain&&h.lifetime==3)||(h.body==&freeBody&&h.lifetime==4);
  if(live){pk::PhysicalFacts next;next.alive=h.body!=&sdk0;next.updateContext=true;out=next;return true;}
  e="engineering actual SDK lifetime missing";return false;
 }
} physical;
nativecontrol::Request request(float strength=.075f){nativecontrol::Request r;r.hasController=true;r.demo=Demo::Inactive;r.camera.side={1,0,0};r.camera.up={0,1,0};r.camera.view={0,0,1};r.subStick={0,strength};return r;}
void fresh(std::string& e){
 if(!reader().naviParameterBytes().empty()){world.mode=Phase::Inactive;sdkRoster.clear();state.count=0;check(reader().retireAfterBodyConsumers(e));}
 ++scene.serial;world.mode=Phase::Loading;canonical=&scene;actualBank=&bank;timer.reset();actualTransaction=true;bankBound=true;captainLifetime=true;physicalAvailable=true;censusAvailable=true;poseSupported=true;
 sdkRoster.clear();physicalReads.clear();events.clear();refreshCount=-1;refreshStrength=-1;censusReads=0;plateCallback={};physicalCallback={};state={};lastPose={};
 a.current=&typed;b.current=&typed;a.mKontroller=&controller;b.mKontroller=nullptr;a.mSRT.t={2,4,6};a.mVelocity={3,5,7};a.mFaceDirection=0;
 check(reader().initializeAfterBodyReset(e));
 static pk::Plate solePlate(reader().plateSource());actualPlate=&solePlate;
 check(nr::bindNativePlateDriver(e));check(pc_p2_original_captain_native_reader()==&reader());
}
void active(std::string& e){nr::Plan plan;check(reader().prepareCStick(&a,request(),plan,e));check(reader().commitCStick(plan,e));}
bool neutral(std::string& e,float targetSpeed=0){nr::Plan plan;auto input=request(0);input.proposedTargetVelocity.x=targetSpeed;check(reader().prepareCStick(&a,input,plan,e));return reader().commitCStick(plan,e);}
pk::Frame member(Piki* body,std::uint64_t lifetime,Navi* captain,int slot){pk::Frame f;f.handle={body,lifetime};f.position={float(slot+1),2,3};f.captain=captain;f.formationSlot=slot;f.species=1;f.happa=2;f.state=pk::State::Walk;f.throwable=true;return f;}
void dense(){state.count=2;sdkRoster={member(&sdk1,2,&a,1),member(&otherCaptain,3,&b,0),member(&freeBody,4,nullptr,-1),member(&sdk0,1,&a,0)};}
void unchangedFrame(std::string& e){pk::CaptainFrame output;output.face=77;output.position={88,99,111};check(!reader().frame(&a,output,e));check(output.face==77&&output.position.x==88&&output.position.y==99&&output.position.z==111);}
}
const LoadedScene* pc_p2_original_captain_loaded_scene(){return canonical;}
const World* pc_p2_original_captain_world(){return &world;}
SourceBank* pc_p2_original_captain_source_bank(){return actualBank;}
bool pc_p2_original_captain_actor_lifetime(const Navi* body,bool& out){if(!captainLifetime||(body!=&a&&body!=&b))return false;out=true;return true;}
bool pc_p2_original_captain_actor_timers(const Navi*,PcOriginalCaptainTimers& out){out={};return true;}
namespace p2original {namespace captain {
struct SourceBank::Impl {};SourceBank::SourceBank():m(new Impl){}SourceBank::~SourceBank()=default;
bool SourceBank::sourceBytes(SourceResource,std::string& out,std::string&)const{out=parameterBytes;return true;}
bool SourceBank::parameters(SourceParameters& out,std::string&)const{out.neutralStick=.1f;return true;}
bool SourceBank::state(const Navi*,MotionState& out,std::string&)const{out.generation=1;return bankBound;}
bool SourceBank::jointWorld(Navi*,unsigned joint,std::array<float,12>& out,std::string&){check(joint==10);out={1,0,0,10,0,1,0,20,0,0,1,30};return bankBound;}
namespace nativecontrol {
std::optional<float> sceneAnimationTimer(const Navi*){return timer;}
bool cStickControlTransaction(Navi*,std::string&){return actualTransaction;}
bool resetCStickSceneAnimationTimer(Navi*,std::string&){if(!actualTransaction)return false;timer=0;events.push_back("reset");return true;}
}
}}
namespace p2original {namespace piki {
Plate* nativePlate(const captain::LoadedScene& owner,std::string&){return &owner==canonical?actualPlate:nullptr;}
PhysicalSource* nativePhysicalSource(const captain::LoadedScene& owner,std::string&){return physicalAvailable&&&owner==canonical?&physical:nullptr;}
bool readOwnership(Ownership& out,std::string&){out={};return true;}
unsigned Plate::retainedSlots()const noexcept{return 0;}
bool Plate::state(Navi*,PlateState& out,std::string&)const{out=::state;return true;}
bool Plate::refresh(Navi*,int count,float strength,std::string&){events.push_back("refresh");refreshCount=count;refreshStrength=strength;::state.maxRadiusKnown=true;if(plateCallback)plateCallback();return true;}
bool Plate::setPos(Navi* n,std::string& e){
 events.push_back("setPos");PlatePose pose;check(reader().readPose(n,pose,e));lastPose=pose;
 if(!poseSupported){e="engineering unsupported source pose";return false;}
 auto next=::state;next.positionKnown=true;next.maxPositionKnown=true;next.scaleKnown=true;next.angle=pose.angle;next.velocity=pose.velocity;next.maxPositionOffset=pose.position;::state=next;
 if(plateCallback)plateCallback();
 return true;
}
bool Plate::setPosGray(Navi* n,std::string& e){events.push_back("gray");return setPos(n,e);}
bool Plate::rearrange(Navi*,const Vector3f&,std::string&){events.push_back("rearrange");return true;}
bool roster(std::vector<Frame>& out,std::string& e){++censusReads;if(!censusAvailable){e="engineering genuine SDK census unavailable";return false;}out=sdkRoster;return true;}
bool nativePhysicalFacts(Handle h,const PhysicalSource* source,PhysicalFacts& out,std::string& e){
 if(source!=&physical)return false;
 physicalReads.push_back(h);if(!source->readPhysical(h,out,e))return false;
 if(physicalCallback)physicalCallback();
 return true;
}
}}
int main(int argc,char** argv){
 check(argc==2);std::ifstream source(argv[1],std::ios::binary);parameterBytes.assign(std::istreambuf_iterator<char>(source),{});check(!parameterBytes.empty());std::string e;
 fresh(e);state.count=3;active(e);check((events==std::vector<std::string>{"reset","refresh","setPos"}));check(refreshCount==3&&refreshStrength>0&&refreshStrength<1);check(lastPose.position.x==2&&lastPose.velocity.z==7&&lastPose.scale==1&&lastPose.moveStrength==refreshStrength);pk::CaptainFrame frame;check(reader().frame(&a,frame,e)&&frame.plateOffset.x==2);
 fresh(e);active(e);dense();physicalReads.clear();check(!neutral(e));check(e=="source neutral C-stick reads unknown mCStickState");check(censusReads==1&&physicalReads.size()==2);check(physicalReads[0].body==&sdk0&&physicalReads[1].body==&sdk1); // CF-dead sdk0 retained.
 fresh(e);active(e);dense();state.count=3;check(!neutral(e));check(e=="source CPlate census differs from committed Body formation membership");check(physicalReads.empty());
 fresh(e);active(e);dense();sdkRoster.back().formationSlot=2;check(!neutral(e));check(physicalReads.empty());
 fresh(e);active(e);dense();censusAvailable=false;check(!neutral(e));check(censusReads==1&&physicalReads.empty());check(e=="engineering genuine SDK census unavailable");
 fresh(e);active(e);dense();physicalAvailable=false;check(!neutral(e));check(censusReads==0&&physicalReads.empty());
 fresh(e);active(e);dense();state.maxPositionKnown=false;check(!neutral(e,100));check(physicalReads.size()==2);check(!state.maxPositionKnown);unchangedFrame(e);
 fresh(e);active(e);dense();physicalCallback=[&]{actualBank=nullptr;};check(!neutral(e));check(physicalReads.size()==1);unchangedFrame(e);actualBank=&bank;physicalCallback={};
 fresh(e);active(e);dense();physicalCallback=[&]{bankBound=false;};check(!neutral(e));check(physicalReads.size()==1);unchangedFrame(e);bankBound=true;physicalCallback={};
 fresh(e);active(e);dense();physicalCallback=[&]{captainLifetime=false;};check(!neutral(e));check(physicalReads.size()==1);unchangedFrame(e);captainLifetime=true;physicalCallback={};
 fresh(e);active(e);dense();sdkRoster.back().handle.lifetime=999;check(!neutral(e));check(physicalReads.size()==1);check(e=="engineering actual SDK lifetime missing");
 fresh(e);plateCallback=[&]{a.current=&replaced;};nr::Plan plan;check(reader().prepareCStick(&a,request(),plan,e));check(!reader().commitCStick(plan,e));check((events==std::vector<std::string>{"reset","refresh"}));a.current=&typed;plateCallback={};
 fresh(e);active(e);auto old=state.maxPositionOffset;poseSupported=false;check(reader().prepareCStick(&a,request(.5f),plan,e));check(!reader().commitCStick(plan,e));check(e=="engineering unsupported source pose");check(state.maxPositionOffset.x==old.x&&state.maxPositionOffset.y==old.y&&state.maxPositionOffset.z==old.z);check(reader().frame(&a,frame,e)&&frame.plateOffset.x==old.x);
 fresh(e);state.maxPositionKnown=false;poseSupported=false;check(reader().prepareCStick(&a,request(),plan,e));check(!reader().commitCStick(plan,e));unchangedFrame(e);
 fresh(e);pk::Plate* sole=actualPlate;actualPlate=nullptr;check(!nr::bindNativePlateDriver(e));unchangedFrame(e);actualPlate=sole;
 fresh(e);pk::Plate replacement(reader().plateSource());sole=actualPlate;actualPlate=&replacement;check(!nr::bindNativePlateDriver(e));unchangedFrame(e);actualPlate=sole;check(nr::bindNativePlateDriver(e));
 world.mode=Phase::Inactive;sdkRoster.clear();state.count=0;check(reader().retireAfterBodyConsumers(e));
 std::cout<<checks<<" actual native Plate driver engineering checks PASS\n";
}
