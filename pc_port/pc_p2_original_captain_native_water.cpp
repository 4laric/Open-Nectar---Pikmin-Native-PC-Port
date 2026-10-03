#include "pc_p2_original_captain_native_water.h"
#include "pc_p2_retail_scene.h"
namespace p2original {namespace captain {namespace water {
namespace {bool fail(std::string& e,const char* s){e=s;return false;}}
NativeCache::NativeCache(const p2retail::SceneContext& c,Navi* n,bodyphases::Owner& p)
 :context_(&c),actor_(n),phases_(&p),scene_(pc_p2_original_captain_loaded_scene()),
 serial_(c.snapshot().scene.serial),revision_(c.selectionRevision()),birth_(0),
 campaign_(c.campaignSha256()),session_(c.sessionSha256()),catalog_(scene_?scene_->sourceCatalog():""){}
std::unique_ptr<NativeCache> NativeCache::create(const p2retail::SceneContext& c,Navi* n,bodyphases::Owner& p,std::string& e){
 auto next=std::unique_ptr<NativeCache>(new NativeCache(c,n,p));bodyphases::Fields f;
 if(!p.readFields(n,f,e)||!f.initializationSerial)return {};
 next->birth_=f.initializationSerial;Owner owner;
 const auto* w=pc_p2_original_captain_world();
 if(!w||w->phase()!=Phase::Loading||!next->currentOwner(owner,e)||!initialize(next->cache_,*next,e))return {};
 return next;
}
bool NativeCache::currentOwner(Owner& out,std::string& e)const {
 const auto* s=pc_p2_original_captain_loaded_scene();const auto* w=pc_p2_original_captain_world();
 if(!s||s!=scene_||!w||!serial_||!birth_||pc_p2_retail_scene_prepared()!=context_
  ||s->incarnation()!=serial_||s->selectedCampaign()!=campaign_||s->selectedFingerprint()!=session_||s->sourceCatalog()!=catalog_
  ||w->incarnation()!=serial_||w->selectedCampaign()!=campaign_||w->selectedFingerprint()!=session_||w->sourceCatalog()!=catalog_
  ||(w->phase()!=Phase::Loading&&w->phase()!=Phase::GameWorldActive)
  ||(s->captainAt(0)!=actor_&&s->captainAt(1)!=actor_)
  ||pc_p2_original_captain_body_phase_owner(actor_)!=phases_)
  return fail(e,"cached source water lost canonical scene/body phase owner");
 if(context_->snapshot().scene.serial!=serial_||context_->selectionRevision()!=revision_
  ||context_->campaignSha256()!=campaign_||context_->sessionSha256()!=session_)
  return fail(e,"cached source water selected scene was replaced");
 bodyphases::Fields f;bool alive;
 if(!pc_p2_original_captain_actor_lifetime(actor_,alive)||!phases_->readFields(actor_,f,e)||f.initializationSerial!=birth_)
  return fail(e,"cached source water actual actor birth expired");
 out={s,actor_,serial_,birth_};return true;
}
bool NativeCache::coldInitialization(ColdInitialization& out,std::string& e)const {
 Owner owner;if(!currentOwner(owner,e))return false;
 out={owner,ColdEvent::InitFakePiki,birth_};return true;
}
bool NativeCache::boundingSphere(Sphere& out,std::string& e)const {
 Owner owner;bodyphases::Fields f;
 if(!currentOwner(owner,e)||!phases_->readFields(actor_,f,e)||!f.bounding)
  return fail(e,"source FakePiki cached sphere center has not been written");
 const auto& s=*f.bounding;out={s.center.x,s.center.y,s.center.z,s.radius};return true;
}
bool NativeCache::mapPresent(std::optional<bool>& out,std::string& e)const {
 Owner owner;if(!currentOwner(owner,e))return false;
 if(!context_->map())return fail(e,"selected source scene map ownership unavailable");
 out=true;return true;
}
bool NativeCache::findWater(const Sphere& sphere,std::optional<Handle>& out,std::string& e)const {
 Owner owner;if(!currentOwner(owner,e))return false;
 p2retail::SourceWaterResult result;
 if(!pc_p2_retail_scene_find_water(*context_,serial_,revision_,{sphere.x,sphere.y,sphere.z},result,e)
  ||!currentOwner(owner,e))return false;
 if(result.state!=p2retail::SourceWaterState::KnownDry||result.nativeSerial!=serial_
  ||result.selectionRevision!=revision_||!result.inputs||!result.geometry||!result.registeredRooms)
  return fail(e,"source registered empty SeaMgr query lacked exact owner identity");
 out.reset();return true;
}
// Scene930 currently constructs only the actual selected count-zero SeaMgr.
// It cannot produce a WaterBox; refuse every nonempty handle/callback instead
// of fabricating a volume or calling P1 effects.
bool NativeCache::liveWater(Handle,std::string& e)const{return fail(e,"selected empty source SeaMgr has no WaterBox lifetime");}
bool NativeCache::containsSphere(Handle,const Sphere&,bool&,std::string& e)const{return fail(e,"selected empty source SeaMgr has no containsSphere receiver");}
bool NativeCache::inWaterCallback(Handle,std::string& e){return fail(e,"source water entry requires a genuine WaterBox/effects producer");}
bool NativeCache::outWaterCallback(std::string& e){return fail(e,"source water exit requires a genuine retained water/effects producer");}
bool NativeCache::animation(const bodyphases::Sphere& given,std::string& e){
 Sphere actual;if(!boundingSphere(actual,e))return false;
 if(actual.x!=given.center.x||actual.y!=given.center.y||actual.z!=given.center.z||actual.radius!=given.radius)
  return fail(e,"source animation water argument differs from actual cached sphere");
 return checkWater(cache_,*this,e);
}
bool NativeCache::cached(std::optional<Handle>& out,std::string& e)const{return cachedWater(cache_,*this,out,e);}
}}}
