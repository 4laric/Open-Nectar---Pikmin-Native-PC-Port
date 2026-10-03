// Actual action translation unit with engine doubles: controls, not gameplay.
#include "pc_p2_original_captain_dope.h"
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
Navi navis[2];std::string raw;std::vector<int> keys,boundKeys;unsigned boundAdvances=0,allAdvances=0;bool missingClip=false;unsigned controls=0;unsigned flying=0,throws=0,calls=0,stops=0,automaticUpdates=0;bool provider=true;
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
bool SourceBank::supports(Navi*,Motion,std::string& e)const{return !missingClip||(e="missing authenticated source clip",false);}
bool SourceBank::startMotion(Navi* n,Motion self,Motion bound,Listener sl,Listener bl,std::string& e){assert(self==Motion::GrowUp2&&bound==self&&sl==Listener::SourceActor&&bl==Listener::None);return start(n,self,e);}
bool SourceBank::enableMotionBlend(Navi* n,std::string&){auto& a=m->states[n];a.state[1].motion=Motion::Nigeru;a.state[1].frame=10;++a.state[1].generation;a.listener[1]=Listener::SourceActor;return true;}
}}
namespace {
Creature targets[4];p2originalresource::ResourceState inventory,uninstalled;
struct SourceDope:dope::DopeSource {
 p2originalresource::ResourceState* selectedInventory=&::inventory;
 std::vector<dope::TargetFrame> squadFrames;std::vector<dope::Target> candidates;std::vector<dope::TargetFrame> targetFrames;
 mutable dope::Sphere searched;unsigned smokeCount=0,mapCount=0;std::vector<Creature*> recipients;bool reject=true,changeSquad=false,changeCursor=false,stale=false;
 dope::Spray sprayed=dope::Spray::Spicy;Vec3 smokeOrigin,smokeDirection;
 const LoadedScene& scene()const override{return ::scene;}
 p2originalresource::ResourceState* inventory()const override{return selectedInventory;}
 bool squad(const Navi&,std::vector<dope::TargetFrame>& out,std::string&)const override{out=squadFrames;return true;}
 bool mapSearch(dope::Sphere s,std::vector<dope::Target>& out,std::string&)const override{searched=s;out=candidates;return true;}
 bool target(dope::Target h,dope::TargetFrame& out,std::string&)const override{for(const auto& t:targetFrames)if(t.handle.actor==h.actor){out=t;if(stale)++out.handle.lifetime;return true;}return false;}
 bool stimulate(Navi& n,dope::Target h,dope::Spray type,bool& accepted,std::string&)override{assert(::inventory.sprayCount(type==dope::Spray::Spicy?p2originalresource::HoneyKind::Spicy:p2originalresource::HoneyKind::Bitter)>0);recipients.push_back(h.actor);accepted=!reject;if(changeCursor)source.frames[source.slot(n)].cursor={100,0,0};return true;}
 bool smoke(Navi&,dope::Spray type,Vec3 origin,Vec3 direction,std::string&)override{++smokeCount;sprayed=type;smokeOrigin=origin;smokeDirection=direction;if(changeSquad)squadFrames={targetFrames[1]};return true;}
} dopeSource;
void stock(int spicy,int bitter){p2originalresource::ResourceSnapshot s;s.sprayCounts={spicy,bitter};p2originalresource::EggContents contents;std::string e;assert(inventory.restore(s,contents,e));}
}
dope::DopeSource* pc_p2_original_captain_dope_source(const Navi*){return provider?&dopeSource:nullptr;}
int main(){
 NaviStateMachine fsms[2];std::string e;
 for(unsigned i=0;i<2;++i){navis[i].fsm=&fsms[i];registerDopeState(fsms[i]);fsms[i].registerState(new SinkState(StateId::Walk));source.frames[i].controller=true;source.frames[i].cursor={0,0,100};bank.start(&navis[i],Motion::Wait,e);}
 for(unsigned i=0;i<4;++i)dopeSource.targetFrames.push_back({{&targets[i],i+1},{float(i*20+10),0,0},i<2});
 dopeSource.squadFrames=dopeSource.targetFrames;stock(2,2);
 dopeSource.selectedInventory=&uninstalled;assert(!dope::begin(&navis[0],dope::Spray::Spicy,e));dopeSource.selectedInventory=&inventory;
 assert(!pc_p2_original_captain_dope_preflight(&navis[0],e));assert(!dope::begin(&navis[0],static_cast<dope::Spray>(2),e));
 provider=false;assert(!dope::begin(&navis[0],dope::Spray::Spicy,e));provider=true;missingClip=true;assert(!dope::begin(&navis[0],dope::Spray::Spicy,e));missingClip=false;
 dopeSource.changeSquad=true;assert(dope::begin(&navis[0],dope::Spray::Spicy,e));assert(navis[0].current->invincible(&navis[0]));
 assert(dopeSource.smokeCount==1&&dopeSource.smokeDirection.x==1&&dopeSource.smokeOrigin.x==0);assert(dopeSource.recipients.size()==1&&dopeSource.recipients[0]==&targets[1]);
 assert(inventory.sprayCount(p2originalresource::HoneyKind::Spicy)==1); // receiver rejected but consumes exactly one AFTER receiver
 auto* state=dynamic_cast<NativeState*>(navis[0].current);assert(state);unsigned before=allAdvances;assert(state->sourceAnimationKey(&navis[0],2,e));assert(allAdvances==before&&dopeSource.recipients.size()==1);
 assert(!state->sourceAnimationKey(&navis[0],1000,e)&&e.empty());assert(fsms[0].last==nativeId(StateId::Walk));
 dopeSource.changeSquad=false;dopeSource.squadFrames={dopeSource.targetFrames[2]};unsigned smoke=dopeSource.smokeCount;assert(dope::begin(&navis[0],dope::Spray::Spicy,e));assert(dopeSource.smokeCount==smoke&&inventory.sprayCount(p2originalresource::HoneyKind::Spicy)==1);before=controls;navis[0].current->exec(&navis[0]);assert(controls==before+1&&fsms[0].last==nativeId(StateId::Walk));
 stock(0,0);assert(dope::begin(&navis[0],dope::Spray::Bitter,e));navis[0].current->exec(&navis[0]);assert(dopeSource.smokeCount==smoke&&fsms[0].last==nativeId(StateId::Walk));
 stock(0,2);dopeSource.squadFrames.clear();dopeSource.recipients.clear();dopeSource.targetFrames[0].position={0,0,190};dopeSource.targetFrames[1].position={0,0,190.01f};dopeSource.targetFrames[2].position={0,0,-90};dopeSource.targetFrames[3].position={0,0,50};
 dopeSource.candidates={dopeSource.targetFrames[0].handle,dopeSource.targetFrames[1].handle,dopeSource.targetFrames[2].handle,dopeSource.targetFrames[3].handle};
 assert(dope::begin(&navis[0],dope::Spray::Bitter,e));assert(dopeSource.searched.center.z==70&&dopeSource.searched.radius==140&&dopeSource.smokeDirection.z==1);assert(dopeSource.recipients.size()==3&&dopeSource.recipients[0]==&targets[0]&&dopeSource.recipients[1]==&targets[2]&&dopeSource.recipients[2]==&targets[3]);assert(inventory.sprayCount(p2originalresource::HoneyKind::Bitter)==1);
 keys={1000,2};boundKeys={1000};before=boundAdvances;assert(pc_p2_original_captain_dope_advance_animation(&navis[0],1,e));assert(fsms[0].last==nativeId(StateId::Walk)&&boundAdvances==before);
 // Actual smoke callbacks re-read actor/whistle for each source target.
 dopeSource.recipients.clear();dopeSource.changeCursor=true;dopeSource.candidates={dopeSource.targetFrames[0].handle,dopeSource.targetFrames[1].handle};dopeSource.targetFrames[1].position={190,0,0};source.frames[0].cursor={0,0,100};
 assert(dope::begin(&navis[0],dope::Spray::Bitter,e));assert(dopeSource.recipients.size()==2&&inventory.sprayCount(p2originalresource::HoneyKind::Bitter)==0);dopeSource.changeCursor=false;
 // Shared story inventory is spent by either genuine roster captain.
 stock(1,1);dopeSource.squadFrames={dopeSource.targetFrames[0]};dopeSource.recipients.clear();assert(dope::begin(&navis[1],dope::Spray::Spicy,e));assert(inventory.sprayCount(p2originalresource::HoneyKind::Spicy)==0&&fsms[0].last==nativeId(StateId::Dope));
 stock(0,1);dopeSource.stale=true;source.frames[0].cursor={0,0,100};before=dopeSource.recipients.size();assert(dope::begin(&navis[0],dope::Spray::Bitter,e));assert(dopeSource.recipients.size()==before&&inventory.sprayCount(p2originalresource::HoneyKind::Bitter)==1);assert(!pc_p2_original_captain_dope_advance_animation(&navis[0],1,e));
 p2originalresource::ResourceSnapshot saved;assert(inventory.snapshot(saved,e));assert(saved.sprayCounts[1]==1&&saved.sprayUses[1]==0); // refused stale receiver never consumes
 assert(!pc_p2_original_captain_dope_advance_animation(&navis[1],-1,e));
 ++scene.epoch;assert(!pc_p2_original_captain_dope_advance_animation(&navis[1],1,e));
 std::cout<<"PASS actual source Dope TU and genuine ResourceState controls; no gameplay qualification checks="<<checkCount<<"\n";
}
