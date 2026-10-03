#include "pc_p2_original_captain_damage.h"
#include "pc_p2_original_actor.h"
#include "pc_p2_original_progress.h"
#include "Navi.h"
#include "gameflow.h"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace p2original::captain;
EquipmentGameflow gameflow;
namespace {
std::string campaign(64,'a'),fingerprint(64,'b');bool selected=true,armor=false;
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
struct SourceState:NaviState,State,DamageTransitions {
 StateId id=StateId::KokeDamage;bool alive=true,immune=false,deathReady=true;
 unsigned char frames=0;int deaths=0,feedback=0;bool wrongPointer=false,missingFrames=false;
 const NaviState* nativeState()const override{return wrongPointer?nullptr:this;}
 StateId sourceStateId()const override{return id;}
 bool sourceAlive(const Navi&)const override{return alive;}
 bool sourceInvincible()const override{return immune;}
 std::optional<std::uint8_t> actorInvincibleFrames(const Navi&)const override{if(missingFrames)return {};return frames;}
 bool canEnterSourceDead(const Navi&)const override{return deathReady;}
 void enterSourceDead(Navi&)override{++deaths;id=StateId::Dead;alive=false;}
 void sourceDamageFeedback(Navi&)override{++feedback;}
 bool damagedReady=true;int damagedEntries=0;float healthAtDamagedEntry=0;
 bool canEnterSourceDamaged(const Navi&)const override{return damagedReady;}
 void enterSourceDamaged(Navi& n,float)override{++damagedEntries;healthAtDamagedEntry=n.mHealth;id=StateId::Damaged;}
};
int checks=0;
void check(bool value){++checks;if(!value)throw std::runtime_error("captain damage check "+std::to_string(checks));}
}
const World* pc_p2_original_captain_world(){return live;}
bool pc_randomizer_original_session(){return selected;}
std::string pc_randomizer_original_campaign(){return campaign;}
std::string pc_randomizer_session_fingerprint(){return fingerprint;}
bool pc_p2_campaign_treasure_seen(const char* id){return armor&&std::string(id)=="suit_powerup";}
int main(){
 Navi a,b,outsider;SourceState sa,sb;NaviState p1;a.current=&sa;b.current=&sb;world.captains[0]=&a;world.captains[1]=&b;
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
 check(p2original::originalProgress().reunite(e));check(flickAdmission(&enemy,&a)==Refusal::MissingEnemy);
 p2original::CatalogRow row;row.course="tutorial";row.member="defaultgen.txt";row.sourceKey="tutorial/defaultgen.txt#0";row.enemy.uid=p2original::originalGeneratorUid(row.sourceKey);row.enemy.source=26;row.enemy.count=1;
 auto& actors=p2original::originalActors();check(actors.install(std::string(64,'d'),{row},[](const p2original::CatalogRow&,std::string&){return true;},e));
 int generator=0;std::uint64_t gh=0,eh=0;unsigned token=0;
 auto* gen=reinterpret_cast<Generator*>(&generator);check(actors.generator(gen,row.enemy.uid,gh,e));check(actors.actor(&enemy,row.enemy.uid,0,1,token,eh,e));
 sa=SourceState();a.current=&sa;a.mHealth=3;armor=true;
 r=attack(&a,&enemy,4);check(bool(r)&&r.applied==2&&a.mHealth==1&&sa.id==StateId::Damaged);
 r=attack(&a,&outsider,4);check(r.refusal==Refusal::MissingEnemy&&a.mHealth==1);
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
