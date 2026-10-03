// Actual action translation unit with engine doubles: controls, not gameplay.
#include "pc_p2_original_captain_punch.h"
#include "pc_p2_equipment.h"
#include <cmath>
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
Navi navis[2];std::string raw;std::vector<int> keys;unsigned advances=0;unsigned flying=0,throws=0,calls=0,stops=0,automaticUpdates=0;bool provider=true,knuckles=true,motionAvailable=true;unsigned assists=0;
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
 bool holdFields(Navi&,float,float,float,std::string&)override{return true;}
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
bool SourceBank::advance(Navi* n,float,const std::function<bool(int)>& emit,std::string&){++advances;auto generation=m->states[n].generation;auto pendingKeys=keys;keys.clear();for(int key:pendingKeys){if(!emit(key)||m->states[n].generation!=generation)break;}return true;}
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

class CollPart{};
namespace {
Creature enemy;CollPart collpart;
struct PunchProvider:punch::PunchSource {
 punch::RedPikminFlag flag=punch::RedPikminFlag::Met;
 punch::TargetFrame enemyFrame{{&enemy,1},5,true,false,true};
 punch::PartFrame partFrame{{{&enemy,1},&collpart,5},{0,0,30}};
 mutable punch::Sphere sphere;bool accepted=true,stale=false;unsigned attacks=0,hits=0,swings=0;float damage=0;Vec3 effect;
 const LoadedScene& scene()const override{return ::scene;}
 bool redPikminFlag(punch::RedPikminFlag& f,std::string&)const override{f=flag;return true;}
 bool query(const Navi& n,punch::Sphere s,std::vector<punch::TargetFrame>& out,std::string&)const override{
  sphere=s;out={{{const_cast<Navi*>(&n),7},7,true,true,false},{{&enemy,1},0,true,false,true},{{&enemy,1},5,false,false,true},enemyFrame};return true;
 }
 bool target(punch::TargetHandle,punch::TargetFrame& out,std::string&)const override{out=enemyFrame;if(stale)++out.handle.lifetime;return true;}
 bool collision(punch::TargetHandle,punch::Sphere,std::vector<punch::PartFrame>& out,std::string&)const override{out={{{{&enemy,1},nullptr,5},{}},partFrame};return true;}
 bool part(punch::PartHandle,punch::PartFrame& out,std::string&)const override{out=partFrame;return true;}
 bool attack(Navi&,punch::PartHandle,float value,bool& result,std::string&)override{++attacks;damage=value;result=accepted;partFrame.position={30,0,0};return true;}
 bool feedback(Navi&,punch::Feedback value,Vec3 pos,std::string&)override{if(value==punch::Feedback::Swing)++swings;else{++hits;effect=pos;}return true;}
 bool enableMotionBlend(Navi&,std::string&)override{return true;}
} punchProvider;
}
punch::PunchSource* pc_p2_original_captain_punch_source(const Navi*){return provider?&punchProvider:nullptr;}
bool pc_p2_original_captain_motion_preflight(Navi*,unsigned,std::string& e){return motionAvailable||(e="missing actual bank resource",false);}
bool pc_p2_equipment_has(p2equipment::Item item){return item==p2equipment::BruteKnuckles&&knuckles;}
namespace p2original {namespace captain {namespace party {
bool assistPunch(Navi* n,EnemyHandle target){assert(n==&navis[1]&&target.actor==&enemy&&target.lifetime==1);++assists;return true;}
}}}
int main(){
 NaviStateMachine fsms[2];std::string e;
 for(unsigned i=0;i<2;++i){navis[i].fsm=&fsms[i];registerPunchState(fsms[i]);fsms[i].registerState(new SinkState(StateId::Walk));fsms[i].registerState(new SinkState(StateId::Follow));source.frames[i].controller=true;bank.start(&navis[i],Motion::Wait,e);}
 auto enter=[&](unsigned i=0,bool following=false,StateId next=StateId::Walk){assert(punch::begin(&navis[i],following,next,e));};
 auto animate=[&](std::vector<int> events){keys=events;assert(pc_p2_original_captain_punch_advance_animation(&navis[0],1,e));};
 auto motion=[&](unsigned i=0){MotionState s;assert(bank.state(&navis[i],s,e));return s.motion;};
 provider=false;assert(!pc_p2_original_captain_punch_preflight(&navis[0],e));provider=true;
 punchProvider.flag=punch::RedPikminFlag::Unknown;assert(!pc_p2_original_captain_punch_preflight(&navis[0],e));
 punchProvider.flag=punch::RedPikminFlag::Met;motionAvailable=false;assert(!pc_p2_original_captain_punch_preflight(&navis[0],e));motionAvailable=true;
 assert(!punch::begin(&navis[0],false,StateId::Dead,e));
 enter();assert(motion()==Motion::Punch);assert(!navis[0].current->invincible(&navis[0]));
 punchProvider.flag=punch::RedPikminFlag::NotMet;auto* punchState=dynamic_cast<NativeState*>(navis[0].current);assert(punchState);unsigned before=advances;
 assert(punchState->sourceAnimationKey(&navis[0],2,e)&&e.empty());assert(advances==before&&punchProvider.attacks==0);
 punchProvider.flag=punch::RedPikminFlag::Met;punchProvider.accepted=false;animate({2});assert(punchProvider.attacks==1&&punchProvider.hits==0&&assists==0);
 punchProvider.accepted=true;animate({2});assert(punchProvider.attacks==2&&punchProvider.damage==7.5f&&punchProvider.hits==1&&assists==1);
 assert(punchProvider.sphere.center.x==0&&punchProvider.sphere.center.y==20&&punchProvider.sphere.center.z==15&&punchProvider.sphere.radius==20);
 assert(punchProvider.effect.x==15&&punchProvider.effect.z==0); // post-receiver part, not cached pre-hit position
 source.frames[0].pressedA=true;navis[0].current->exec(&navis[0]);animate({1000,2});assert(motion()==Motion::Punch2&&punchProvider.attacks==2); // old-generation key is stopped
 navis[0].current->exec(&navis[0]);animate({1000});assert(motion()==Motion::Punch3);
 animate({2});assert(punchProvider.damage==18.75f&&punchProvider.sphere.center.y==35&&punchProvider.sphere.center.z==25&&punchProvider.sphere.radius==35);
 navis[0].current->exec(&navis[0]);animate({1000});assert(motion()==Motion::Wait);navis[0].current->exec(&navis[0]);assert(fsms[0].last==nativeId(StateId::Walk)); // source idle4 does not invent delay after WAIT
 enter();knuckles=false;navis[0].current->exec(&navis[0]);animate({1000});assert(motion()==Motion::Wait);navis[0].current->exec(&navis[0]);assert(fsms[0].last==nativeId(StateId::Walk));knuckles=true;
 unsigned previousAssists=assists;enter(0,true,StateId::Follow);navis[0].current->exec(&navis[0]);animate({2,1000});assert(assists==previousAssists&&motion()==Motion::Wait);navis[0].current->exec(&navis[0]);assert(fsms[0].last==nativeId(StateId::Follow));
 source.frames[0].pressedA=false;enter(0);enter(1);assert(motion(0)==Motion::Punch&&motion(1)==Motion::Punch);
 punchProvider.stale=true;unsigned previousAttacks=punchProvider.attacks;animate({2});assert(punchProvider.attacks==previousAttacks);assert(!pc_p2_original_captain_punch_advance_animation(&navis[0],1,e));punchProvider.stale=false;
 enter();++scene.epoch;assert(!pc_p2_original_captain_punch_advance_animation(&navis[0],1,e));--scene.epoch;
 assert(!pc_p2_original_captain_punch_advance_animation(&navis[1],-1,e));
 source.frames[0].face=1.57079632679f;punch::Sphere s;assert(punch::hitSphere(source.frames[0],false,s,e));assert(std::fabs(s.center.x-15)<.001f);
 Vec3 effect;assert(punch::effectPosition({0,0,0},{0,0,0},effect,e)&&effect.x==0);
 std::cout<<"PASS actual source Punch TU controls; no gameplay qualification\n";
}

