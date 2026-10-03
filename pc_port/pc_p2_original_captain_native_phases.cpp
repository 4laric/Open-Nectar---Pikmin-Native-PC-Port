#include "pc_p2_original_captain_native_phases.h"
#include "pc_p2_original_captain_native_water.h"
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
