#include "pc_p2_original_captain_damage.h"
#include "Navi.h"
#include "NaviState.h"
#include "Controller.h"
#include "Kontroller.h"
#include <cmath>
#include <array>
#include <limits>
extern const p2original::captain::LoadedScene* pc_p2_original_captain_loaded_scene() __attribute__((weak));
extern bool pc_randomizer_original_session() __attribute__((weak));
extern std::string pc_randomizer_original_campaign() __attribute__((weak));
extern std::string pc_randomizer_session_fingerprint() __attribute__((weak));
extern bool pc_p2_original_captain_down_demo(p2original::captain::Demo&) __attribute__((weak));
extern bool pc_p2_original_captain_bootstrap_complete(const p2original::captain::LoadedScene&,std::string&) __attribute__((weak));
extern bool pc_p2_original_captain_continuation_valid(const p2original::captain::LoadedScene&,std::string&) __attribute__((weak));
using namespace p2original::captain;
namespace {
MoviePlayer* observedMovie=nullptr;Demo observedDemo=Demo::Unknown;
class Runtime final:public World {
public:
 std::string campaign,fingerprint,catalog;
 std::uint64_t epoch=0;const LoadedScene* scene=nullptr;
 Phase game=Phase::Inactive;MoviePlayer* player=nullptr;
 bool completedBootstrap=false;
 std::array<Navi*,2> captains{};
 std::array<bool,2> alive{};
 std::array<std::uint8_t,2> frames{};
 std::array<PcOriginalCaptainTimers,2> timers{};
 const std::string& selectedCampaign()const override{return campaign;}
 const std::string& selectedFingerprint()const override{return fingerprint;}
 const std::string& sourceCatalog()const override{return catalog;}
 std::uint64_t incarnation()const override{return epoch;}
 Phase phase()const override{return game;}
 Demo demo()const override{
  Demo source;
  if(pc_p2_original_captain_down_demo&&pc_p2_original_captain_down_demo(source))return source;
  return !player?Demo::Absent:(player==observedMovie?observedDemo:Demo::Unknown);
 }
 Navi* captainAt(unsigned slot)const override{return slot<2?captains[slot]:nullptr;}
 void clear(){scene=nullptr;epoch=0;campaign.clear();fingerprint.clear();catalog.clear();game=Phase::Inactive;completedBootstrap=false;player=nullptr;captains={};alive={};frames={};timers={};}
 const LoadedScene* canonical()const {
  if(!pc_p2_original_captain_loaded_scene||!pc_randomizer_original_session
   ||!pc_randomizer_original_session()||!pc_randomizer_original_campaign
   ||!pc_randomizer_session_fingerprint)return nullptr;
  auto* s=pc_p2_original_captain_loaded_scene();
  if(!s||!s->incarnation()||s->selectedCampaign().empty()||s->selectedFingerprint().empty()
   ||s->sourceCatalog().empty()||s->selectedCampaign()!=pc_randomizer_original_campaign()
   ||s->selectedFingerprint()!=pc_randomizer_session_fingerprint()||!s->captainAt(0)
   ||!s->captainAt(1)||s->captainAt(0)==s->captainAt(1))return nullptr;
  return s;
 }
 bool valid(){
  const auto* s=canonical();
  if(!scene||s!=scene||s->incarnation()!=epoch||s->selectedCampaign()!=campaign
   ||s->selectedFingerprint()!=fingerprint||s->sourceCatalog()!=catalog
   ||s->moviePlayer()!=player||s->captainAt(0)!=captains[0]||s->captainAt(1)!=captains[1]){clear();return false;}
  return true;
 }
 int slot(const Navi* n){if(!n||!valid())return -1;for(int i=0;i<2;++i)if(captains[i]==n)return i;return -1;}
 bool loadedAfterReset(std::string& e){
  e.clear();const auto* s=canonical();if(!s){clear();e="missing canonical two-body source scene after actual reset";return false;}
  // A repeated event is never a lifecycle reset or activation capability.
  if(scene==s&&valid())return true;
  clear();scene=s;epoch=s->incarnation();campaign=s->selectedCampaign();fingerprint=s->selectedFingerprint();catalog=s->sourceCatalog();
  captains={s->captainAt(0),s->captainAt(1)};player=s->moviePlayer();
  // Source Creature::init enables CF_IsAlive; Navi::onInit resets the u8
  // invincibility counter to zero. This event is after actual original roster
  // reset/loading, not an inference from HP or native Creature::isAlive().
  alive={true,true};frames={0,0};timers={};game=Phase::Loading;return true;
 }
 bool activate(std::string& e){
  e.clear();if(!valid()){e="source activation requires actual reset/loading event";return false;}
  if(game==Phase::GameWorldActive)return true;
  const auto* expectedScene=scene;const auto expectedEpoch=epoch;
  const auto expectedPhase=game;const bool expectedCompletion=completedBootstrap;
  if(!completedBootstrap){
   if(game!=Phase::Loading){e="source initial bootstrap is not in Loading";return false;}
   if(!pc_p2_original_captain_bootstrap_complete){e="missing actual typed Walk/control bootstrap completion owner";return false;}
   if(!pc_p2_original_captain_bootstrap_complete(*scene,e)){if(e.empty())e="actual typed Walk/control bootstrap incomplete";return false;}
  }else {
   if(!pc_p2_original_captain_continuation_valid){e="missing actual source state/control continuation owner";return false;}
   if(!pc_p2_original_captain_continuation_valid(*scene,e)){if(e.empty())e="actual source state/control continuation refused";return false;}
  }
  if(!valid()||scene!=expectedScene||epoch!=expectedEpoch||game!=expectedPhase||completedBootstrap!=expectedCompletion){e="source lifecycle changed during activation preflight";return false;}
  completedBootstrap=true;game=Phase::GameWorldActive;return true;
 }
} runtime;
State* ownedState(Navi* n,StateId id){
 if(runtime.slot(n)<0||!n)return nullptr;
 auto* native=n->getCurrState();auto* state=dynamic_cast<State*>(native);
 return state&&state->nativeState()==native&&state->sourceStateId()==id?state:nullptr;
}
}
const World* pc_p2_original_captain_world(){return runtime.valid()?&runtime:nullptr;}
bool pc_p2_original_captain_body_reset_loaded(std::string& e){return runtime.loadedAfterReset(e);}
bool pc_p2_original_captain_activate_after_bootstrap(std::string& e){return runtime.activate(e);}
void pc_p2_original_captain_main_game_entered(){std::string e;runtime.activate(e);}
void pc_p2_original_captain_main_game_left(){if(runtime.valid())runtime.game=Phase::Inactive;}
void pc_p2_original_captain_movie_started(MoviePlayer* p){if(p){observedMovie=p;observedDemo=Demo::Playing;}}
void pc_p2_original_captain_movie_ended(MoviePlayer* p){if(p){observedMovie=p;observedDemo=Demo::Inactive;}}
void pc_p2_original_captain_actor_update(Navi* n){
 const int slot=runtime.slot(n);if(slot<0)return;
 // Retail Navi::update decrements mInvincibleTimer once per actor update,
 // independent of delta seconds and current state, never per global idle tick.
 if(runtime.frames[slot])--runtime.frames[slot];
 auto& timers=runtime.timers[slot];
 const auto& velocity=n->mTargetVelocity;
 const float speed=std::sqrt(velocity.x*velocity.x+velocity.y*velocity.y+velocity.z*velocity.z);
 if(timers.disbandDisable>0&&speed>20.0f)--timers.disbandDisable;
 if(timers.throwDisable){
  if(n->mKontroller&&(n->mKontroller->mCurrentInput&KBBTN_A))timers.throwDisable=10;
  --timers.throwDisable;
 }
}
bool pc_p2_original_captain_actor_alive(const Navi* n){const int slot=runtime.slot(n);return slot>=0&&runtime.alive[slot];}
bool pc_p2_original_captain_actor_lifetime(const Navi* n,bool& out){const int slot=runtime.slot(n);if(slot<0)return false;out=runtime.alive[slot];return true;}
bool pc_p2_original_captain_actor_frames(const Navi* n,std::uint8_t& out){const int slot=runtime.slot(n);if(slot<0)return false;out=runtime.frames[slot];return true;}
bool pc_p2_original_captain_damaged_cleanup(Navi* n){
 if(!ownedState(n,StateId::Damaged))return false;
 runtime.frames[runtime.slot(n)]=60;return true;
}
bool pc_p2_original_captain_dead_entered(Navi* n){
 if(!ownedState(n,StateId::Dead))return false;
 runtime.alive[runtime.slot(n)]=false;return true;
}

bool pc_p2_original_captain_actor_timers(const Navi* n,PcOriginalCaptainTimers& out){const int slot=runtime.slot(n);if(slot<0)return false;out=runtime.timers[slot];return true;}
bool pc_p2_original_captain_start_throw_disable(Navi* n){if(!ownedState(n,StateId::Nuku))return false;runtime.timers[runtime.slot(n)].throwDisable=10;return true;}
bool pc_p2_original_captain_party_released(Navi* n){const int slot=runtime.slot(n);auto* native=n?n->getCurrState():nullptr;auto* state=dynamic_cast<State*>(native);if(slot<0||runtime.game!=Phase::GameWorldActive||!state||state->nativeState()!=native)return false;runtime.timers[slot].disbandDisable=60;return true;}
