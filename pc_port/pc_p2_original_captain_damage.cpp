#include "pc_p2_original_captain_damage.h"
#include "pc_p2_original_actor.h"
#include "pc_p2_original_progress.h"
#include "pc_p2_equipment.h"
#include "Navi.h"
#include "NaviState.h"
#include <cmath>
extern const p2original::captain::World* pc_p2_original_captain_world() __attribute__((weak));
extern bool pc_randomizer_original_session() __attribute__((weak));
extern std::string pc_randomizer_original_campaign() __attribute__((weak));
extern std::string pc_randomizer_session_fingerprint() __attribute__((weak));
namespace p2original { namespace captain {
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
} }
