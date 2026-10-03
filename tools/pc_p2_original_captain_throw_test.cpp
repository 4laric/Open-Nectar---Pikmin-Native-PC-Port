// Actual action translation unit with engine doubles: controls, not gameplay.
#include "pc_p2_original_captain_throw.h"
#include "pc_p2_original_captain_states.h"
#include "pc_p2_original_captain_motion.h"
#include <cassert>
#include <fstream>
#include <iterator>
#include <unordered_map>
#include <iostream>
using namespace p2original::captain;
using namespace p2original::captain::actions;
class Piki{};
namespace {
Navi navis[2];Piki pikis[2];std::string raw;std::vector<int> keys;unsigned flying=0,throws=0,calls=0,stops=0,automaticUpdates=0;bool provider=true;
struct Scene:LoadedScene,World {
 std::string campaign="source-campaign",fingerprint="selected-source-session",catalog="source-catalog";std::uint64_t epoch=1;
 const std::string& selectedCampaign()const override{return campaign;}const std::string& selectedFingerprint()const override{return fingerprint;}const std::string& sourceCatalog()const override{return catalog;}
 MoviePlayer* moviePlayer()const override{return nullptr;}std::uint64_t incarnation()const override{return epoch;}Navi* captainAt(unsigned i)const override{return i<2?&navis[i]:nullptr;}
 Phase phase()const override{return Phase::GameWorldActive;}Demo demo()const override{return Demo::Absent;}
} scene;
struct Source:ActionSource {
 ActorFrame frames[2];PikiFrame ps[2];WhistleFrame whistles[2];float holdTimes[2]={};
 unsigned slot(const Navi& n)const{return &n==&navis[0]?0:1;}
 const LoadedScene& scene()const override{return ::scene;}
 const std::string& parameterBytes()const override{return raw;}
 bool frame(const Navi& n,ActorFrame& a,std::string&)const override{a=frames[slot(n)];return true;}
 bool squad(const Navi& n,std::vector<PikiFrame>& p,std::string&)const override{p={ps[slot(n)]};return true;}
 bool piki(const Navi& n,PikiHandle h,PikiFrame& p,std::string&)const override{p=ps[slot(n)];return p.handle.actor==h.actor&&p.handle.lifetime==h.lifetime;}
 bool control(Navi&,std::string&)override{return true;}
 bool whistle(const Navi& n,WhistleFrame& w,std::string&)const override{w=whistles[slot(n)];return true;}
 bool startWhistle(Navi&,std::string&)override{return true;}
 bool stopWhistle(Navi&,std::string&)override{++stops;return true;}
 bool updateWhistle(Navi&,Vec3 pos,bool automatic,std::string&)override{assert(automatic&&pos.x==0&&pos.y==0&&pos.z==0);++automaticUpdates;return true;}
 bool callPikis(Navi&,std::string&)override{++calls;return true;}
 bool transitionPiki(Navi& n,PikiHandle,PikiState state,std::string&)override{ps[slot(n)].state=state;if(state==PikiState::Flying)++flying;return true;}
 bool positionPiki(Navi& n,PikiHandle,Vec3 pos,std::string&)override{ps[slot(n)].position=pos;return true;}
 bool sortFormation(Navi&,PikiHandle,int,std::string&)override{return true;}
 bool holdFields(Navi& n,float timer,float distance,float height,std::string&)override{holdTimes[slot(n)]=timer;assert(distance==130&&height==72.5f);return true;}
 bool nextThrowPiki(Navi&,std::optional<PikiHandle>,std::string&)override{return true;}
 bool findNextThrowPiki(Navi&,std::string&)override{return true;}
 bool throwPiki(Navi&,PikiHandle,Vec3,std::string&)override{++throws;return true;}
 bool feedback(Navi&,Feedback,PikiHandle,std::string&)override{return true;}
} source;
struct SinkState:NaviState {explicit SinkState(StateId id):NaviState(nativeId(id)){} };
}
namespace p2original {namespace captain {
struct SourceBank::Impl {std::unordered_map<const Navi*,MotionState> states;};
SourceBank::SourceBank():m(new Impl){}SourceBank::~SourceBank()=default;
bool SourceBank::start(Navi* n,Motion motion,std::string&){auto& s=m->states[n];s.motion=motion;++s.generation;return true;}
bool SourceBank::state(const Navi* n,MotionState& state,std::string&)const{auto it=m->states.find(n);if(it==m->states.end())return false;state=it->second;return true;}
bool SourceBank::advance(Navi* n,float,const std::function<bool(int)>& emit,std::string&){auto generation=m->states[n].generation;auto pendingKeys=keys;keys.clear();for(int key:pendingKeys){if(!emit(key)||m->states[n].generation!=generation)break;}return true;}
bool NativeState::sourceAlive(const Navi&)const{return true;}std::optional<std::uint8_t> NativeState::actorInvincibleFrames(const Navi&)const{return 0;}
bool NativeState::canEnterSourceDead(const Navi&)const{return true;}void NativeState::enterSourceDead(Navi&){}void NativeState::sourceDamageFeedback(Navi&){}
bool NativeState::canEnterSourceDamaged(const Navi&)const{return true;}void NativeState::enterSourceDamaged(Navi&,float){}
namespace control {const char* parameterSha256(){return "dfcc8e0cf89195f06ea78fdc1a342631da4e5d2495d85380212af7d72eb2eba0";}}
}}
SourceBank bank;
ActionSource* pc_p2_original_captain_action_source(const Navi*){return provider?&source:nullptr;}
const LoadedScene* pc_p2_original_captain_loaded_scene(){return &scene;}
const World* pc_p2_original_captain_world(){return &scene;}
SourceBank* pc_p2_original_captain_source_bank(){return &bank;}
bool pc_p2_original_captain_actor_alive(const Navi*){return true;}
bool pc_p2_original_captain_transit(Navi* n,StateId id,std::string& e){return n->fsm->transit(n,nativeId(id))||(e="unregistered source state",false);}
int main(int argc,char** argv){
 assert(argc==2);std::ifstream in(argv[1],std::ios::binary);raw.assign(std::istreambuf_iterator<char>(in),{});
 NaviStateMachine fsms[2];std::string e;
 for(unsigned i=0;i<2;++i){navis[i].fsm=&fsms[i];registerThrowStates(fsms[i]);fsms[i].registerState(new SinkState(StateId::Walk));fsms[i].registerState(new SinkState(StateId::Punch));
  source.frames[i].controller=true;source.frames[i].heldA=true;source.frames[i].hand={0,6,0};source.frames[i].delta=.02f;source.frames[i].throwDisableFrames=0;
  source.ps[i].handle={&pikis[i],i+1};source.ps[i].kind=1;source.ps[i].state=PikiState::Walk;source.ps[i].throwable=true;bank.start(&navis[i],Motion::Wait,e);
 }
 provider=false;assert(!pc_p2_original_captain_throw_preflight(&navis[0],StateId::Gather,e));provider=true;
 auto good=raw;raw[0]^=1;assert(!pc_p2_original_captain_throw_preflight(&navis[0],StateId::ThrowWait,e));raw=good;
 assert(!pc_p2_original_captain_throw_preflight(&navis[0],StateId::Throw,e));
 assert(pc_p2_original_captain_transit(&navis[0],StateId::Gather,e));navis[0].current->exec(&navis[0]);assert(calls==1);
 source.frames[0].releasedB=true;navis[0].current->exec(&navis[0]);assert(stops==1&&fsms[0].last==nativeId(StateId::Walk));source.frames[0].releasedB=false;
 for(unsigned i=0;i<2;++i){assert(pc_p2_original_captain_transit(&navis[i],StateId::ThrowWait,e));assert(source.ps[i].state==PikiState::Hanged);assert(pc_p2_original_captain_throw_after_animation(&navis[i],e));assert(source.ps[i].position.y==0);}
 keys={1,1,1,1};assert(pc_p2_original_captain_throw_advance_animation(&navis[0],30,e));
 source.frames[0].heldA=false;navis[0].current->exec(&navis[0]);assert(fsms[0].last==nativeId(StateId::Throw)&&source.ps[1].state==PikiState::Hanged);
 assert(source.holdTimes[0]==2.5f); // loop charge caps at three
 assert(!pc_p2_original_captain_throw_preflight(&navis[1],StateId::Throw,e));
 keys={2};assert(pc_p2_original_captain_throw_advance_animation(&navis[0],1,e));assert(throws==1&&flying==1&&source.ps[0].state==PikiState::Flying);
 keys={1000,2};assert(pc_p2_original_captain_throw_advance_animation(&navis[0],1,e));assert(fsms[0].last==nativeId(StateId::Walk)&&throws==1);
 // Stale lifetime is refused: actual provider never substitutes another Piki.
 ++source.ps[1].handle.lifetime;auto pos=source.ps[1].position;
 assert(!pc_p2_original_captain_throw_after_animation(&navis[1],e));assert(source.ps[1].position.y==pos.y);
 // Actual source GoHang approach, strict timeout, and nearest-hand boundary.
 source.ps[1].position={-50,0,0};source.ps[1].state=PikiState::Walk;
 assert(pc_p2_original_captain_transit(&navis[1],StateId::ThrowWait,e));assert(source.ps[1].state==PikiState::GoHang);
 source.frames[1].delta=3;navis[1].current->exec(&navis[1]);assert(fsms[1].last==nativeId(StateId::ThrowWait));
 source.frames[1].delta=.01f;navis[1].current->exec(&navis[1]);assert(fsms[1].last==nativeId(StateId::Walk));
 source.ps[1].state=PikiState::Walk;source.frames[1].delta=.01f;
 assert(pc_p2_original_captain_transit(&navis[1],StateId::ThrowWait,e));
 source.ps[1].position={0,0,0};navis[1].current->exec(&navis[1]);assert(source.ps[1].state==PikiState::Hanged);
 navis[1].current->resume(&navis[1]);assert(fsms[1].last==nativeId(StateId::ThrowWait));assert(!pc_p2_original_captain_throw_after_animation(&navis[1],e));
 navis[1].current->restart(&navis[1]);assert(fsms[1].last==nativeId(StateId::Walk));
 assert(pc_p2_original_captain_transit(&navis[0],StateId::Gather,e));source.whistles[0].timedOut=true;navis[0].current->exec(&navis[0]);assert(fsms[0].last==nativeId(StateId::Walk));
 source.frames[0].controller=false;source.whistles[0].timedOut=false;
 assert(pc_p2_original_captain_begin_gather(&navis[0],GatherMode::Automatic,e));navis[0].current->exec(&navis[0]);assert(automaticUpdates==1&&fsms[0].last==nativeId(StateId::Gather));
 source.whistles[0].timedOut=true;navis[0].current->exec(&navis[0]);assert(automaticUpdates==2&&fsms[0].last==nativeId(StateId::Walk));
 assert(!pc_p2_original_captain_throw_advance_animation(&navis[0],-1,e));
 std::cout<<"PASS actual source action TU controls; no gameplay qualification\n";
}
