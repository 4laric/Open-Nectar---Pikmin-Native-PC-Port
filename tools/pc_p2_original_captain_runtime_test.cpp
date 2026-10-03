#include "pc_p2_original_captain_damage.h"
#include "Navi.h"
#include <iostream>
#include <stdexcept>
#include <array>
using namespace p2original::captain;
namespace {
std::string campaign(64,'a'),fingerprint(64,'b');bool selected=true;
struct Scene:LoadedScene {
 std::string c=campaign,f=fingerprint,catalog=campaign;std::uint64_t epoch=1;
 Navi* navis[2]={};MoviePlayer* player=nullptr;
 const std::string& selectedCampaign()const override{return c;}
 const std::string& selectedFingerprint()const override{return f;}
 const std::string& sourceCatalog()const override{return catalog;}
 MoviePlayer* moviePlayer()const override{return player;}
 std::uint64_t incarnation()const override{return epoch;}
 Navi* captainAt(unsigned s)const override{return s<2?navis[s]:nullptr;}
} scene;
const LoadedScene* loaded=nullptr;
struct SourceState:NaviState,State {
 StateId id=StateId::Damaged;
 const NaviState* nativeState()const override{return this;}
 StateId sourceStateId()const override{return id;}
 bool sourceAlive(const Navi& n)const override{return pc_p2_original_captain_actor_alive(&n);}
 bool sourceInvincible()const override{return true;}
 std::optional<std::uint8_t> actorInvincibleFrames(const Navi& n)const override{std::uint8_t frames;if(!pc_p2_original_captain_actor_frames(&n,frames))return {};return frames;}
 bool canEnterSourceDead(const Navi&)const override{return false;}
 void enterSourceDead(Navi&)override{}void sourceDamageFeedback(Navi&)override{}
};
// Engineering lifecycle/query doubles exercise the actual runtime TU. These
// typed state and control-reset records are deliberate controls, not production
// bootstrap authority or ordinary gameplay acceptance.
std::array<SourceState,2> bootstrapWalk;
struct ControlReset {const LoadedScene* scene=nullptr;std::uint64_t epoch=0;Navi* actor=nullptr;};
std::array<ControlReset,2> controlResets;
int bootstrapQueries=0,continuationQueries=0;bool changeEpochDuringQuery=false;
void actualResetControls(){for(unsigned i=0;i<2;++i){bootstrapWalk[i].id=StateId::Walk;scene.navis[i]->current=&bootstrapWalk[i];controlResets[i]={&scene,scene.epoch,scene.navis[i]};}}
bool checkOwnedControls(const LoadedScene& source,std::string& e,bool initial){
 for(unsigned i=0;i<2;++i){auto* n=source.captainAt(i);auto* typed=n?dynamic_cast<State*>(n->current):nullptr;const auto& reset=controlResets[i];
  if(&source!=&scene||!typed||typed->nativeState()!=n->current||reset.scene!=&source||reset.epoch!=source.incarnation()||reset.actor!=n||(initial&&(n->current!=&bootstrapWalk[i]||typed->sourceStateId()!=StateId::Walk))){e="engineered actual typed state/control binding missing";return false;}
 }
 if(changeEpochDuringQuery)++scene.epoch;
 return true;
}
int checks=0;void check(bool b){++checks;if(!b)throw std::runtime_error("captain runtime check "+std::to_string(checks));}
}
const LoadedScene* pc_p2_original_captain_loaded_scene(){return loaded;}
bool pc_randomizer_original_session(){return selected;}
std::string pc_randomizer_original_campaign(){return campaign;}
std::string pc_randomizer_session_fingerprint(){return fingerprint;}
#ifndef OMIT_BOOTSTRAP_OWNER
bool pc_p2_original_captain_bootstrap_complete(const LoadedScene& s,std::string& e){++bootstrapQueries;return checkOwnedControls(s,e,true);}
#endif
#ifndef OMIT_CONTINUATION_OWNER
bool pc_p2_original_captain_continuation_valid(const LoadedScene& s,std::string& e){++continuationQueries;return checkOwnedControls(s,e,false);}
#endif
int main(){
 Navi a,b,outsider;SourceState state;NaviState p1;scene.navis[0]=&a;scene.navis[1]=&b;a.current=&state;b.current=&state;
 std::uint8_t frames=99;
 bool life=true;
#ifdef OMIT_BOOTSTRAP_OWNER
 loaded=&scene;std::string absentError;check(pc_p2_original_captain_body_reset_loaded(absentError));actualResetControls();check(!pc_p2_original_captain_activate_after_bootstrap(absentError)&&absentError=="missing actual typed Walk/control bootstrap completion owner"&&pc_p2_original_captain_world()->phase()==Phase::Loading);
 std::cout<<"P2_ORIGINAL_CAPTAIN_RUNTIME_MISSING_BOOTSTRAP_OWNER_PASS\n";return 0;
#endif
 check(!pc_p2_original_captain_actor_lifetime(&a,life)&&life);
 check(!pc_p2_original_captain_world());pc_p2_original_captain_main_game_entered();check(!pc_p2_original_captain_world());
 check(!pc_p2_original_captain_actor_frames(&a,frames)&&frames==99);
 loaded=&scene;check(!pc_p2_original_captain_world()); // loaded alone is not game-active
 scene.navis[1]=nullptr;pc_p2_original_captain_main_game_entered();check(!pc_p2_original_captain_world());scene.navis[1]=&b;
 scene.navis[1]=&a;pc_p2_original_captain_main_game_entered();check(!pc_p2_original_captain_world());scene.navis[1]=&b;
 selected=false;pc_p2_original_captain_main_game_entered();check(!pc_p2_original_captain_world());selected=true;
 scene.f=std::string(64,'c');pc_p2_original_captain_main_game_entered();check(!pc_p2_original_captain_world());scene.f=fingerprint;
 std::string error;pc_p2_original_captain_main_game_entered();check(!pc_p2_original_captain_world()); // activation cannot bind a merely loaded scene
 check(pc_p2_original_captain_body_reset_loaded(error));auto* world=pc_p2_original_captain_world();check(world&&world->phase()==Phase::Loading&&world->demo()==Demo::Absent);
 check(!pc_p2_original_captain_activate_after_bootstrap(error)&&!error.empty()&&world->phase()==Phase::Loading&&bootstrapQueries==1);
 bootstrapWalk[0].id=bootstrapWalk[1].id=StateId::Walk;a.current=&bootstrapWalk[0];b.current=&bootstrapWalk[1];check(!pc_p2_original_captain_activate_after_bootstrap(error)&&world->phase()==Phase::Loading); // typed states alone do not prove control reset
 actualResetControls();b.current=&p1;check(!pc_p2_original_captain_activate_after_bootstrap(error)&&world->phase()==Phase::Loading);b.current=&bootstrapWalk[1];
 changeEpochDuringQuery=true;check(!pc_p2_original_captain_activate_after_bootstrap(error)&&!pc_p2_original_captain_world());changeEpochDuringQuery=false;
 check(pc_p2_original_captain_body_reset_loaded(error));check(!pc_p2_original_captain_activate_after_bootstrap(error));actualResetControls();check(pc_p2_original_captain_activate_after_bootstrap(error));world=pc_p2_original_captain_world();check(world&&world->phase()==Phase::GameWorldActive);a.current=b.current=&state;
#ifdef OMIT_CONTINUATION_OWNER
 pc_p2_original_captain_main_game_left();check(!pc_p2_original_captain_activate_after_bootstrap(error)&&error=="missing actual source state/control continuation owner"&&world->phase()==Phase::Inactive);
 std::cout<<"P2_ORIGINAL_CAPTAIN_RUNTIME_MISSING_CONTINUATION_OWNER_PASS\n";return 0;
#endif
 // Actor lifetime initializes independently of native HP; zero does not kill.
 a.mHealth=0;check(pc_p2_original_captain_actor_alive(&a));check(pc_p2_original_captain_actor_alive(&b));
 check(pc_p2_original_captain_actor_lifetime(&a,life)&&life);
 check(pc_p2_original_captain_actor_frames(&a,frames)&&frames==0);
 check(!pc_p2_original_captain_actor_frames(&outsider,frames));
 check(pc_p2_original_captain_damaged_cleanup(&a));check(pc_p2_original_captain_actor_frames(&a,frames)&&frames==60);
 pc_p2_original_captain_actor_update(&a);check(pc_p2_original_captain_actor_frames(&a,frames)&&frames==59);
 pc_p2_original_captain_main_game_entered();check(pc_p2_original_captain_actor_frames(&a,frames)&&frames==59); // no duplicate-enter refill
 const int beforeDuplicate=bootstrapQueries;check(pc_p2_original_captain_body_reset_loaded(error)&&pc_p2_original_captain_world()->phase()==Phase::GameWorldActive&&pc_p2_original_captain_actor_frames(&a,frames)&&frames==59&&bootstrapQueries==beforeDuplicate);
 b.current=&p1;check(!pc_p2_original_captain_damaged_cleanup(&b));check(!pc_p2_original_captain_dead_entered(&b));b.current=&state;
 state.id=StateId::KokeDamage;check(!pc_p2_original_captain_damaged_cleanup(&a));state.id=StateId::Dead;
 check(pc_p2_original_captain_dead_entered(&a));check(!pc_p2_original_captain_actor_alive(&a)&&pc_p2_original_captain_actor_alive(&b));
 check(pc_p2_original_captain_actor_lifetime(&a,life)&&!life);
 pc_p2_original_captain_main_game_left();check(pc_p2_original_captain_world()->phase()==Phase::Inactive);
 // Exact source timer semantics: held A never arms an idle counter.
 PcOriginalCaptainTimers timers;Controller controller;a.mKontroller=&controller;controller.mCurrentInput=KBBTN_A;
 check(pc_p2_original_captain_actor_timers(&a,timers)&&timers.throwDisable==0&&timers.disbandDisable==0);
 pc_p2_original_captain_actor_update(&a);check(pc_p2_original_captain_actor_timers(&a,timers)&&timers.throwDisable==0);
 state.id=StateId::Walk;check(!pc_p2_original_captain_start_throw_disable(&a));
 state.id=StateId::Nuku;check(pc_p2_original_captain_start_throw_disable(&a));
 pc_p2_original_captain_actor_update(&a);check(pc_p2_original_captain_actor_timers(&a,timers)&&timers.throwDisable==9);
 pc_p2_original_captain_actor_update(&a);check(pc_p2_original_captain_actor_timers(&a,timers)&&timers.throwDisable==9);
 controller.mCurrentInput=0;for(int i=0;i<12;++i)pc_p2_original_captain_actor_update(&a);
 check(pc_p2_original_captain_actor_timers(&a,timers)&&timers.throwDisable==0);
 check(!pc_p2_original_captain_party_released(&a));b.current=&p1;check(!pc_p2_original_captain_activate_after_bootstrap(error)&&world->phase()==Phase::Inactive);b.current=&state;pc_p2_original_captain_main_game_entered();check(world->phase()==Phase::GameWorldActive&&continuationQueries==2&&!pc_p2_original_captain_actor_alive(&a));
 check(pc_p2_original_captain_party_released(&a));a.mTargetVelocity.x=20;
 pc_p2_original_captain_actor_update(&a);check(pc_p2_original_captain_actor_timers(&a,timers)&&timers.disbandDisable==60);
 a.mTargetVelocity.x=21;pc_p2_original_captain_actor_update(&a);check(pc_p2_original_captain_actor_timers(&a,timers)&&timers.disbandDisable==59);
 a.mTargetVelocity.x=0;pc_p2_original_captain_actor_update(&a);check(pc_p2_original_captain_actor_timers(&a,timers)&&timers.disbandDisable==59);
 pc_p2_original_captain_main_game_entered();check(pc_p2_original_captain_actor_timers(&a,timers)&&timers.disbandDisable==59);
 timers.throwDisable=88;check(!pc_p2_original_captain_actor_timers(&outsider,timers)&&timers.throwDisable==88);
 // Changed incarnation is refused even when all addresses are reused.
 ++scene.epoch;check(!pc_p2_original_captain_world());check(!pc_p2_original_captain_actor_frames(&a,frames));
 int movie=0,wrongMovie=0;scene.player=reinterpret_cast<MoviePlayer*>(&movie);
 pc_p2_original_captain_movie_started(scene.player);check(pc_p2_original_captain_body_reset_loaded(error));actualResetControls();check(pc_p2_original_captain_activate_after_bootstrap(error));a.current=b.current=&state;world=pc_p2_original_captain_world();check(world&&world->demo()==Demo::Playing); // start before scene bind preserved
 pc_p2_original_captain_movie_ended(reinterpret_cast<MoviePlayer*>(&wrongMovie));check(world->demo()==Demo::Unknown); // different player cannot prove inactive
 pc_p2_original_captain_movie_ended(scene.player);check(world->demo()==Demo::Inactive);
 state.id=StateId::Damaged;check(pc_p2_original_captain_damaged_cleanup(&a));pc_p2_original_captain_movie_started(scene.player);
 pc_p2_original_captain_actor_update(&a);check(pc_p2_original_captain_actor_frames(&a,frames)&&frames==59); // once per actual actor update, even movie
 for(int i=0;i<70;++i)pc_p2_original_captain_actor_update(&a);check(pc_p2_original_captain_actor_frames(&a,frames)&&frames==0); // no u8 underflow
 loaded=nullptr;check(!pc_p2_original_captain_world());check(!pc_p2_original_captain_actor_alive(&a));
 std::cout<<"P2_ORIGINAL_CAPTAIN_RUNTIME_CONTROLS_PASS checks="<<checks<<" gameplay=UNTESTED\n";
}
