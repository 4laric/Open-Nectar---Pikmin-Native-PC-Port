// Actual action translation unit with engine doubles: controls, not gameplay.
#include "pc_p2_original_captain_container.h"
#include "pc_p2_original_resource_state.h"
#include "pc_p2_equipment.h"
#include <cmath>
#include <array>
#include <cstdlib>
#include "pc_p2_original_captain_states.h"
#include "pc_p2_original_captain_motion.h"
#include <cassert>
#include <fstream>
#include <iterator>
#include <unordered_map>
#include <iostream>
namespace {unsigned checkCount=0;}
#undef assert
#define assert(condition) do {++checkCount;if(!(condition)){std::cerr<<"FAIL line="<<__LINE__<<" check="<<#condition<<"\n";std::abort();}} while(false)
using namespace p2original::captain;
using namespace p2original::captain::actions;
class Piki{};
namespace {
Navi navis[2];Piki pikis[5];std::string raw;std::vector<int> keys,boundKeys;unsigned boundAdvances=0,allAdvances=0;bool missingClip=false;unsigned controls=0;unsigned flying=0,throws=0,calls=0,stops=0,automaticUpdates=0;bool provider=true;
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
 bool control(Navi&,std::string&)override{++controls;return true;}
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
bool SourceBank::advanceAnimator(Navi* n,Animator channel,float,const std::function<bool(int)>& emit,std::string&){unsigned c=unsigned(channel);++allAdvances;if(c)++boundAdvances;auto generation=m->states[n].state[c].generation;auto pendingKeys=c?boundKeys:keys;(c?boundKeys:keys).clear();for(int key:pendingKeys){bool keep=m->states[n].listener[c]==Listener::None||emit(key);if(!keep||m->states[n].state[c].generation!=generation)break;}return true;}
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
bool SourceBank::supports(Navi*,Motion motion,std::string& e)const{assert(motion==Motion::Mizunomi);return !missingClip||(e="missing actual Mizunomi",false);}
bool SourceBank::startMotion(Navi* n,Motion self,Motion bound,Listener sl,Listener bl,std::string& e){assert(self==Motion::Mizunomi&&bound==self&&sl==Listener::SourceActor&&bl==Listener::None);return start(n,self,e);}
bool SourceBank::finish(Navi* n,std::string&){for(auto& s:m->states[n].state)s.finishing=true;return true;}
}}
namespace Screen {class Game2DMgr{};}namespace Game {class CameraMgr{};}
namespace {
Creature onyonBody,honeyBody;Screen::Game2DMgr screenBody;Game::CameraMgr cameraBody;
struct ContainerProvider:items::ContainerSource {
 items::OnyonFrame f{{&onyonBody,88},1,0,0};items::ScreenHandle ui{&screenBody,12};items::ContainerFacts statistics;
 bool uiMissing=false,opens=true,frozen=false,paused=false;items::MenuCheck status=items::MenuCheck::Open;int a=0,b=0;
 items::MenuData menu;items::ShipMenu shipMenu;unsigned boundPad=0,openCalls=0,exits=0,zeroVelocity=0;int exited=0,exitColor=0;std::vector<PikiHandle> entered;std::vector<actions::PikiFrame> squadFrames;
 const LoadedScene& scene()const override{return ::scene;}
 bool screen(items::ScreenHandle& out,std::string&)const override{out=ui;return !uiMissing;}
 bool onyon(items::OnyonHandle h,items::OnyonFrame& out,std::string&)const override{out=f;return h.actor==f.handle.actor&&h.lifetime==f.handle.lifetime;}
 bool facts(const Navi&,items::ContainerFacts& out,std::string&)const override{out=statistics;return true;}
 bool setGamePad(items::ScreenHandle h,Navi&,std::string&)override{assert(h.actor==ui.actor&&h.lifetime==ui.lifetime);++boundPad;return true;}
 bool openOnyon(items::ScreenHandle,const items::MenuData& data,bool& opened,std::string&)override{++openCalls;menu=data;opened=opens;return true;}
 bool openShip(items::ScreenHandle,const items::ShipMenu& data,bool& opened,std::string&)override{++openCalls;shipMenu=data;opened=opens;return true;}
 bool check(items::ScreenHandle,bool,items::MenuCheck& out,std::string&)const override{out=status;return true;}
 bool result(items::ScreenHandle,bool,int& first,int& second,std::string&)const override{first=a;second=b;return true;}
 bool freeze(bool f,std::string&)override{frozen=f;return true;}
 bool moviePause(bool f,std::string&)override{paused=f;return true;}
 bool velocities(Navi&,Vec3 actual,Vec3 target,std::string&)override{assert(actual.x==0&&actual.y==0&&actual.z==0&&target.x==0&&target.y==0&&target.z==0);++zeroVelocity;return true;}
 bool squad(const Navi&,std::vector<actions::PikiFrame>& out,std::string&)const override{out=squadFrames;return true;}
 bool enterPiki(Navi&,PikiHandle p,items::OnyonHandle h,std::string&)override{assert(h.actor==f.handle.actor);entered.push_back(p);squadFrames.clear();return true;} // real brain can change the CPlate after selection
 bool exitPikis(items::OnyonHandle h,int count,int color,std::string&)override{assert(h.actor==f.handle.actor);++exits;exited=count;exitColor=color;return true;}
} container;
struct AbsorbProvider:items::AbsorbSource {
 items::HoneyFrame f{{&honeyBody,42},{10,0,10},items::Honey::Red,true,true,false};items::CameraHandle cameraOwner{&cameraBody,91};
 bool missingCamera=false,reject=true,locked=false;unsigned retained=0,releases=0,drinks=0,nearLow=0,finishes=0,interactions=0,credits=0;int credited=-1;Vec3 actualVelocity;
 const LoadedScene& scene()const override{return ::scene;}
 bool camera(items::CameraHandle& out,std::string&)const override{out=cameraOwner;return !missingCamera;}
 bool honey(items::HoneyHandle h,items::HoneyFrame& out,std::string&)const override{out=f;return h.actor==f.handle.actor&&h.lifetime==f.handle.lifetime;}
 bool retain(items::HoneyHandle h,std::string&)override{assert(h.actor==f.handle.actor);++retained;return true;}
 bool release(items::HoneyHandle,std::string&)override{++releases;return true;}
 bool drinkSound(Navi&,std::string&)override{++drinks;return true;}
 bool turnTo(Navi&,Vec3 p,std::string&)override{assert(p.x==10&&p.z==10);return true;}
 bool lockCamera(items::CameraHandle h,Navi&,bool flag,std::string&)override{assert(h.actor==cameraOwner.actor);locked=flag;return true;}
 bool startNearLow(items::CameraHandle,Navi&,std::string&)override{++nearLow;return true;}
 bool finishCamera(items::CameraHandle,Navi&,std::string&)override{++finishes;return true;}
 bool velocities(Navi&,Vec3 actual,Vec3 target,std::string&)override{assert(target.x==0&&target.y==0&&target.z==0);actualVelocity=actual;return true;}
 bool absorb(Navi&,items::HoneyHandle h,bool& accepted,std::string&)override{assert(h.actor==f.handle.actor);++interactions;accepted=!reject;return true;}
 bool creditSpray(Navi&,items::HoneyHandle h,int type,std::string&)override{assert(h.actor==f.handle.actor);++credits;credited=type;return true;}
} absorb;
}
items::ContainerSource* pc_p2_original_captain_container_source(const Navi*){return provider?&container:nullptr;}
items::AbsorbSource* pc_p2_original_captain_absorb_source(const Navi*){return provider?&absorb:nullptr;}
int main(){
 NaviStateMachine fsms[2];std::string e;
 for(unsigned i=0;i<2;++i){navis[i].fsm=&fsms[i];registerContainerAbsorbStates(fsms[i]);fsms[i].registerState(new SinkState(StateId::Walk));source.frames[i].velocity={3,7,8};bank.start(&navis[i],Motion::Wait,e);}
 container.statistics.stored={4,10,3,2,1};container.statistics.squad={1,3,0,2,2};container.statistics.partyTotal=5;container.statistics.mapCount=75;container.statistics.zikatuCount=5;
 for(unsigned i=0;i<5;++i){actions::PikiFrame f;f.handle={&pikis[i],i+1};f.kind=i==2?2:1;container.squadFrames.push_back(f);}
 auto beginContainer=[&](){assert(items::beginContainer(&navis[0],container.f.handle,e));};
 auto beginAbsorb=[&](unsigned i=0){assert(items::beginAbsorb(&navis[i],absorb.f.handle,e));};
 auto key=[&](int k){auto* state=dynamic_cast<NativeState*>(navis[0].current);assert(state);return state->sourceAnimationKey(&navis[0],k,e);};
 assert(!pc_p2_original_captain_container_absorb_preflight(&navis[0],StateId::Container,e));container.uiMissing=true;assert(!items::beginContainer(&navis[0],container.f.handle,e));container.uiMissing=false;
 beginContainer();auto* containerFlags=dynamic_cast<NativeState*>(navis[0].current);assert(containerFlags&&containerFlags->sourcePressable()&&containerFlags->sourceVsUsableY());assert(container.boundPad==1&&container.frozen&&container.paused&&navis[0].current->invincible(&navis[0]));assert(container.menu.color==1&&container.menu.inOnyon==10&&container.menu.currField==128000&&container.menu.inSquad==3&&container.menu.maxOnField==100&&container.menu.inParty==5&&container.menu.onMap==70&&container.menu.maxPikis==95);
 navis[0].current->exec(&navis[0]);assert(container.zeroVelocity==1);
 container.status=items::MenuCheck::Confirmed;container.a=3;navis[0].current->exec(&navis[0]);assert(container.entered.size()==3&&container.entered[0].actor==&pikis[0]&&container.entered[1].actor==&pikis[1]&&container.entered[2].actor==&pikis[3]);assert(fsms[0].last==nativeId(StateId::Walk)&&!container.frozen&&!container.paused);
 beginContainer();container.a=-2;navis[0].current->exec(&navis[0]);assert(container.exited==2&&container.exitColor==1);
 container.f.type=4;container.f.whitesQueued=7;container.f.purplesQueued=1;container.statistics.whiteOwned=true;container.statistics.purpleOwned=true;container.statistics.debtPaid=true;beginContainer();assert(container.shipMenu.white.inOnyon==0&&container.shipMenu.purple.inOnyon==1&&container.shipMenu.white.color==4&&container.shipMenu.purple.color==3&&container.shipMenu.debtPaid);
 container.a=-3;container.b=-9;navis[0].current->exec(&navis[0]);assert(container.exited==3&&container.exitColor==4); // source white first when both menu values nonzero
 beginContainer();container.a=0;container.b=-4;navis[0].current->exec(&navis[0]);assert(container.exited==4&&container.exitColor==3);
 unsigned priorOpen=container.openCalls;container.statistics.whiteOwned=container.statistics.purpleOwned=false;beginContainer();navis[0].current->exec(&navis[0]);assert(container.openCalls==priorOpen&&fsms[0].last==nativeId(StateId::Walk));
 container.f.type=1;container.opens=false;beginContainer();navis[0].current->exec(&navis[0]);assert(fsms[0].last==nativeId(StateId::Walk)&&!container.frozen);container.opens=true;
 beginContainer();container.status=items::MenuCheck::Cancel;navis[0].current->exec(&navis[0]);assert(fsms[0].last==nativeId(StateId::Walk)&&!container.paused);
 absorb.missingCamera=true;assert(!items::beginAbsorb(&navis[0],absorb.f.handle,e));absorb.missingCamera=false;missingClip=true;assert(!items::beginAbsorb(&navis[0],absorb.f.handle,e));missingClip=false;
 assert(!items::beginAbsorb(&navis[0],{},e));beginAbsorb();auto* absorbFlags=dynamic_cast<NativeState*>(navis[0].current);assert(absorbFlags&&!absorbFlags->sourcePressable()&&!absorbFlags->sourceVsUsableY());assert(absorb.retained==1&&absorb.locked&&absorb.drinks==1&&absorb.nearLow==1&&navis[0].current->invincible(&navis[0]));
 navis[0].current->exec(&navis[0]);assert(absorb.interactions==0&&absorb.actualVelocity.x==0&&absorb.actualVelocity.y==7&&absorb.actualVelocity.z==0);
 unsigned before=allAdvances;assert(key(0));assert(allAdvances==before);navis[0].current->exec(&navis[0]);navis[0].current->exec(&navis[0]);assert(absorb.interactions==1); // rejected source interaction still marks the Navi absorbed
 absorb.f.shrinking=true;assert(key(1));MotionState motion;assert(bank.state(&navis[0],motion,e)&&!motion.finishing);
 absorb.f.alive=false;assert(key(1));assert(bank.state(&navis[0],motion,e)&&motion.finishing);assert(!key(1000)&&e.empty());assert(absorb.credits==1&&absorb.credited==0&&absorb.releases==1&&!absorb.locked&&absorb.finishes==1&&fsms[0].last==nativeId(StateId::Walk));
 absorb.f.alive=true;absorb.f.shrinking=false;absorb.f.type=items::Honey::Black;beginAbsorb();assert(key(0));navis[0].current->exec(&navis[0]);assert(!key(1000)&&e.empty());assert(absorb.credits==2&&absorb.credited==1);
 beginAbsorb();assert(!key(1000)&&e.empty());assert(absorb.credits==2); // no physical absorb invocation => no source increment
 beginAbsorb(0);beginAbsorb(1);assert(key(0));navis[0].current->exec(&navis[0]);auto* second=dynamic_cast<NativeState*>(navis[1].current);assert(second&&second->sourceAnimationKey(&navis[1],0,e));navis[1].current->exec(&navis[1]);unsigned priorCredits=absorb.credits;assert(!key(1000)&&e.empty());assert(second->sourceAnimationKey(&navis[1],1,e));assert(!second->sourceAnimationKey(&navis[1],1000,e)&&e.empty());assert(absorb.credits==priorCredits+2); // source permits both captains, inventory owner dedupes child+captain
 beginAbsorb();auto* old=dynamic_cast<NativeState*>(navis[0].current);assert(old);keys={1000,0};boundKeys={1000};before=boundAdvances;assert(pc_p2_original_captain_container_absorb_advance_animation(&navis[0],1,e));assert(boundAdvances==before&&fsms[0].last==nativeId(StateId::Walk));assert(!old->sourceAnimationKey(&navis[0],0,e));
 beginAbsorb();++absorb.f.handle.lifetime;priorCredits=absorb.credits;navis[0].current->exec(&navis[0]);assert(absorb.credits==priorCredits&&!pc_p2_original_captain_container_absorb_advance_animation(&navis[0],1,e));
 std::cout<<"PASS actual Container/Absorb TU controls; no gameplay qualification checks="<<checkCount<<"\n";
}
