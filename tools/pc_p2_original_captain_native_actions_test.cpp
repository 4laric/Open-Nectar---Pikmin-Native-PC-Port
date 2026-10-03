#include "pc_p2_original_captain_native_actions.h"
#include "pc_p2_original_captain_native_control.h"
#include "Navi.h"
#include "NaviState.h"
#include "Kontroller.h"
#include <fstream>
#include <iterator>
#include <iostream>
#include <stdexcept>
#include <limits>
class Piki {};
using namespace p2original::captain;
namespace na=nativeactions;namespace pk=p2original::piki;
// Strong engineering doubles isolate ACTUAL native_actions.cpp. SDK calls use
// actual public types, with explicit controlled handles/membership/expiry.
// They do not establish real SDK admission, rendering, or gameplay acceptance.
namespace {
Navi a,b,foreign;Piki p,q;std::string resource,error;int checks=0;
void check(bool yes,const char* label){++checks;if(!yes)throw std::runtime_error(std::string(label)+": "+error);}
struct Scene:LoadedScene {
 std::string campaign="campaign",fingerprint="session",catalog="captains";unsigned epoch=1;
 const std::string& selectedCampaign()const override{return campaign;}const std::string& selectedFingerprint()const override{return fingerprint;}const std::string& sourceCatalog()const override{return catalog;}
 MoviePlayer* moviePlayer()const override{return nullptr;}std::uint64_t incarnation()const override{return epoch;}Navi* captainAt(unsigned i)const override{return i==0?&a:i==1?&b:nullptr;}
} scene;
struct W:World {
 Phase value=Phase::GameWorldActive;std::string catalog="captains";
 const std::string& selectedCampaign()const override{return scene.campaign;}const std::string& selectedFingerprint()const override{return scene.fingerprint;}const std::string& sourceCatalog()const override{return catalog;}
 std::uint64_t incarnation()const override{return scene.epoch;}Phase phase()const override{return value;}Demo demo()const override{return Demo::Inactive;}Navi* captainAt(unsigned i)const override{return scene.captainAt(i);}
} world;
const LoadedScene* canonical=&scene;bool lifetimeAvailable=true,alive=true;SourceBank* bankProvider=nullptr;
struct Typed:NaviState,State {
 const NaviState* nativeState()const override{return this;}StateId sourceStateId()const override{return StateId::Walk;}bool sourceAlive(const Navi&)const override{return true;}bool sourceInvincible()const override{return false;}
 std::optional<std::uint8_t> actorInvincibleFrames(const Navi&)const override{return 0;}bool canEnterSourceDead(const Navi&)const override{return false;}void enterSourceDead(Navi&)override{}void sourceDamageFeedback(Navi&)override{}
} typed,other;
struct Services:pk::Services {
 const LoadedScene& scene()const override{return ::scene;}
 const std::string& pikiParameterBytes()const override{return resource;}
 const std::string& naviParameterBytes()const override{return resource;}
 bool gravity(float&,std::string&)const override{return false;}
 bool captainFrame(const Navi*,pk::CaptainFrame&,std::string&)const override{return false;}
 bool supports(pk::Handle,pk::Motion,std::string&)const override{return false;}
 bool motion(pk::Handle,pk::Motion,std::string&) override{return false;}
 bool animate(pk::Handle,float ,std::string&) override{return false;}
 bool currentMotion(pk::Handle,pk::Motion&,std::string&)const override{return false;}
 bool canRemoveFreeEffects(pk::Handle,std::string&)const override{return false;}
 bool freeEffects(pk::Handle,bool,std::string&) override{return false;}
 bool canRemoveThrowEffects(pk::Handle,std::string&)const override{return false;}
 bool throwEffects(pk::Handle,bool,std::string&) override{return false;}
 bool hangSound(pk::Handle,std::string&) override{return false;}
 bool landSound(pk::Handle,std::string&) override{return false;}
 bool calledSound(pk::Handle,std::string&) override{return false;}
 bool nudgeRumble(pk::Handle,Navi*,std::string&) override{return false;}
 bool allocateSlot(pk::Handle,Navi*,int&,std::string&) override{return false;}
 bool canReleaseSlot(pk::Handle,Navi*,int,std::string&)const override{return false;}
 bool releaseSlot(pk::Handle,Navi*,int,std::string&) override{return false;}
 bool slotPosition(pk::Handle,Navi*,int,Vector3f&,std::string&)const override{return false;}
 bool formed(pk::Handle,Navi*,std::string&) override{return false;}
 bool sortSlot(pk::Handle,Navi*,int,int ,std::string&) override{return false;}
 bool freeTaskAvailable(pk::Handle,pk::FreeSearch,bool& ,std::string&)const override{return false;}
 bool animationStatus(pk::Handle,pk::Motion&,float& ,bool& ,std::string&)const override{return false;}
 bool animationSpeed(pk::Handle,float,std::string&) override{return false;}
 bool finishMotion(pk::Handle,std::string&) override{return false;}
 bool loopStart(pk::Handle,std::string&) override{return false;}
 bool boreVoice(pk::Handle,bool ,std::string&) override{return false;}
 bool random(float&,std::string&) override{return false;}
} services;
struct Physical:pk::PhysicalSource {
 const LoadedScene& scene()const override{return ::scene;}bool readPhysical(pk::Handle,pk::PhysicalFacts& out,std::string&)const override{out.alive=alive;return true;}
} physical;
struct PlateSource:pk::PlateSource {
 const LoadedScene& scene()const override{return ::scene;}bool readParameters(const Navi*,pk::PlateParameters&,std::string&)const override{return false;}
 bool readPose(const Navi*,pk::PlatePose&,std::string&)const override{return false;}bool setFormed(pk::Handle,Navi*,std::string&)override{return false;}
} plateSource;
struct Actor:na::ActorSource {
 bool expire=false;std::optional<actions::PikiHandle> next;int effects=0;std::vector<na::WhistleCandidate> candidates;
 const LoadedScene& scene()const override{return ::scene;}
 bool observe(const Navi&,na::Observation& out,std::string&)const override{out={{9,0,7},.5f,false};if(expire)a.current=&other;return true;}
 bool world(party::WorldFacts& out,std::string&)const override{out.active=true;return true;}
 bool whistle(const Navi&,actions::WhistleFrame& out,std::string&)const override{out={{1,2,3},10,false};return true;}
 bool startWhistle(Navi&,std::string&)override{++effects;return true;}bool stopWhistle(Navi&,std::string&)override{++effects;return true;}
 bool updateWhistle(Navi&,actions::Vec3,bool,std::string&)override{++effects;return true;}
 bool whistleCandidates(const Navi&,std::vector<na::WhistleCandidate>& out,std::string&)const override{out=candidates;return true;}
 bool holdFields(Navi&,float,float,float,std::string&)override{++effects;if(expire)a.current=&other;return true;}
 bool nextThrowPiki(Navi&,std::optional<actions::PikiHandle> h,std::string&)override{next=h;return true;}
 bool feedback(Navi&,actions::Feedback,actions::PikiHandle,std::string&)override{++effects;return true;}
 bool togglePlayer(Navi&,Navi&,std::string&)override{++effects;return true;}bool changeVoice(Navi&,std::string&)override{++effects;return true;}
 bool dismissSound(Navi&,std::string&)override{++effects;return true;}bool disbandTimer(Navi&,unsigned,std::string&)override{++effects;return true;}
 bool followFrame(const Navi&,party::FollowFrame&,std::string&)const override{return true;}bool randomChoice(float& out,std::string&)override{out=.25;return true;}
 bool followFeedback(Navi&,party::FollowFeedback,std::string&)override{++effects;return true;}bool enemy(party::EnemyHandle,party::EnemyFrame&,std::string&)const override{return false;}
 bool followPunch(Navi&,party::EnemyHandle,actions::Vec3,std::string&)override{++effects;return true;}
} actor;
std::vector<pk::Frame> frames;int transitions=0,launches=0,whistles=0,gathers=0,positions=0,controls=0;bool sdkExpire=false,sdkAvailable=true;
pk::Frame* find(pk::Handle h){for(auto& f:frames)if(f.handle.body==h.body&&f.handle.lifetime==h.lifetime)return &f;return nullptr;}
}
const LoadedScene* pc_p2_original_captain_loaded_scene(){return canonical;}const World* pc_p2_original_captain_world(){return &world;}
SourceBank* pc_p2_original_captain_source_bank(){return bankProvider;}
bool pc_p2_original_captain_actor_lifetime(const Navi* n,bool& out){if(!lifetimeAvailable||(n!=&a&&n!=&b))return false;out=alive;return true;}
bool pc_p2_original_captain_actor_timers(const Navi*,PcOriginalCaptainTimers& out){out.throwDisable=8;return true;}
namespace p2original {namespace captain {
struct SourceBank::Impl {};SourceBank::SourceBank():m(new Impl){}SourceBank::~SourceBank()=default;
bool SourceBank::sourceBytes(SourceResource,std::string& out,std::string&)const{out=resource;return true;}
bool SourceBank::state(const Navi*,MotionState& out,std::string&)const{out={};return true;}
bool SourceBank::jointWorld(Navi*,unsigned joint,std::array<float,12>& out,std::string&){if(joint!=10)return false;out={1,0,0,10,0,1,0,20,0,0,1,30};return true;}
namespace nativecontrol {std::optional<float> sceneAnimationTimer(const Navi*){return 2;}bool control(Navi*,std::string&){++controls;return true;}}
namespace party {bool whistleCaptain(Navi* receiver,Navi* caller,bool,bool,std::string&){return receiver==&b&&caller==&a;}}
}}
namespace p2original {namespace piki {
bool handle(const Piki* body,Handle& out){if(!sdkAvailable)return false;for(const auto& f:frames)if(f.handle.body==body){out=f.handle;return true;}return false;}
bool frame(Handle h,Frame& out,std::string&){auto* f=find(h);if(!sdkAvailable||!f)return false;out=*f;return true;}
bool squad(Navi* n,std::vector<Frame>& out,std::string&){if(!sdkAvailable)return false;out.clear();for(const auto& f:frames)if(f.captain==n)out.push_back(f);return true;}
bool nativePhysicalFacts(Handle h,const PhysicalSource* p,PhysicalFacts& out,std::string& e){return find(h)&&p&&p->readPhysical(h,out,e);}
bool transition(Handle h,State state,std::string&){auto* f=find(h);if(!f)return false;++transitions;f->state=state;if(sdkExpire)a.current=&other;return true;}
bool position(Handle h,const Vector3f& v,std::string&){auto* f=find(h);if(!f)return false;++positions;f->position=v;return true;}
bool sortFormation(Handle h,int,std::string&){return find(h)!=nullptr;}
bool whistle(Handle h,Navi* n,std::string&){auto* f=find(h);if(!f)return false;++whistles;f->captain=n;return true;}
bool gather(Handle h,const Vector3f& goal,float,std::string&){auto* f=find(h);if(!f)return false;++gathers;f->position=goal;f->captain=nullptr;return true;}
bool launch(Handle h,Navi*,const Vector3f&,std::string&){auto* f=find(h);if(!f)return false;++launches;f->state=State::Flying;return true;}
bool Plate::slotPosition(Handle,Navi*,int,Vector3f& out,std::string&)const{out={4,5,6};return true;}
}}
int main(int argc,char** argv){try{
 check(argc==2,"actual private parameter input");std::ifstream f(argv[1],std::ios::binary);resource.assign(std::istreambuf_iterator<char>(f),{});
 SourceBank bank;bankProvider=&bank;pk::Plate plate(plateSource);a.current=&typed;b.current=&typed;Kontroller pad;a.mKontroller=&pad;pad.held=KBBTN_A;pad.pressed=KBBTN_B;pad.released=KBBTN_B;
 auto bridge=na::Bridge::create(actor,services,physical,plate,bank,error);check(bool(bridge),"authentic parameter composition accepts");
 frames={{{&p,10},{100,0,0},pk::State::Walk,0,1,&a,0,true,true},{{&q,20},{20,0,0},pk::State::Walk,2,2,&a,1,true,true}};
 actions::ActorFrame observation;check(bridge->frame(a,observation,error),"actual physical frame");check(observation.hand.x==13&&observation.hand.y==20&&observation.hand.z==30,"rhnd joint10 literal offset");
 check(observation.heldA&&observation.pressedB&&observation.releasedB&&observation.throwDisableFrames==8&&observation.firstFormationSlot->z==6,"actual hardware timers source slot");
 std::vector<actions::PikiFrame> list;check(bridge->squad(a,list,error)&&list.size()==2,"positive SDK squad");check(bridge->findNextThrowPiki(a,error)&&actor.next->actor==&q,"source strict nearest XZ plate order");
 check(bridge->transitionPiki(a,{&q,20},actions::PikiState::GoHang,error)&&transitions==1,"positive exact SDK state");
 check(bridge->positionPiki(a,{&q,20},{2,3,4},error)&&positions==1,"source held positioning");check(bridge->throwPiki(a,{&q,20},{0,0,0},error)&&launches==1,"SDK launch");
 check(bridge->transitionPiki(a,{&q,20},actions::PikiState::Flying,error)&&transitions==1,"already launched Flying never reinitializes");
 check(bridge->freeMember(a,{&p,10},5,{1,2,3},true,error)&&gathers==1,"SDK positive dismissal gather");
 check(bridge->whistleMember(a,{&p,10},false,true,error)&&whistles==1,"SDK positive whistle");
 check(!bridge->whistleMember(a,{&p,10},true,false,error)&&whistles==1,"unsupported combining refuses explicitly");
 actions::PikiFrame result;result.happa=99;check(!bridge->piki(a,{&p,11},result,error)&&result.happa==99,"stale exact lifetime output unchanged");
 frames[0].captain=&b;check(!bridge->transitionPiki(a,{&p,10},actions::PikiState::GoHang,error)&&transitions==1,"foreign party mutation refused");frames[0].captain=&a;
 alive=false;check(!bridge->piki(a,{&p,10},result,error)&&result.happa==99,"actual CF dead refused");alive=true;
 actor.candidates={{{&p,10},nullptr},{{},&b}};check(bridge->callPikis(a,error)&&whistles==2,"ordered source census dispatches Piki and partner");
 check(bridge->control(a,error)&&controls==1,"actual source control called once");check(bridge->moveRotation(a,false,error)&&(a.flags&CF_UsePriorityFaceDir),"physical priority face flag");
 actor.expire=true;observation.face=99;check(!bridge->frame(a,observation,error)&&observation.face==99,"read callback state expiry leaves output unchanged");a.current=&typed;
 check(!bridge->holdFields(a,1,2,3,error),"write callback exact state expiry refuses continuation");actor.expire=false;a.current=&typed;
 sdkExpire=true;check(!bridge->transitionPiki(a,{&p,10},actions::PikiState::GoHang,error),"SDK callback state expiry refuses continuation");sdkExpire=false;a.current=&typed;
 world.value=Phase::Loading;check(bridge->frame(a,observation,error),"physical bootstrap frame in Loading");check(!bridge->control(a,error)&&controls==1,"Loading action mutation refused");world.value=Phase::GameWorldActive;
 lifetimeAvailable=false;check(!bridge->control(a,error)&&controls==1,"missing captain lifetime refuses before control");lifetimeAvailable=true;
 world.catalog="wrong";check(!bridge->startWhistle(a,error),"canonical world catalog mismatch");world.catalog=scene.catalog;
 canonical=nullptr;check(!bridge->control(a,error),"missing canonical scene");canonical=&scene;
 ++scene.epoch;check(!bridge->control(a,error),"same address scene incarnation expired");--scene.epoch;
 std::string original=resource;resource[0]^=1;check(!na::Bridge::create(actor,services,physical,plate,bank,error),"raw selected bytes mutation rejects creation");resource=original;
 check(!bridge->positionPiki(a,{&p,10},{std::numeric_limits<float>::infinity(),0,0},error),"nonfinite physical mutation refuses");
 std::cout<<checks<<" native source action bridge controls PASS (engineering doubles; no gameplay claim)\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
