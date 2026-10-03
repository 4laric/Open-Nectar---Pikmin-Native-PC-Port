#include "pc_p2_original_piki_native_services.h"
#include "pc_p2_original_captain_damage.h"

namespace p2original { namespace piki {
struct NativeServices::Observation {
 const captain::LoadedScene* scene=nullptr;const captain::World* world=nullptr;
 std::uint64_t incarnation=0;std::string campaign,fingerprint,catalog;
 const Navi* first=nullptr;const Navi* second=nullptr;
 captain::Phase phase=captain::Phase::Inactive;
};
NativeServices::NativeServices(NativeBodyBank& b,NativeAnimator& a,Plate& p,
 NativeCaptainReader& c,NativeEffects& e,NativeTaskEnvironment& t,PhysicalSource& f)
 :bank(b),animator(a),plate(p),captains(c),effects(e),environment(t),physical(f){}
const captain::LoadedScene& NativeServices::scene()const{return captains.scene();}
bool NativeServices::beginObservation(Observation& o,std::string& e)const{
 if(!bound(e))return false;
 o.scene=pc_p2_original_captain_loaded_scene();o.world=pc_p2_original_captain_world();
 o.incarnation=o.scene->incarnation();o.campaign=o.scene->selectedCampaign();
 o.fingerprint=o.scene->selectedFingerprint();o.catalog=o.scene->sourceCatalog();
 o.first=o.scene->captainAt(0);o.second=o.scene->captainAt(1);o.phase=o.world->phase();
 return true;
}
bool NativeServices::finishObservation(const Observation& o,std::string& e)const{
 // Recheck pointers BEFORE dereferencing a descriptor borrowed before dispatch.
 const auto matches=[&](){return pc_p2_original_captain_loaded_scene()==o.scene
  &&pc_p2_original_captain_world()==o.world&&o.scene->incarnation()==o.incarnation
  &&o.world->incarnation()==o.incarnation&&o.scene->selectedCampaign()==o.campaign
  &&o.world->selectedCampaign()==o.campaign&&o.scene->selectedFingerprint()==o.fingerprint
  &&o.world->selectedFingerprint()==o.fingerprint&&o.scene->sourceCatalog()==o.catalog
  &&o.world->sourceCatalog()==o.catalog&&o.scene->captainAt(0)==o.first
  &&o.world->captainAt(0)==o.first&&o.scene->captainAt(1)==o.second
  &&o.world->captainAt(1)==o.second&&o.world->phase()==o.phase;};
 if(!matches()||!bound(e)||!matches()){e="native Body Services producer changed observation authority";return false;}
 return true;
}
bool NativeServices::finishBodyObservation(const Observation& o,Handle h,std::string& e)const{
 // finishObservation itself observes virtual producer owners. They can retire
 // this body without changing the scene. Check the canonical body map only
 // AFTER those final callbacks, immediately before publishing any body result.
 if(!finishObservation(o,e)||!pc_p2_original_piki_body_current(h.body,h.lifetime)){
  e="native Body Services producer changed body lifetime";return false;
 }
 return true;
}
bool NativeServices::bound(std::string& error)const{
 const auto* loaded=pc_p2_original_captain_loaded_scene();
 const auto* world=pc_p2_original_captain_world();
 if(!loaded||!world){error="native Body Services has no canonical owner";return false;}
 const auto incarnation=loaded->incarnation();
 const auto campaign=loaded->selectedCampaign(),fingerprint=loaded->selectedFingerprint(),catalog=loaded->sourceCatalog();
 const auto phase=world->phase();
 const auto* first=loaded->captainAt(0);const auto* second=loaded->captainAt(1);
 if(!bank.current()||&captains.scene()!=loaded
    ||effects.retainedScene()!=loaded||&environment.scene()!=loaded||&physical.scene()!=loaded
    ||&captains.plateSource().scene()!=loaded){
  error="native Body Services lost its actual selected resource/producer owner";return false;
 }
 if(pc_p2_original_captain_loaded_scene()!=loaded||pc_p2_original_captain_world()!=world
    ||loaded->incarnation()!=incarnation||world->incarnation()!=incarnation
    ||loaded->selectedCampaign()!=campaign||world->selectedCampaign()!=campaign
    ||loaded->selectedFingerprint()!=fingerprint||world->selectedFingerprint()!=fingerprint
    ||loaded->sourceCatalog()!=catalog||world->sourceCatalog()!=catalog
    ||loaded->captainAt(0)!=first||world->captainAt(0)!=first
    ||loaded->captainAt(1)!=second||world->captainAt(1)!=second
    ||world->phase()!=phase||!bank.current()){
  error="native Body Services canonical scene changed";return false;
 }
 return true;
}
const std::string& NativeServices::pikiParameterBytes()const{
 const auto* bytes=bank.bytes("p2-original/piki-bodies/red/pikiParms.txt");
 // Empty is deliberately rejected by the source parameter parser. It is not
 // a fallback parameter set or an initialization/ownership grant.
 static const std::string absent;
 return bytes?*bytes:absent;
}
const std::string& NativeServices::naviParameterBytes()const{return captains.naviParameterBytes();}
bool NativeServices::gravity(float& v,std::string& e)const{Observation o;float next=0;if(!beginObservation(o,e)||!environment.gravity(next,e)||!finishObservation(o,e))return false;v=next;return true;}
bool NativeServices::captainFrame(const Navi* n,CaptainFrame& f,std::string& e)const{Observation o;CaptainFrame next;if(!beginObservation(o,e)||!captains.frame(n,next,e)||!finishObservation(o,e))return false;f=next;return true;}
bool NativeServices::bodyAlive(Handle h,bool& alive,std::string& e)const{Observation o;PhysicalFacts next;if(!beginObservation(o,e)||!nativePhysicalFacts(h,&physical,next,e)||!finishBodyObservation(o,h,e))return false;alive=next.alive;return true;}
bool NativeServices::supports(Handle h,Motion m,std::string& e)const{return bound(e)&&animator.supports(h,m,e);}
bool NativeServices::motion(Handle h,Motion m,std::string& e){return bound(e)&&animator.start(h,m,e);}
bool NativeServices::animate(Handle h,float dt,std::string& e){return bound(e)&&animator.advance(h,dt,e);}
bool NativeServices::currentMotion(Handle h,Motion& m,std::string& e)const{Observation o;Motion next;float rate=0;bool done=false;if(!beginObservation(o,e)||!animator.status(h,next,rate,done,e)||!finishBodyObservation(o,h,e))return false;m=next;return true;}
bool NativeServices::canRemoveFreeEffects(Handle h,std::string& e)const{return bound(e)&&effects.canRemoveFree(h,e);}
bool NativeServices::freeEffects(Handle h,bool enabled,std::string& e){return bound(e)&&effects.free(h,enabled,e);}
bool NativeServices::canRemoveThrowEffects(Handle h,std::string& e)const{return bound(e)&&effects.canRemoveThrow(h,e);}
bool NativeServices::throwEffects(Handle h,bool enabled,std::string& e){return bound(e)&&effects.thrown(h,enabled,e);}
bool NativeServices::hangSound(Handle h,std::string& e){return bound(e)&&effects.hangSound(h,e);}
bool NativeServices::landSound(Handle h,std::string& e){return bound(e)&&effects.landSound(h,e);}
bool NativeServices::calledSound(Handle h,std::string& e){return bound(e)&&effects.calledSound(h,e);}
bool NativeServices::nudgeRumble(Handle h,Navi* n,std::string& e){return bound(e)&&effects.nudgeRumble(h,n,e);}
bool NativeServices::allocateSlot(Handle h,Navi* n,int& s,std::string& e){return bound(e)&&plate.allocateSlot(h,n,s,e);}
bool NativeServices::canReleaseSlot(Handle h,Navi* n,int s,std::string& e)const{return bound(e)&&plate.canReleaseSlot(h,n,s,e);}
bool NativeServices::releaseSlot(Handle h,Navi* n,int s,std::string& e){return bound(e)&&plate.releaseSlot(h,n,s,e);}
bool NativeServices::slotPosition(Handle h,Navi* n,int s,Vector3f& p,std::string& e)const{Observation o;Vector3f next;if(!beginObservation(o,e)||!plate.slotPosition(h,n,s,next,e)||!finishBodyObservation(o,h,e))return false;p=next;return true;}
bool NativeServices::formed(Handle h,Navi* n,std::string& e){return bound(e)&&plate.formed(h,n,e);}
bool NativeServices::sortSlot(Handle h,Navi* n,int s,int happa,std::string& e){return bound(e)&&plate.sortSlot(h,n,s,happa,e);}
bool NativeServices::freeTaskAvailable(Handle h,FreeSearch search,bool& available,std::string& e)const{Observation o;bool next=false;if(!beginObservation(o,e)||!environment.task(h,search,next,e)||!finishBodyObservation(o,h,e))return false;available=next;return true;}
bool NativeServices::animationStatus(Handle h,Motion& m,float& rate,bool& completed,std::string& e)const{Observation o;Motion nm;float nr=0;bool nc=false;if(!beginObservation(o,e)||!animator.status(h,nm,nr,nc,e)||!finishBodyObservation(o,h,e))return false;m=nm;rate=nr;completed=nc;return true;}
bool NativeServices::animationSpeed(Handle h,float rate,std::string& e){return bound(e)&&animator.speed(h,rate,e);}
bool NativeServices::finishMotion(Handle h,std::string& e){return bound(e)&&animator.finish(h,e);}
bool NativeServices::loopStart(Handle h,std::string& e){return bound(e)&&animator.loopStart(h,e);}
bool NativeServices::boreVoice(Handle h,bool sleep,std::string& e){return bound(e)&&effects.boreVoice(h,sleep,e);}
bool NativeServices::random(float& v,std::string& e){Observation o;float next=0;if(!beginObservation(o,e)||!environment.random(next,e)||!finishObservation(o,e))return false;v=next;return true;}
} }
