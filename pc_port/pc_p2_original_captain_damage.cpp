#include "pc_p2_original_captain_damage.h"
#include "pc_p2_original_actor.h"
#include "pc_p2_original_progress.h"
#include "pc_p2_equipment.h"
#include "pc_p2_original_piki_origin.h"
#include "pc_p2_original_source_uid.h"
#include "pc_p2_original_piki_recruit.h"
#include "Navi.h"
#include "Piki.h"
#include "NaviState.h"
#include <cmath>
extern const p2original::captain::World* pc_p2_original_captain_world() __attribute__((weak));
extern bool pc_randomizer_original_session() __attribute__((weak));
extern std::string pc_randomizer_original_campaign() __attribute__((weak));
extern std::string pc_randomizer_session_fingerprint() __attribute__((weak));
extern bool pc_p2_original_piki_body_handle(const Piki*,OriginalPikiBodyHandle&) __attribute__((weak));
extern bool pc_p2_original_piki_body_current(const Piki*,std::uint64_t) noexcept __attribute__((weak));
extern bool pc_p2_original_captain_actor_lifetime(const Navi*,bool&) __attribute__((weak));
extern const p2original::captain::LoadedScene* pc_p2_original_captain_loaded_scene() __attribute__((weak));
extern const std::string& pc_p2_original_piki_catalog_fingerprint() noexcept __attribute__((weak));
extern bool pc_p2_original_piki_recruit_pair_ready() noexcept __attribute__((weak));
namespace p2original { namespace captain {
bool selectedOriginal(){return pc_randomizer_original_session&&pc_randomizer_original_session();}
namespace {
Refusal worldFor(const Navi* n,const World*& world) {
 world=pc_p2_original_captain_world?pc_p2_original_captain_world():nullptr;
 if(!world||!world->incarnation())return Refusal::MissingWorld;
 if(!pc_randomizer_original_session||!pc_randomizer_original_session()
  ||!pc_randomizer_original_campaign||!pc_randomizer_session_fingerprint)
  return Refusal::WrongSession;
 const auto campaign=pc_randomizer_original_campaign();
 const auto fingerprint=pc_randomizer_session_fingerprint();
 if(campaign.empty()||fingerprint.empty()||world->selectedCampaign()!=campaign
  ||world->selectedFingerprint()!=fingerprint)return Refusal::WrongSession;
 if(world->phase()!=Phase::GameWorldActive)return Refusal::InactiveWorld;
 if(!n||(world->captainAt(0)!=n&&world->captainAt(1)!=n))return Refusal::MissingCaptain;
 return Refusal::None;
}
}
DamageResult addDamage(Navi* n,float raw,bool feedback) {
 const World* world=nullptr;DamageResult result;
 result.refusal=worldFor(n,world);if(result.refusal!=Refusal::None)return result;
 if(world->demo()==Demo::Unknown){result.refusal=Refusal::MissingMovieAuthority;return result;}
 if(world->demo()==Demo::Playing){result.refusal=Refusal::DemoPlaying;return result;}
 if(!std::isfinite(raw)||raw<0){result.refusal=Refusal::InvalidDamage;return result;}
 // Retail 0x80144610 applies the authenticated shield once inside its outer
 // movie/world guard, before the alive/FSM/actor invincibility rejection.
 const float damage=pc_p2_equipment_damage(raw);
 if(!std::isfinite(damage)||damage<0){result.refusal=Refusal::InvalidDamage;return result;}
 auto* native=n->getCurrState();
 auto* state=dynamic_cast<State*>(native);
 if(!state||state->nativeState()!=native){result.refusal=Refusal::MissingSourceState;return result;}
 if(!state->sourceAlive(*n)){result.refusal=Refusal::NotAlive;return result;}
 if(state->sourceInvincible()){result.refusal=Refusal::StateInvincible;return result;}
 const auto frames=state->actorInvincibleFrames(*n);
 if(!frames){result.refusal=Refusal::MissingActorAuthority;return result;}
 if(*frames){result.refusal=Refusal::ActorInvincible;return result;}
 if(!std::isfinite(n->mHealth)){result.refusal=Refusal::InvalidHealth;return result;}
 const float next=n->mHealth-damage;
 if(!std::isfinite(next)){result.refusal=Refusal::InvalidDamage;return result;}
 const bool dead=next<1.0f&&state->sourceStateId()!=StateId::Dead;
 if(dead&&!state->canEnterSourceDead(*n)){result.refusal=Refusal::MissingDeadTransition;return result;}
 n->mHealth=next;
 if(feedback)state->sourceDamageFeedback(*n);
 if(dead)state->enterSourceDead(*n);
 result.refusal=Refusal::None;result.applied=damage;result.knockedOut=dead;
 return result;
}
DamageResult startDamage(Navi* n,float raw) {
 const World* world=nullptr;DamageResult result;
 result.refusal=worldFor(n,world);if(result.refusal!=Refusal::None)return result;
 if(world->demo()==Demo::Unknown){result.refusal=Refusal::MissingMovieAuthority;return result;}
 if(world->demo()==Demo::Playing){result.refusal=Refusal::DemoPlaying;return result;}
 auto* native=n->getCurrState();auto* state=dynamic_cast<State*>(native);
 if(!state||state->nativeState()!=native){result.refusal=Refusal::MissingSourceState;return result;}
 if(!state->sourceAlive(*n)){result.refusal=Refusal::NotAlive;return result;}
 if(state->sourceInvincible()){result.refusal=Refusal::StateInvincible;return result;}
 const auto frames=state->actorInvincibleFrames(*n);
 if(!frames){result.refusal=Refusal::MissingActorAuthority;return result;}
 if(*frames){result.refusal=Refusal::ActorInvincible;return result;}
 if(!std::isfinite(raw)||raw<0){result.refusal=Refusal::InvalidDamage;return result;}
 const float damage=pc_p2_equipment_damage(raw);
 if(!std::isfinite(damage)||damage<0){result.refusal=Refusal::InvalidDamage;return result;}
 // Source startDamage's entire mutation branch is conditional on !Damaged.
 if(state->sourceStateId()==StateId::Damaged){result.refusal=Refusal::None;return result;}
 if(!std::isfinite(n->mHealth)){result.refusal=Refusal::InvalidHealth;return result;}
 const float next=n->mHealth-damage;
 if(!std::isfinite(next)){result.refusal=Refusal::InvalidDamage;return result;}
 auto* transition=dynamic_cast<DamageTransitions*>(native);
 if(!transition||!transition->canEnterSourceDamaged(*n)){result.refusal=Refusal::MissingDamagedTransition;return result;}
 const bool dead=next<1.0f;
 if(dead&&!state->canEnterSourceDead(*n)){result.refusal=Refusal::MissingDeadTransition;return result;}
 transition->enterSourceDamaged(*n,damage);
 auto* current=n->getCurrState();auto* damaged=dynamic_cast<State*>(current);
 if(!damaged||damaged->nativeState()!=current||damaged->sourceStateId()!=StateId::Damaged){result.refusal=Refusal::MissingSourceState;return result;}
 n->mHealth=next;damaged->sourceDamageFeedback(*n);
 if(dead)damaged->enterSourceDead(*n);
 result.refusal=Refusal::None;result.applied=damage;result.knockedOut=dead;return result;
}
Refusal flickAdmission(const Creature* enemy,const Navi* n) {
 const World* world=nullptr;auto refusal=worldFor(n,world);
 if(refusal!=Refusal::None)return refusal;
 const auto& progress=originalProgress();
 if(!progress.ready()||progress.context().campaign!=world->selectedCampaign())return Refusal::WrongSession;
 if(!progress.context().reunited)return Refusal::NotReunited;
 unsigned source=0,token=0;
 if(world->sourceCatalog().empty()||world->sourceCatalog()!=originalActors().fingerprint())return Refusal::MissingEnemy;
 if(!enemy||!originalActors().query(enemy,source,token)||!token)return Refusal::MissingEnemy;
 return Refusal::None;
}
DamageResult attack(Navi* n,const Creature* source,float raw){
 DamageResult result;const World* world=nullptr;
 result.refusal=worldFor(n,world);if(result.refusal!=Refusal::None)return result;
 const auto* scene=pc_p2_original_captain_loaded_scene?pc_p2_original_captain_loaded_scene():nullptr;
 if(!scene||!scene->captainAt(0)||!scene->captainAt(1)||scene->captainAt(0)==scene->captainAt(1)
  ||scene->incarnation()!=world->incarnation()||scene->selectedCampaign()!=world->selectedCampaign()
  ||scene->selectedFingerprint()!=world->selectedFingerprint()||scene->sourceCatalog()!=world->sourceCatalog()
  ||scene->captainAt(0)!=world->captainAt(0)||scene->captainAt(1)!=world->captainAt(1)){
  result.refusal=Refusal::MissingWorld;return result;
 }
 const auto& progress=originalProgress();
 if(!progress.ready()||progress.context().campaign!=world->selectedCampaign()){result.refusal=Refusal::WrongSession;return result;}
 if(!progress.context().reunited){result.refusal=Refusal::NotReunited;return result;}
 const auto* piki=dynamic_cast<const Piki*>(source);OriginalPikiBodyHandle body;
 if(piki){
  if(!pc_p2_original_piki_body_handle||!pc_p2_original_piki_body_current
   ||!pc_p2_original_piki_catalog_fingerprint||!pc_p2_original_piki_recruit_pair_ready
   ||!pc_p2_original_piki_recruit_pair_ready()
   ||!pc_p2_original_piki_body_handle(piki,body)||!body.nativeLifetime
   ||!pc_p2_original_piki_body_current(piki,body.nativeLifetime)
   ||pc_p2_original_piki_catalog_fingerprint().empty()||body.body.origin.catalogFingerprint!=pc_p2_original_piki_catalog_fingerprint()
   ||body.body.origin.sourceKey.empty()||!body.body.origin.activation
   ||body.body.origin.recordUid!=originalSourceCatalogUid(body.body.origin.sourceKey)
   ||body.body.state.species>5||(body.body.state.wild&&!body.body.state.wasWild)){
   result.refusal=Refusal::MissingActorAuthority;return result;
  }
 }else {
  result.refusal=flickAdmission(source,n);if(result.refusal!=Refusal::None)return result;
 }
 // Literal interactNavi.cpp:300: reunion, Navi::invincible(), then Piki raw=0
 // and startDamage. Zero damage never bypasses any source immunity authority.
 if(world->demo()==Demo::Unknown){result.refusal=Refusal::MissingMovieAuthority;return result;}
 if(world->demo()==Demo::Playing){result.refusal=Refusal::DemoPlaying;return result;}
 auto* native=n->getCurrState();auto* state=dynamic_cast<State*>(native);
 if(!state||state->nativeState()!=native){result.refusal=Refusal::MissingSourceState;return result;}
 const auto frames=state->actorInvincibleFrames(*n);
 if(!frames){result.refusal=Refusal::MissingActorAuthority;return result;}
 if(*frames){result.refusal=Refusal::ActorInvincible;return result;}
 if(state->sourceInvincible()){result.refusal=Refusal::StateInvincible;return result;}
 if(piki&&!pc_p2_original_piki_body_current(piki,body.nativeLifetime)){result.refusal=Refusal::MissingActorAuthority;return result;}
 result=startDamage(n,piki?0.0f:raw);
 result.interactionAccepted=bool(result);
 if(result.refusal==Refusal::NotAlive){
  bool alive=true;
  if(pc_p2_original_captain_actor_lifetime&&pc_p2_original_captain_actor_lifetime(n,alive)&&!alive)result.interactionAccepted=true;
  else result.refusal=Refusal::MissingActorAuthority;
 }
 return result;
}
} }
