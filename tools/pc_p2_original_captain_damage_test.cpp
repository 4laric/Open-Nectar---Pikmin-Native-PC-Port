#include "pc_p2_original_captain_damage.h"
#include "pc_p2_original_actor.h"
#include "pc_p2_original_progress.h"
#include "Navi.h"
#include "Piki.h"
#include "pc_p2_original_piki_origin.h"
#include "pc_p2_original_source_uid.h"
#include "pc_p2_original_piki_recruit.h"
#include "gameflow.h"
#include <iostream>
#include <limits>
#include <stdexcept>
#include <map>
using namespace p2original::captain;
EquipmentGameflow gameflow;
namespace {
std::string campaign(64,'a'),fingerprint(64,'b');bool selected=true,armor=false,lifetimeAvailable=true;
struct TestWorld:World {
 std::string c=campaign,f=fingerprint;std::uint64_t epoch=1;
 Phase p=Phase::GameWorldActive;Demo d=Demo::Inactive;Navi* captains[2]={};
 const std::string& selectedCampaign()const override{return c;}
 const std::string& selectedFingerprint()const override{return f;}
 const std::string& sourceCatalog()const override{static std::string catalog(64,'d');return catalog;}
 std::uint64_t incarnation()const override{return epoch;}
 Phase phase()const override{return p;}Demo demo()const override{return d;}
 Navi* captainAt(unsigned s)const override{return s<2?captains[s]:nullptr;}
} world;
const World* live=&world;
struct Scene:LoadedScene {
 std::string c=campaign,f=fingerprint,catalog=std::string(64,'d');std::uint64_t epoch=1;Navi* actors[2]={};
 const std::string& selectedCampaign()const override{return c;}const std::string& selectedFingerprint()const override{return f;}const std::string& sourceCatalog()const override{return catalog;}
 std::uint64_t incarnation()const override{return epoch;}MoviePlayer* moviePlayer()const override{return nullptr;}Navi* captainAt(unsigned i)const override{return i<2?actors[i]:nullptr;}
} scene;
const LoadedScene* loaded=&scene;
std::string pikiCatalog(64,'e'),admittedCampaign,admittedCatalog;
Piki* retireDuringFrames=nullptr;
void retire(Piki*);
struct SourceState:NaviState,State,DamageTransitions {
 StateId id=StateId::KokeDamage;bool alive=true,immune=false,deathReady=true;
 unsigned char frames=0;int deaths=0,feedback=0;bool wrongPointer=false,missingFrames=false;
 const NaviState* nativeState()const override{return wrongPointer?nullptr:this;}
 StateId sourceStateId()const override{return id;}
 bool sourceAlive(const Navi&)const override{return alive;}
 bool sourceInvincible()const override{return immune;}
 std::optional<std::uint8_t> actorInvincibleFrames(const Navi&)const override{if(missingFrames)return {};if(retireDuringFrames){auto* p=retireDuringFrames;retireDuringFrames=nullptr;retire(p);}return frames;}
 bool canEnterSourceDead(const Navi&)const override{return deathReady;}
 void enterSourceDead(Navi&)override{++deaths;id=StateId::Dead;alive=false;}
 void sourceDamageFeedback(Navi&)override{++feedback;}
 bool damagedReady=true;int damagedEntries=0;float healthAtDamagedEntry=0;
 bool canEnterSourceDamaged(const Navi&)const override{return damagedReady;}
 void enterSourceDamaged(Navi& n,float)override{++damagedEntries;healthAtDamagedEntry=n.mHealth;id=StateId::Damaged;}
};
int checks=0;
void checkAt(bool value,int line){++checks;if(!value)throw std::runtime_error("captain damage check "+std::to_string(checks)+" line "+std::to_string(line));}
#define check(value) checkAt((value),__LINE__)
// Optional USE_REAL_PIKI_SDK links the actual immutable ancestry/lifetime TU.
// Default handle providers are explicit engineering doubles. Neither variant
// demonstrates actual source stock, metadata generation, or ordinary gameplay.
std::map<const Piki*,OriginalPikiBodyHandle> pikiBodies;
bool attach(Piki* p,const OriginalPikiBody& body){
#ifdef USE_REAL_PIKI_SDK
 return pc_p2_original_piki_body_associate_birth(p,body);
#else
 pikiBodies[p]={body,std::uint64_t(pikiBodies.size()+1)};return true;
#endif
}
void retire(Piki* p){
#ifdef USE_REAL_PIKI_SDK
 pc_p2_original_piki_origin_forget(p);
#else
 pikiBodies.erase(p);
#endif
}
}
const World* pc_p2_original_captain_world(){return live;}
const LoadedScene* pc_p2_original_captain_loaded_scene(){return loaded;}
bool pc_randomizer_original_session(){return selected;}
std::string pc_randomizer_original_campaign(){return campaign;}
std::string pc_randomizer_session_fingerprint(){return fingerprint;}
bool pc_p2_campaign_treasure_seen(const char* id){return armor&&std::string(id)=="suit_powerup";}
bool pc_p2_original_captain_actor_lifetime(const Navi* n,bool& out){auto* state=n?dynamic_cast<SourceState*>(n->current):nullptr;if(!lifetimeAvailable||!state)return false;out=state->alive;return true;}
#ifdef USE_REAL_PIKI_SDK
bool pc_p2_cave_campaign_survivor_permit(const std::string&,std::uint32_t,std::uint32_t,std::uint64_t,const std::string&,std::uint64_t*,std::uint8_t[32]){return false;}
bool pc_p2_cave_campaign_survivor_body(const std::string&,std::uint32_t,std::uint32_t,std::uint64_t,const std::string&,OriginalPikiBodyState&,std::uint64_t*,std::uint8_t[32]){return false;}
bool pc_p2_cave_campaign_party_associate_birth(Piki* p,const char*,std::uint32_t,std::uint32_t,std::uint64_t,const char*){return dynamic_cast<const Creature*>(p)!=nullptr;}
#elif !defined(OMIT_PIKI_QUERIES)
const std::string& pc_p2_original_piki_catalog_fingerprint()noexcept{return pikiCatalog;}
bool pc_p2_original_piki_recruit_pair_ready()noexcept{return admittedCampaign==p2original::originalProgress().context().campaign&&admittedCatalog==pikiCatalog&&!admittedCampaign.empty();}
bool pc_p2_original_piki_body_handle(const Piki* p,OriginalPikiBodyHandle& out){auto i=pikiBodies.find(p);if(i==pikiBodies.end())return false;out=i->second;return true;}
bool pc_p2_original_piki_body_current(const Piki* p,std::uint64_t life)noexcept{auto i=pikiBodies.find(p);return life&&i!=pikiBodies.end()&&i->second.nativeLifetime==life;}
#endif
int main(){
 Navi a,b,outsider;SourceState sa,sb;NaviState p1;a.current=&sa;b.current=&sb;world.captains[0]=&a;world.captains[1]=&b;
 scene.actors[0]=&a;scene.actors[1]=&b;
 auto refusal=[&](Refusal expected){float hp=a.mHealth;auto r=addDamage(&a,2,true);check(r.refusal==expected&&a.mHealth==hp&&sa.deaths==0&&sa.feedback==0);};
 live=nullptr;refusal(Refusal::MissingWorld);live=&world;
 world.epoch=0;refusal(Refusal::MissingWorld);world.epoch=1;
 selected=false;refusal(Refusal::WrongSession);selected=true;
 world.c=std::string(64,'c');refusal(Refusal::WrongSession);world.c=campaign;
 world.f=std::string(64,'c');refusal(Refusal::WrongSession);world.f=fingerprint;
 world.p=Phase::Loading;refusal(Refusal::InactiveWorld);world.p=Phase::Inactive;refusal(Refusal::InactiveWorld);world.p=Phase::GameWorldActive;
 world.d=Demo::Unknown;refusal(Refusal::MissingMovieAuthority);world.d=Demo::Playing;refusal(Refusal::DemoPlaying);world.d=Demo::Inactive;
 check(addDamage(&outsider,1,false).refusal==Refusal::MissingCaptain);
 check(addDamage(nullptr,1,false).refusal==Refusal::MissingCaptain);
 a.current=nullptr;refusal(Refusal::MissingSourceState);a.current=&p1;refusal(Refusal::MissingSourceState);a.current=&sa;
 sa.wrongPointer=true;refusal(Refusal::MissingSourceState);sa.wrongPointer=false;
 sa.alive=false;refusal(Refusal::NotAlive);sa.alive=true;
 sa.immune=true;refusal(Refusal::StateInvincible);sa.immune=false;
 sa.missingFrames=true;refusal(Refusal::MissingActorAuthority);sa.missingFrames=false;
 sa.frames=1;refusal(Refusal::ActorInvincible);sa.frames=255;refusal(Refusal::ActorInvincible);sa.frames=0;
 sa.deathReady=false;a.mHealth=2;refusal(Refusal::MissingDeadTransition);sa.deathReady=true;a.mHealth=10;
 check(addDamage(&a,-1,false).refusal==Refusal::InvalidDamage);
 check(addDamage(&a,std::numeric_limits<float>::infinity(),false).refusal==Refusal::InvalidDamage);
 a.mHealth=std::numeric_limits<float>::quiet_NaN();check(addDamage(&a,1,false).refusal==Refusal::InvalidHealth);a.mHealth=10;
 // Source alive is independent of HP and armor runs once at the actual END.
 a.mHealth=3;armor=true;auto r=addDamage(&a,4,false);check(bool(r)&&r.applied==2&&a.mHealth==1&&!r.knockedOut&&sa.alive);
 check(sa.feedback==0&&sa.deaths==0);
 armor=false;r=addDamage(&a,.5f,true);check(bool(r)&&a.mHealth==.5f&&r.knockedOut&&sa.deaths==1&&sa.feedback==1&&!sa.alive);
 // Partner still carries independent live state; inactive slot is accepted.
 r=addDamage(&b,9,false);check(bool(r)&&b.mHealth==1&&!r.knockedOut&&sb.alive);
 r=addDamage(&b,.5f,false);check(bool(r)&&b.mHealth==.5f&&r.knockedOut&&sb.deaths==1&&!sb.alive);
 // Dead actor refusal at positive HP catches the P1 HP>0 shortcut.
 check(addDamage(&a,0,false).refusal==Refusal::NotAlive);
 // A source actor explicitly alive at zero HP still reaches source Dead.
 sa=SourceState();a.current=&sa;a.mHealth=0;r=addDamage(&a,0,false);check(bool(r)&&r.knockedOut);
 sa=SourceState();a.current=&sa;a.mHealth=2;world.d=Demo::Absent;r=addDamage(&a,1,false);check(bool(r)&&a.mHealth==1);world.d=Demo::Inactive;
 // Flick outer guard is distinct: no early demo/actor/FSM immunity or RNG.
 std::string e;check(p2original::originalProgress().initialize(campaign,e));
 Creature enemy;check(flickAdmission(&enemy,&a)==Refusal::NotReunited);
 Piki beforeReunion;const auto hpBeforeReunion=a.mHealth;r=attack(&a,&beforeReunion,99);check(r.refusal==Refusal::NotReunited&&!r.interactionAccepted&&a.mHealth==hpBeforeReunion);
 check(p2original::originalProgress().reunite(e));check(flickAdmission(&enemy,&a)==Refusal::MissingEnemy);
 p2original::CatalogRow row;row.course="tutorial";row.member="defaultgen.txt";row.sourceKey="tutorial/defaultgen.txt#0";row.enemy.uid=p2original::originalGeneratorUid(row.sourceKey);row.enemy.source=26;row.enemy.count=1;
 auto& actors=p2original::originalActors();check(actors.install(std::string(64,'d'),{row},[](const p2original::CatalogRow&,std::string&){return true;},e));
 int generator=0;std::uint64_t gh=0,eh=0;unsigned token=0;
 auto* gen=reinterpret_cast<Generator*>(&generator);check(actors.generator(gen,row.enemy.uid,gh,e));check(actors.actor(&enemy,row.enemy.uid,0,1,token,eh,e));
 sa=SourceState();a.current=&sa;a.mHealth=3;armor=true;
 r=attack(&a,&enemy,4);check(bool(r)&&r.interactionAccepted&&r.applied==2&&a.mHealth==1&&sa.id==StateId::Damaged);
 r=attack(&a,&outsider,4);check(r.refusal==Refusal::MissingEnemy&&a.mHealth==1);
 Piki friendly,partnerPiki,p1Piki;const std::string pikiKey="tutorial/defaultgen.txt#2";OriginalPikiBody pikiBody;pikiBody.origin={pikiKey,p2original::originalSourceCatalogUid(pikiKey),0,1,pikiCatalog};pikiBody.state={0,false,false};
#ifdef USE_REAL_PIKI_SDK
 check(pc_p2_original_piki_origin_install(pikiCatalog,{{pikiKey,pikiBody.origin.recordUid,2,0}},e));
 check(pc_p2_original_piki_recruit_bind(campaign,pikiCatalog,e));
#else
 admittedCampaign=campaign;admittedCatalog=pikiCatalog;
#endif
 check(attach(&friendly,pikiBody));pikiBody.origin.attempt=1;check(attach(&partnerPiki,pikiBody));
 sa=SourceState();a.current=&sa;a.mHealth=3;
#ifdef OMIT_PIKI_QUERIES
 r=attack(&a,&friendly,99);check(r.refusal==Refusal::MissingActorAuthority&&!r.interactionAccepted&&a.mHealth==3&&sa.damagedEntries==0);
 std::cout<<"P2_ORIGINAL_CAPTAIN_DAMAGE_MISSING_PIKI_QUERY_PASS\n";return 0;
#endif
 for(float anyRaw:{0.0f,7.0f,-9.0f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}){sa=SourceState();a.current=&sa;a.mHealth=3;r=attack(&a,&friendly,anyRaw);check(bool(r)&&r.interactionAccepted&&r.applied==0&&a.mHealth==3&&sa.damagedEntries==1&&sa.feedback==1&&sa.healthAtDamagedEntry==3);}
 sb=SourceState();b.current=&sb;b.mHealth=4;r=attack(&b,&partnerPiki,99);check(bool(r)&&r.interactionAccepted&&r.applied==0&&b.mHealth==4&&sb.damagedEntries==1);
 sa=SourceState();a.current=&sa;a.mHealth=3;auto refusePiki=[&](Refusal expected){const auto before=a.mHealth;const auto entries=sa.damagedEntries;r=attack(&a,&friendly,0);check(r.refusal==expected&&!r.interactionAccepted&&a.mHealth==before&&sa.damagedEntries==entries);};
 world.d=Demo::Playing;refusePiki(Refusal::DemoPlaying);world.d=Demo::Unknown;refusePiki(Refusal::MissingMovieAuthority);world.d=Demo::Inactive;
 sa.frames=60;refusePiki(Refusal::ActorInvincible);sa.frames=0;sa.missingFrames=true;refusePiki(Refusal::MissingActorAuthority);sa.missingFrames=false;sa.immune=true;refusePiki(Refusal::StateInvincible);sa.immune=false;
 sa.wrongPointer=true;refusePiki(Refusal::MissingSourceState);sa.wrongPointer=false;a.current=&p1;refusePiki(Refusal::MissingSourceState);a.current=&sa;
 sa.damagedReady=false;refusePiki(Refusal::MissingDamagedTransition);sa.damagedReady=true;a.mHealth=.5f;sa.deathReady=false;refusePiki(Refusal::MissingDeadTransition);sa.deathReady=true;a.mHealth=3;
 sa.alive=false;r=attack(&a,&friendly,5);check(r.refusal==Refusal::NotAlive&&r.interactionAccepted&&a.mHealth==3&&sa.damagedEntries==0);lifetimeAvailable=false;refusePiki(Refusal::MissingActorAuthority);lifetimeAvailable=true;sa.alive=true;
 r=attack(&a,&p1Piki,0);check(r.refusal==Refusal::MissingActorAuthority&&!r.interactionAccepted&&a.mHealth==3);
 world.p=Phase::Loading;refusePiki(Refusal::InactiveWorld);world.p=Phase::GameWorldActive;world.c=std::string(64,'f');refusePiki(Refusal::WrongSession);world.c=campaign;
 loaded=nullptr;refusePiki(Refusal::MissingWorld);loaded=&scene;++scene.epoch;refusePiki(Refusal::MissingWorld);--scene.epoch;scene.actors[1]=&outsider;refusePiki(Refusal::MissingWorld);scene.actors[1]=&b;
#ifndef USE_REAL_PIKI_SDK
 admittedCampaign=std::string(64,'f');refusePiki(Refusal::MissingActorAuthority);admittedCampaign=campaign;admittedCatalog=world.sourceCatalog();refusePiki(Refusal::MissingActorAuthority);admittedCatalog=pikiCatalog;
 auto validBody=pikiBodies[&friendly];pikiBodies[&friendly].body.origin.catalogFingerprint=std::string(64,'f');refusePiki(Refusal::MissingActorAuthority);pikiBodies[&friendly]=validBody;pikiBodies[&friendly].nativeLifetime=0;refusePiki(Refusal::MissingActorAuthority);pikiBodies[&friendly]=validBody;pikiBodies[&friendly].body.origin.activation=0;refusePiki(Refusal::MissingActorAuthority);pikiBodies[&friendly]=validBody;pikiBodies[&friendly].body.origin.sourceKey.clear();refusePiki(Refusal::MissingActorAuthority);pikiBodies[&friendly]=validBody;pikiBodies[&friendly].body.origin.recordUid^=1;refusePiki(Refusal::MissingActorAuthority);pikiBodies[&friendly]=validBody;
#else
 pc_p2_original_piki_recruit_unbind();refusePiki(Refusal::MissingActorAuthority);check(pc_p2_original_piki_recruit_bind(campaign,pikiCatalog,e));
#endif
 retireDuringFrames=&friendly;refusePiki(Refusal::MissingActorAuthority);refusePiki(Refusal::MissingActorAuthority);retire(&partnerPiki);
 armor=false;
 a.current=&p1;world.d=Demo::Playing;check(flickAdmission(&enemy,&a)==Refusal::None);
 world.p=Phase::Inactive;check(flickAdmission(&enemy,&a)==Refusal::InactiveWorld);world.p=Phase::GameWorldActive;
 check(actors.retire(&enemy,eh));check(flickAdmission(&enemy,&a)==Refusal::MissingEnemy);
 sa=SourceState();a.current=&sa;a.mHealth=3;world.d=Demo::Inactive;armor=true;
 r=startDamage(&a,4);check(bool(r)&&r.applied==2&&a.mHealth==1&&!r.knockedOut);
 check(sa.damagedEntries==1&&sa.healthAtDamagedEntry==3&&sa.id==StateId::Damaged&&sa.feedback==1);
 r=startDamage(&a,20);check(bool(r)&&r.applied==0&&a.mHealth==1&&sa.damagedEntries==1&&sa.feedback==1);
 sa.id=StateId::Walk;sa.damagedReady=false;r=startDamage(&a,0);check(r.refusal==Refusal::MissingDamagedTransition&&a.mHealth==1);
 sa.damagedReady=true;sa.deathReady=false;r=startDamage(&a,1);check(r.refusal==Refusal::MissingDeadTransition&&a.mHealth==1&&sa.damagedEntries==1);
 sa.deathReady=true;r=startDamage(&a,1);check(bool(r)&&r.knockedOut&&r.applied==.5f&&a.mHealth==.5f&&sa.deaths==1);
 armor=false;
 std::cout<<"P2_ORIGINAL_CAPTAIN_DAMAGE_CONTROLS_PASS checks="<<checks<<" ordinary_gameplay=UNTESTED source_producers=UNAVAILABLE\n";
}
