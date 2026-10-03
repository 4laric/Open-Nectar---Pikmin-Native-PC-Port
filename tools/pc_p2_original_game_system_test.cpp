#include "pc_p2_original_game_system.h"
#include "pc_p2_retail_scene.h"
#include <iostream>
#include <memory>
using namespace p2original;
namespace {
unsigned checks=0;void check(bool c,const char* t){++checks;if(!c){std::cerr<<t<<'\n';std::exit(1);}}
const std::string campaign(64,'a'),session(64,'b'),catalog="catalog";
bool threadCurrent=true,sceneCurrent=true,callbackPhaseChange=false;
const p2retail::SceneContext* prepared=nullptr;const p2retail::SceneContext* committed=nullptr;
GameSystem* reenter=nullptr;
struct Scene final:captain::LoadedScene {
 const std::string& selectedCampaign()const override{return campaign;}
 const std::string& selectedFingerprint()const override{return session;}
 const std::string& sourceCatalog()const override{return catalog;}
 MoviePlayer* moviePlayer()const override{return nullptr;}
 std::uint64_t incarnation()const override{return 8;}
 Navi* captainAt(unsigned i)const override{return reinterpret_cast<Navi*>(0x4000+i*0x100);}
} scene;
}
namespace p2retail {
class SceneRuntime {public:
 static SceneContext* make(){auto* p=new SceneContext;p->mStage=reinterpret_cast<StageInfo*>(0x2000);p->mMap=reinterpret_cast<MapMgr*>(0x3000);
  p->mSnapshot.scene.serial=8;p->mRevision=3;p->mCampaign=campaign;p->mSession=session;return p;}
 static void phase(SceneContext& s,ScenePhase p){s.mPhase=p;}
 static void revision(SceneContext& s){++s.mRevision;}
};
bool SceneContext::ownsCurrentThread()const noexcept{return threadCurrent;}
}
const p2retail::SceneContext* pc_p2_retail_scene_prepared()noexcept{return prepared;}
const p2retail::SceneContext* pc_p2_retail_scene_committed()noexcept{return committed;}
namespace p2original {namespace piki {
class NativeBodyFactory {public:
 static std::unique_ptr<GameSystem> make(){return std::unique_ptr<GameSystem>(new GameSystem);}
 static bool init(GameSystem& g,const p2retail::SceneContext& s,std::string& e){return g.sourceInit(s,e);}
 static bool start(GameSystem& g,std::string& e){return g.sourceStartFrame(e);}
 static bool end(GameSystem& g,std::string& e){return g.sourceEndFrame(e);}
 static bool surface(GameSystem& g,std::string& e){return g.sourceGameStateInit(e);}
 static bool cave(GameSystem& g,std::string& e){return g.sourceCaveStateInit(e);}
 static bool gameStart(GameSystem& g,std::string& e){return g.sourceGameStart(e);}
 static bool left(GameSystem& g,std::string& e){return g.sourceSectionLeft(e);}
 static bool wait(GameSystem& g,std::string& e){return g.sourceWaitSyncLoadPause(e);}
 static bool complete(GameSystem& g,std::string& e){return g.sourceWaitSyncLoadComplete(e);}
 static bool open(GameSystem& g,Navi* n,std::string& e){return g.sourceContainerScreenOpened(n,e);}
 static bool cleanup(GameSystem& g,Navi* n,std::string& e){return g.sourceContainerCleanup(n,e);}
 static bool release(GameSystem& g,std::string& e){return g.sourceRelease(e);}
 // Pure overflow boundary injection, no game event can set this counter.
 static void frameBoundary(GameSystem& g){g.mState.frameTimer=0x40000000;}
};
bool nativeSceneBinding(SceneBinding& out,bool,std::string&){if(!sceneCurrent)return false;
 SceneBinding b;b.scene=&scene;b.incarnation=8;b.campaign=campaign;b.fingerprint=session;b.catalog=catalog;b.captains[0]=scene.captainAt(0);b.captains[1]=scene.captainAt(1);out=b;return true;}
bool nativeSceneCurrent(const SceneBinding& b,bool,std::string& e){
 if(callbackPhaseChange){callbackPhaseChange=false;p2retail::SceneRuntime::phase(*const_cast<p2retail::SceneContext*>(prepared),p2retail::ScenePhase::Releasing);}
 if(reenter){auto* g=reenter;reenter=nullptr;GameSystemState out;out.flags=99;check(!g->readState(out,e)&&out.flags==99,"nested read unchanged");}
 return sceneCurrent&&b.scene==&scene&&b.incarnation==8;
}
bool nativeCaptainFacts(const SceneBinding& b,const Navi* n,bool,std::string& e){return nativeSceneCurrent(b,true,e)&&(n==b.captains[0]||n==b.captains[1]);}
} }
using Owner=piki::NativeBodyFactory;
int main(){
 std::unique_ptr<p2retail::SceneContext> stage(p2retail::SceneRuntime::make());prepared=stage.get();
 auto g=Owner::make();std::string e;GameSystemState s;s.flags=99;
 check(!g->readState(s,e)&&s.flags==99,"uninitialized read refuses unchanged");
 check(Owner::init(*g,*stage,e),"actual-header Stage init");
 check(g->readState(s,e)&&s.flags==0&&!s.frozen&&!s.moviePause&&!s.softPause&&s.pauseCountdown==0&&s.frameTimer==0&&s.unused==0&&s.mode==GameSystemMode::Story,"literal actual GameSystem init defaults");
 check(!Owner::init(*g,*stage,e),"repeat init cannot erase state");
 bool cave=true;check(!g->inCave(cave,e)&&cave,"mIsInCave unavailable before actual section");
 check(!g->readTime(e)&&!g->readMovie(e)&&!g->readMovieDraw(e),"unsupported producers refuse instead of false facts");
 check(Owner::wait(*g,e)&&g->readState(s,e)&&s.pauseCountdown==3&&s.softPause&&!s.paused(),"pause grace countdown expression");
 check(Owner::start(*g,e)&&Owner::end(*g,e)&&g->readState(s,e)&&s.pauseCountdown==1&&s.frameTimer==1&&!s.paused(),"both frame boundaries decrement");
 check(Owner::start(*g,e)&&g->readState(s,e)&&s.pauseCountdown==0&&s.paused(),"paused activates at zero");
 check(Owner::wait(*g,e)&&g->readState(s,e)&&s.pauseCountdown==0,"already paused startPause does not refresh");
 check(Owner::complete(*g,e)&&g->readState(s,e)&&s.pauseCountdown==3&&!s.softPause&&!s.paused(),"wait complete clears soft bit with grace3");
 for(unsigned i=0;i<5;++i)check(Owner::end(*g,e),"endFrame saturated countdown");
 check(g->readState(s,e)&&s.pauseCountdown==0&&!s.paused(),"countdown never underflows");
 Owner::frameBoundary(*g);check(Owner::start(*g,e)&&g->readState(s,e)&&s.frameTimer==0,"retail frame count wrap");
 check(!Owner::cave(*g,e)&&g->readState(s,e)&&s.flags==0,"uncommitted physical floor cannot enter section");
 committed=stage.get();check(!Owner::cave(*g,e),"committed lookup cannot substitute wrong Stage phase");
 p2retail::SceneRuntime::phase(*stage,p2retail::ScenePhase::Committed);
 check(Owner::cave(*g,e)&&g->inCave(cave,e)&&cave,"actual cave section sets source inCave");
 check(g->readState(s,e)&&(s.flags&0x20)&&!(s.flags&2),"world active distinct from game playing");
 check(Owner::gameStart(*g,e)&&g->readState(s,e)&&(s.flags&2),"actual gameStart enables playing");
 check(!Owner::release(*g,e),"active section blocks release");
 check(!Owner::open(*g,reinterpret_cast<Navi*>(0x5000),e),"foreign source roster refuses container event");
 check(Owner::cleanup(*g,scene.captainAt(0),e)&&g->readState(s,e)&&!s.frozen&&!s.moviePause,"source cleanup also clears after screen never opened");
 check(Owner::open(*g,scene.captainAt(0),e)&&g->readState(s,e)&&s.frozen&&s.moviePause,"actual successful screen event frozen/moviePause");
 check(!Owner::cleanup(*g,scene.captainAt(1),e)&&g->readState(s,e)&&s.frozen,"other roster slot cannot erase retained container");
 check(!Owner::release(*g,e),"live container blocks release");
 check(Owner::cleanup(*g,scene.captainAt(0),e)&&g->readState(s,e)&&!s.frozen&&!s.moviePause,"exact source Container cleanup");
 reenter=g.get();check(!Owner::start(*g,e),"nested callback read blocks outer frame event");
 check(g->readState(s,e)&&s.frameTimer==0,"failed callback does not advance frame");
 callbackPhaseChange=true;check(!Owner::start(*g,e),"source callback Stage phase fence");p2retail::SceneRuntime::phase(*stage,p2retail::ScenePhase::Committed);
 threadCurrent=false;check(!Owner::start(*g,e),"actual creating thread fence");threadCurrent=true;
 sceneCurrent=false;s.flags=99;check(!g->readState(s,e)&&s.flags==99,"canonical source scene fence unchanged read");sceneCurrent=true;
 check(Owner::left(*g,e)&&Owner::surface(*g,e)&&g->inCave(cave,e)&&!cave,"surface enter after cave is actual distinct source event");
 check(g->readState(s,e)&&(s.flags&0x20)&&!(s.flags&2),"section init clears playing");
 check(Owner::left(*g,e)&&Owner::release(*g,e),"empty section teardown");
 check(!g->readState(s,e),"released state unavailable");
 auto stale=Owner::make();check(!Owner::init(*stale,*stage,e),"GameSystem init cannot start after physical commit");
 p2retail::SceneRuntime::phase(*stage,p2retail::ScenePhase::Prepared);
 check(Owner::init(*stale,*stage,e),"new owner init");p2retail::SceneRuntime::revision(*stage);
 check(!Owner::start(*stale,e),"selection revision refuses source frame");
 std::cout<<checks<<" source GameSystem scalar controls passed; synthetic authority only, movie/time unavailable\n";
}
