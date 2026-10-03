#include "pc_p2_original_captain_native_control.h"
#include "pc_p2_original_captain_motion.h"
#include "pc_p2_original_captain_throw.h"
#include "Navi.h"
#include "NaviState.h"
#include "Camera.h"
#include <fstream>
#include <iterator>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <limits>
using namespace p2original::captain;
namespace nc=nativecontrol;
// Engineered canonical providers and strong SourceBank method doubles below
// test the ACTUAL native_control.cpp TU. They do not qualify resource rendering,
// real callback delivery, session authority or ordinary gameplay acceptance.
namespace {
Navi a,b;
struct Scene:LoadedScene {
 std::string c="control-campaign",f="control-session",catalog="control-catalog";unsigned epoch=1;
 const std::string& selectedCampaign()const override{return c;}const std::string& selectedFingerprint()const override{return f;}const std::string& sourceCatalog()const override{return catalog;}
 std::uint64_t incarnation()const override{return epoch;}MoviePlayer* moviePlayer()const override{return nullptr;}Navi* captainAt(unsigned i)const override{return i==0?&a:i==1?&b:nullptr;}
} scene;
struct TestWorld:World {
 const std::string& selectedCampaign()const override{return scene.c;}const std::string& selectedFingerprint()const override{return scene.f;}const std::string& sourceCatalog()const override{return scene.catalog;}
 std::uint64_t incarnation()const override{return scene.epoch;}Phase phase()const override{return Phase::GameWorldActive;}Demo demo()const override{return Demo::Inactive;}Navi* captainAt(unsigned i)const override{return scene.captainAt(i);}
} world;
struct Typed:NaviState,State {
 StateId id=StateId::Punch;const NaviState* nativeState()const override{return this;}StateId sourceStateId()const override{return id;}bool sourceAlive(const Navi&)const override{return true;}
 bool sourceInvincible()const override{return false;}std::optional<std::uint8_t> actorInvincibleFrames(const Navi&)const override{return 0;}bool canEnterSourceDead(const Navi&)const override{return false;}void enterSourceDead(Navi&)override{}void sourceDamageFeedback(Navi&)override{}
} typed,alternate;
std::string resource;bool frameExpires=false;
struct Actions:actions::ActionSource {
 const LoadedScene& scene()const override{return ::scene;}const std::string& parameterBytes()const override{return resource;}
 bool frame(const Navi&,actions::ActorFrame& f,std::string&)const override{f={};f.delta=1;if(frameExpires)a.current=&alternate;return true;}
 bool squad(const Navi&,std::vector<actions::PikiFrame>&,std::string&)const override{return false;}
 bool piki(const Navi&,actions::PikiHandle,actions::PikiFrame&,std::string&)const override{return false;}
 bool control(Navi&,std::string&)override{return false;}bool whistle(const Navi&,actions::WhistleFrame&,std::string&)const override{return false;}
 bool startWhistle(Navi&,std::string&)override{return false;}bool stopWhistle(Navi&,std::string&)override{return false;}bool updateWhistle(Navi&,actions::Vec3,bool,std::string&)override{return false;}
 bool callPikis(Navi&,std::string&)override{return false;}bool transitionPiki(Navi&,actions::PikiHandle,actions::PikiState,std::string&)override{return false;}
 bool positionPiki(Navi&,actions::PikiHandle,actions::Vec3,std::string&)override{return false;}bool sortFormation(Navi&,actions::PikiHandle,int,std::string&)override{return false;}
 bool holdFields(Navi&,float,float,float,std::string&)override{return false;}bool nextThrowPiki(Navi&,std::optional<actions::PikiHandle>,std::string&)override{return false;}bool findNextThrowPiki(Navi&,std::string&)override{return false;}
 bool throwPiki(Navi&,actions::PikiHandle,actions::Vec3,std::string&)override{return false;}bool feedback(Navi&,actions::Feedback,actions::PikiHandle,std::string&)override{return false;}
} actionsProvider;
nc::AnimationFrame observation;int commitMode=0,commitCalls=0;bool controlFacts=false;
struct Plan:nc::PreparedEffects {float resultingSceneAnimationTimer()const override{return 7;}bool commit(Navi&,std::string&)override;};
struct Effects:nc::Effects {
 const LoadedScene& scene()const override{return ::scene;}bool facts(const Navi&,nc::ControlFacts& out,std::string&)const override{out={};return controlFacts;}
 bool prepare(const Navi&,const nc::Request&,std::unique_ptr<nc::PreparedEffects>& out,std::string&)const override{out.reset(new Plan);return true;}
 bool animationFrame(const Navi&,nc::AnimationFrame& out,std::string&)const override{out=observation;return true;}
} effects;
SourceBank* bankProvider=nullptr;const nc::Effects* effectsProvider=&effects;
bool Plan::commit(Navi& n,std::string& e){++commitCalls;if(commitMode==1){e="actual postRefresh facts unavailable";return false;}if(commitMode==2)nc::forget(&n);if(commitMode==3){nc::forget(&n);if(!nc::resetAfterBootstrap(&n,e))return false;}if(commitMode==4)a.current=&alternate;if(commitMode==5)effectsProvider=nullptr;if(commitMode==6)++scene.epoch;if(commitMode==7){std::string nested;nc::control(&n,nested);}if(commitMode==8&&!nc::resetCStickSceneAnimationTimer(&n,e))return false;return true;}
MotionState channels[2];Listener listeners[2];int lock=-1;unsigned long long generation=10;
struct Start {Animator channel;Motion motion;bool preserve;Listener listener;float oldFrame;};
std::vector<Start> starts;std::vector<Animator> advances;std::vector<float> amounts;bool sendEvents=false,listenerAvailable=true;
int checks=0;std::string error;
void check(bool value,const std::string& label){++checks;if(!value)throw std::runtime_error("check "+std::to_string(checks)+": "+label);}
void reset(Motion self,Motion bound,int boundLock){
 ++scene.epoch;a.current=&typed;b.current=&typed;typed.id=StateId::Punch;nc::forget(&a);
 channels[0]={self,29,++generation,false,false};channels[1]={bound,11,++generation,false,false};listeners[0]=Listener::SourceActor;listeners[1]=Listener::None;lock=boundLock;
 starts.clear();advances.clear();amounts.clear();sendEvents=false;listenerAvailable=true;effectsProvider=&effects;observation={};observation.deltaTime=1;observation.gameFrozen=false;
 check(nc::resetAfterBootstrap(&a,error),error);
}
bool animate(){return nc::animateWalk(&a,[](int){return true;},error);}
}
const LoadedScene* pc_p2_original_captain_loaded_scene(){return &scene;}const World* pc_p2_original_captain_world(){return &world;}
bool pc_p2_original_captain_actor_alive(const Navi* n){return n==&a||n==&b;}
bool controlledAlive=true,lifetimeKnown=true;
bool pc_p2_original_captain_actor_lifetime(const Navi* n,bool& out){if(!lifetimeKnown||(n!=&a&&n!=&b))return false;out=controlledAlive;return true;}
actions::ActionSource* pc_p2_original_captain_action_source(const Navi*){return &actionsProvider;}
const nc::Effects* pc_p2_original_captain_control_effects(const Navi*){return effectsProvider;}
SourceBank* pc_p2_original_captain_source_bank(){return bankProvider;}
float pc_p2_equipment_speed(float raw){return raw;}
namespace p2original { namespace captain {
namespace bodyphases {unsigned sourceFlags=0;bool setterAvailable=true;bool setMoveRotation(Navi*,bool enable,std::string&){if(!setterAvailable)return false;if(enable)sourceFlags&=~1u;else sourceFlags|=1;return true;}}
struct SourceBank::Impl{};SourceBank::SourceBank():m(new Impl){}SourceBank::~SourceBank()=default;
bool SourceBank::ready()const{return true;}
bool SourceBank::parameters(SourceParameters& out,std::string&)const{out={};out.rawSourceSha=control::parameterSha256();return true;}
bool SourceBank::state(const Navi* n,MotionState& out,std::string& e)const{return stateAnimator(n,Animator::Self,out,e);}
bool SourceBank::stateAnimator(const Navi*,Animator channel,MotionState& out,std::string&)const{out=channels[unsigned(channel)];return true;}
bool SourceBank::listenerAnimator(const Navi*,Animator channel,Listener& out,std::string& e)const{if(!listenerAvailable){e="test listener authority missing";return false;}out=listeners[unsigned(channel)];return true;}
bool SourceBank::boundMotionLock(const Navi*,int& out,std::string&)const{out=lock;return true;}
bool SourceBank::supports(Navi*,Motion,std::string&)const{return true;}
bool SourceBank::startAnimator(Navi*,Animator channel,Motion target,bool preserve,Listener listener,std::string&){auto i=unsigned(channel);starts.push_back({channel,target,preserve,listener,channels[i].frame});channels[i]={target,preserve?channels[i].frame:0,++generation,false,false};listeners[i]=listener;return true;}
bool SourceBank::advanceAnimator(Navi*,Animator channel,float amount,const std::function<bool(int)>& emit,std::string&){auto i=unsigned(channel);advances.push_back(channel);amounts.push_back(amount);channels[i].frame+=amount;if(sendEvents&&listeners[i]!=Listener::None){if(!emit(200))return true;emit(1000);}return true;}
}}
int main(int argc,char** argv){try{
 check(argc==2,"verified private resource argument");std::ifstream file(argv[1],std::ios::binary);resource=std::string((std::istreambuf_iterator<char>(file)),{});control::Params p;check(control::parseParameters(resource,p,error),error);SourceBank bank;bankProvider=&bank;
 reset(Motion::Punch,Motion::Nigeru,64);observation.gameFrozen.reset();check(!animate()&&starts.empty()&&advances.empty(),"missing gameFrozen authority refuses beforeclockmutation");
 observation.gameFrozen=true;observation.displacement={10,0};for(int i=0;i<5;++i)check(animate(),error);
 check(starts.size()==1&&starts[0].channel==Animator::Bound&&starts[0].motion==Motion::Walk&&starts[0].preserve&&starts[0].listener==Listener::SourceActor,"locked SelfPunch survives genuine Bound moving transition");
 check(channels[0].motion==Motion::Punch&&channels[0].frame==29&&channels[1].frame==11&&advances.empty(),"frozen actualobservation suppresses bothclocks and preservesdistinctframes");
 reset(Motion::Walk,Motion::Walk,-1);observation.displacement={40,0};observation.gameFrozen=true;for(int i=0;i<5;++i)check(animate(),error);
 check(starts.size()==2&&starts[0].channel==Animator::Bound&&starts[1].channel==Animator::Self&&starts[0].preserve&&starts[1].preserve,"literal moving transition order Bound thenSelf preserving each");
 check(channels[0].frame==29&&channels[1].frame==11&&listeners[0]==Listener::None&&listeners[1]==Listener::SourceActor,"unlocked genuine clocks retained separately and source listeners assigned");
 observation.gameFrozen=false;sendEvents=true;int events=0;check(nc::animateWalk(&a,[&](int){++events;return true;},error),error);
 check(advances.size()==2&&advances[0]==Animator::Self&&advances[1]==Animator::Bound&&events==2,"explicit Self thenBound advancement emits only current sourceActor listener");check(channels[0].frame==79&&channels[1].frame==61,"common Run source50frames applied separately");
 reset(Motion::Wait,Motion::Wait,-1);observation.gameFrozen=true;observation.displacement={10,0};check(animate(),error);check(animate(),error);
 check(starts.size()==2&&starts[0].channel==Animator::Self&&starts[1].channel==Animator::Bound&&!starts[0].preserve&&!starts[1].preserve&&channels[0].frame==0&&channels[1].frame==0,"WAIT boundary starts unlockedSelf thenBound reset");
 reset(Motion::Punch,Motion::Nigeru,64);observation.gameFrozen=true;observation.displacement={0,0};check(animate(),error);check(animate(),error);check(starts.size()==1&&starts[0].channel==Animator::Bound&&!starts[0].preserve&&starts[0].listener==Listener::None&&channels[0].motion==Motion::Punch,"locked moving-toWait changesonlyBound without listener");
 reset(Motion::Jkoke,Motion::Nigeru,-1);check(!animate()&&starts.empty()&&advances.empty(),"actual SelfJKOKE rejects locomotion assertion path");
 reset(Motion::Damage,Motion::Damage,-1);check(animate()&&starts.empty()&&advances.size()==2&&channels[0].frame==59&&channels[1].frame==41,"unsupported Bound motion advances common default30 acrosssourceDamaged");
 reset(Motion::Damage,Motion::Damage,-1);sendEvents=true;events=0;alternate.id=StateId::Throw;check(nc::animateWalk(&a,[&](int){++events;a.current=&alternate;return true;},error),error);
 check(events==1&&advances.size()==1&&advances[0]==Animator::Self&&channels[1].frame==11,"Selfcallback statechange stopsoldkeys and staleBound advance");
 reset(Motion::Damage,Motion::Damage,-1);sendEvents=true;events=0;check(nc::animateWalk(&a,[&](int){++events;channels[1].generation=++generation;return true;},error),error);
 check(events==1&&advances.size()==1,"Selfcallback sameState boundgeneration reentry stops staleBound");
 reset(Motion::Damage,Motion::Damage,-1);sendEvents=true;events=0;check(nc::animateWalk(&a,[&](int){++events;nc::forget(&a);return true;},error),error);check(events==1&&advances.size()==1,"retired actorcallback stops sourceclockdelivery");
 reset(Motion::Damage,Motion::Damage,-1);sendEvents=true;events=0;check(nc::animateWalk(&a,[&](int){++events;return false;},error),error);check(events==1&&advances.size()==1,"callback stop cancels siblingBound even unchangedgeneration");
 reset(Motion::Damage,Motion::Damage,-1);sendEvents=true;events=0;check(nc::animateWalk(&a,[&](int){++events;effectsProvider=nullptr;return true;},error),error);check(events==1&&advances.size()==1,"canonical observation provider replacement stops staleBound");
 reset(Motion::Wait,Motion::Wait,-1);check(!nc::animateWalk(&a,std::function<bool(int)>{},error)&&starts.empty()&&advances.empty(),"missing actual eventcallback refuses beforestart");
 reset(Motion::Throw,Motion::Nigeru,33);typed.id=StateId::Throw;listeners[0]=Listener::SourceState;listeners[1]=Listener::SourceActor;sendEvents=true;
 check(!animate()&&!error.empty()&&starts.empty()&&advances.empty(),"legacy callback refuses ambiguous SourceState listener before mutation");
 std::vector<std::pair<Animator,Listener>> delivered;check(nc::animateWalk(&a,[&](Animator channel,Listener listener,int){delivered.push_back({channel,listener});return true;},error),error);
 check(delivered.size()==4&&delivered[0]==std::make_pair(Animator::Self,Listener::SourceState)&&delivered[1]==delivered[0]&&delivered[2]==std::make_pair(Animator::Bound,Listener::SourceActor)&&delivered[3]==delivered[2],"Throw Self state keys and Bound actor keys keep distinct listener identity");
 reset(Motion::ThrowWait,Motion::Nigeru,34);typed.id=StateId::ThrowWait;listeners[0]=Listener::SourceState;listenerAvailable=false;check(!nc::animateWalk(&a,[](Animator,Listener,int){return true;},error)&&starts.empty()&&advances.empty(),"missing real listener query refuses before clocks");
 reset(Motion::Walk,Motion::Walk,-1);check(nc::animationSpeed(&a)==30,"genuine body bootstrap retains retail constructor animation rate30");observation.displacement={40,0};
 for(int i=0;i<5;++i)check(animate(),error);
 check(nc::animationSpeed(&a)==50&&amounts.back()==30,"simulation selectingRun50 consumes prior30 in same animation pass");
 check(animate()&&amounts.back()==50,"next actual animation pass consumes previously selectedRun50");
 typed.id=StateId::Walk;check(!nc::resetThrowAnimationSpeed(&a,error)&&nc::animationSpeed(&a)==50,"rate reset refuses unrelated typed state without altering rate");typed.id=StateId::Throw;check(nc::resetThrowAnimationSpeed(&a,error)&&nc::animationSpeed(&a)==30,"genuine Throw init event resets next animation rate30");
 typed.id=StateId::ThrowWait;check(nc::resetThrowAnimationSpeed(&a,error)&&nc::animationSpeed(&a)==30,"genuine held ThrowWait init event resets animation rate30");
 ++scene.epoch;check(!nc::animationSpeed(&a)&&!nc::resetThrowAnimationSpeed(&a,error),"stale body incarnation cannot observe or reset rate");
 reset(Motion::Walk,Motion::Walk,-1);observation.displacement={std::numeric_limits<float>::quiet_NaN(),0};
 check(nc::advanceAnimation(&a,[](Animator,Listener,int){return true;},error)&&starts.empty()&&advances.size()==2&&amounts[0]==30&&amounts[1]==30&&nc::animationSpeed(&a)==30,"independent prephysics clock phase consumes prior rate without sampling postmove displacement or selector");
 const float phaseSelf=channels[0].frame,phaseBound=channels[1].frame;const auto phaseAdvances=advances.size();observation.displacement={40,0};
 for(int i=0;i<5;++i)check(nc::selectWalkAnimation(&a,error),error);
 check(advances.size()==phaseAdvances&&channels[0].frame==phaseSelf&&channels[1].frame==phaseBound&&nc::animationSpeed(&a)==50,"independent postsimulation selector preserves both clocks while choosing next rate");
 const auto phaseStarts=starts.size();check(nc::advanceAnimation(&a,[](Animator,Listener,int){return true;},error)&&starts.size()==phaseStarts&&amounts.back()==50&&nc::animationSpeed(&a)==50,"next prephysics phase uses selected rate and never selects additional motion");
 reset(Motion::Throw,Motion::Nigeru,33);typed.id=StateId::Throw;listeners[0]=Listener::SourceState;sendEvents=true;events=0;
 check(nc::advanceAnimation(&a,[&](Animator channel,Listener listener,int){check(channel==Animator::Self&&listener==Listener::SourceState,"independent clock key carries actual state listener");++events;a.current=&alternate;return true;},error)&&events==1&&advances.size()==1&&starts.empty(),"independent clock statechange stops stale Bound without selector effects");
 reset(Motion::Walk,Motion::Walk,-1);observation.gameFrozen.reset();check(!nc::selectWalkAnimation(&a,error)&&starts.empty()&&advances.empty(),"independent selector requires actual frozen authority");check(!nc::advanceAnimation(&a,[](Animator,Listener,int){return true;},error)&&starts.empty()&&advances.empty(),"independent clock requires actual frozen authority");
 reset(Motion::Wait,Motion::Wait,-1);resource[0]^=1;check(!animate()&&starts.empty()&&advances.empty(),"mutated actual parameter resource refuses beforeclock");resource[0]^=1;
 reset(Motion::Wait,Motion::Wait,-1);NaviState p1;a.current=&p1;check(!animate()&&starts.empty()&&advances.empty(),"common selector neverfalls back from untyped P1state");
 reset(Motion::Wait,Motion::Wait,-1);effectsProvider=nullptr;check(!animate()&&starts.empty()&&advances.empty(),"missing actual observation provider refuses");
 Camera camera;a.camera=&camera;controlFacts=true;
 reset(Motion::Wait,Motion::Wait,-1);observation.displacementKnown=false;
 check(nc::advanceAnimation(&a,[](Animator,Listener,int){return true;},error)&&advances.size()==2,"clock phase does not read undefined previous position");
 check(!nc::selectWalkAnimation(&a,error),"selector refuses undefined displacement");
 check(!nc::resetCStickSceneAnimationTimer(&a,error),"C-stick timer write outside real effects transaction refuses");
 reset(Motion::Wait,Motion::Wait,-1);commitMode=8;
 check(nc::control(&a,error)&&nc::sceneAnimationTimer(&a)==7,"actual control child resets clock before eventual resulting timer publication");
 reset(Motion::Wait,Motion::Wait,-1);commitMode=7;
 check(!nc::control(&a,error)&&nc::sceneAnimationTimer(&a)==0,"nested control refusal invalidates outer publication");
 reset(Motion::Damage,Motion::Damage,-1);controlledAlive=false;
 check(animate()&&advances.size()==2,"known CF-dead open body retains common animation clocks");
 check(!nc::control(&a,error),"CF-dead body refuses living control");
 lifetimeKnown=false;check(!animate(),"unknown lifetime refuses common clocks");lifetimeKnown=true;controlledAlive=true;
 reset(Motion::Wait,Motion::Wait,-1);commitMode=0;bodyphases::setterAvailable=false;const auto missingOwnerCommits=commitCalls;
 check(!nc::control(&a,error)&&commitCalls==missingOwnerCommits&&nc::sceneAnimationTimer(&a)==0,"missing genuine FakePiki flag owner refuses before effects");bodyphases::setterAvailable=true;
 reset(Motion::Wait,Motion::Wait,-1);commitMode=0;commitCalls=0;check(nc::control(&a,error)&&commitCalls==1&&nc::sceneAnimationTimer(&a)==7,"completed actual effects publishes timer");
 reset(Motion::Wait,Motion::Wait,-1);commitMode=1;check(!nc::control(&a,error)&&nc::sceneAnimationTimer(&a)==0,"postRefresh refusal never publishes proposed timer");
 reset(Motion::Wait,Motion::Wait,-1);commitMode=2;check(!nc::control(&a,error)&&!nc::sceneAnimationTimer(&a),"commit callback retirement leaves forgotten actor untouched");
 reset(Motion::Wait,Motion::Wait,-1);commitMode=3;check(!nc::control(&a,error)&&nc::sceneAnimationTimer(&a)==0,"same-slot same-incarnation rebootstrap cannot receive old control timer");
 reset(Motion::Wait,Motion::Wait,-1);commitMode=4;check(!nc::control(&a,error)&&nc::sceneAnimationTimer(&a)==0,"commit state replacement prevents stale timer publication");
 reset(Motion::Wait,Motion::Wait,-1);commitMode=5;check(!nc::control(&a,error)&&nc::sceneAnimationTimer(&a)==0,"commit effects provider replacement prevents stale timer publication");
 reset(Motion::Wait,Motion::Wait,-1);commitMode=6;check(!nc::control(&a,error)&&!nc::sceneAnimationTimer(&a),"commit incarnation replacement prevents stale timer publication");
 reset(Motion::Wait,Motion::Wait,-1);commitMode=0;frameExpires=true;const auto previousCommits=commitCalls;check(!nc::control(&a,error)&&commitCalls==previousCommits&&nc::sceneAnimationTimer(&a)==0,"frame callback state expiry refuses before effects commit");frameExpires=false;
 // No SourceBank::advance definition is linked. Using state-style advancement
 // would fail the standalone link rather than silently pass an empty counter.
 std::cout<<"P2_ORIGINAL_NATIVE_DUAL_ANIMATOR_ACTUAL_TU_CONTROLS_PASS checks="<<checks<<" gameplay=UNTESTED providers=DOUBLES\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
