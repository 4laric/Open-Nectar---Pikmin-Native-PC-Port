#include "pc_p2_original_piki_native_facts.h"
#include "pc_p2_original_piki_recruit.h"
#include "pc_p2_original_progress.h"
#include "Piki.h"
#include <cmath>
namespace p2original { namespace piki {
namespace {
bool fail(std::string& e,const char* t){e=t;return false;}
bool finite(const Vector3f& v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
}
bool nativeSceneBinding(SceneBinding& out,bool cleanup,std::string& e){
 auto* s=pc_p2_original_captain_loaded_scene();auto* w=pc_p2_original_captain_world();
 if(!s||!w||!s->incarnation()||s->incarnation()!=w->incarnation()
 ||s->selectedCampaign().empty()||s->selectedCampaign()!=w->selectedCampaign()
 ||s->selectedFingerprint().empty()||s->selectedFingerprint()!=w->selectedFingerprint()
 ||s->sourceCatalog().empty()||s->sourceCatalog()!=w->sourceCatalog()
 ||(!cleanup&&w->phase()==captain::Phase::Inactive))
  return fail(e,"formation has no canonical source scene");
 SceneBinding next;next.scene=s;next.incarnation=s->incarnation();
 next.campaign=s->selectedCampaign();next.fingerprint=s->selectedFingerprint();next.catalog=s->sourceCatalog();
 for(unsigned i=0;i<2;++i){next.captains[i]=s->captainAt(i);
  if(!next.captains[i]||next.captains[i]!=w->captainAt(i))return fail(e,"formation source roster unavailable");}
 if(next.captains[0]==next.captains[1])return fail(e,"formation requires two distinct source captains");
 out=std::move(next);return true;
}
bool nativeSceneCurrent(const SceneBinding& b,bool cleanup,std::string& e){
 SceneBinding next;if(!nativeSceneBinding(next,cleanup,e))return false;
 if(next.scene!=b.scene||next.incarnation!=b.incarnation||next.campaign!=b.campaign
 ||next.fingerprint!=b.fingerprint||next.catalog!=b.catalog
 ||next.captains[0]!=b.captains[0]||next.captains[1]!=b.captains[1])
  return fail(e,"formation scene incarnation/roster changed");
 return true;
}
bool nativeBodyFacts(Handle h,NativeBodyFacts& out,std::string& e){
 if(!h.body||!h.lifetime||!pc_p2_original_piki_body_current(h.body,h.lifetime))return fail(e,"stale formation body lifetime");
 OriginalPikiBodyHandle current;
 if(!pc_p2_original_piki_body_handle(h.body,current)||current.nativeLifetime!=h.lifetime)
  return fail(e,"formation requires committed GenPiki body");
 const auto& catalog=pc_p2_original_piki_catalog_fingerprint();
 const auto& progress=originalProgress();
 if(catalog.empty()||current.body.origin.catalogFingerprint!=catalog||!progress.ready()
 ||!pc_p2_original_piki_recruit_pair_ready())return fail(e,"formation campaign/catalog pair unavailable");
 PcP2SourceBody source;
 if(pc_p2_source_body_query(h.body,source)!=PcP2SourceBodyKind::GenPiki
 ||!pc_p2_source_body_admitted(source,progress.snapshot().campaign,catalog,e))return false;
 if(current.body.state.species>6||h.body->mHappa<0||h.body->mHappa>2||!finite(h.body->mSRT.t))
  return fail(e,"invalid native source kind/growth/position");
 if(!pc_p2_original_piki_body_current(h.body,h.lifetime))return fail(e,"formation body retired during read");
 NativeBodyFacts next;next.handle=h;next.species=current.body.state.species;
 next.happa=static_cast<unsigned>(h.body->mHappa);next.position=h.body->mSRT.t;next.body=std::move(current.body);
 out=std::move(next);return true;
}
bool nativeCaptainFacts(const SceneBinding& b,const Navi* n,bool cleanup,std::string& e){
 if(!nativeSceneCurrent(b,cleanup,e)||(n!=b.captains[0]&&n!=b.captains[1]))return fail(e,"formation captain not current source roster");
 // Cleanup follows nativeControl/retained throw-handle retirement. The exact
 // canonical scene roster still owns the native body; erased action/timers
 // cannot be required to release retained formation references.
 if(cleanup)return true;
 if(!pc_p2_original_captain_actor_alive(n))return fail(e,"formation captain source life unavailable or CF-dead");
 return nativeSceneCurrent(b,cleanup,e);
}
bool nativePhysicalFacts(Handle h,const PhysicalSource* source,PhysicalFacts& out,std::string& e){
 if(!source)return fail(e,"actual source physical facts producer unavailable");
 SceneBinding binding;NativeBodyFacts body;
 if(!nativeSceneBinding(binding,false,e)||&source->scene()!=binding.scene
 ||!nativeBodyFacts(h,body,e)||body.body.origin.catalogFingerprint!=pc_p2_original_piki_catalog_fingerprint()
 ||originalProgress().snapshot().campaign!=binding.campaign)return false;
 PhysicalFacts next;
 if(!source->readPhysical(h,next,e)||!nativeSceneCurrent(binding,false,e)
 ||!pc_p2_original_piki_body_current(h.body,h.lifetime))return false;
 out=next;return true;
}
} }
