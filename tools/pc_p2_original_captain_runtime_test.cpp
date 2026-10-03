#include "pc_p2_original_captain_damage.h"
#include "Navi.h"
#include <iostream>
#include <stdexcept>
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
int checks=0;void check(bool b){++checks;if(!b)throw std::runtime_error("captain runtime check "+std::to_string(checks));}
}
const LoadedScene* pc_p2_original_captain_loaded_scene(){return loaded;}
bool pc_randomizer_original_session(){return selected;}
std::string pc_randomizer_original_campaign(){return campaign;}
std::string pc_randomizer_session_fingerprint(){return fingerprint;}
int main(){
 Navi a,b,outsider;SourceState state;NaviState p1;scene.navis[0]=&a;scene.navis[1]=&b;a.current=&state;b.current=&state;
 std::uint8_t frames=99;
 bool life=true;
 check(!pc_p2_original_captain_actor_lifetime(&a,life)&&life);
 check(!pc_p2_original_captain_world());pc_p2_original_captain_main_game_entered();check(!pc_p2_original_captain_world());
 check(!pc_p2_original_captain_actor_frames(&a,frames)&&frames==99);
 loaded=&scene;check(!pc_p2_original_captain_world()); // loaded alone is not game-active
 scene.navis[1]=nullptr;pc_p2_original_captain_main_game_entered();check(!pc_p2_original_captain_world());scene.navis[1]=&b;
 scene.navis[1]=&a;pc_p2_original_captain_main_game_entered();check(!pc_p2_original_captain_world());scene.navis[1]=&b;
 selected=false;pc_p2_original_captain_main_game_entered();check(!pc_p2_original_captain_world());selected=true;
 scene.f=std::string(64,'c');pc_p2_original_captain_main_game_entered();check(!pc_p2_original_captain_world());scene.f=fingerprint;
 pc_p2_original_captain_main_game_entered();auto* world=pc_p2_original_captain_world();check(world&&world->phase()==Phase::GameWorldActive&&world->demo()==Demo::Absent);
 // Actor lifetime initializes independently of native HP; zero does not kill.
 a.mHealth=0;check(pc_p2_original_captain_actor_alive(&a));check(pc_p2_original_captain_actor_alive(&b));
 check(pc_p2_original_captain_actor_lifetime(&a,life)&&life);
 check(pc_p2_original_captain_actor_frames(&a,frames)&&frames==0);
 check(!pc_p2_original_captain_actor_frames(&outsider,frames));
 check(pc_p2_original_captain_damaged_cleanup(&a));check(pc_p2_original_captain_actor_frames(&a,frames)&&frames==60);
 pc_p2_original_captain_actor_update(&a);check(pc_p2_original_captain_actor_frames(&a,frames)&&frames==59);
 pc_p2_original_captain_main_game_entered();check(pc_p2_original_captain_actor_frames(&a,frames)&&frames==59); // no duplicate-enter refill
 b.current=&p1;check(!pc_p2_original_captain_damaged_cleanup(&b));check(!pc_p2_original_captain_dead_entered(&b));b.current=&state;
 state.id=StateId::KokeDamage;check(!pc_p2_original_captain_damaged_cleanup(&a));state.id=StateId::Dead;
 check(pc_p2_original_captain_dead_entered(&a));check(!pc_p2_original_captain_actor_alive(&a)&&pc_p2_original_captain_actor_alive(&b));
 check(pc_p2_original_captain_actor_lifetime(&a,life)&&!life);
 pc_p2_original_captain_main_game_left();check(pc_p2_original_captain_world()->phase()==Phase::Inactive);
 // Changed incarnation is refused even when all addresses are reused.
 scene.epoch=2;check(!pc_p2_original_captain_world());check(!pc_p2_original_captain_actor_frames(&a,frames));
 int movie=0,wrongMovie=0;scene.player=reinterpret_cast<MoviePlayer*>(&movie);
 pc_p2_original_captain_movie_started(scene.player);pc_p2_original_captain_main_game_entered();world=pc_p2_original_captain_world();check(world&&world->demo()==Demo::Playing); // start before scene bind preserved
 pc_p2_original_captain_movie_ended(reinterpret_cast<MoviePlayer*>(&wrongMovie));check(world->demo()==Demo::Unknown); // different player cannot prove inactive
 pc_p2_original_captain_movie_ended(scene.player);check(world->demo()==Demo::Inactive);
 state.id=StateId::Damaged;check(pc_p2_original_captain_damaged_cleanup(&a));pc_p2_original_captain_movie_started(scene.player);
 pc_p2_original_captain_actor_update(&a);check(pc_p2_original_captain_actor_frames(&a,frames)&&frames==59); // once per actual actor update, even movie
 for(int i=0;i<70;++i)pc_p2_original_captain_actor_update(&a);check(pc_p2_original_captain_actor_frames(&a,frames)&&frames==0); // no u8 underflow
 loaded=nullptr;check(!pc_p2_original_captain_world());check(!pc_p2_original_captain_actor_alive(&a));
 std::cout<<"P2_ORIGINAL_CAPTAIN_RUNTIME_CONTROLS_PASS checks="<<checks<<" gameplay=UNTESTED\n";
}
