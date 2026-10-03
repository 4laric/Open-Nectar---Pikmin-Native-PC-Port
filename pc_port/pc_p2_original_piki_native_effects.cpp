#include "pc_p2_original_piki_native_effects.h"
#include "pc_p2_original_piki_native_facts.h"
#include "pc_p2_original_captain_scene.h"
#include "pc_p2_piki_jpa_native.h"
#include "pc_p2_piki_jpa_render_scope.h"
#include "pc_p2_retail_scene.h"
#include "pc_randomizer.h"
#include <array>
#include <list>
#include <memory>
#include <thread>
#include <cmath>
#include <exception>
#include <cstdio>
#include <cstdlib>
namespace p2original {namespace piki {
namespace {
bool fail(std::string& e,const char* why){e=why;return false;}
bool same(Handle a,Handle b){return a.body==b.body&&a.lifetime==b.lifetime;}
struct Owner;
struct RenderScene final:pikiJPA::Scene {
 Owner& o;explicit RenderScene(Owner& v):o(v){}
 bool selectedCurrent(const pikiJPA::SelectedIdentity&,std::string&)const override;
 bool active()const noexcept override;
 bool position(const PcP2SourceBody&,pikiJPA::Position&,std::string&)const override;
 bool clipped(pikiJPA::Position,unsigned,bool&,std::string&)const override;
 bool beginHaloDraw(Graphics&,std::string&)override;
 bool endHaloDraw(Graphics&,std::string&)override;
};
struct Owner final:NativeEffects {
 SceneBinding binding;const p2retail::SceneContext* stage=nullptr;
 std::thread::id thread;std::uint64_t serial=0,revision=0;
 std::string stamp,packet;bool busy=false,reentered=false,started=false,prepared=false;
 RenderScene view; pikiJPA::HaloRenderScope render;
 std::unique_ptr<pikiJPA::Manager> manager;
 std::unique_ptr<pikiJPA::NativeEffects> halo;
 std::array<pikiJPA::EmitterHandle,6> shared{};
 struct Record {Handle handle;PcP2SourceBody source;bool freeAttempt=false,freeRegistered=false,throwAttempt=false;};
 std::list<Record> records;
 Owner():view(*this){}
 // This singleton is intentionally never destroyed. Failed cleanup retains
 // real resources and native borrows; stage heap reuse must remain refused.
 const captain::LoadedScene* retainedScene()const noexcept override{
  if(!prepared||!stage||!manager||!halo||std::this_thread::get_id()!=thread
   ||pc_p2_retail_scene_prepared()!=stage||pc_p2_original_captain_loaded_scene()!=binding.scene)
   return nullptr;
  if(!stage->ownsCurrentThread()||stage->nativeSerial()!=serial||stage->selectionRevision()!=revision)
   return nullptr;
  if(stage->campaignSha256()!=binding.campaign||stage->sessionSha256()!=binding.fingerprint
   ||binding.scene->incarnation()!=binding.incarnation||binding.scene->selectedCampaign()!=binding.campaign
   ||binding.scene->selectedFingerprint()!=binding.fingerprint||binding.scene->sourceCatalog()!=binding.catalog
   ||binding.scene->captainAt(0)!=binding.captains[0]||binding.scene->captainAt(1)!=binding.captains[1])return nullptr;
  try{std::string error;
   if(!pc_p2_original_captain_piki_jpa_current(manager->bank().selected(),error))return nullptr;
  }catch(...){return nullptr;}
  return binding.scene;
 }
 const captain::LoadedScene& scene()const override{
  // The reference ABI cannot express absence. Compare the live canonical
  // pointer BEFORE dereferencing; stale installed delegates must stop rather
  // than invent a scene or read a retired descriptor.
  if(!binding.scene||pc_p2_original_captain_loaded_scene()!=binding.scene){
   std::fputs("P2_PIKI_EFFECTS_STALE_SCENE_REFERENCE_REFUSED\n",stderr);std::fflush(stderr);std::abort();
  }
  return *binding.scene;
 }
 bool exact(std::string& e)const {
  if(!stage||!manager||std::this_thread::get_id()!=thread
   ||pc_p2_retail_scene_prepared()!=stage||!stage->ownsCurrentThread()
   ||!nativeSceneCurrent(binding,true,e))return fail(e,"Piki effects lost actual Stage/thread/scene ownership");
  if(stage->nativeSerial()!=serial||stage->selectionRevision()!=revision
   ||stage->campaignSha256()!=binding.campaign||stage->sessionSha256()!=binding.fingerprint
   ||pc_randomizer_session_fingerprint()!=packet)return fail(e,"Piki effects selected manager incarnation changed");
  return true;
 }
 bool action(std::string& e)const {
  if(!exact(e)||!pc_p2_original_captain_piki_jpa_current(manager->bank().selected(),e))return false;
  const auto* world=pc_p2_original_captain_world();
  if(!world||(world->phase()!=captain::Phase::Loading&&world->phase()!=captain::Phase::GameWorldActive))
   return fail(e,"Piki effect creation requires actual Loading or Active World");
  return true;
 }
 bool body(Handle h,PcP2SourceBody& out,std::string& e)const {
  if(!exact(e))return false;
  NativeBodyFacts facts;if(!nativeBodyFacts(h,facts,e))return false;
  PcP2SourceBody source;
  if(pc_p2_source_body_query(h.body,source)!=PcP2SourceBodyKind::GenPiki
   ||source.nativeBody!=h.body||source.nativeLifetime!=h.lifetime
   ||source.state.species!=facts.species||facts.body.origin.catalogFingerprint!=source.genPiki.origin.catalogFingerprint)
   return fail(e,"Piki effects body inventory/lifetime observation differs");
  if(!exact(e))return false;
  out=std::move(source);return true;
 }
 Record* find(Handle h){for(auto& r:records)if(same(r.handle,h))return &r;return nullptr;}
 const Record* find(Handle h)const{for(const auto& r:records)if(same(r.handle,h))return &r;return nullptr;}
 bool ownership(Ownership& out,std::string& e)const override {
  if(!exact(e))return false;
  Ownership next;
  next.inFlightOperations=busy?1:0;
  for(const auto& r:records){next.freeContexts+=r.freeAttempt;next.throwContexts+=r.throwAttempt;}
  if(halo&&halo->owners()!=registered())return fail(e,"Piki effects independent native halo census differs");
  out=next;e.clear();return true;
 }
 std::size_t registered()const{std::size_t n=0;for(const auto& r:records)n+=r.freeRegistered;return n;}
 bool readyToRetire(std::string& e)const {
  if(!exact(e)||render.retained())return fail(e,"Piki effects retained render owner");
  if(!records.empty()||(halo&&halo->owners()))return fail(e,"Piki effects retained actual body contexts");
  // All placement-created handles remain independently owned. Shared TPk
  // emitters can be deleted here only after the body/particle owner is empty.
  unsigned expected=0;for(const auto& h:shared)if(h){if(!manager->owns(h))return fail(e,"Piki shared emitter lifetime lost");++expected;}
  if(manager->live()!=expected)return fail(e,"Piki effects retained scene emitter owners");
  e.clear();return true;
 }
 bool canRetire(std::string& e)const override {
  if(busy)return fail(e,"Piki effects retained in-flight operation");
  return readyToRetire(e);
 }
 bool retire(std::string&)override;
 bool canRemoveFree(Handle h,std::string& e)const override {
  PcP2SourceBody source;if(!body(h,source,e))return false;
  const auto* r=find(h);if(r&&r->freeRegistered)return halo&&halo->canRemoveIdleHalo(r->source,e);
  e.clear();return true; // independently observed no admitted halo resource
 }
 bool free(Handle,bool,std::string&)override;
 bool canRemoveThrow(Handle h,std::string& e)const override {PcP2SourceBody s;return body(h,s,e);}
 bool thrown(Handle,bool,std::string&)override;
 bool hangSound(Handle h,std::string& e)override{return voice(h,0x2838,e);}
 bool landSound(Handle h,std::string& e)override{return voice(h,0x2804,e);}
 bool calledSound(Handle h,std::string& e)override{return voice(h,0x2801,e);}
 bool nudgeRumble(Handle,Navi*,std::string&)override;
 bool boreVoice(Handle h,bool sleep,std::string& e)override{return voice(h,sleep?0x283e:0x2834,e);}
 bool voice(Handle,unsigned,std::string&);
};
Owner& owner(){static auto* value=new Owner;return *value;}
struct Operation {
 Owner& o;bool entered=false;
 explicit Operation(Owner& value):o(value){if(o.busy)o.reentered=true;else{o.busy=true;o.reentered=false;entered=true;}}
 ~Operation(){if(entered)o.busy=false;}
 bool finish(bool result,std::string& e){return entered&&!o.reentered?result:fail(e,"Piki effects reentrant owner mutation refused");}
};
bool Owner::free(Handle h,bool enabled,std::string& e){
 Operation op(*this);if(!op.entered)return op.finish(false,e);
 try{
 PcP2SourceBody source;if(!body(h,source,e)||(enabled&&!action(e)))return false;
 auto* r=find(h);
 if(enabled){
  if(!started||!halo)return fail(e,"Piki effects require actual shared TPk manager start");
  if(!r){records.push_back({h,source,false,false,false});r=&records.back();}
  r->freeAttempt=true;
  if(!halo->sharedIdleHalo(r->source,source.state.species,pikiJPA::haloId(source.state.species),e))return op.finish(false,e);
  r->freeRegistered=true;return op.finish(exact(e),e);
 }
 if(!r||!r->freeAttempt){e.clear();return op.finish(exact(e),e);}
 if(r->freeRegistered&&(!halo||!halo->removeIdleHalo(r->source,e)))return op.finish(false,e);
 r->freeAttempt=false;r->freeRegistered=false;
 if(!r->throwAttempt)for(auto i=records.begin();i!=records.end();++i)if(&*i==r){records.erase(i);break;}
 return op.finish(exact(e),e);
 }catch(const std::exception& x){e=std::string("Piki retained free effect exception: ")+x.what();return false;}
}
bool Owner::thrown(Handle h,bool enabled,std::string& e){
 Operation op(*this);if(!op.entered)return op.finish(false,e);
 try{
 PcP2SourceBody source;if(!body(h,source,e)||(enabled&&!action(e)))return false;
 auto* r=find(h);
 if(enabled){
  if(!r){records.push_back({h,source,false,false,false});r=&records.back();}
  r->throwAttempt=true; // retained failed attempt, no fake blur/sphere owner
  return fail(e,"P2_PIKI_THROW_EFFECT_UNAVAILABLE: actual stripe/sphere backend required");
 }
 if(!r||!r->throwAttempt){e.clear();return op.finish(exact(e),e);}
 r->throwAttempt=false;
 if(!r->freeAttempt)for(auto i=records.begin();i!=records.end();++i)if(&*i==r){records.erase(i);break;}
 return op.finish(exact(e),e);
 }catch(const std::exception& x){e=std::string("Piki retained throw effect exception: ")+x.what();return false;}
}
bool Owner::retire(std::string& e){
 Operation op(*this);if(!op.entered||!readyToRetire(e))return false;
 if(reentered)return fail(e,"Piki retirement authority callback reentered owner");
 halo.reset(); // zero owners verified; native texture disposal is real.
 for(auto& h:shared)if(h&&!manager->erase(h,e))return false;
 // Floating-scene cleanup does not destroy the fixed source manager or reset
 // its successful-admission frontier. Its copied bank owns no Stage pointers.
 stage=nullptr;binding=SceneBinding{};stamp.clear();packet.clear();
 started=false;prepared=false;e.clear();return op.finish(true,e);
}
bool Owner::voice(Handle h,unsigned id,std::string& e){
 Operation op(*this);if(!op.entered)return op.finish(false,e);
 PcP2SourceBody source;if(!action(e)||!body(h,source,e))return false;
 if(!halo)return fail(e,"Piki JPA source shared manager has not started");
 return op.finish(halo->voice(source,id,90,e),e);
}
bool Owner::nudgeRumble(Handle h,Navi*,std::string& e){
 Operation op(*this);if(!op.entered)return op.finish(false,e);
 PcP2SourceBody s;if(!action(e)||!body(h,s,e))return false;
 return fail(e,"P2_PIKI_RUMBLE_UNAVAILABLE: actual source rumble owner required");
}
bool RenderScene::selectedCurrent(const pikiJPA::SelectedIdentity& s,std::string& e)const {
 if(!o.exact(e)||!pc_p2_original_captain_piki_jpa_current(s,e))return false;
 if(s.campaignSHA!=o.binding.campaign||s.packetSHA!=o.packet||s.session!=o.stamp)
  return fail(e,"Piki JPA full selected effect-owner stamp differs");
 e.clear();return true;
}
bool RenderScene::active()const noexcept {
 const auto* world=pc_p2_original_captain_world();
 return o.stage&&pc_p2_retail_scene_committed()==o.stage&&world
  &&pc_p2_original_captain_loaded_scene()==o.binding.scene
  &&world->phase()==captain::Phase::GameWorldActive&&pc_p2_retail_scene_game_active();
}
bool RenderScene::position(const PcP2SourceBody& retained,pikiJPA::Position& out,std::string& e)const {
 PcP2SourceBody current;Handle h{const_cast<Piki*>(retained.nativeBody),retained.nativeLifetime};
 if(!o.body(h,current,e))return false;
 const auto& a=retained.genPiki.origin;const auto& b=current.genPiki.origin;
 if(retained.kind!=current.kind||retained.state.species!=current.state.species
  ||a.sourceKey!=b.sourceKey||a.recordUid!=b.recordUid||a.attempt!=b.attempt
  ||a.activation!=b.activation||a.catalogFingerprint!=b.catalogFingerprint)
  return fail(e,"Piki JPA retained immutable body ancestry differs");
 NativeBodyFacts facts;if(!nativeBodyFacts(h,facts,e)||!o.exact(e))return false;
 out={facts.position.x,facts.position.y,facts.position.z};e.clear();return true;
}
bool RenderScene::clipped(pikiJPA::Position,unsigned,bool&,std::string& e)const {
 // Six authenticated BEM userWork=0x10 need actual source Viewport sphere
 // radiusS10 and disableCulling lifecycle. No host-P1 or always-visible fallback.
 return fail(e,"P2_PIKI_SOURCE_VIEWPORT_UNAVAILABLE: genuine radiusS10 culling producer required");
}
bool RenderScene::beginHaloDraw(Graphics& g,std::string& e){return o.exact(e)&&active()&&o.render.begin(g,e);}
bool RenderScene::endHaloDraw(Graphics& g,std::string& e){return o.exact(e)&&o.render.end(g,e);}
}
bool prepareNativeEffects(const p2retail::SceneContext& stage,std::string& e){
 auto& o=owner();Operation op(o);if(!op.entered)return op.finish(false,e);
 if(o.stage){
  if(o.stage==&stage&&o.prepared)return op.finish(o.exact(e)
   &&pc_p2_original_captain_piki_jpa_current(o.manager->bank().selected(),e),e);
  return fail(e,"Piki effects retain preceding or partial manager; checked retirement required");
 }
 SceneBinding binding;
 if(pc_p2_retail_scene_prepared()!=&stage||!stage.ownsCurrentThread()
  ||stage.phase()!=p2retail::ScenePhase::Prepared||!stage.stage()||!stage.map()
  ||!nativeSceneBinding(binding,false,e)||!pc_p2_original_captain_world()
  ||pc_p2_original_captain_world()->phase()!=captain::Phase::Loading
  ||stage.campaignSha256()!=binding.campaign||stage.sessionSha256()!=binding.fingerprint)
  return fail(e,"Piki effect manager birth requires actual selected Prepared Stage/Loading scene");
 const auto initialSerial=stage.nativeSerial(),initialRevision=stage.selectionRevision();
 const auto initialPhase=stage.phase();
 try{
  const auto* selected=pc_p2_original_captain_piki_jpa_bank(e);
  if(!selected)return false;
  // The canonical Captain scene owns selected17 inputs and the complete,
  // lossless session representation. This consumer only retains its bank.
  const pikiJPA::Bank bank=*selected;
  const auto identity=bank.selected();
  if(identity.campaignSHA!=binding.campaign||identity.packetSHA!=binding.fingerprint)
   return fail(e,"Piki canonical selected bank differs from retained Body binding");
  if(!pc_p2_original_captain_piki_jpa_current(identity,e))return false;
  if(o.reentered||pc_p2_retail_scene_prepared()!=&stage||stage.nativeSerial()!=initialSerial
   ||stage.selectionRevision()!=initialRevision||stage.phase()!=initialPhase||!nativeSceneCurrent(binding,false,e)
   ||pc_p2_original_captain_piki_jpa_bank(e)!=selected
   ||!pc_p2_original_captain_piki_jpa_current(identity,e))
   return fail(e,"Piki selected manager birth changed full Stage tuple");
  std::unique_ptr<pikiJPA::Manager> fresh;
  if(!o.manager)fresh=std::make_unique<pikiJPA::Manager>(bank); // actual first manager constructor0
  else if(!o.manager->rebindSelectedBank(bank,e))return false; // preserves frontier across scene reset
  o.binding=binding;o.serial=initialSerial;o.revision=initialRevision;
  o.stamp=identity.session;o.packet=identity.packetSHA;o.thread=std::this_thread::get_id();
  if(fresh)o.manager=std::move(fresh);
  o.stage=&stage; // retain before native renderer construction
  o.halo=std::make_unique<pikiJPA::NativeEffects>(o.view);
  if(!op.finish(o.exact(e),e))return false;
  o.prepared=true;return true;
 }catch(const std::exception& x){e=std::string("Piki effects resource birth exception: ")+x.what();return false;}
}
NativeEffects* nativeEffects(const captain::LoadedScene& scene,std::string& e){
 auto& o=owner();if(!o.prepared||!o.halo||o.binding.scene!=&scene||!o.exact(e)
  ||!pc_p2_original_captain_piki_jpa_current(o.manager->bank().selected(),e)){
  fail(e,"Piki effects concrete composition remains unavailable or partial");return nullptr;}
 e.clear();return &o;
}
bool canRetireNativeEffects(const p2retail::SceneContext& stage,std::string& e){
 auto& o=owner();if(o.stage!=&stage)return fail(e,"Piki effects retained Stage differs");return o.canRetire(e);
}
bool retireNativeEffects(const p2retail::SceneContext& stage,std::string& e){
 auto& o=owner();if(o.stage!=&stage)return fail(e,"Piki effects retained Stage differs");return o.retire(e);
}
bool createNativeSceneEmitter(const captain::LoadedScene& scene,unsigned id,pikiJPA::EmitterHandle& out,std::string& e){
 auto& o=owner();Operation op(o);
 if(!op.entered||o.binding.scene!=&scene||!o.action(e))return false;
 (void)id;(void)out;
 return fail(e,"P2_SCENE_EMITTER_UNAVAILABLE: nonshared actual emitter clock/backend required");
}
bool removeNativeSceneEmitter(const captain::LoadedScene& scene,pikiJPA::EmitterHandle& h,std::string& e){
 auto& o=owner();Operation op(o);
 if(!op.entered||o.binding.scene!=&scene||!o.exact(e))return false;
 for(const auto& shared:o.shared)if(shared==h)return fail(e,"Piki shared emitter is TPk-owner retained");
 return op.finish(o.manager->erase(h,e)&&o.exact(e),e);
}
bool startNativeSharedEffects(const captain::LoadedScene& scene,std::string& e){
 auto& o=owner();Operation op(o);
 if(!op.entered||!o.prepared||!o.halo||o.binding.scene!=&scene||!o.action(e))return false;
 if(o.started)return fail(e,"Piki TPk shared emitters already started");
 try{
 std::array<std::uint32_t,6> seeds{};
 for(unsigned i=0;i<6;++i){if(!o.shared[i]&&!o.manager->create(pikiJPA::haloId(i),o.shared[i],e))return false;
  if(!o.manager->owns(o.shared[i]))return fail(e,"Piki TPk shared emitter ownership differs");
  seeds[i]=o.shared[i]->seed();}
 if(!o.halo||!o.halo->prepare(o.manager->bank(),seeds,2000,e)||!o.exact(e))return false;
 o.started=true;return op.finish(true,e);
 }catch(const std::exception& x){e=std::string("Piki retained shared emitter start exception: ")+x.what();return false;}
}
bool nativeEffectsSourceFrame(const captain::LoadedScene& scene,std::string& e){
 auto& o=owner();Operation op(o);
 if(!op.entered||o.binding.scene!=&scene||!o.exact(e)||!o.started||!o.halo)return false;
 try{return op.finish(o.halo->sourceFrame(e)&&o.exact(e),e);}
 catch(const std::exception& x){e=std::string("Piki retained source frame exception: ")+x.what();return false;}
}
bool drawNativeEffects(const captain::LoadedScene& scene,Graphics& g,std::string& e){
 auto& o=owner();Operation op(o);
 if(!op.entered||o.binding.scene!=&scene||!o.exact(e)||!o.started||!o.halo)return false;
 return op.finish(o.halo->draw(g,e)&&o.exact(e),e);
}
bool nativeEffectFrontier(const captain::LoadedScene& scene,NativeEffectFrontier& out,std::string& e){
 auto& o=owner();if(o.binding.scene!=&scene||!o.exact(e))return false;
 out={o.manager->frontier(),o.manager->admissions(),o.manager->live()};e.clear();return true;
}
}}
