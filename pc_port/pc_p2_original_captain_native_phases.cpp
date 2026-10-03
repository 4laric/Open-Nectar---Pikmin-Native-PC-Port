#include "pc_p2_original_captain_native_phases.h"
#include "pc_p2_original_captain_native_water.h"
#include "pc_p2_original_captain_camera_pose.h"
#include "pc_p2_retail_scene.h"
#include "Navi.h"
#include <array>
#include <cmath>
namespace p2original {namespace captain {namespace bodyphases {
namespace {
bool fail(std::string& e,const char* s){e=s;return false;}
}
class NativeComposition final:public Provider {
public:
 const p2retail::SceneContext* context;Provider& source;SourceSceneTrace& trace;SourceBank& bank;
 const LoadedScene* loaded;std::uint64_t epoch,revision;std::string campaign,session,catalog;
 std::array<Navi*,2> actors;std::unique_ptr<Owner> phases;
 std::array<std::unique_ptr<water::NativeCache>,2> waters;
 std::optional<unsigned> lastFrame;bool frameInFlight=false;mutable unsigned readers=0;
 bool cameraResetInFlight=false,cameraResetReentered=false;
 NativeComposition(const p2retail::SceneContext& c,Provider& p,SourceSceneTrace& t,SourceBank& b)
  :context(&c),source(p),trace(t),bank(b),loaded(&p.scene()),epoch(loaded->incarnation()),revision(c.selectionRevision()),
   campaign(loaded->selectedCampaign()),session(loaded->selectedFingerprint()),catalog(loaded->sourceCatalog()),actors{loaded->captainAt(0),loaded->captainAt(1)}{}
 bool current()const {
  auto* s=pc_p2_original_captain_loaded_scene();auto* w=pc_p2_original_captain_world();
  return s&&s==loaded&&w&&epoch&&pc_p2_retail_scene_prepared()==context
   &&s->incarnation()==epoch&&s->selectedCampaign()==campaign&&s->selectedFingerprint()==session&&s->sourceCatalog()==catalog
   &&s->captainAt(0)==actors[0]&&s->captainAt(1)==actors[1]&&actors[0]&&actors[1]&&actors[0]!=actors[1]
   &&w->incarnation()==epoch&&w->selectedCampaign()==campaign&&w->selectedFingerprint()==session&&w->sourceCatalog()==catalog
   &&w->captainAt(0)==actors[0]&&w->captainAt(1)==actors[1]
   &&&source.scene()==s&&&trace.scene()==s&&pc_p2_original_captain_source_bank()==&bank
   &&context->snapshot().scene.serial==epoch&&context->selectionRevision()==revision
   &&context->campaignSha256()==campaign&&context->sessionSha256()==session;
 }
 int slot(const Navi* n)const{return n==actors[0]?0:n==actors[1]?1:-1;}
 const LoadedScene& scene()const override{return *loaded;}
 bool facts(const Navi& n,Facts& out,std::string& e)const override{
  ++readers;struct End {const NativeComposition* owner;~End(){--owner->readers;}} end{this};
  const int i=slot(&n);Facts f;std::optional<water::Handle> cached;
  if(!current()||i<0||!waters[i]||!source.facts(n,f,e)||!waters[i]->cached(cached,e)||!current())return false;
  f.inWater=bool(cached);out=f;return true;
 }
 bool water(Navi& n,const Sphere& sphere,std::string& e)override{
  const int i=slot(&n);return current()&&i>=0&&waters[i]&&waters[i]->animation(sphere,e);
 }
 bool cellLOD(Navi& n,float a,float b,std::string& e)override{return source.cellLOD(n,a,b,e);}
 bool animationKey(Navi& n,Animator a,Listener l,int k,std::string& e)override{return source.animationKey(n,a,l,k,e);}
 bool geometry(Navi& n,bool transform,std::string& e)override{return source.geometry(n,transform,e);}
 bool cursor(Navi& n,std::string& e)override{return source.cursor(n,e);}
 bool event(Navi& n,UpdateEvent event,std::string& e)override{return source.event(n,event,e);}
 bool menus(Navi& n,bool& handled,std::string& e)override{return source.menus(n,handled,e);}
 bool plateUpdate(Navi& n,std::string& e)override{return source.plateUpdate(n,e);}
 bool bounce(Navi& n,FloorHandle floor,std::string& e)override{return source.bounce(n,floor,e);}
 bool wall(Navi& n,Vec3 v,std::string& e)override{return source.wall(n,v,e);}
 bool random(float& r,std::string& e)override{return source.random(r,e);}
 bool walkEffect(Navi& n,bool water,std::string& e)override{return source.walkEffect(n,water,e);}
 bool belowMap(const Navi& n,bool& below,std::string& e)const override{return source.belowMap(n,below,e);}
 bool recoverBelowMap(Navi& n,std::string& e)override{return source.recoverBelowMap(n,e);}
 bool managerSimulationEnded(std::string& e)override{return source.managerSimulationEnded(e);}
 bool initialize(std::string& e){
  for(unsigned i=0;i<2;++i){
   if(!phases->initializeAfterBodyReset(actors[i],e))return false;
   waters[i]=water::NativeCache::create(*context,actors[i],*phases,e);
   if(!waters[i])return false;
  }
  return true;
 }
};
namespace {
// Stage explicitly retires this composition before any borrowed bank/scene or
// provider storage is reused. No static destructor touches stage heap objects.
NativeComposition* composition=nullptr;
}
bool createNativePhases(const p2retail::SceneContext& c,Provider& p,SourceSceneTrace& t,SourceBank& b,std::string& e){
 if(composition)return fail(e,"source captain phase composition already retains its scene");
 auto next=std::make_unique<NativeComposition>(c,p,t,b);const auto* w=pc_p2_original_captain_world();
 if(!next->current()||!w||w->phase()!=Phase::Loading)return fail(e,"source captain phase composition requires actual Loading reset");
 auto owner=Owner::create(*next,t,b,e);if(!owner)return false;
 next->phases=std::move(owner);composition=next.release();
 // Publish the real composition before authenticated cold body events. Failed
 // second initialization remains retained for checked stage retirement.
 if(!composition->initialize(e))return false;
 e.clear();return true;
}
bool retireNativePhases(const LoadedScene& scene,std::string& e){
 if(!composition){e.clear();return true;}
 const auto* w=pc_p2_original_captain_world();
 if(composition->loaded!=&scene||!composition->current()||!w||w->phase()!=Phase::Inactive)
  return fail(e,"source captain phase retirement requires exact retained inactive scene");
 if(composition->frameInFlight||composition->readers)return fail(e,"source captain phase retirement during actual frame/observation callback");
 if(!composition->phases->canRetire(e))return false;
 auto* old=composition;composition=nullptr;delete old;e.clear();return true;
}
bool cachedNativeWater(const Navi* n,bool& out,std::string& e){
 if(!composition||!composition->current())return fail(e,"cached source water composition absent");
 const int i=composition->slot(n);std::optional<water::Handle> cached;
 if(i<0||!composition->waters[i]||!composition->waters[i]->cached(cached,e))return false;
 out=bool(cached);return true;
}
bool tickNativePhases(float rate,std::string& e){
 if(!composition||!composition->current())return fail(e,"actual source captain frame composition unavailable");
 auto* live=composition;const auto* w=pc_p2_original_captain_world();
 if(!w||w->phase()!=Phase::GameWorldActive)return fail(e,"source captain frame requires genuine active World");
 if(live->frameInFlight)return fail(e,"source captain frame callback reentered dispatcher");
 live->frameInFlight=true;
 struct End {NativeComposition* owner;~End(){owner->frameInFlight=false;}} end{live};
 Facts frame;
 if(!live->facts(*live->actors[0],frame,e)||!live->current()||!frame.gamePaused||!frame.frameTimer
  ||!std::isfinite(rate)||rate<0||rate!=frame.deltaTime)return fail(e,"source captain frame lacks actual GameSystem counter/pause/time");
 if(live->lastFrame&&*live->lastFrame==*frame.frameTimer)return fail(e,"duplicate source captain GameSystem frame dispatch");
 // Claim the real frame before callbacks: failure cannot replay side effects.
 live->lastFrame=frame.frameTimer;
 if(*frame.gamePaused){e.clear();return true;} // NaviMgr::pausable() == true.
 if(!live->phases->managerAnimation(e)||composition!=live||!live->current())return false;
 return live->phases->managerSimulation(rate,e)&&composition==live&&live->current();
}
bool nativeAnimationFrame(const Navi* n,nativecontrol::AnimationFrame& out,std::string& e){
 if(!composition||!composition->current()||composition->slot(n)<0)return fail(e,"source animation observation lacks actual composition");
 auto* live=composition;Fields fields,after;Facts facts;
 if(!live->phases->readFields(n,fields,e)||!fields.faceDirectionOffset
  ||!live->facts(*n,facts,e)||!facts.gameFrozen||!live->phases->readFields(n,after,e)
  ||after.initializationSerial!=fields.initializationSerial||!live->current())return false;
 nativecontrol::AnimationFrame next;
 next.displacementKnown=bool(fields.previous);
 if(fields.previous)next.displacement={n->mSRT.t.x-fields.previous->x,n->mSRT.t.z-fields.previous->z};
 next.deltaTime=facts.deltaTime;next.faceDirectionOffset=*fields.faceDirectionOffset;next.gameFrozen=facts.gameFrozen;
 out=next;return true;
}
Owner* nativePhaseOwner(const Navi* n){
 if(!composition||!composition->current()||composition->slot(n)<0)return nullptr;
 const auto* w=pc_p2_original_captain_world();
 return w&&(w->phase()==Phase::Loading||w->phase()==Phase::GameWorldActive)?composition->phases.get():nullptr;
}
}}}
p2original::captain::bodyphases::Owner* pc_p2_original_captain_body_phase_owner(const Navi* n){return p2original::captain::bodyphases::nativePhaseOwner(n);}

namespace p2original {namespace captain {namespace camera {
struct CameraResetPoseScope::Impl {
 bodyphases::NativeComposition* owner=nullptr;const World* world=nullptr;
 bool bodyHeld=false;std::uint64_t bodyRevision=0;
 std::array<std::uint64_t,2> births{},motions{};std::array<const void*,2> states{};
 ~Impl(){if(owner){if(bodyHeld)owner->phases->endCameraReset();--owner->readers;owner->cameraResetInFlight=false;owner->cameraResetReentered=false;}}
 bool identity(std::string& error)const {
  if(!owner||bodyphases::composition!=owner||!owner->cameraResetInFlight||owner->cameraResetReentered||!owner->readers||
     pc_p2_original_captain_world()!=world||!world||world->phase()!=Phase::Loading||
     pc_p2_retail_scene_committed()!=owner->context||!owner->current()||!bodyHeld||!owner->phases->cameraResetCurrent(bodyRevision,error)){
   error="source camera reset lost exact Loading composition/committed floor";return false;
  }
  return true;
 }
 bool current(std::string& error)const {
  if(!identity(error))return false;
  for(unsigned slot=0;slot<2;++slot){
   auto* actor=owner->actors[slot];bodyphases::Fields fields;MotionState motion;
   if(actor->getCurrState()!=states[slot]||!owner->phases->readFields(actor,fields,error)||
      fields.initializationSerial!=births[slot]||!births[slot]||!identity(error)||
      !owner->bank.state(actor,motion,error)||motion.generation!=motions[slot]||!identity(error)||actor->getCurrState()!=states[slot]){
    error="source camera reset body birth/FSM/bank observation expired";return false;
   }
  }
  // Later actor observations cannot invalidate the already observed partner.
  for(unsigned slot=0;slot<2;++slot)if(owner->actors[slot]->getCurrState()!=states[slot]){
   error="source camera reset partner FSM changed";return false;
  }
  error.clear();return identity(error);
 }
};
CameraResetPoseScope::CameraResetPoseScope(std::unique_ptr<Impl> impl):m(std::move(impl)){}
CameraResetPoseScope::~CameraResetPoseScope()=default;
std::unique_ptr<CameraResetPoseScope> CameraResetPoseScope::begin(const p2retail::SceneContext& context,const LoadedScene& descriptor,std::string& error){
 auto* live=bodyphases::composition;
 if(live&&live->cameraResetInFlight){live->cameraResetReentered=true;error="source camera reset reentered actual pose scope";return {};}
 if(!live||live->context!=&context||live->loaded!=&descriptor||!live->phases||live->frameInFlight){
  error="source camera reset requires initialized actual Loading body phases and committed floor";return {};
 }
 auto next=std::make_unique<Impl>();next->owner=live;
 live->cameraResetInFlight=true;live->cameraResetReentered=false;++live->readers;
 // Retain storage before any virtual World/provider identity observations.
 next->world=pc_p2_original_captain_world();
 if(!next->world||next->world->phase()!=Phase::Loading||pc_p2_retail_scene_committed()!=&context||!live->current()){
  error="source camera reset requires actual Loading World and committed floor";return {};
 }
 if(!live->phases->beginCameraReset(next->bodyRevision,error))return {};
 next->bodyHeld=true;
 for(unsigned slot=0;slot<2;++slot){
  auto* actor=live->actors[slot];next->states[slot]=actor->getCurrState();bodyphases::Fields fields;MotionState motion;
  if(!next->identity(error)||!live->phases->readFields(actor,fields,error)||!next->identity(error)||
     !live->bank.state(actor,motion,error)||!next->identity(error)||actor->getCurrState()!=next->states[slot])return {};
  next->births[slot]=fields.initializationSerial;next->motions[slot]=motion.generation;
 }
 if(!next->current(error))return {};
 error.clear();return std::unique_ptr<CameraResetPoseScope>(new CameraResetPoseScope(std::move(next)));
}
bool CameraResetPoseScope::current(std::string& error)const{return m->current(error);}
bool CameraResetPoseScope::read(unsigned slot,ActorPose& out,std::string& error)const {
 if(slot>1||!m->current(error)){error="source camera initial pose lacks exact reset slot/scope";return false;}
 const Demo demo=m->world->demo();
 if((demo!=Demo::Absent&&demo!=Demo::Inactive)||!m->current(error)){
  error="source camera initial movie/model-translation authority unavailable";return false;
 }
 const auto* actor=m->owner->actors[slot];ActorPose next{{actor->mSRT.t.x,actor->mSRT.t.y,actor->mSRT.t.z},actor->mFaceDirection};
 if(!std::isfinite(next.position[0])||!std::isfinite(next.position[1])||!std::isfinite(next.position[2])||!std::isfinite(next.face)){
  error="source camera initial actor pose is nonfinite";return false;
 }
 if(!m->current(error))return false;
 if(m->world->demo()!=demo||!m->current(error)){
  error="source camera initial movie/body observation expired";return false;
 }
 out=next;error.clear();return true;
}
}}}
