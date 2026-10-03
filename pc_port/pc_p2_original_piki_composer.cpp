#include "pc_p2_original_piki_composer.h"
#include "pc_p2_retail_scene.h"
#include <memory>
#include <exception>
namespace p2original { namespace piki {
namespace {
bool fail(std::string& e,const char* text){e=text;return false;}
struct Composition;
struct BankView final : AnimationBank {
 Composition& owner;
 explicit BankView(Composition& value):owner(value){}
 const captain::LoadedScene& scene()const override;
 bool current(std::string&)const override;
 Shape* shape(const std::string& role)const override{return NativeBodyBank::instance().shape(role);}
};
// Deliberately process retained: an exception or callback refusal must never
// destroy a partially prepared animator, plate, bank or producer borrow.
struct Composition {
 const p2retail::SceneContext* stage=nullptr;
 const captain::LoadedScene* scene=nullptr;
 const captain::World* world=nullptr;
 std::uint64_t serial=0,revision=0,incarnation=0;
 std::string campaign,session,catalog;
 Navi* first=nullptr;Navi* second=nullptr;
 NativeCaptainReader* captains=nullptr;PlateSource* plateSource=nullptr;
 NativeEffects* effects=nullptr;NativeTaskEnvironment* environment=nullptr;
 PhysicalSource* physical=nullptr;
 BankView view;
 std::unique_ptr<NativeAnimator> animator;
 std::unique_ptr<Plate> plate;
 std::unique_ptr<NativeServices> services;
 bool busy=false,reentered=false,prepared=false,installed=false;
 Composition():view(*this){}
 bool exact()const {
  // Compare current pointers before dereferencing any retained descriptor.
  return stage&&scene&&world&&pc_p2_retail_scene_prepared()==stage
   &&pc_p2_original_captain_loaded_scene()==scene&&pc_p2_original_captain_world()==world
   &&stage->ownsCurrentThread()&&stage->stage()&&stage->map()
   &&stage->nativeSerial()==serial&&stage->selectionRevision()==revision
   &&stage->campaignSha256()==campaign&&stage->sessionSha256()==session
   &&scene->incarnation()==incarnation&&world->incarnation()==incarnation
   &&scene->selectedCampaign()==campaign&&world->selectedCampaign()==campaign
   &&scene->selectedFingerprint()==session&&world->selectedFingerprint()==session
   &&scene->sourceCatalog()==catalog&&world->sourceCatalog()==catalog
   &&scene->captainAt(0)==first&&scene->captainAt(1)==second
   &&world->captainAt(0)==first&&world->captainAt(1)==second;
 }
 bool observed(std::string& e)const {
  if(!exact()||!NativeBodyBank::instance().current())return fail(e,"native composition lost selected Stage/bank ownership");
  const auto phase=world->phase();const auto stagePhase=stage->phase();
  // Each producer observation may call another owner. Recheck after EVERY
  // callback, before dereferencing/dispatching through the next retained one.
  if(!captains||&captains->scene()!=scene||!exact()||world->phase()!=phase||stage->phase()!=stagePhase)
   return fail(e,"native composition Captain observation changed authority");
  if(&captains->plateSource()!=plateSource||!exact()||world->phase()!=phase||stage->phase()!=stagePhase)
   return fail(e,"native composition PlateSource observation changed authority");
  if(!plateSource||&plateSource->scene()!=scene||!exact()||world->phase()!=phase||stage->phase()!=stagePhase)
   return fail(e,"native composition Plate observation changed authority");
  if(!effects||&effects->scene()!=scene||!exact()||world->phase()!=phase||stage->phase()!=stagePhase)
   return fail(e,"native composition effect observation changed authority");
  if(!environment||&environment->scene()!=scene||!exact()||world->phase()!=phase||stage->phase()!=stagePhase)
   return fail(e,"native composition environment observation changed authority");
  if(!physical||&physical->scene()!=scene||!exact()||world->phase()!=phase||stage->phase()!=stagePhase)
   return fail(e,"native composition physical observation changed authority");
  if(reentered||!NativeBodyBank::instance().current()||!exact()||world->phase()!=phase||stage->phase()!=stagePhase)
   return fail(e,"native composition reentered or changed after producer observations");
  e.clear();return true;
 }
};
Composition& owner(){static auto* value=new Composition;return *value;}
struct Operation {
 Composition& value;bool entered=false;
 explicit Operation(Composition& v):value(v){if(value.busy){value.reentered=true;}else{value.busy=true;value.reentered=false;entered=true;}}
 ~Operation(){if(entered)value.busy=false;}
};
const captain::LoadedScene& BankView::scene()const{return *owner.scene;}
bool BankView::current(std::string& e)const{return owner.exact()&&NativeBodyBank::instance().current()?true:fail(e,"source animation bank lost exact native composition");}
}
bool prepareNativeComposition(const p2retail::SceneContext& stage,NativeCaptainReader& captains,
 NativeEffects& effects,NativeTaskEnvironment& environment,PhysicalSource& physical,std::string& e){
 auto& o=owner();Operation operation(o);
 const auto* scene=pc_p2_original_captain_loaded_scene();const auto* world=pc_p2_original_captain_world();
 if(!operation.entered||pc_p2_retail_scene_prepared()!=&stage||!stage.ownsCurrentThread()
  ||!stage.stage()||!stage.map()||stage.phase()!=p2retail::ScenePhase::Prepared||!scene||!world||world->phase()!=captain::Phase::Loading
  ||!scene->captainAt(0)||!scene->captainAt(1)||scene->captainAt(0)==scene->captainAt(1))
  return fail(e,"native composition requires actual retained Stage and Loading two-captain roster");
 if(o.stage){
  if(o.stage!=&stage||o.scene!=scene||o.world!=world||o.captains!=&captains
   ||o.effects!=&effects||o.environment!=&environment||o.physical!=&physical)
   return fail(e,"native composition retains another scene or producer set");
 }else{
  // Fallible immutable strings copied before retaining the binding. No
  // native/graphics resource has been touched if copying fails.
  const auto campaign=stage.campaignSha256(),session=stage.sessionSha256(),catalog=scene->sourceCatalog();
  o.campaign=campaign;o.session=session;o.catalog=catalog;
  o.scene=scene;o.world=world;o.serial=stage.nativeSerial();o.revision=stage.selectionRevision();
  o.incarnation=scene->incarnation();o.first=scene->captainAt(0);o.second=scene->captainAt(1);
  o.captains=&captains;o.effects=&effects;o.environment=&environment;o.physical=&physical;
  o.stage=&stage; // Ownership exists BEFORE any native model preparation.
 }
 if(!o.exact())return fail(e,"native composition descriptor pairing refused");
 try{
  if(!NativeBodyBank::instance().current()){
   if(NativeBodyBank::instance().owned())return fail(e,"native composition retains incomplete model graph; cleanup required");
   if(!NativeBodyBank::instance().prepare(stage,e)||!o.exact())return false;
  }
  // Retain the actual callback-supplied PlateSource before construction. It
  // cannot be substituted on a retry of this same retained composition.
  auto* source=&captains.plateSource();
  if(!o.exact()||o.reentered)return fail(e,"Captain changed composition during PlateSource observation");
  if(o.plateSource&&o.plateSource!=source)return fail(e,"retained native PlateSource changed");
  o.plateSource=source;
  if(!o.observed(e))return false;
  if(!o.animator)o.animator=std::make_unique<NativeAnimator>(o.view);
  if(!o.plate)o.plate=std::make_unique<Plate>(*o.plateSource);
  if(!o.services)o.services=std::make_unique<NativeServices>(NativeBodyBank::instance(),*o.animator,*o.plate,captains,effects,environment,physical);
  if(!o.observed(e))return false;
  if(!o.prepared){if(!o.animator->prepare(1,e)||!o.observed(e))return false;o.prepared=true;}
  // Real source registration is after resources/producers exist; this grants
  // neither physical body creation nor World activation.
  if(!o.installed){if(!installServices(*o.services))return fail(e,"native Services already has another process owner");o.installed=true;}
  if(!o.observed(e))return fail(e,"native Services final authority refused");
  e.clear();return true;
 }catch(...){return fail(e,"native composition construction failed; partial owners retained");}
}
Plate* nativePlate(const captain::LoadedScene& scene,std::string& e){auto& o=owner();Operation op(o);return op.entered&&o.scene==&scene&&o.plate&&o.observed(e)?o.plate.get():nullptr;}
NativeServices* nativeServices(const captain::LoadedScene& scene,std::string& e){auto& o=owner();Operation op(o);return op.entered&&o.scene==&scene&&o.services&&o.prepared&&o.installed&&o.observed(e)?o.services.get():nullptr;}
PhysicalSource* nativePhysicalSource(const captain::LoadedScene& scene,std::string& e){auto& o=owner();Operation op(o);return op.entered&&o.scene==&scene&&o.physical&&o.observed(e)?o.physical:nullptr;}
} }


