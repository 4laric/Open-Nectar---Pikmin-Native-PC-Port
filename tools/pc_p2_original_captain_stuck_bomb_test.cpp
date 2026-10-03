// Actual leaf TU linked with engineering doubles, not gameplay evidence.
#include "pc_p2_original_captain_stuck_bomb.h"
#include "pc_p2_original_captain_states.h"
#include "pc_p2_original_captain_motion.h"
#include <unordered_map>
#include <cmath>
#include <iostream>
#include <cstdlib>
using namespace p2original::captain;using namespace p2original::captain::attachments;
namespace {unsigned checks=0;}
#define CHECK(c) do{++checks;if(!(c)){std::cerr<<"FAIL line="<<__LINE__<<" "<<#c<<"\n";std::abort();}}while(false)
namespace {
Navi navis[2];Creature bombBody,stickers[3];bool provider=true,missingClip=false;std::vector<int> events;
struct Scene:LoadedScene,World {
 std::string campaign="original",fingerprint="fixture",catalog="source";std::uint64_t epoch=1;Phase stage=Phase::GameWorldActive;
 const std::string& selectedCampaign()const override{return campaign;}const std::string& selectedFingerprint()const override{return fingerprint;}const std::string& sourceCatalog()const override{return catalog;}std::uint64_t incarnation()const override{return epoch;}
 MoviePlayer* moviePlayer()const override{return nullptr;}Navi* captainAt(unsigned i)const override{return i<2?&navis[i]:nullptr;}Phase phase()const override{return stage;}Demo demo()const override{return Demo::Absent;}
} scene;
struct Source:AttachmentSource {
 Frame f{{10,20,30},0,.01f,1,0,true,false,false,1};BombFrame bf{{&bombBody,12},true,nullptr};bool missingBomb=false,missingSticker=false;int expireOn=0,bombExpiresOn=0;std::vector<float> rng;unsigned draws=0,controls=0,releases=0,startCalls=0,endCalls=0,updates=0;CaptureMatrix capture,rotation;Vec3 velocity;struct Flick {CreatureHandle handle;float knock,damage,angle;};std::vector<Flick> flicks;
 void expire(int operation){if(bombExpiresOn==operation){bombExpiresOn=0;missingBomb=true;}if(expireOn==operation){expireOn=0;++::scene.epoch;}}
 const LoadedScene& scene()const override{return ::scene;}
 bool frame(const Navi&,Frame& out,std::string&)const override{out=f;return true;}
 bool control(Navi&,std::string&)override{++controls;events.push_back(1);expire(1);return true;}
 bool releasePikis(Navi&,bool& released,std::string&)override{released=false;++releases;events.push_back(2);expire(2);return true;}
 bool forEachSticker(Navi&,const std::function<bool(std::optional<CreatureHandle>)>& visit,std::string&)override{CHECK(visit(std::nullopt));for(unsigned i=0;i<3;++i)if(!visit(CreatureHandle{&stickers[i],i+1}))return false;return true;}
 bool sticker(const Navi&,CreatureHandle h,std::string& e)const override{if(missingSticker)e="missing source sticker lifetime";return !missingSticker&&h.actor&&h.lifetime;}
 bool randomFloat(float& out,std::string&)override{CHECK(draws<rng.size());out=rng[draws++];events.push_back(3);expire(3);return true;}
 bool flick(Navi&,CreatureHandle h,float knock,float damage,float angle,bool& accepted,std::string&)override{flicks.push_back({h,knock,damage,angle});accepted=false;events.push_back(4);expire(4);return true;}
 bool bomb(BombHandle h,BombFrame& out,std::string& e)const override{out=bf;if(missingBomb)e="actual Bomb lifetime expired";return !missingBomb&&h.actor==bf.handle.actor&&h.lifetime==bf.handle.lifetime;}
 bool sound(Navi&,Sound s,std::string&)override{events.push_back(s==Sound::PickupBomb?5:6);expire(s==Sound::PickupBomb?5:6);return true;}
 bool startCapture(Navi&,BombHandle h,const CaptureMatrix* m,std::string&)override{CHECK(h.actor==&bombBody&&m);bf.capturedBy=m;capture=*m;++startCalls;events.push_back(7);expire(7);return true;}
 bool updateCapture(Navi&,BombHandle h,const CaptureMatrix& m,std::string&)override{CHECK(h.actor==&bombBody);rotation=m;if(bf.capturedBy)capture=*bf.capturedBy;++updates;events.push_back(8);expire(8);return true;}
 bool bombVelocity(BombHandle h,Vec3 v,std::string&)override{CHECK(h.actor==&bombBody);velocity=v;events.push_back(9);expire(9);return true;}
 bool endCapture(BombHandle h,std::string&)override{CHECK(h.actor==&bombBody);bf.capturedBy=nullptr;++endCalls;events.push_back(10);expire(10);return true;}
} source;
struct Sink:NaviState {explicit Sink(StateId id):NaviState(nativeId(id)){}void init(Navi*)override{events.push_back(11);}};
}
namespace p2original {namespace captain {
struct SourceBank::Impl {struct A {std::array<MotionState,2> state;std::array<Listener,2> listener;};std::unordered_map<const Navi*,A> actors;};
SourceBank::SourceBank():m(new Impl){}SourceBank::~SourceBank()=default;
bool SourceBank::supports(Navi*,Motion motion,std::string& e)const{CHECK(motion==Motion::PickPut||motion==Motion::Nigeru);if(missingClip)e="missing actual PickPut clip";return !missingClip;}
bool SourceBank::state(const Navi* n,MotionState& out,std::string& e)const{return stateAnimator(n,Animator::Self,out,e);}
bool SourceBank::stateAnimator(const Navi* n,Animator a,MotionState& out,std::string&)const{auto it=m->actors.find(n);if(it==m->actors.end())return false;out=it->second.state[unsigned(a)];return true;}
bool SourceBank::startMotion(Navi* n,Motion s,Motion b,Listener sl,Listener bl,std::string&){CHECK(s==b);for(unsigned i=0;i<2;++i){auto& f=m->actors[n].state[i];f.motion=s;++f.generation;m->actors[n].listener[i]=i?bl:sl;}if(s==Motion::PickPut){CHECK(sl==Listener::SourceActor&&bl==Listener::None);events.push_back(12);}return true;}
bool SourceBank::enableMotionBlend(Navi* n,std::string&){auto& f=m->actors[n].state[1];f.motion=Motion::Nigeru;f.frame=10;++f.generation;m->actors[n].listener[1]=Listener::SourceActor;events.push_back(13);return true;}
bool SourceBank::finish(Navi* n,std::string&){for(auto& f:m->actors[n].state)f.finishing=true;events.push_back(14);return true;}
bool NativeState::sourceAlive(const Navi&)const{return true;}std::optional<std::uint8_t> NativeState::actorInvincibleFrames(const Navi&)const{return 0;}
bool NativeState::canEnterSourceDead(const Navi&)const{return true;}void NativeState::enterSourceDead(Navi&){}void NativeState::sourceDamageFeedback(Navi&){}
bool NativeState::canEnterSourceDamaged(const Navi&)const{return true;}void NativeState::enterSourceDamaged(Navi&,float){}
}}
SourceBank bank;
AttachmentSource* pc_p2_original_captain_attachment_source(const Navi*){return provider?&source:nullptr;}
const LoadedScene* pc_p2_original_captain_loaded_scene(){return &scene;}const World* pc_p2_original_captain_world(){return &scene;}SourceBank* pc_p2_original_captain_source_bank(){return &bank;}bool pc_p2_original_captain_actor_alive(const Navi*){return true;}
bool pc_p2_original_captain_transit(Navi* n,StateId id,std::string& e){return n->fsm->transit(n,nativeId(id))||(e="missing actual state",false);}
int main(){std::string e;NaviStateMachine fsm;registerStuckCarryBombStates(fsm);fsm.registerState(new Sink(StateId::Walk));for(auto& n:navis){n.fsm=&fsm;CHECK(bank.startMotion(&n,Motion::Wait,Motion::Wait,Listener::None,Listener::None,e));}
 auto* stuck=dynamic_cast<NativeState*>(fsm.find(nativeId(StateId::Stuck)));auto* carry=dynamic_cast<NativeState*>(fsm.find(nativeId(StateId::CarryBomb)));CHECK(stuck&&carry);CHECK(!stuck->sourceInvincible()&&stuck->sourcePressable()&&!stuck->sourceVsUsableY());CHECK(!carry->sourceInvincible()&&carry->sourcePressable()&&carry->sourceVsUsableY());
 provider=false;CHECK(!pc_p2_original_captain_stuck_bomb_preflight(&navis[0],StateId::Stuck,e));provider=true;CHECK(!pc_p2_original_captain_stuck_bomb_preflight(&navis[0],StateId::CarryBomb,e));
 CHECK(pc_p2_original_captain_transit(&navis[0],StateId::Stuck,e));CHECK(source.releases==1);CHECK(source.draws==0); // genuine release false is valid ignored return
 source.rng={.05f,.8f,.5f,.1f,.9f,.25f,.9f,0};for(unsigned i=0;i<9;++i){source.f.stickX=source.f.stickX==1?-1:1;stuck->exec(&navis[0]);CHECK(source.draws==0);}source.f.stickX=-source.f.stickX;stuck->exec(&navis[0]);CHECK(source.draws==8&&source.flicks.size()==2);CHECK(source.flicks[0].knock==170&&source.flicks[0].damage==5&&source.flicks[0].angle==-1000);float angle=std::atan2(source.f.stickX,0.f)+.9424779f*(-.5f);if(angle<0)angle+=6.2831853071795864769f;CHECK(std::fabs(source.flicks[1].angle-angle)<.00001f);CHECK(source.flicks[1].knock==145);
 unsigned draws=source.draws;source.f.delta=.21f;source.f.stickX=-source.f.stickX;stuck->exec(&navis[0]);source.f.delta=.01f;for(unsigned i=0;i<9;++i){source.f.stickX=-source.f.stickX;stuck->exec(&navis[0]);}CHECK(source.draws==draws); // timeout reset occurred after decrement
 source.f.stickCount=0;stuck->exec(&navis[0]);CHECK(fsm.last==nativeId(StateId::Walk));source.f.stickCount=1;
 CHECK(pc_p2_original_captain_transit(&navis[1],StateId::Stuck,e));source.f.controller=false;stuck->exec(&navis[1]);CHECK(fsm.last==nativeId(StateId::Walk));source.f.controller=true;
 CHECK(beginCarryBomb(&navis[0],std::nullopt,e));CHECK(fsm.last==nativeId(StateId::Walk));CHECK(source.startCalls==0);CHECK(!beginCarryBomb(&navis[0],BombHandle{&bombBody,99},e));missingClip=true;CHECK(!beginCarryBomb(&navis[0],source.bf.handle,e));missingClip=false;
 source.f.face=0;events.clear();CHECK(beginCarryBomb(&navis[0],source.bf.handle,e));CHECK(events==std::vector<int>({12,13,5,7}));CHECK(source.capture.values[3]==10&&source.capture.values[7]==24.8f&&source.capture.values[11]==47);MotionState self,bound;CHECK(bank.stateAnimator(&navis[0],Animator::Self,self,e)&&bank.stateAnimator(&navis[0],Animator::Bound,bound,e));CHECK(self.motion==Motion::PickPut&&bound.motion==Motion::Nigeru&&bound.frame==10);
 events.clear();CHECK(carry->sourceAnimationKey(&navis[0],1,e));CHECK(events.empty()); // no throw until A requested finish
 source.f.face=1.5707963267948966f;source.f.pressedA=true;source.f.pressedB=true;events.clear();carry->exec(&navis[0]);CHECK(events==std::vector<int>({1,8,14}));CHECK(fsm.last==nativeId(StateId::CarryBomb));CHECK(std::fabs(source.capture.values[3]-27)<.00001f&&std::fabs(source.capture.values[11]-30)<.00001f);CHECK(source.rotation.values[3]==0&&source.rotation.values[7]==0&&source.rotation.values[11]==0);
 events.clear();CHECK(carry->sourceAnimationKey(&navis[0],1,e));CHECK(events==std::vector<int>({6,9,10}));CHECK(std::fabs(source.velocity.x-260)<.00001f&&source.velocity.y==340&&std::fabs(source.velocity.z)<.0001f);unsigned ends=source.endCalls;CHECK(carry->sourceAnimationKey(&navis[0],1,e));CHECK(source.endCalls==ends);CHECK(!carry->sourceAnimationKey(&navis[0],1000,e)&&e.empty());CHECK(source.endCalls==ends&&fsm.last==nativeId(StateId::Walk));
 source.f.pressedA=false;source.f.pressedB=true;CHECK(beginCarryBomb(&navis[0],source.bf.handle,e));carry->exec(&navis[0]);CHECK(fsm.last==nativeId(StateId::Walk)&&source.endCalls==ends+1);source.f.pressedB=false;
 CHECK(beginCarryBomb(&navis[0],source.bf.handle,e));source.bf.capturedBy=nullptr;carry->exec(&navis[0]);CHECK(fsm.last==nativeId(StateId::Walk));
 CHECK(beginCarryBomb(&navis[0],source.bf.handle,e));scene.stage=Phase::Inactive;events.clear();carry->cleanup(&navis[0]);CHECK(events==std::vector<int>({10}));scene.stage=Phase::GameWorldActive;CHECK(pc_p2_original_captain_transit(&navis[0],StateId::Walk,e));
 source.expireOn=5;events.clear();CHECK(beginCarryBomb(&navis[0],source.bf.handle,e));CHECK(events==std::vector<int>({12,13,5}));events.clear();carry->cleanup(&navis[0]);CHECK(events.empty());CHECK(pc_p2_original_captain_transit(&navis[0],StateId::Walk,e));
 CHECK(beginCarryBomb(&navis[0],source.bf.handle,e));source.f.pressedA=true;carry->exec(&navis[0]);source.expireOn=6;events.clear();CHECK(!carry->sourceAnimationKey(&navis[0],1,e));CHECK(events==std::vector<int>({6}));CHECK(pc_p2_original_captain_transit(&navis[0],StateId::Walk,e));source.f.pressedA=false;
 CHECK(pc_p2_original_captain_transit(&navis[0],StateId::Stuck,e));source.expireOn=1;events.clear();stuck->exec(&navis[0]);CHECK(events==std::vector<int>({1}));CHECK(!stuck->sourceAnimationKey(&navis[0],1000,e));
 // Threshold equality and source iterator callback expiry are significant.
 CHECK(pc_p2_original_captain_transit(&navis[0],StateId::Walk,e));source.f.stickX=1;CHECK(pc_p2_original_captain_transit(&navis[0],StateId::Stuck,e));unsigned startDraws=source.draws;for(unsigned i=0;i<12;++i){source.f.stickX=.5f;stuck->exec(&navis[0]);source.f.stickX=1;stuck->exec(&navis[0]);}CHECK(source.draws==startDraws);source.f.stickX=.3f;for(unsigned i=0;i<12;++i)stuck->exec(&navis[0]);CHECK(source.draws==startDraws);
 source.f.stickX=1;CHECK(pc_p2_original_captain_transit(&navis[0],StateId::Stuck,e));source.rng={.9f,.4f,.1f};source.draws=0;source.expireOn=4;unsigned count=source.flicks.size();for(unsigned i=0;i<10;++i){source.f.stickX=-source.f.stickX;stuck->exec(&navis[0]);}CHECK(source.draws==3&&source.flicks.size()==count+1);CHECK(!stuck->sourceAnimationKey(&navis[0],0,e));
 CHECK(pc_p2_original_captain_transit(&navis[0],StateId::Walk,e));source.f.stickX=1;CHECK(pc_p2_original_captain_transit(&navis[0],StateId::Stuck,e));source.missingSticker=true;source.draws=0;for(unsigned i=0;i<10;++i){source.f.stickX=-source.f.stickX;stuck->exec(&navis[0]);}CHECK(source.draws==0);source.missingSticker=false;
 CHECK(pc_p2_original_captain_transit(&navis[0],StateId::Walk,e));CHECK(beginCarryBomb(&navis[0],source.bf.handle,e));source.f.pressedA=true;carry->exec(&navis[0]);source.bombExpiresOn=6;events.clear();CHECK(!carry->sourceAnimationKey(&navis[0],1,e));CHECK(events==std::vector<int>({6}));source.missingBomb=false;source.f.pressedA=false;CHECK(pc_p2_original_captain_transit(&navis[0],StateId::Walk,e));
 // Fresh source Stuck with no controller has no previous-stick observation.
 NaviStateMachine second;registerStuckCarryBombStates(second);second.registerState(new Sink(StateId::Walk));navis[1].fsm=&second;navis[1].current=nullptr;source.f.controller=false;CHECK(pc_p2_original_captain_transit(&navis[1],StateId::Stuck,e));auto* fresh=dynamic_cast<NativeState*>(navis[1].getCurrState());source.f.controller=true;unsigned oldControls=source.controls;fresh->exec(&navis[1]);CHECK(source.controls==oldControls+1);CHECK(!fresh->sourceAnimationKey(&navis[1],0,e));
 std::cout<<"original Stuck/CarryBomb engineering controls PASS checks="<<checks<<"\n";
}
