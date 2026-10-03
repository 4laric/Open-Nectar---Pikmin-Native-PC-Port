// Actual action translation unit with engine doubles: controls, not gameplay.
#include "pc_p2_original_captain_pluck.h"
#include "pc_p2_equipment.h"
#include <cmath>
#include <array>
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
Navi navis[2];Piki pikis[2];std::string raw;std::vector<int> keys,boundKeys;unsigned boundAdvances=0;unsigned flying=0,throws=0,calls=0,stops=0,automaticUpdates=0;bool provider=true,motionAvailable=true;
struct Scene:LoadedScene,World {
 std::string campaign="source-campaign",fingerprint="selected-source-session",catalog="source-catalog";std::uint64_t epoch=1;
 const std::string& selectedCampaign()const override{return campaign;}const std::string& selectedFingerprint()const override{return fingerprint;}const std::string& sourceCatalog()const override{return catalog;}
 MoviePlayer* moviePlayer()const override{return nullptr;}std::uint64_t incarnation()const override{return epoch;}Navi* captainAt(unsigned i)const override{return i<2?&navis[i]:nullptr;}
 Demo currentDemo=Demo::Absent;Phase phase()const override{return Phase::GameWorldActive;}Demo demo()const override{return currentDemo;}
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
 bool holdFields(Navi&,float,float,float,std::string&)override{return true;}
 bool nextThrowPiki(Navi&,std::optional<PikiHandle>,std::string&)override{return true;}
 bool findNextThrowPiki(Navi&,std::string&)override{return true;}
 bool throwPiki(Navi&,PikiHandle,Vec3,std::string&)override{++throws;return true;}
 bool feedback(Navi&,Feedback,PikiHandle,std::string&)override{return true;}
} source;
struct SinkState:NaviState {explicit SinkState(StateId id):NaviState(nativeId(id)){} };
}
namespace p2original {namespace captain {
struct SourceBank::Impl {struct Actor {std::array<MotionState,2> state;std::array<Listener,2> listener;};std::unordered_map<const Navi*,Actor> states;};
SourceBank::SourceBank():m(new Impl){}SourceBank::~SourceBank()=default;
bool SourceBank::start(Navi* n,Motion motion,std::string&){for(unsigned c=0;c<2;++c){auto& s=m->states[n].state[c];s.motion=motion;++s.generation;m->states[n].listener[c]=c?Listener::None:Listener::SourceActor;}return true;}
bool SourceBank::state(const Navi* n,MotionState& out,std::string& e)const{return stateAnimator(n,Animator::Self,out,e);}
bool SourceBank::stateAnimator(const Navi* n,Animator channel,MotionState& out,std::string&)const{auto it=m->states.find(n);if(it==m->states.end())return false;out=it->second.state[unsigned(channel)];return true;}
bool SourceBank::advanceAnimator(Navi* n,Animator channel,float,const std::function<bool(int)>& emit,std::string&){unsigned c=unsigned(channel);if(c)++boundAdvances;auto generation=m->states[n].state[c].generation;auto pendingKeys=c?boundKeys:keys;(c?boundKeys:keys).clear();for(int key:pendingKeys){bool keep=m->states[n].listener[c]==Listener::None||emit(key);if(!keep||m->states[n].state[c].generation!=generation)break;}return true;}
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


namespace p2original {namespace captain {
bool SourceBank::startMotion(Navi* n,Motion self,Motion bound,Listener sl,Listener bl,std::string& e){assert(self==bound&&bl==Listener::None);assert(sl==(self==Motion::Walk?Listener::None:Listener::SourceActor));start(n,self,e);m->states[n].listener={sl,bl};return true;}
bool SourceBank::enableMotionBlend(Navi* n,std::string&){auto& a=m->states[n];a.state[1].motion=Motion::Nigeru;a.state[1].frame=10;++a.state[1].generation;a.listener[1]=Listener::SourceActor;return true;}

}}
namespace {
Creature heads[2];
struct PluckProvider:pluck::PluckSource {
 pluck::HeadFrame hf[2];pluck::BodyFrame bodies[2];float mass[2]={1,1};bool rotation[2]={true,true},capacity=true;unsigned births=0,kills=0,nukares=0,disable=0,pulling=0,pullout=0,csticks=0;bool already=false,handled=false,notNew=false;
 unsigned slot(const Navi& n)const{return &n==&navis[0]?0:1;}
 const LoadedScene& scene()const override{return ::scene;}
 bool body(const Navi& n,pluck::BodyFrame& f,std::string&)const override{f=bodies[slot(n)];return true;}
 bool head(pluck::HeadHandle h,pluck::HeadFrame& f,std::string&)const override{f=hf[h.actor==&heads[0]?0:1];return f.handle.actor==h.actor&&f.handle.lifetime==h.lifetime;}
 bool setPluckingCounter(Navi& n,std::uint8_t count,std::string&)override{bodies[slot(n)].pluckingCounter=count;return true;}
 bool setMass(Navi& n,float f,std::string&)override{mass[slot(n)]=f;return true;}
 bool setMoveRotation(Navi& n,bool f,std::string&)override{rotation[slot(n)]=f;return true;}
 bool setFace(Navi& n,float f,std::string&)override{source.frames[slot(n)].face=f;return true;}
 bool setVelocities(Navi& n,Vec3 actual,Vec3 target,std::string&)override{assert(actual.x==target.x&&actual.y==target.y&&actual.z==target.z);source.frames[slot(n)].velocity=actual;return true;}
 bool makeCStick(Navi&,bool f,std::string&)override{assert(!f);++csticks;return true;}
 bool markFirstPluck(std::string&)override{return true;}
 bool birthForced(std::optional<PikiHandle>& h,std::string&)override{++births;if(capacity)h=PikiHandle{&pikis[0],1};else h.reset();return true;}
 bool initializeBorn(PikiHandle h,unsigned color,unsigned happa,Vec3 pos,std::string&)override{assert(h.actor==&pikis[0]&&color==1&&happa==2&&pos.z==6);return true;}
 bool killHead(pluck::HeadHandle h,std::string&)override{++kills;hf[h.actor==&heads[0]?0:1].alive=false;return true;}
 bool enterNukare(PikiHandle,Navi&,bool f,std::string&)override{++nukares;already=f;return true;}
 bool feedback(Navi&,pluck::Feedback f,std::string&)override{if(f==pluck::Feedback::Pulling)++pulling;else ++pullout;return true;}
 bool startThrowDisable(Navi&,std::string&)override{++disable;return true;}
 bool actionButton(Navi&,bool& f,std::string&)override{f=handled;return true;}
 bool follow(Navi& n,bool f,std::string& e)override{notNew=f;return pc_p2_original_captain_transit(&n,StateId::Follow,e);}
} pluckProvider;
}
pluck::PluckSource* pc_p2_original_captain_pluck_source(const Navi*){return provider?&pluckProvider:nullptr;}
bool pc_p2_original_captain_motion_preflight(Navi*,unsigned,std::string& e){return motionAvailable||(e="missing actual bank resource",false);}
int main(int argc,char** argv){
 assert(argc==2);std::ifstream in(argv[1],std::ios::binary);raw.assign(std::istreambuf_iterator<char>(in),{});assert(!raw.empty());
 NaviStateMachine fsms[2];std::string e;
 for(unsigned i=0;i<2;++i){navis[i].fsm=&fsms[i];registerPluckStates(fsms[i]);fsms[i].registerState(new SinkState(StateId::Walk));fsms[i].registerState(new SinkState(StateId::Follow));source.frames[i].controller=true;source.frames[i].delta=.02f;pluckProvider.hf[i]={{&heads[i],i+1},{0,0,6},1,2,true};bank.start(&navis[i],Motion::Wait,e);}
 auto adjust=[&](unsigned i=0,bool follower=false){assert(pluck::beginAdjust(&navis[i],pluckProvider.hf[i].handle,follower,e));};
 auto nuku=[&](unsigned i=0,bool follower=false){assert(pluck::beginNuku(&navis[i],follower,e));};
 auto animate=[&](std::vector<int> events){keys=events;assert(pc_p2_original_captain_pluck_advance_animation(&navis[0],1,e));};
 auto motion=[&](unsigned i=0){MotionState s;assert(bank.state(&navis[i],s,e));return s.motion;};
 provider=false;assert(!pc_p2_original_captain_pluck_preflight(&navis[0],StateId::Nuku,e));provider=true;
 auto good=raw;raw[0]^=1;assert(!pc_p2_original_captain_pluck_preflight(&navis[0],StateId::Nuku,e));raw=good;
 motionAvailable=false;assert(!pc_p2_original_captain_pluck_preflight(&navis[0],StateId::NukuAdjust,e));motionAvailable=true;
 assert(!pluck::beginAdjust(&navis[0],{},false,e));
 auto stale=pluckProvider.hf[0].handle;++stale.lifetime;assert(!pluck::beginAdjust(&navis[0],stale,false,e));
 adjust();unsigned previousBound=boundAdvances;boundKeys={2,1000};animate({2});assert(boundAdvances==previousBound+1);assert(pluckProvider.mass[0]==0&&!pluckProvider.rotation[0]);navis[0].current->exec(&navis[0]);assert(pluckProvider.births==1&&pluckProvider.kills==1&&pluckProvider.nukares==1&&!pluckProvider.already);assert(fsms[0].last==nativeId(StateId::Nuku)&&motion()==Motion::Nuku&&pluckProvider.rotation[0]&&navis[0].current->invincible(&navis[0]));
 animate({2});assert(pluckProvider.pullout==0); // exact source p042=0 wraps u16 to 65535
 source.frames[0].heldA=true;navis[0].current->exec(&navis[0]);source.frames[0].heldA=false;navis[0].current->exec(&navis[0]);assert(pluckProvider.bodies[0].pluckingCounter==1);
 navis[0].current->exec(&navis[0]);assert(pluckProvider.bodies[0].pluckingCounter==2); // literal source increments on every released-A exec
 previousBound=boundAdvances;animate({1000});assert(boundAdvances==previousBound);assert(fsms[0].last==nativeId(StateId::Walk)&&pluckProvider.mass[0]==1&&pluckProvider.disable==1&&pluckProvider.bodies[0].pluckingCounter==0);
 pluckProvider.bodies[0].pluckingCounter=1;nuku();assert(motion()==Motion::Nuku3);animate({1000});assert(pluckProvider.bodies[0].pluckingCounter==0);
 nuku(0,true);source.frames[0].heldA=true;navis[0].current->exec(&navis[0]);assert(pluckProvider.bodies[0].pluckingCounter==0);animate({1000});assert(fsms[0].last==nativeId(StateId::Follow)&&pluckProvider.notNew);
 pluckProvider.hf[0].alive=true;source.frames[0].heldA=false;pluckProvider.capacity=false;adjust();navis[0].current->exec(&navis[0]);assert(fsms[0].last==nativeId(StateId::Walk)&&pluckProvider.kills==1);pluckProvider.capacity=true;
 source.frames[0].position={0,0,-20};adjust();navis[0].current->exec(&navis[0]);assert(source.frames[0].velocity.z==100); // actual source approach
 for(unsigned i=0;i<10;++i){assert(pluck::wall(&navis[0],e));}navis[0].current->exec(&navis[0]);assert(fsms[0].last==nativeId(StateId::NukuAdjust));assert(pluck::wall(&navis[0],e));navis[0].current->exec(&navis[0]);assert(fsms[0].last==nativeId(StateId::Walk)&&pluckProvider.mass[0]==1&&pluckProvider.rotation[0]);
 adjust();source.frames[0].heldB=true;navis[0].current->exec(&navis[0]);assert(fsms[0].last==nativeId(StateId::Walk));source.frames[0].heldB=false;
 adjust();assert(pluck::collision(&navis[0],{0,0,-19},true,false,true,e));assert(pluck::collision(&navis[0],{0,0,-19},false,false,true,e));navis[0].current->exec(&navis[0]);assert(std::fabs(source.frames[0].velocity.x)>0); // actual obstacle slide
 nuku(0);nuku(1);scene.currentDemo=Demo::Playing;navis[0].current->exec(&navis[0]);assert(fsms[0].last==nativeId(StateId::Walk)&&fsms[1].last==nativeId(StateId::Nuku));scene.currentDemo=Demo::Absent;
 nuku(0);source.frames[0].heldA=true;navis[0].current->exec(&navis[0]);pluckProvider.bodies[0].pluckingCounter=255;source.frames[0].heldA=false;navis[0].current->exec(&navis[0]);assert(pluckProvider.bodies[0].pluckingCounter==0);
 pluckProvider.handled=true;animate({1000});assert(fsms[0].last==nativeId(StateId::Nuku));pluckProvider.handled=false;animate({1000});assert(fsms[0].last==nativeId(StateId::Walk));
 source.frames[0].position={0,0,-20};pluckProvider.hf[0].alive=true;adjust(0,true);pluckProvider.hf[0].alive=false;navis[0].current->exec(&navis[0]);assert(fsms[0].last==nativeId(StateId::Follow)&&!pluckProvider.notNew);pluckProvider.hf[0].alive=true;
 adjust();++pluckProvider.hf[0].handle.lifetime;unsigned previousBirths=pluckProvider.births;navis[0].current->exec(&navis[0]);assert(pluckProvider.births==previousBirths);assert(!pc_p2_original_captain_pluck_advance_animation(&navis[0],1,e));
 assert(pluck::ignoreAtari(true,false)&&pluck::ignoreAtari(false,true)&&!pluck::ignoreAtari(false,false));
 assert(!pc_p2_original_captain_pluck_advance_animation(&navis[1],-1,e));
 std::cout<<"PASS actual source Nuku/NukuAdjust TU controls; no gameplay qualification\n";
}
