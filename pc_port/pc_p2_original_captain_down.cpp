#include "pc_p2_original_captain_down.h"
#include "pc_p2_original_captain_down_sequence.h"
#include "pc_p2_original_captain_motion.h"
#include "Navi.h"
#include <cmath>
#include <cstdio>
#include <memory>

extern p2original::captain::DownSection* pc_p2_original_captain_down_section() __attribute__((weak));
extern p2original::captain::SourceBank* pc_p2_original_captain_source_bank() __attribute__((weak));
using namespace p2original::captain;
namespace {
struct Movie {
 const LoadedScene* scene=nullptr;std::uint64_t incarnation=0;
 DownSection* section=nullptr;SourceBank* bank=nullptr;Navi* actor=nullptr;
 std::unique_ptr<p2original::down::Clock> clock;
 bool failed=false;
};
Movie movie;
const LoadedScene* bitsScene=nullptr;std::uint64_t bitsIncarnation=0;unsigned deadBits=0;
bool canonical(const World*& w,const LoadedScene*& scene,DownSection*& section,SourceBank*& bank,std::string& e){
 w=pc_p2_original_captain_world();scene=pc_p2_original_captain_loaded_scene();
 section=pc_p2_original_captain_down_section?pc_p2_original_captain_down_section():nullptr;
 bank=pc_p2_original_captain_source_bank?pc_p2_original_captain_source_bank():nullptr;
 if(!w||!scene||w->incarnation()!=scene->incarnation()
  ||w->selectedCampaign()!=scene->selectedCampaign()||w->selectedFingerprint()!=scene->selectedFingerprint()
  ||w->sourceCatalog()!=scene->sourceCatalog()||w->captainAt(0)!=scene->captainAt(0)||w->captainAt(1)!=scene->captainAt(1)
  ||!section||&section->scene()!=scene||!bank||!bank->ready()){
  e="missing canonical source section, roster or movie bank";return false;
 }
 if(bitsScene!=scene||bitsIncarnation!=scene->incarnation()){
  bitsScene=scene;bitsIncarnation=scene->incarnation();deadBits=0;
 }
 return true;
}
bool current(std::string& e){
 const World* w;const LoadedScene* scene;DownSection* section;SourceBank* bank;
 if(!canonical(w,scene,section,bank,e)||!movie.scene||scene!=movie.scene
  ||scene->incarnation()!=movie.incarnation||section!=movie.section||bank!=movie.bank
  ||(scene->captainAt(0)!=movie.actor&&scene->captainAt(1)!=movie.actor)){
  movie=Movie{};e="source Down movie ownership retired";return false;
 }
 return true;
}
bool consume(const std::vector<p2original::down::Event>& events,std::string& e){
 for(const auto& event:events){
  using p2original::down::Kind;bool ok=true;
  switch(event.kind){
  case Kind::CameraCommand:ok=movie.section->cameraCommand(*movie.actor,event.value,e);break;
  case Kind::ActorDemoAnimation:break; // bank.startDownMovie bound the actual BCK
  case Kind::SectionCommand:ok=movie.section->movieCommand(*movie.actor,event.value,e);break;
  case Kind::ActorCommand:ok=movie.section->actorCommand(*movie.actor,event.value,e);break;
  case Kind::StudioExhausted:ok=movie.section->beginFinishing(*movie.actor,e);break;
  case Kind::FinishCallback:{
   ok=movie.section->laydownAndInformDeath(*movie.actor,e);
   if(ok){unsigned slot=movie.scene->captainAt(0)==movie.actor?0:1;deadBits|=1u<<slot;
    ok=movie.section->finished(*movie.actor,deadBits==3,e);}
   break;
  }
  case Kind::Inactive:ok=movie.section->inactive(*movie.actor,e);break;
  }
  if(!ok){movie.failed=true;return false;}
 }
 return true;
}
}
bool pc_p2_original_captain_down_preflight(const Navi* n,std::string& e){
 e.clear();
 const World* w;const LoadedScene* scene;DownSection* section;SourceBank* bank;
 if(!canonical(w,scene,section,bank,e))return false;
 if(w->phase()!=Phase::GameWorldActive||!n||(scene->captainAt(0)!=n&&scene->captainAt(1)!=n)){
  e="source Down requires active original roster actor";return false;
 }
 bool alive=false;if(!pc_p2_original_captain_actor_lifetime(n,alive)||!alive){e="source Down actor is not alive";return false;}
 if(movie.scene&&current(e)&&movie.clock&&(movie.failed||movie.clock->phase()!=p2original::down::Phase::Inactive)){e="source Down movie already active or refused";return false;}
 MotionState motion;if(!bank->state(n,motion,e))return false;
 std::string bytes;if(!bank->sourceBytes(SourceResource::DownStudio,bytes,e)
  ||!p2original::down::Sequence::authenticate(bytes.data(),bytes.size(),e))return false;
 return section->preflight(*const_cast<Navi*>(n),e);
}
bool pc_p2_original_captain_down_begin(Navi* n,std::string& e){
 if(!pc_p2_original_captain_down_preflight(n,e))return false;
 const World* w;const LoadedScene* scene;DownSection* section;SourceBank* bank;
 if(!canonical(w,scene,section,bank,e))return false;
 std::string bytes;if(!bank->sourceBytes(SourceResource::DownStudio,bytes,e))return false;
 auto sequence=p2original::down::Sequence::authenticate(bytes.data(),bytes.size(),e);if(!sequence)return false;
 Movie next;next.scene=scene;next.incarnation=scene->incarnation();next.actor=n;next.section=section;next.bank=bank;
 next.clock=std::make_unique<p2original::down::Clock>(sequence);
 if(!bank->startDownMovie(n,e)||!section->movieStarted(*n,e))return false;
 movie=std::move(next);std::vector<p2original::down::Event> events;
 return movie.clock->start(events,e)&&consume(events,e);
}
void pc_p2_original_captain_down_update(MoviePlayer* player,float delta){
 if(!movie.scene||!movie.clock||movie.failed)return;
 std::string e;if(!current(e))return;
 if(player!=movie.scene->moviePlayer())return; // actual owning native update only
 if(!std::isfinite(delta)||delta<0){movie.failed=true;e="invalid source movie delta";}
 else if(movie.clock->phase()==p2original::down::Phase::Studio){
  // Retail MoviePlayer::update calls StudioControl::forward(1) once per
  // active update. seconds-per-frame config controls studio interpolation;
  // it does not turn the sequence into an elapsed wall-clock timer.
  auto events=movie.clock->advanceStudio(1);
  if(!movie.bank->updateDownMovie(movie.actor,static_cast<float>(movie.clock->tick()),e))movie.failed=true;
  else consume(events,e);
 }else if(movie.clock->phase()==p2original::down::Phase::Finishing){
  std::vector<p2original::down::Event> events;
  if(!movie.clock->advanceFinishing(delta,events,e)||!consume(events,e))movie.failed=true;
 }
 if(movie.failed)std::fprintf(stderr,"[original captain Down refused] %s\n",e.c_str());
}
bool pc_p2_original_captain_down_demo(Demo& out){
 if(!movie.scene||!movie.clock)return false;std::string e;if(!current(e))return false;
 out=!movie.failed&&movie.clock->phase()==p2original::down::Phase::Inactive?Demo::Inactive:Demo::Playing;return true;
}
bool pc_p2_original_captain_down_dead_bits(unsigned& out){
 const World* w;const LoadedScene* scene;DownSection* section;SourceBank* bank;std::string e;
 if(!canonical(w,scene,section,bank,e)||movie.failed)return false;out=deadBits;return true;
}
