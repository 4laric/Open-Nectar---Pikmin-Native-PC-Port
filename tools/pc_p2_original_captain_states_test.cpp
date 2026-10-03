#include "pc_p2_original_captain_states.h"
#include "pc_p2_original_captain_motion.h"
#include "pc_p2_original_captain_native_control.h"
#include "pc_p2_original_captain_down.h"
#include <fstream>
#include <iterator>
#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <map>
using namespace p2original::captain;
// Strong canonical scene/world/bank/environment/control/lifetime providers
// below are engineered controls. This executable links the ACTUAL coreStates
// TU and policies; no provider here proves ordinary source gameplay authority.
namespace {
Navi a,b,outsider;
struct Scene:LoadedScene {
 std::string campaign="control-campaign",fingerprint="control-session",catalog="control-catalog";
 unsigned epoch=1;Navi* actors[2]={&a,&b};
 const std::string& selectedCampaign()const override{return campaign;}
 const std::string& selectedFingerprint()const override{return fingerprint;}
 const std::string& sourceCatalog()const override{return catalog;}
 std::uint64_t incarnation()const override{return epoch;}
 MoviePlayer* moviePlayer()const override{return nullptr;}
 Navi* captainAt(unsigned s)const override{return s<2?actors[s]:nullptr;}
} scene,wrongScene;
struct TestWorld:World {
 std::string campaign=scene.campaign,fingerprint=scene.fingerprint,catalog=scene.catalog;
 unsigned epoch=1;Phase game=Phase::GameWorldActive;Navi* actors[2]={&a,&b};
 const std::string& selectedCampaign()const override{return campaign;}
 const std::string& selectedFingerprint()const override{return fingerprint;}
 const std::string& sourceCatalog()const override{return catalog;}
 std::uint64_t incarnation()const override{return epoch;}
 Phase phase()const override{return game;}
 Demo demo()const override{return Demo::Inactive;}
 Navi* captainAt(unsigned s)const override{return s<2?actors[s]:nullptr;}
} world;
std::vector<std::string> order;int transitions=0,controls=0,downBegins=0,deadEvents=0,cleanupEvents=0,findQueries=0,actionRequests=0,dismissRequests=0;
const LoadedScene* loaded=&scene;const World* active=&world;SourceBank* bankProvider=nullptr;WalkEnvironment* envProvider=nullptr;
bool alive[2]={true,true},framesAvailable=true,timerAvailable=true,controlAvailable=true,downAvailable=true,bankReady=true,bankBound=true,partyAvailable=true;
std::uint8_t frames[2]={0,0};std::string resourceBytes;std::map<const Navi*,MotionState> motions,boundMotions;
std::map<const Navi*,int> blendLocks;std::map<const Navi*,Listener> boundListeners;
bool damageSupported=true,nigeruSupported=true,emitEnd=false;unsigned long long motionGeneration=0;int bankAdvances=0;
walk::Frame observed;
int slot(const Navi* n){return n==&a?0:n==&b?1:-1;}
walk::Frame ordinary(){walk::Frame f;f.actor=walk::Actor{};f.actor->alive=true;f.actor->hasController=true;f.world=walk::World{};f.world->demoInactive=true;f.buttons=walk::Buttons{};f.stickCount=0;f.onionQueryComplete=true;f.deltaTime=.1f;f.cellCandidates=std::vector<walk::Candidate>{};return f;}
struct Environment:WalkEnvironment {
 const LoadedScene* identity=&::scene;bool captureAvailable=true,preflightAvailable=true,executionAvailable=true;
 bool actionHandled=false,throwable=true,dismissReleased=false;
 const LoadedScene& scene()const override{return *identity;}
 bool capture(const Navi& n,walk::Frame& out,std::string& e)const override{if(!captureAvailable){e="test capture absent";return false;}out=observed;if(out.actor)out.actor->alive=slot(&n)>=0&&alive[slot(&n)];return true;}
 bool preflight(const Navi&,const std::vector<walk::Command>&,std::string& e)const override{if(!preflightAvailable)e="test commands refused";return preflightAvailable;}
 bool execute(Navi&,const walk::Command& c,std::string& e)override{if(!executionAvailable){e="test execute refused";return false;}order.push_back("execute"+std::to_string(int(c.kind)));if(c.kind==walk::Kind::FindNextThrowPiki)++findQueries;if(c.kind==walk::Kind::RequestActionButton)++actionRequests;if(c.kind==walk::Kind::RequestDismiss)++dismissRequests;return true;}
 bool actionButton(Navi&,bool& handled,std::optional<bool>& canThrow,std::string&)override{order.push_back("action-callback");handled=actionHandled;canThrow=throwable;return true;}
 bool dismiss(Navi&,bool& released,std::string&)override{order.push_back("dismiss-callback");released=dismissReleased;return true;}
 bool damageFeedback(Navi&,std::string&)override{order.push_back("damage-feedback");return true;}
} env;
class TypedState:public NativeState {
public:explicit TypedState(StateId id):NativeState(id){}bool sourceInvincible()const override{return false;}
};
class KeyState:public TypedState {
public:int calls=0,lastKey=0;KeyState():TypedState(StateId::Follow){}
 bool sourceAnimationKey(Navi*,int key,std::string& e)override{++calls;lastKey=key;e.clear();return true;}
};
class ReceiverState:public NaviState,public State {
 StateId source;bool forged;
public:ReceiverState(int native,StateId id,bool spoof=false):NaviState(native),source(id),forged(spoof){}
 const NaviState* nativeState()const override{return forged?nullptr:this;}
 StateId sourceStateId()const override{return source;}
 bool sourceAlive(const Navi& n)const override{return pc_p2_original_captain_actor_alive(&n);}
 bool sourceInvincible()const override{return false;}
 std::optional<std::uint8_t> actorInvincibleFrames(const Navi& n)const override{std::uint8_t f;if(!pc_p2_original_captain_actor_frames(&n,f))return {};return f;}
 bool canEnterSourceDead(const Navi&)const override{return false;}
 void enterSourceDead(Navi&)override{}void sourceDamageFeedback(Navi&)override{}
};
int checks=0;void check(bool v,const std::string& name){++checks;if(!v)throw std::runtime_error("check "+std::to_string(checks)+": "+name);}
std::string error;
void clearCounts(){order.clear();transitions=controls=downBegins=deadEvents=cleanupEvents=findQueries=actionRequests=dismissRequests=0;}
}
void captain_state_stub_transition(Navi*,int id){++transitions;order.push_back("transit"+std::to_string(id));}
const LoadedScene* pc_p2_original_captain_loaded_scene(){return loaded;}
const World* pc_p2_original_captain_world(){return active;}
SourceBank* pc_p2_original_captain_source_bank(){return bankProvider;}
WalkEnvironment* pc_p2_original_captain_walk_environment(const Navi*){return envProvider;}
bool pc_p2_original_captain_actor_alive(const Navi* n){int i=slot(n);return i>=0&&alive[i];}
bool pc_p2_original_captain_actor_frames(const Navi* n,std::uint8_t& out){int i=slot(n);if(i<0||!framesAvailable)return false;out=frames[i];return true;}
bool pc_p2_original_captain_damaged_cleanup(Navi* n){auto* state=dynamic_cast<State*>(n->getCurrState());if(!state||state->sourceStateId()!=StateId::Damaged)return false;++cleanupEvents;frames[slot(n)]=60;order.push_back("damaged-cleanup60");return true;}
bool pc_p2_original_captain_dead_entered(Navi* n){auto* state=dynamic_cast<State*>(n->getCurrState());if(!state||state->sourceStateId()!=StateId::Dead)return false;++deadEvents;order.push_back("clear-source-alive");alive[slot(n)]=false;return true;}
bool pc_p2_original_captain_down_preflight(const Navi*,std::string& e){if(!downAvailable)e="test down authority missing";return downAvailable;}
bool pc_p2_original_captain_down_begin(Navi* n,std::string& e){++downBegins;order.push_back("begin-down-alive"+std::to_string(pc_p2_original_captain_actor_alive(n)));if(!downAvailable)e="test down begin missing";return downAvailable;}
bool pc_p2_original_captain_party_preflight(Navi*,StateId,std::string& e){if(!partyAvailable)e="test party unavailable";return partyAvailable;}
bool pc_p2_original_captain_throw_preflight(Navi*,StateId,std::string& e){e="test throw owner deliberately unavailable";return false;}
namespace p2original { namespace captain {
struct SourceBank::Impl{};SourceBank::SourceBank():m(new Impl){}SourceBank::~SourceBank()=default;
bool SourceBank::ready()const{return bankReady;}
bool SourceBank::supports(Navi* n,Motion motion,std::string& e)const{order.push_back("supports"+std::to_string(unsigned(motion)));if(!bankReady||!bankBound||slot(n)<0||(motion==Motion::Damage&&!damageSupported)||(motion==Motion::Nigeru&&!nigeruSupported)){e="test authored motion unavailable";return false;}return true;}
bool SourceBank::enableMotionBlend(Navi* n,std::string&){order.push_back("enable-blend");blendLocks[n]=int(boundMotions[n].motion);boundMotions[n]={Motion::Nigeru,10,++motionGeneration,false,false};boundListeners[n]=Listener::SourceActor;return true;}
bool SourceBank::state(const Navi* n,MotionState& out,std::string& e)const{if(!bankBound||slot(n)<0){e="test bank actor missing";return false;}out=motions[n];return true;}
bool SourceBank::sourceBytes(SourceResource r,std::string& out,std::string& e)const{if(r!=SourceResource::Parameters){e="test source resource unavailable";return false;}out=resourceBytes;return true;}
bool SourceBank::start(Navi* n,Motion m,std::string&){order.push_back("motion"+std::to_string(unsigned(m)));motions[n]={m,0,++motionGeneration,false,false};boundMotions[n]={m,0,++motionGeneration,false,false};blendLocks[n]=-1;boundListeners[n]=Listener::None;return true;}
bool SourceBank::advance(Navi*,float,const std::function<bool(int)>& emit,std::string&){++bankAdvances;if(emitEnd){emitEnd=false;return emit(1000);}return true;}
namespace nativecontrol {
std::optional<float> sceneAnimationTimer(const Navi*){if(!timerAvailable)return {};return 0;}
bool control(Navi*,std::string& e){++controls;order.push_back("control");if(!controlAvailable)e="test control provider unavailable";return controlAvailable;}
}
}}
int main(int argc,char** argv){try{
 check(argc==2,"private verified parameter argument");std::ifstream file(argv[1],std::ios::binary);resourceBytes=std::string((std::istreambuf_iterator<char>(file)),{});control::Params params;check(control::parseParameters(resourceBytes,params,error),error);
 observed=ordinary();SourceBank bank;bankProvider=&bank;envProvider=&env;NaviStateMachine fsm;a.mStateMachine=&fsm;b.mStateMachine=&fsm;
 registerCoreStates(fsm);check(fsm.mStateCount==3,"actual factory registers ONLY3core states");check(fsm.mStates[0]->getID()==48&&fsm.mStates[1]->getID()==61&&fsm.mStates[2]->getID()==67,"actual typed Walk/Damaged/Dead native48/61/67");
 for(int i=0;i<3;++i){auto* state=dynamic_cast<State*>(fsm.mStates[i]);check(state&&state->nativeState()==fsm.mStates[i],"registered source State exact pointer");check(state->sourceInvincible()==(i==2),"source Walk/Damaged false and Dead true invincibility");}
 int mapped=777;loaded=nullptr;check(pc_p2_original_captain_route_transition(&a,1,mapped)==PcOriginalCaptainRoute::NonSource&&mapped==777,"nonSource route preserves output");check(!pc_p2_original_captain_transit(&a,StateId::Damaged,error)&&transitions==0&&a.mHealth==50,"missing scene refuses before transit/HP");loaded=&scene;
 active=nullptr;check(!pc_p2_original_captain_core_preflight(&a,StateId::Walk,error),"missing world refuses");active=&world;
 check(!pc_p2_original_captain_core_preflight(&outsider,StateId::Damaged,error),"outsider is not actual source captain slot");
 world.fingerprint="wrong";check(!pc_p2_original_captain_transit(&a,StateId::Damaged,error)&&transitions==0,"wrong session descriptor refuses");world.fingerprint=scene.fingerprint;
 world.campaign="wrong";check(!pc_p2_original_captain_core_preflight(&a,StateId::Damaged,error),"wrong campaign refuses");world.campaign=scene.campaign;
 world.actors[1]=&outsider;check(!pc_p2_original_captain_core_preflight(&a,StateId::Damaged,error),"world roster mismatch refuses");world.actors[1]=&b;
 ++world.epoch;check(!pc_p2_original_captain_core_preflight(&a,StateId::Damaged,error),"world stale incarnation refuses");world.epoch=scene.epoch;
 bankProvider=nullptr;check(!pc_p2_original_captain_transit(&a,StateId::Damaged,error)&&transitions==0,"missing bank refuses before source state");bankProvider=&bank;
 bankReady=false;check(!pc_p2_original_captain_core_preflight(&a,StateId::Damaged,error),"unprepared actual motion bank refuses");bankReady=true;bankBound=false;check(!pc_p2_original_captain_core_preflight(&a,StateId::Damaged,error),"bank doesnot bind exact actor");bankBound=true;
 damageSupported=false;clearCounts();check(!pc_p2_original_captain_transit(&a,StateId::Damaged,error)&&transitions==0&&a.mHealth==50,"missing genuineDamage clip refuses beforemutation");damageSupported=true;nigeruSupported=false;clearCounts();check(!pc_p2_original_captain_transit(&a,StateId::Damaged,error)&&transitions==0&&a.mHealth==50,"missing genuineNigeru blend clip refuses beforemutation");nigeruSupported=true;
 envProvider=nullptr;check(!pc_p2_original_captain_transit(&a,StateId::Walk,error)&&transitions==0,"missing Walk environment refuses");envProvider=&env;
 env.identity=&wrongScene;check(!pc_p2_original_captain_core_preflight(&a,StateId::Walk,error),"wrong Walk environment scene refuses");env.identity=&scene;
 env.captureAvailable=false;check(!pc_p2_original_captain_transit(&a,StateId::Walk,error)&&transitions==0,"missing completed Walk capture refuses beforetransition");env.captureAvailable=true;
 timerAvailable=false;check(!pc_p2_original_captain_transit(&a,StateId::Walk,error)&&transitions==0,"missing actual timer refuses beforetransit");timerAvailable=true;
 world.game=Phase::Inactive;check(!pc_p2_original_captain_transit(&a,StateId::Damaged,error)&&transitions==0,"inactive source world must refuse beforetransit");world.game=Phase::GameWorldActive;
 check(pc_p2_original_captain_route_transition(&a,NAVISTATE_Walk,mapped)==PcOriginalCaptainRoute::Handled&&mapped==48,"source recoveryWalk routes actual typed48");
 for(int id=1;id<48;++id){if(id==NAVISTATE_Dead)continue;mapped=777;check(pc_p2_original_captain_route_transition(&a,id,mapped)==PcOriginalCaptainRoute::Refused&&mapped==777,"all unsupported P1 requests refuse");}
 fsm.registerState(new NaviState(38));mapped=777;check(pc_p2_original_captain_route_transition(&a,38,mapped)==PcOriginalCaptainRoute::Refused&&mapped==777,"untyped sameID38 receiver cannot authenticate");
 fsm.registerState(new ReceiverState(39,StateId::Flick));fsm.registerState(new ReceiverState(40,StateId::KokeDamage));check(pc_p2_original_captain_route_transition(&a,39,mapped)==PcOriginalCaptainRoute::Handled&&mapped==39,"registered typed Flick receiver passes");check(pc_p2_original_captain_route_transition(&a,40,mapped)==PcOriginalCaptainRoute::Handled&&mapped==40,"registered typed Koke receiver passes");
 fsm.registerState(new ReceiverState(41,StateId::Flick,true));mapped=777;check(pc_p2_original_captain_route_transition(&a,41,mapped)==PcOriginalCaptainRoute::Refused&&mapped==777,"forged typed receiver pointer mustrefuse");
 world.game=Phase::Inactive;mapped=777;check(pc_p2_original_captain_route_transition(&a,39,mapped)==PcOriginalCaptainRoute::Refused&&mapped==777,"inactive typed receiver refuses");world.game=Phase::GameWorldActive;
 fsm.registerState(new TypedState(StateId::Follow));check(pc_p2_original_captain_transit(&a,StateId::Walk,error),error);check(dynamic_cast<State*>(a.current)->sourceStateId()==StateId::Walk,"real factory Walk init active");
 check(pc_p2_original_captain_transit(&a,StateId::Damaged,error),error);auto* actual=dynamic_cast<State*>(a.current);frames[0]=23;check(actual->actorInvincibleFrames(a)==23,"sourceState readonlyframes comes from actualactorprovider");framesAvailable=false;check(!actual->actorInvincibleFrames(a),"sourceState absentactorframes preserves missing ratherzero");framesAvailable=true;check(motions[&a].motion==Motion::Damage&&boundMotions[&a].motion==Motion::Nigeru&&boundMotions[&a].frame==10&&blendLocks[&a]==int(Motion::Damage)&&boundListeners[&a]==Listener::SourceActor,"actual Damaged init DamageSelf then sourceblend BoundNigeru frame10 lockDamage");
 auto damageStart=std::find(order.begin(),order.end(),"motion4"),blendStart=std::find(order.begin(),order.end(),"enable-blend");check(damageStart!=order.end()&&blendStart!=order.end()&&damageStart<blendStart,"actual Damage motion starts before source enableBlend");emitEnd=true;check(pc_p2_original_captain_core_advance_animation(&a,1,error)&&dynamic_cast<State*>(a.current)->sourceStateId()==StateId::Walk,"actual source Damaged END restores Walk backup");check(cleanupEvents==1&&frames[0]==60,"source Damaged cleanup iframe60 event before oldstate removed");
 check(pc_p2_original_captain_transit(&a,StateId::Follow,error),error);check(pc_p2_original_captain_transit(&a,StateId::Damaged,error),error);emitEnd=true;check(pc_p2_original_captain_core_advance_animation(&a,1,error)&&dynamic_cast<State*>(a.current)->sourceStateId()==StateId::Follow,"source Damaged END restores actual Follow backup");
 auto* typed=dynamic_cast<DamageTransitions*>(a.current);check(typed&&typed->canEnterSourceDamaged(a),"actual NativeState owns typed damage transition seam");a.mHealth=1;typed->enterSourceDamaged(a,.5f);check(dynamic_cast<State*>(a.current)->sourceStateId()==StateId::Damaged&&a.mHealth==1,"state transition callback never directly mutatesHP");
 check(pc_p2_original_captain_transit(&a,StateId::Walk,error),error);clearCounts();a.current->exec(&a);check(controls==1&&findQueries==1,"ordinary Walk control called once before exactposttimer policy");
 observed=ordinary();observed.actor->hasController=false;walk::Candidate enemy;enemy.identity=123;enemy.position={0,0,21};enemy.sphereRadius=5;enemy.alive=true;enemy.teki=true;enemy.living=true;enemy.emotionNonzero=true;enemy.weakBitterDrop=false;observed.cellCandidates=std::vector<walk::Candidate>{enemy};observed.randomValues={.5f};check(pc_p2_original_captain_transit(&a,StateId::Walk,error),error);clearCounts();a.current->exec(&a);check(controls==1,"WaitAI check establishes sourceEscape with firstcontrol only");observed.currentTarget=enemy;observed.cellCandidates=std::vector<walk::Candidate>{};clearCounts();a.current->exec(&a);check(controls==2&&findQueries==1,"source Escape preserves secondAI control cadence");
 observed=ordinary();observed.buttons->aDown=true;check(pc_p2_original_captain_transit(&a,StateId::Walk,error),error);clearCounts();a.current->exec(&a);check(controls==1&&findQueries==1&&actionRequests==1,"actual ActionButton continuation mustnot replay priorcontrol/query/request");
 observed=ordinary();observed.buttons->xDown=true;observed.buttons->xHeld=true;check(pc_p2_original_captain_transit(&a,StateId::Walk,error),error);clearCounts();a.current->exec(&a);check(controls==1&&findQueries==1&&dismissRequests==1,"actual Dismiss continuation mustnot replay sourceprefix");
 observed=ordinary();check(pc_p2_original_captain_transit(&a,StateId::Walk,error),error);controlAvailable=false;clearCounts();a.current->exec(&a);check(controls==1&&findQueries==0,"missing control provider blocks policyeffects");controlAvailable=true;
 // The actual dispatcher authenticates current typed identity and emits a key;
 // animator generation/callback guards belong to the common animator TU.
 auto* savedCurrent=a.current;const auto savedSelf=motions[&a],savedBound=boundMotions[&a];const int savedAdvances=bankAdvances;
 check(pc_p2_original_captain_animation_key(&a,17,error)&&error.empty(),"actual Walk dispatch delegates benign key to current policy");
 check(bankAdvances==savedAdvances&&motions[&a].generation==savedSelf.generation&&boundMotions[&a].generation==savedBound.generation&&motions[&a].frame==savedSelf.frame&&boundMotions[&a].frame==savedBound.frame,"key dispatch never advances or replaces either animator clock");
 loaded=nullptr;check(!pc_p2_original_captain_animation_key(&a,17,error)&&!error.empty(),"key dispatch missing canonical scene refuses");loaded=&scene;
 ++world.epoch;check(!pc_p2_original_captain_animation_key(&a,17,error)&&!error.empty(),"key dispatch stale scene incarnation refuses");world.epoch=scene.epoch;
 world.actors[1]=&outsider;check(!pc_p2_original_captain_animation_key(&a,17,error)&&!error.empty(),"key dispatch mismatched source roster refuses");world.actors[1]=&b;
 NaviState untyped(91);a.current=&untyped;check(!pc_p2_original_captain_animation_key(&a,17,error)&&!error.empty(),"key dispatch refuses P1 current state");
 ReceiverState forgedKey(92,StateId::Flick,true);a.current=&forgedKey;check(!pc_p2_original_captain_animation_key(&a,17,error)&&error=="missing exact current source animation state","key dispatch refuses forged source receiver identity before hook");
 ReceiverState flickKey(93,StateId::Flick),kokeKey(94,StateId::KokeDamage);a.current=&flickKey;check(!pc_p2_original_captain_animation_key(&a,17,error)&&error=="actual source receiver key handler is unavailable","actual Flick receiver missing independent owner hook refuses");a.current=&kokeKey;check(!pc_p2_original_captain_animation_key(&a,17,error)&&!error.empty(),"actual Koke receiver missing owner hook refuses");
 TypedState unavailableKey(StateId::Follow);a.current=&unavailableKey;check(!pc_p2_original_captain_animation_key(&a,17,error)&&error=="source state key handler unavailable","default NativeState key handler explicitly refuses");
 KeyState firstKey,secondKey;a.current=&firstKey;check(pc_p2_original_captain_animation_key(&a,201,error)&&firstKey.calls==1&&firstKey.lastKey==201,"key dispatch invokes exact current NativeState virtual");a.current=&secondKey;check(pc_p2_original_captain_animation_key(&a,202,error)&&firstKey.calls==1&&secondKey.calls==1&&secondKey.lastKey==202,"changed current state receives key without reusing prior state handler");
 check(bankAdvances==savedAdvances&&motions[&a].generation==savedSelf.generation&&boundMotions[&a].generation==savedBound.generation,"refused and custom key handlers leave authentic animator generations unchanged");a.current=savedCurrent;
 check(pc_p2_original_captain_transit(&a,StateId::Damaged,error),error);auto* damageCurrent=a.current;check(pc_p2_original_captain_animation_key(&a,17,error)&&a.current==damageCurrent&&error.empty(),"actual Damaged nonEND key permits remaining current generation");
 const int advancesBeforeEnd=bankAdvances,cleanupBeforeEnd=cleanupEvents;check(!pc_p2_original_captain_animation_key(&a,1000,error)&&error.empty()&&dynamic_cast<State*>(a.current)->sourceStateId()==StateId::Walk,"actual Damaged END restores Walk and signals successful statechange with false plus empty error");check(cleanupEvents==cleanupBeforeEnd+1&&frames[0]==60&&bankAdvances==advancesBeforeEnd,"direct END dispatch performs genuine cleanup without duplicate clock advance");
 clearCounts();alive[0]=true;check(pc_p2_original_captain_transit(&a,StateId::Dead,error),error);check(downBegins==1&&deadEvents==1&&order.size()>=3&&order[1]=="begin-down-alive1"&&order[2]=="clear-source-alive","actual Dead init beginsDown BEFORE sourceCFalive clear");check(a.current->invincible(&a),"actual Dead sourceState invincible");a.mTargetVelocity.set(1,2,3);a.mVelocity.set(4,5,6);a.current->exec(&a);check(a.mTargetVelocity.x==0&&a.mTargetVelocity.y==0&&a.mTargetVelocity.z==0&&a.mVelocity.x==0&&a.mVelocity.y==0&&a.mVelocity.z==0,"actual Dead zeroes both real velocity fields");
 std::cout<<"P2_ORIGINAL_CORE_STATES_ACTUAL_TU_CONTROLS_PASS checks="<<checks<<" gameplay=UNTESTED providers=DOUBLES\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
