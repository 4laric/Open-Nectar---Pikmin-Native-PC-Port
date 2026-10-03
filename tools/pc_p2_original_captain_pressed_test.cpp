// Actual native leaf TU with engineering doubles; no gameplay qualification.
#include "pc_p2_original_captain_pressed.h"
#include "pc_p2_original_captain_states.h"
#include "pc_p2_original_captain_motion.h"
#include <array>
#include <unordered_map>
#include <cmath>
#include <iostream>
#include <cstdlib>
using namespace p2original::captain;
using namespace p2original::captain::physical;
namespace {unsigned checks=0;}
#define CHECK(c) do{++checks;if(!(c)){std::cerr<<"FAIL "<<__LINE__<<" "<<#c<<"\n";std::abort();}}while(false)
namespace Sys {struct Triangle {};}
namespace {
Navi navis[2];Sys::Triangle triangle;bool provider=true,missingClip=false,alive=true,expireDamage=false;std::vector<int> events;
struct Scene:LoadedScene,World {
 std::string campaign="original",fingerprint="fixture",catalog="source";std::uint64_t epoch=1;Demo movie=Demo::Absent;Phase stage=Phase::GameWorldActive;
 const std::string& selectedCampaign()const override{return campaign;}const std::string& selectedFingerprint()const override{return fingerprint;}const std::string& sourceCatalog()const override{return catalog;}std::uint64_t incarnation()const override{return epoch;}
 MoviePlayer* moviePlayer()const override{return nullptr;}Navi* captainAt(unsigned i)const override{return i<2?&navis[i]:nullptr;}Phase phase()const override{return stage;}Demo demo()const override{return movie;}
} scene;
struct Body:PhysicalSource {
 BodyFrame f{{10,20,30},{2,3,4},{5,6,7},{8,9,10},.4f,.1f};Landing l{{&triangle,3},false,80};bool contact=true,koke=true,transform=true,collision=true;int expireOn=0;void expire(int operation){if(expireOn==operation){expireOn=0;++::scene.epoch;}}Vec3 matrixScale,rotation,position;unsigned sounds=0,stickEnds=0,effects=0,rumbles=0,kokes=0;float applied=0;bool water=false;
 const LoadedScene& scene()const override{return ::scene;}
 bool frame(const Navi&,BodyFrame& out,std::string&)const override{out=f;return true;}
 bool scale(Navi&,Vec3 v,std::string&)override{f.scale=v;events.push_back(1);expire(1);return true;}
 bool updateTrMatrix(Navi&,bool v,std::string&)override{transform=v;events.push_back(v?12:2);return true;}
 bool baseSRT(Navi&,Vec3 s,Vec3 r,Vec3 p,std::string&)override{matrixScale=s;rotation=r;position=p;events.push_back(3);return true;}
 bool atari(Navi&,bool v,std::string&)override{collision=v;events.push_back(v?11:4);expire(v?11:4);return true;}
 bool damageSoundAndOptionalDirector(Navi&,std::string&)override{++sounds;events.push_back(5);return true;}
 bool velocities(Navi&,Vec3 a,Vec3 t,std::string&)override{f.velocity=a;f.targetVelocity=t;events.push_back(6);return true;}
 bool endStick(Navi&,std::string&)override{++stickEnds;events.push_back(7);return true;}
 bool landing(const Navi&,Contact c,Landing& out,std::string&)const override{out=l;return contact&&c.triangle==l.contact.triangle&&c.lifetime==l.contact.lifetime;}
 bool landingEffect(Navi&,bool w,Vec3 p,float s,std::string&)override{CHECK(s==.5f);water=w;position=p;++effects;events.push_back(8);expire(8);return true;}
 bool nudgeRumble(Navi&,std::string&)override{++rumbles;events.push_back(9);return true;}
 bool preflightKoke(const Navi&,float,std::string& e)const override{if(!koke)e="missing real Koke owner";return koke;}
 bool enterKoke(Navi& n,float d,std::string& e)override;
} body;
struct Sink:NaviState {explicit Sink(StateId id):NaviState(nativeId(id)){}void init(Navi*)override{events.push_back(13);}};
}
namespace p2original {namespace captain {
struct SourceBank::Impl {struct A {std::array<MotionState,2> state;std::array<Listener,2> listener;};std::unordered_map<const Navi*,A> actors;};
SourceBank::SourceBank():m(new Impl){}SourceBank::~SourceBank()=default;
bool SourceBank::supports(Navi*,Motion motion,std::string& e)const{CHECK(motion==Motion::Fall||motion==Motion::Getup);if(missingClip)e="missing exact original fall resource";return !missingClip;}
bool SourceBank::state(const Navi* n,MotionState& f,std::string& e)const{return stateAnimator(n,Animator::Self,f,e);}
bool SourceBank::stateAnimator(const Navi* n,Animator a,MotionState& f,std::string&)const{auto it=m->actors.find(n);if(it==m->actors.end())return false;f=it->second.state[unsigned(a)];return true;}
bool SourceBank::startMotion(Navi* n,Motion s,Motion b,Listener sl,Listener bl,std::string&){CHECK(s==b);if(s==Motion::Fall)CHECK(sl==Listener::None&&bl==Listener::None);for(unsigned i=0;i<2;++i){auto& v=m->actors[n].state[i];v.motion=i?b:s;++v.generation;m->actors[n].listener[i]=i?bl:sl;}events.push_back(14);return true;}
bool NativeState::sourceAlive(const Navi&)const{return alive;}std::optional<std::uint8_t> NativeState::actorInvincibleFrames(const Navi&)const{return 0;}
bool NativeState::canEnterSourceDead(const Navi&)const{return true;}void NativeState::enterSourceDead(Navi&){}void NativeState::sourceDamageFeedback(Navi&){}
bool NativeState::canEnterSourceDamaged(const Navi&)const{return true;}void NativeState::enterSourceDamaged(Navi&,float){}
DamageResult addDamage(Navi*,float d,bool feedback){CHECK(d==0&&feedback);events.push_back(10);if(expireDamage){expireDamage=false;++scene.epoch;}DamageResult r;r.refusal=Refusal::None;return r;}
}}
SourceBank bank;
PhysicalSource* pc_p2_original_captain_physical_source(const Navi*){return provider?&body:nullptr;}
const LoadedScene* pc_p2_original_captain_loaded_scene(){return &scene;}const World* pc_p2_original_captain_world(){return &scene;}SourceBank* pc_p2_original_captain_source_bank(){return &bank;}
bool pc_p2_original_captain_actor_alive(const Navi*){return alive;}
bool pc_p2_original_captain_transit(Navi* n,StateId id,std::string& e){return n->fsm->transit(n,nativeId(id))||(e="missing state",false);}
bool Body::enterKoke(Navi& n,float d,std::string& e){++kokes;applied=d;events.push_back(15);return pc_p2_original_captain_transit(&n,StateId::KokeDamage,e);}
int main(){std::string e;NaviStateMachine fsm;registerPressedFallMeckStates(fsm);fsm.registerState(new Sink(StateId::Walk));fsm.registerState(new Sink(StateId::KokeDamage));for(auto& n:navis){n.fsm=&fsm;CHECK(bank.startMotion(&n,Motion::Fall,Motion::Fall,Listener::None,Listener::None,e));}
 auto* press=dynamic_cast<NativeState*>(fsm.find(nativeId(StateId::Pressed)));auto* fall=dynamic_cast<NativeState*>(fsm.find(nativeId(StateId::FallMeck)));
 CHECK(press&&fall);CHECK(press->sourceInvincible()&&!press->sourcePressable()&&!press->sourceVsUsableY());CHECK(!fall->sourceInvincible()&&!fall->sourcePressable()&&fall->sourceVsUsableY());
 provider=false;CHECK(!pc_p2_original_captain_pressed_fall_preflight(&navis[0],StateId::Pressed,e));provider=true;
 scene.movie=Demo::Unknown;CHECK(!pc_p2_original_captain_pressed_fall_preflight(&navis[0],StateId::Pressed,e));scene.movie=Demo::Absent;
 CHECK(pc_p2_original_captain_pressed_fall_preflight(&navis[0],StateId::Pressed,e));events.clear();CHECK(pc_p2_original_captain_transit(&navis[0],StateId::Pressed,e));CHECK(events==std::vector<int>({1,2,3,4,5}));CHECK(body.f.scale.x==1.5f&&body.f.scale.y==.01f&&!body.transform&&!body.collision);CHECK(body.position.y==22&&body.rotation.x==0&&body.rotation.y==.4f);
 body.f.delta=2;press->exec(&navis[0]);CHECK(navis[0].getCurrState()==press);CHECK(body.f.velocity.x==0&&body.f.targetVelocity.y==0);
 body.f.delta=.35f;press->exec(&navis[0]);float t=.35f,y=1-t/.7f;y+=(.5f*(1-y))*std::sin(t*6.2831853071795864769f*4);CHECK(std::fabs(body.f.scale.x-(y*2+1.5f*(1-y)))<.00001f);CHECK(body.f.scale.z==body.f.scale.x);
 body.f.delta=.36f;events.clear();press->exec(&navis[0]);CHECK(fsm.last==nativeId(StateId::Walk));CHECK(body.f.scale.x==2&&body.f.scale.y==3&&body.f.scale.z==4&&body.transform&&body.collision);CHECK(events.back()==6); // zero velocities after transition cleanup
 body.f.scale={2,3,4};CHECK(pc_p2_original_captain_transit(&navis[0],StateId::Pressed,e));scene.movie=Demo::Playing;events.clear();press->exec(&navis[0]);CHECK(fsm.last==nativeId(StateId::Walk));CHECK(events.back()==13);scene.movie=Demo::Absent;
 missingClip=true;CHECK(!beginFallMeck(&navis[0],std::nullopt,e));missingClip=false;CHECK(!beginFallMeck(&navis[0],NAN,e));
 body.f.velocity={5,6,7};body.f.targetVelocity={8,9,10};CHECK(beginFallMeck(&navis[0],std::nullopt,e));CHECK(body.f.velocity.x==5&&body.f.velocity.y==-100&&body.f.velocity.z==7);CHECK(body.f.targetVelocity.x==8&&body.f.targetVelocity.y==-100&&body.f.targetVelocity.z==10);CHECK(body.stickEnds==1);
 MotionState before,after;CHECK(bank.state(&navis[0],before,e));CHECK(fall->sourceAnimationKey(&navis[0],1000,e));CHECK(navis[0].getCurrState()==fall);CHECK(bank.state(&navis[0],after,e));CHECK(before.generation==after.generation&&before.frame==after.frame&&after.motion==Motion::Fall); // no fabricated GetUp phase and no clock advancement
 CHECK(!bounce(&navis[0],{},e));body.contact=false;CHECK(!bounce(&navis[0],{&triangle,3},e));body.contact=true;
 events.clear();CHECK(bounce(&navis[0],{&triangle,3},e));CHECK(events==std::vector<int>({8,9,13}));CHECK(!body.water&&body.position.y==20);CHECK(body.rumbles==1);
 CHECK(beginFallMeck(&navis[1],20,e));CHECK(body.f.velocity.y==-400&&body.f.targetVelocity.y==-400);body.l.inWater=true;body.l.seaHeight=NAN;unsigned prior=body.effects;CHECK(!bounce(&navis[1],{&triangle,3},e));CHECK(body.effects==prior);body.l.seaHeight=80;
 body.koke=false;CHECK(!bounce(&navis[1],{&triangle,3},e));CHECK(body.effects==prior);body.koke=true;events.clear();CHECK(bounce(&navis[1],{&triangle,3},e));CHECK(events==std::vector<int>({8,10,15,13}));CHECK(body.water&&body.position.y==80&&body.applied==20&&body.kokes==1);
 CHECK(beginFallMeck(&navis[0],-5,e));CHECK(body.f.velocity.y==-100);CHECK(bank.startMotion(&navis[0],Motion::Getup,Motion::Getup,Listener::SourceActor,Listener::None,e));fall->exec(&navis[0]);CHECK(fsm.last==nativeId(StateId::Walk));
 CHECK(beginFallMeck(&navis[0],0,e));++scene.epoch;CHECK(!fall->sourceAnimationKey(&navis[0],1000,e));CHECK(!e.empty());--scene.epoch;
 CHECK(!bounce(&navis[1],{&triangle,3},e));CHECK(!pc_p2_original_captain_pressed_fall_preflight(&navis[1],StateId::Punch,e));
 // Callback expiry stops the remaining chain; stale cleanup never writes a
 // roster/body that has been reused. Inactive same-incarnation cleanup works.
 CHECK(pc_p2_original_captain_transit(&navis[0],StateId::Walk,e));body.f.scale={2,3,4};body.expireOn=1;events.clear();CHECK(pc_p2_original_captain_transit(&navis[0],StateId::Pressed,e));CHECK(events==std::vector<int>({1}));events.clear();press->cleanup(&navis[0]);CHECK(events.empty());
 CHECK(pc_p2_original_captain_transit(&navis[0],StateId::Walk,e));body.f.scale={2,3,4};CHECK(pc_p2_original_captain_transit(&navis[0],StateId::Pressed,e));scene.stage=Phase::Inactive;events.clear();press->cleanup(&navis[0]);CHECK(events==std::vector<int>({11,12,1}));CHECK(body.f.scale.z==4);scene.stage=Phase::GameWorldActive;
 CHECK(pc_p2_original_captain_transit(&navis[0],StateId::Walk,e));CHECK(pc_p2_original_captain_transit(&navis[0],StateId::Pressed,e));body.expireOn=11;events.clear();press->cleanup(&navis[0]);CHECK(events==std::vector<int>({11}));
 CHECK(pc_p2_original_captain_transit(&navis[0],StateId::Walk,e));CHECK(beginFallMeck(&navis[0],10,e));body.expireOn=8;events.clear();CHECK(!bounce(&navis[0],{&triangle,3},e));CHECK(events==std::vector<int>({8}));
 CHECK(pc_p2_original_captain_transit(&navis[0],StateId::Walk,e));CHECK(beginFallMeck(&navis[0],10,e));expireDamage=true;events.clear();CHECK(!bounce(&navis[0],{&triangle,3},e));CHECK(events==std::vector<int>({8,10}));
 std::cout<<"original Pressed/FallMeck engineering controls PASS checks="<<checks<<"\n";
}
