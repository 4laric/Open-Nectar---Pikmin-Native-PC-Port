// Native direct-control diagnostic. Fixture placement and collision stimuli
// are disclosed controls, not ordinary player or full-course acceptance.
#include <SDL2/SDL.h>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include "system.h"
#include "App.h"
#include "Node.h"
#include "FlowController.h"
#include "MoviePlayer.h"
#include "Navi.h"
#include "NaviMgr.h"
#include "NaviState.h"
#include "Piki.h"
#include "PikiMgr.h"
#include "ItemMgr.h"
#include "PikiHeadItem.h"
#include "GoalItem.h"
#include "Pellet.h"
#include "GameStat.h"
#include "Generator.h"
#include "MapMgr.h"
#include "teki.h"
#include "Collision.h"
#include "Interactions.h"
#include "pc_bbft.h"
#include "pc_window.h"
#include "pc_gpu_preference.h"
#include "pc_p2_original_foliage_native.h"
#include "pc_p2_original_group_engine.h"
#include "pc_randomizer.h"
#include "pc_coop.h"
#include "netplay/pc_sim_rng.h"
#include "settings/pc_settings.h"
#include "settings/pc_settings_p2d.h"
namespace {
using namespace p2original;
using namespace p2original::foliage;
bool human=false,refusal=false;
void require(bool ok,const char* message){if(!ok){std::printf("FAIL ORIGINAL_FOLIAGE %s\n",message);std::fflush(nullptr);std::_Exit(1);}}
void checked(bool ok,const std::string& e){if(!ok)std::fprintf(stderr,"ORIGINAL_FOLIAGE_ERROR %s\n",e.c_str());require(ok,"native foliage operation");}
int heads(){int count=0;Iterator it(itemMgr->getPikiHeadMgr());CI_LOOP(it)if(static_cast<PikiHeadItem*>(*it)->isAlive())++count;return count;}
struct HeapScope{int previous;HeapScope():previous(gsys->setHeap(SYSHEAP_App)){}~HeapScope(){gsys->setHeap(previous);}};
class FoliageApp final:public PlugPikiApp {
 std::chrono::steady_clock::time_point started=std::chrono::steady_clock::now();
 std::unique_ptr<Native> native;
 std::vector<CatalogRow> rows;
 std::array<std::unique_ptr<Generator>,3> generators;
 std::array<std::string,3> cache;
 std::array<Creature*,2> actors{};
 std::array<InstanceIdentity,2> firstIdentity;
 std::array<Vector3f,2> collisionCentre;
 std::array<float,2> health{};
 int ready=0,phase=0,age=0,pelletBaseline=0,rewardBaseline=0,tekiBaseline=0;
 bool captainSeen=false;
 const std::string fingerprint=std::string(64,'f');
 CatalogRow literal(unsigned source,unsigned index,unsigned uid,const Position& at,float facing){
  CatalogRow r;r.course="tutorial";r.member="plantsgen.txt";r.index=index;r.sourceKey="tutorial/plantsgen.txt#"+std::to_string(index);
  r.enemy.source=source;r.enemy.uid=uid;r.enemy.count=1;r.enemy.position=at;r.enemy.directionDegrees=facing;return r;
 }
 int rewards(){auto* onion=itemMgr->getContainer(Red);require(onion,"real Red Onion");return heads()+onion->getTotalStorePikis();}
 void checkEconomy(){require(pelletMgr->getSize()==pelletBaseline&&rewards()==rewardBaseline,"decorative foliage produced no cargo or rewards");}
 void install(bool reentry){
  HeapScope heap;std::string e;std::vector<GroupBinding> bindings;
  for(unsigned i=0;i<3;++i){if(!generators[i])generators[i]=std::make_unique<Generator>();auto* g=generators[i].get();
   g->mGenType=nullptr;g->mGenObject=nullptr;g->mRespawnInterval=0;g->mDayLimit=-1;g->mCarryOverFlags=0;
   GroupBinding b;b.generator=g;b.state.uid=rows[i].enemy.uid;b.state.count=rows[i].enemy.count;b.state.resurrectionDays=g->mRespawnInterval;
   if(reentry)checked(decodeOriginalState(fingerprint,b.state.uid,b.state.count,cache[i],b.state,e),e);
   bindings.push_back(b);
  }
  checked(pc_p2_original_course_install(bindings,native->provider(),e),e);
  // Disc reentry deliberately exercises original respawn/activation. RAM rows
  // with literal reserved=0 require the separate saved-creature loader.
  const bool oldRam=Generator::ramMode;Generator::ramMode=false;
  for(auto& g:generators)g->init();Generator::ramMode=oldRam;
  require(native->provider().size()==2&&tekiMgr->getSize()==tekiBaseline+2,"two native births; count-zero row has no actor");
  actors.fill(nullptr);Iterator it(tekiMgr);CI_LOOP(it){Creature* actor=*it;unsigned source=0,token=0;InstanceIdentity identity;
   if(!originalActors().query(actor,source,token,&identity))continue;
   unsigned i=source==91?0:source==88?1:2;require(i<2&&!actors[i],"distinct original foliage identity");actors[i]=actor;
   require(native->owns(actor)&&identity.generator==rows[i].enemy.uid&&identity.ordinal==0&&token,"native source UID and ordinal association");
   auto* h=native->provider().lookup(actor);require(h&&h->generator==generators[i].get()&&h->token==token,"native generator and family binding");
   if(reentry)require(identity.activation==firstIdentity[i].activation+1&&identity.epoch==firstIdentity[i].epoch+1,"disc cache reentry advances epoch and activation");else firstIdentity[i]=identity;
   require(actor->mCollInfo&&actor->mCollInfo->getBoundingSphere(),"actual source static collider");
   collisionCentre[i]=actor->mCollInfo->getBoundingSphere()->mCentre;health[i]=actor->mHealth;
   std::printf("ORIGINAL_FOLIAGE_BIRTH source=%u uid=%u ordinal=%u epoch=%llu activation=%llu reentry=%d\n",source,identity.generator,identity.ordinal,(unsigned long long)identity.epoch,(unsigned long long)identity.activation,int(reentry));
  }
  require(actors[0]&&actors[1],"both literal variants physically admitted");checkEconomy();
 }
 void setup(Navi* n){
  HeapScope heap;std::string e;require(originalActors().rows().empty(),"fixture does not replace another original course");
  // Literal source/type/tail and UID from day5 rows #0/#4. These nearby
  // positions are fixture relocations, not proof of original course geometry.
  Position a{n->mSRT.t.x+90,0,n->mSRT.t.z},b{n->mSRT.t.x-90,0,n->mSRT.t.z};
  a.y=mapMgr->getMinY(a.x,a.z,true);b.y=mapMgr->getMinY(b.x,b.z,true);require(std::isfinite(a.y)&&std::isfinite(b.y),"actual native arena floor");
  rows={literal(91,0,1390538979u,a,0),literal(88,4,1381420794u,b,95)};
  auto zero=literal(91,0,originalGeneratorUid("fixture/foliage-zero#0"),a,0);zero.course="fixture";zero.member="foliage-zero";zero.sourceKey="fixture/foliage-zero#0";zero.enemy.count=0;rows.push_back(zero);
  native=std::make_unique<Native>();tekiBaseline=tekiMgr->getSize();pelletBaseline=pelletMgr->getSize();rewardBaseline=rewards();
  bool physical=native->provider().preflight(rows,e);
  if(refusal){require(!physical&&native->provider().size()==0&&tekiMgr->getSize()==tekiBaseline&&pelletMgr->getSize()==pelletBaseline,"physical resource refusal before allocation");std::puts("PASS ORIGINAL_FOLIAGE_RESOURCE_REFUSAL births=0 direct_control=1 gameplay=0");std::fflush(nullptr);std::_Exit(0);}
  checked(physical,e);checked(native->geometryOwnershipControl(e),e);
  checked(originalActors().install(fingerprint,rows,[](const CatalogRow& r,std::string& err){return decode(r,err);},e),e);install(false);
  std::puts("ORIGINAL_FOLIAGE_READY sources=91,88 zero_count=1 baseline=20 window=960x540 direct_control=1 original_positions=0 gameplay=0");std::fflush(nullptr);
 }
 void stimulus(Navi* n){
  for(unsigned i=0;i<2;++i){auto* actor=static_cast<BTeki*>(actors[i]);auto* h=native->provider().lookup(actor);require(h,"retained live native host");
   // Calls the real engine collision dispatch using a genuine captain and its
   // velocity. This is a disclosed callback control, not a walking test.
   const Vector3f priorPosition=n->mSRT.t,priorVelocity=n->mVelocity;const bool alreadyActive=h->active;const float priorFrame=h->frame;
   n->mSRT.t=actor->mSRT.t;n->mVelocity.set(2,0,0);CollEvent collision(n,nullptr,actor->mCollInfo->getBoundingSphere());actor->collisionCallback(collision);
   n->mSRT.t=priorPosition;n->mVelocity=priorVelocity;
   require(h->active&&h->touched&&(alreadyActive?h->frame==priorFrame:h->frame==0),"real captain collision starts or retains typed touched motion");
   InteractAttack attack(n,actor->mCollInfo->getBoundingSphere(),1000,false);actor->stimulate(attack);InteractPress press(n,1000);actor->stimulate(press);
   require(actor->mHealth==health[i]&&actor->isAlive()&&h->active,"actual attack and press cannot damage decorative foliage");
  }
  checkEconomy();
 }
 void checkStatic(){for(unsigned i=0;i<2;++i){auto* actor=actors[i];const auto* h=native->provider().lookup(actor);require(h&&actor->mHealth==health[i]&&actor->isAlive(),"native foliage remains invulnerable");
   const auto& centre=actor->mCollInfo->getBoundingSphere()->mCentre;const auto& before=collisionCentre[i];require(std::fabs(centre.x-before.x)<.01f&&std::fabs(centre.y-before.y)<.01f&&std::fabs(centre.z-before.z)<.01f,"animated pose does not move static source collider");
   require(actor->mVelocity.x==0&&actor->mVelocity.y==0&&actor->mVelocity.z==0,"native scenery remains constrained");}
  checkEconomy();
 }
 void reenter(){std::string e;for(unsigned i=0;i<3;++i){checked(pc_p2_original_groups().cache(generators[i].get(),fingerprint,cache[i],e),e);GeneratorState s;unsigned live=0;require(pc_p2_original_groups().state(generators[i].get(),s,live)&&s.deathCount==0&&live==rows[i].enemy.count,"cache preserves literal count and no decorative deaths");}
  checked(pc_p2_original_course_unload(e),e);require(native->provider().size()==0&&tekiMgr->getSize()==tekiBaseline,"native unload cleans only owned foliage");checkEconomy();install(true);
 }
public:
 int idle()override{
  const auto budget=std::chrono::seconds(human?600:90);
  if(std::chrono::steady_clock::now()-started>=budget){std::puts("ORIGINAL_FOLIAGE_GUARD_EXIT exit=86 bounded=1");std::fflush(nullptr);std::_Exit(86);}
  int result=PlugPikiApp::idle();auto* n=naviMgr?naviMgr->getNavi():nullptr;const bool initialized=n&&n->getCurrState();if(initialized)captainSeen=true;
  require(!captainSeen||initialized,"initialized captain retained");if(initialized)require(!GameStat::orimaDead&&!naviMgr->isNaviDead(n)&&n->getCurrState()->getID()!=NAVISTATE_Dead&&std::isfinite(n->mHealth)&&n->mHealth>0,"captain guard");
  if(gameflow.mMoviePlayer&&gameflow.mMoviePlayer->mIsActive){gameflow.mMoviePlayer->requestSkip();return result;}
  if(!initialized||!pc_randomizer_ready()||!pikiMgr||!tekiMgr||!pelletMgr||!itemMgr||!mapMgr||gameflow.mPauseAll||gameflow.mIsUIOverlayActive)return result;
  if(!phase){if(n->getCurrState()->getID()!=NAVISTATE_Walk||++ready<45)return result;int live=0,red=0;Iterator it(pikiMgr);CI_LOOP(it){auto* p=static_cast<Piki*>(*it);if(p->isAlive()){++live;red+=p->mColor==Red;}}require(live==20&&red==20,"actual20 Red Pikmin baseline");setup(n);phase=1;return result;}
  if(human)return result;checkStatic();++age;
  if(phase==1&&age>=15){stimulus(n);phase=2;age=0;}
  else if(phase==2){bool finished=true;for(auto* actor:actors){auto* h=native->provider().lookup(actor);require(h,"active callback retains same family host");if(age==1)require(h->frame>0,"normal engine update advances motion");finished&=!h->active&&!h->touched;}
   if(finished){reenter();phase=3;age=0;}else require(age<900,"normal native animation eventually completes");}
  else if(phase==3&&age>=30){std::string e;checked(pc_p2_original_course_unload(e),e);require(native->provider().size()==0&&tekiMgr->getSize()==tekiBaseline,"final owned cleanup");checkEconomy();std::puts("PASS ORIGINAL_FOLIAGE sources=91,88 resources=1 collider_static=1 invulnerable=1 typed_touch=1 normal_animation=1 cache_disc_reentry=1 no_rewards=1 direct_control=1 gameplay=0");std::fflush(nullptr);std::_Exit(0);}
  return result;
 }
};
}
int main(int argc,char** argv){
 human=std::getenv("P2_ORIGINAL_FOLIAGE_HUMAN")!=nullptr;refusal=std::getenv("P2_ORIGINAL_FOLIAGE_REFUSE_RESOURCES")!=nullptr;
 SDL_setenv("PIKMIN_RANDOMIZER_TEST_BACKGROUND","1",1);SDL_setenv("SDL_AUDIODRIVER","dummy",1);SDL_SetMainReady();
 pc_sim_rng_note_main_thread();std::string e;checked(pc_sim_rng_begin_offline(0x9188,0x8891,e),e);pc_gpu_preference_apply();pc_bbft_init(argc,argv);
 require(pc_randomizer_enabled()&&pc_pikipelago_surface_course()&&!std::strcmp(pc_pikipelago_surface_course(),"tutorial")&&!pc_pikipelago_room_preview(),"real tutorial course assets required");
 require(pc_window_init("Original foliage direct-control diagnostic",960,540),"native window");pc_settings_init();pc_window_set_control_mode(PC_CONTROL_CLASSIC);pc_window_set_display_mode(0);pc_window_set_window_size(960,540);pc_window_center();
 SDL_Window* window=SDL_GL_GetCurrentWindow();int width=0,height=0,x=0,y=0;SDL_GetWindowSize(window,&width,&height);SDL_GetWindowPosition(window,&x,&y);SDL_Rect bounds{};SDL_GetDisplayBounds(SDL_GetWindowDisplayIndex(window),&bounds);
 require(width==960&&height==540&&std::abs(x-(bounds.x+(bounds.w-width)/2))<=2&&std::abs(y-(bounds.y+(bounds.h-height)/2))<=2,"centered960x540 baseline");
 pc_coop_set_pending(false);gsys->Initialise();pc_settings_p2d_init();nodeMgr=new NodeMgr();gsys->run(new FoliageApp());return 0;
}
