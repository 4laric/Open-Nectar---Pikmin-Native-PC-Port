#include "pc_p2_original_captain_actions_party.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
using namespace p2original::captain;
namespace p=party;
void check(bool b,const char* name){if(!b){std::cerr<<name<<'\n';std::exit(1);}}
#if defined(PIKI_PC_PORT)
#include "pc_p2_original_captain_states.h"
#include "pc_p2_original_captain_motion.h"
namespace {
// Explicit engine/provider doubles exercise the actual party TU receiver.
// Actual source authority and ordinary gameplay acceptance remain separate.
Navi receiver,sender;std::string nativeError;unsigned nativeChecks=0;
struct Scene:LoadedScene {
 std::string c="party-campaign",f="party-session",cat="party-catalog";unsigned epoch=1;
 const std::string& selectedCampaign()const override{return c;}const std::string& selectedFingerprint()const override{return f;}const std::string& sourceCatalog()const override{return cat;}
 MoviePlayer* moviePlayer()const override{return nullptr;}std::uint64_t incarnation()const override{return epoch;}Navi* captainAt(unsigned i)const override{return i==0?&receiver:i==1?&sender:nullptr;}
} scene;
struct WorldOwner:World {
 bool wrongSlot=false;Phase value=Phase::GameWorldActive;
 const std::string& selectedCampaign()const override{return scene.c;}const std::string& selectedFingerprint()const override{return scene.f;}const std::string& sourceCatalog()const override{return scene.cat;}
 std::uint64_t incarnation()const override{return scene.epoch;}Phase phase()const override{return value;}Demo demo()const override{return Demo::Inactive;}Navi* captainAt(unsigned i)const override{return wrongSlot?nullptr:scene.captainAt(i);}
} worldOwner;
const World* worldProvider=&worldOwner;const LoadedScene* sceneProvider=&scene;bool providerAvailable=true,lifeAvailable=true,actorAlive=true;
struct Typed:NaviState,State {
 StateId id=StateId::Walk;Typed():NaviState(nativeId(StateId::Walk)){}
 const NaviState* nativeState()const override{return this;}StateId sourceStateId()const override{return id;}bool sourceAlive(const Navi&)const override{return true;}bool sourceInvincible()const override{return false;}
 std::optional<std::uint8_t> actorInvincibleFrames(const Navi&)const override{return 0;}bool canEnterSourceDead(const Navi&)const override{return false;}void enterSourceDead(Navi&)override{}void sourceDamageFeedback(Navi&)override{}
} walk,alternate;
std::vector<std::string> events;
struct Producer:p::PartySource {
 bool controller=false,reunited=true,worldFails=false,captainFails=false,membersFail=false,memberFail=false;unsigned expire=0;
 StateId state=StateId::Walk;std::vector<p::Member> list;
 const LoadedScene& scene()const override{return ::scene;}
 bool world(p::WorldFacts& out,std::string&)const override{events.push_back("world");if(expire==1)providerAvailable=false;if(worldFails)return false;out.active=true;out.reunited=reunited;return true;}
 bool captain(const Navi&,p::CaptainFacts& out,p::Vec3& pos,std::string&)const override{events.push_back("captain");if(expire==2)receiver.current=&alternate;if(captainFails)return false;out={actorAlive,controller,false,state};pos={};return true;}
 bool members(const Navi&,std::vector<p::Member>& out,std::string&)const override{events.push_back("members");if(expire==3)++::scene.epoch;if(membersFail)return false;out=list;return true;}
 bool togglePlayer(Navi&,Navi&,std::string&)override{return false;}bool changeVoice(Navi&,std::string&)override{return false;}
 bool whistleMember(Navi&,actions::PikiHandle,bool combine,bool fresh,std::string&)override{events.push_back(combine&&fresh?"member":"bad member flags");if(expire==4)sender.current=&alternate;return !memberFail;}
 bool dismissSound(Navi&,std::string&)override{return false;}bool freeMember(Navi&,actions::PikiHandle,float,p::Vec3,bool,std::string&)override{return false;}bool disbandTimer(Navi&,unsigned,std::string&)override{return false;}
 bool followFrame(const Navi&,p::FollowFrame&,std::string&)const override{return false;}bool moveRotation(Navi&,bool,std::string&)override{events.push_back("rotate");return true;}
 bool randomChoice(float&,std::string&)override{return false;}bool followFeedback(Navi&,p::FollowFeedback,std::string&)override{events.push_back("alert");return true;}
 bool enemy(p::EnemyHandle,p::EnemyFrame&,std::string&)const override{return false;}bool followPunch(Navi&,p::EnemyHandle,p::Vec3,std::string&)override{return false;}
} producer;
SourceBank* nativeBank=nullptr;
void nativeCheck(bool yes,const char* label){++nativeChecks;check(yes,label);}
void resetInvocation(){receiver.current=&walk;sender.current=&walk;walk.id=StateId::Walk;producer.state=StateId::Walk;producer.controller=false;producer.reunited=true;producer.worldFails=false;producer.captainFails=false;producer.membersFail=false;producer.memberFail=false;producer.expire=0;events.clear();providerAvailable=true;lifeAvailable=true;actorAlive=true;worldProvider=&worldOwner;sceneProvider=&scene;worldOwner.value=Phase::GameWorldActive;worldOwner.wrongSlot=false;}
void nativeControls(){
 NaviStateMachine machine;receiver.mStateMachine=&machine;sender.mStateMachine=&machine;registerPartyStates(machine);SourceBank bank;nativeBank=&bank;
 p::WhistleOutcome out;resetInvocation();producer.controller=true;out.accepted=true;
 nativeCheck(p::invokeWhistleCaptain(&receiver,&sender,false,true,out,nativeError)&&!out.accepted,"normal controlled rejection completes");nativeCheck(events==std::vector<std::string>{"world","captain"},"ordinary rejection does not transit or touch members");
 resetInvocation();walk.id=StateId::Follow;producer.state=StateId::Follow;nativeCheck(p::invokeWhistleCaptain(&receiver,&sender,false,true,out,nativeError)&&!out.accepted,"Follow not callable is completed rejection");
 resetInvocation();actorAlive=false;nativeCheck(p::invokeWhistleCaptain(&receiver,&sender,false,true,out,nativeError)&&!out.accepted,"actual dead receiver completed rejection");
 resetInvocation();producer.reunited=false;nativeCheck(p::invokeWhistleCaptain(&receiver,&sender,false,true,out,nativeError)&&!out.accepted,"day-zero separated ordinary rejection");
 resetInvocation();producer.controller=true;nativeCheck(!p::whistleCaptain(&receiver,&sender,false,true,nativeError),"legacy wrapper preserves rejected bool");
 resetInvocation();producer.list={{{reinterpret_cast<Piki*>(1),10},{},0,true,true},{{reinterpret_cast<Piki*>(2),20},{},0,true,true}};
 nativeCheck(p::invokeWhistleCaptain(&receiver,&sender,false,true,out,nativeError)&&out.accepted,"positive typed Follow invocation");
 nativeCheck(events==std::vector<std::string>{"world","captain","transit","motion","alert","rotate","members","member","member"},"retail transition-init-snapshot-transfer ordering");
 auto* source=dynamic_cast<State*>(receiver.current);nativeCheck(source&&source->sourceStateId()==StateId::Follow,"actual registered Follow factory is current");
 resetInvocation();producer.list.clear();nativeCheck(p::whistleCaptain(&receiver,&sender,true,false,nativeError),"legacy wrapper accepts actual Follow and combines flag is not Navi guard");
 for(unsigned expiry:{1u,2u,3u,4u}){resetInvocation();producer.expire=expiry;producer.list={{{reinterpret_cast<Piki*>(1),10},{},0,true,true}};out.accepted=true;auto epoch=scene.epoch;nativeCheck(!p::invokeWhistleCaptain(&receiver,&sender,false,true,out,nativeError)&&out.accepted,"callback expiry refusal leaves output unchanged");scene.epoch=epoch;}
 resetInvocation();providerAvailable=false;out.accepted=true;nativeCheck(!p::invokeWhistleCaptain(&receiver,&sender,false,true,out,nativeError)&&out.accepted,"missing actual provider refuses");
 resetInvocation();worldProvider=nullptr;nativeCheck(!p::invokeWhistleCaptain(&receiver,&sender,false,true,out,nativeError)&&out.accepted,"missing canonical World refuses");
 resetInvocation();worldOwner.wrongSlot=true;nativeCheck(!p::invokeWhistleCaptain(&receiver,&sender,false,true,out,nativeError)&&out.accepted,"canonical World slot mismatch refuses");
 resetInvocation();lifeAvailable=false;nativeCheck(!p::invokeWhistleCaptain(&receiver,&sender,false,true,out,nativeError)&&out.accepted,"missing actor lifetime refuses");
 resetInvocation();worldOwner.value=Phase::Inactive;nativeCheck(!p::invokeWhistleCaptain(&receiver,&sender,false,true,out,nativeError)&&out.accepted,"inactive canonical World refuses before observations");
 resetInvocation();sceneProvider=nullptr;nativeCheck(!p::invokeWhistleCaptain(&receiver,&sender,false,true,out,nativeError)&&out.accepted,"missing LoadedScene refuses");
 resetInvocation();producer.state=StateId::Pellet;nativeCheck(!p::invokeWhistleCaptain(&receiver,&sender,false,true,out,nativeError)&&out.accepted,"observation state differing from exact typed current refuses");
 resetInvocation();nativeCheck(!p::invokeWhistleCaptain(&receiver,&receiver,false,true,out,nativeError)&&out.accepted,"self caller rejected as missing actual partner invocation");
 for(unsigned failure:{1u,2u,3u}){resetInvocation();producer.worldFails=failure==1;producer.captainFails=failure==2;producer.membersFail=failure==3;nativeCheck(!p::invokeWhistleCaptain(&receiver,&sender,false,true,out,nativeError)&&out.accepted,"missing actual producer observation leaves outcome unchanged");}
 resetInvocation();producer.memberFail=true;producer.list={{{reinterpret_cast<Piki*>(1),10},{},0,true,true}};nativeCheck(!p::invokeWhistleCaptain(&receiver,&sender,false,true,out,nativeError)&&out.accepted,"ambiguous Piki bool false remains development refusal");
 resetInvocation();receiver.mStateMachine=nullptr;nativeCheck(!p::invokeWhistleCaptain(&receiver,&sender,false,true,out,nativeError)&&out.accepted,"missing genuine Follow FSM refuses");receiver.mStateMachine=&machine;
 std::cout<<nativeChecks<<" actual party invocation controls PASS (engineering doubles)\n";
}
}
const World* pc_p2_original_captain_world(){return worldProvider;}const LoadedScene* pc_p2_original_captain_loaded_scene(){return sceneProvider;}
const p::PartySource* pc_p2_original_captain_party_source(const Navi*){return providerAvailable?&producer:nullptr;}
bool pc_p2_original_captain_actor_lifetime(const Navi*,bool& out){if(!lifeAvailable)return false;out=actorAlive;return true;}
bool pc_p2_original_captain_transit(Navi* n,StateId id,std::string&){if(!n||!n->mStateMachine)return false;events.push_back("transit");n->mStateMachine->transit(n,nativeId(id));return true;}
void captain_state_stub_transition(Navi*,int){}
SourceBank* pc_p2_original_captain_source_bank(){return nativeBank;}
actions::ActionSource* pc_p2_original_captain_action_source(const Navi*){return nullptr;}
float pc_p2_equipment_speed(float x){return x;}
namespace p2original {namespace captain {
bool NativeState::sourceAlive(const Navi&)const{return actorAlive;}std::optional<std::uint8_t> NativeState::actorInvincibleFrames(const Navi&)const{return 0;}bool NativeState::canEnterSourceDead(const Navi&)const{return false;}void NativeState::enterSourceDead(Navi&){}void NativeState::sourceDamageFeedback(Navi&){}bool NativeState::canEnterSourceDamaged(const Navi&)const{return false;}void NativeState::enterSourceDamaged(Navi&,float){}
struct SourceBank::Impl{};SourceBank::SourceBank():m(new Impl){}SourceBank::~SourceBank()=default;
bool SourceBank::ready()const{return true;}bool SourceBank::state(const Navi*,MotionState& out,std::string&)const{out={};return true;}
bool SourceBank::start(Navi*,Motion,std::string&){return true;}bool SourceBank::startMotion(Navi*,Motion,Motion,Listener,Listener,std::string&){events.push_back("motion");return true;}
bool SourceBank::advance(Navi*,float,const std::function<bool(int)>&,std::string&){return true;}
}}
#endif
int main(){
 p::WorldFacts w;w.active=true;w.demoInactive=true;w.switchUnlocked=true;
 p::CaptainFacts c;c.alive=true;c.state=StateId::Walk;
 check(p::switchAllowed(w,c),"ordinary switch");
 for(auto state:{StateId::Nuku,StateId::NukuAdjust,StateId::Punch}){c.state=state;check(!p::switchAllowed(w,c),"excluded partner state");}
 c.state=StateId::Walk;
 auto a=w;a.softPaused=true;check(!p::switchAllowed(a,c),"softpause");
 a=w;a.multiplayer=true;check(!p::switchAllowed(a,c),"multiplayer");
 a=w;a.demoInactive=false;check(!p::switchAllowed(a,c),"demo");
 a=w;a.switchUnlocked=false;check(!p::switchAllowed(a,c),"switch unlock distinct from reunion");
 check(!p::whistleAllowed(w,c),"day0 not reunited");
 w.reunited=true;check(p::whistleAllowed(w,c),"reunited callable");
 c.controller=true;check(!p::whistleAllowed(w,c),"controlled captain refuses whistle");
 c.controller=false;c.state=StateId::Follow;check(!p::whistleAllowed(w,c),"Follow not callable");
 c.state=StateId::Pellet;check(p::whistleAllowed(w,c),"Pellet callable");
 check(p::needsChange(StateId::Follow)&&p::needsChange(StateId::Walk)&&!p::needsChange(StateId::Gather),"source change capability");
 p::FollowFrame f;f.leaderPosition={30,0,0};f.leaderTargetVelocity={20,0,0};
 p::Vec3 velocity;bool tooFar=false;std::string followError;
 check(p::followVelocity({0,0,0},f,160,velocity,tooFar,followError)&&velocity.x==90&&!tooFar,"30 boundary retains movement then average");
 f.leaderPosition={29,0,0};check(p::followVelocity({0,0,0},f,160,velocity,tooFar,followError)&&velocity.x==10,"below30 source leader average");
 f.leaderPosition={60,0,0};check(p::followVelocity({0,0,0},f,160,velocity,tooFar,followError)&&velocity.x==160,"60 boundary no average");
 f.leaderPosition={430,0,0};check(p::followVelocity({0,0,0},f,160,velocity,tooFar,followError)&&!tooFar,"430 remains follow");
 f.leaderPosition={431,0,0};check(p::followVelocity({0,0,0},f,160,velocity,tooFar,followError)&&tooFar,"431 leaves follow");
 f.leaderPosition={0,0,100};f.leaderState=StateId::Throw;check(p::followVelocity({0,0,0},f,160,velocity,tooFar,followError)&&velocity.x>0,"authored81degree throw offset");
 auto previous=velocity;f.leaderPosition.y=std::numeric_limits<float>::quiet_NaN();
 check(!p::followVelocity({0,0,0},f,160,velocity,tooFar,followError)&&velocity.x==previous.x,"malformed live frame preserves output");
 f.leaderPosition={0,0,100};f.plateRadius=-1;check(!p::followVelocity({0,0,0},f,160,velocity,tooFar,followError),"invalid actual plate observation");
 std::vector<p::Member> members;
 for(unsigned i=0;i<100;++i){p::Member m;m.handle={reinterpret_cast<Piki*>(std::uintptr_t(i+1)),1};m.position={100,float(i%2)*10,0};m.kind=i%8;m.alive=true;m.releasable=true;members.push_back(m);}
 std::array<p::Group,8> groups{};std::string error;
 check(p::dismissGroups(members,{0,0,0},{500,0,0},true,groups,error),"full100 pool all kinds");
 unsigned total=0;for(auto& g:groups){total+=g.count;check(g.radius==std::sqrt(float(g.count))*6.25f,"source cluster radius");check(std::isfinite(g.center.x)&&std::isfinite(g.center.y),"finite clusters");}
 check(total==100,"all100 retained");
 auto old=groups;members.push_back(members[0]);check(!p::dismissGroups(members,{0,0,0},{500,0,0},true,groups,error),"duplicate refuses");check(groups[0].center.x==old[0].center.x,"failure preserves output");
 members.resize(1);members[0].position={0,60,0};members[0].kind=0;
 check(p::dismissGroups(members,{0,0,0},{0,0,0},false,groups,error),"3D source distance");check(groups[0].center.y==60,"vertical distance not horizontal approximation");
 members[0].alive=false;check(p::dismissGroups(members,{0,0,0},{0,0,0},false,groups,error)&&groups[0].count==0,"dead not dismissed");
 members[0].alive=true;members[0].releasable=false;check(p::dismissGroups(members,{0,0,0},{0,0,0},false,groups,error)&&groups[0].count==0,"unreleasable preserved");
 members[0].releasable=true;members[0].position.x=std::numeric_limits<float>::quiet_NaN();check(!p::dismissGroups(members,{0,0,0},{0,0,0},false,groups,error),"bad member refuses");
#if defined(PIKI_PC_PORT)
 nativeControls();
#endif
 std::cout<<"source captain party controls PASS\n";
}
