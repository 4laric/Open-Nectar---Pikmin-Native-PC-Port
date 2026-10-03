#include "pc_p2_original_piki_update_schedule.h"
#include "pc_p2_original_piki_host.h"
#include "pc_p2_retail_scene.h"
#include <iostream>
#include <map>
#include <memory>
#include <thread>
using namespace p2original::piki;
namespace {
unsigned checks=0;void check(bool c,const char* message){++checks;if(!c){std::cerr<<message<<'\n';std::exit(1);}}
const std::string campaign(64,'a'),session(64,'b'),catalog="actual-test-catalog";
bool threadCurrent=true,sceneCurrent=true,failHost=false;std::map<Piki*,std::uint64_t> bodies;
bool alterPhase=false;
const p2retail::SceneContext* prepared=nullptr;PikiUpdateSchedule* reenter=nullptr;
struct Scene final:p2original::captain::LoadedScene {
 const std::string& selectedCampaign()const override{return campaign;}
 const std::string& selectedFingerprint()const override{return session;}
 const std::string& sourceCatalog()const override{return catalog;}
 MoviePlayer* moviePlayer()const override{return nullptr;}
 std::uint64_t incarnation()const override{return 1;}
 Navi* captainAt(unsigned i)const override{return reinterpret_cast<Navi*>(0x1000+i*0x100);}
} scene;
}
namespace p2retail {
// Synthetic authority fixture using the ACTUAL header/private producer friend.
class SceneRuntime {public:
 static SceneContext* make(){auto* p=new SceneContext;p->mStage=reinterpret_cast<StageInfo*>(0x2000);
  p->mMap=reinterpret_cast<MapMgr*>(0x3000);p->mSnapshot.scene.serial=8;p->mRevision=3;
  p->mCampaign=campaign;p->mSession=session;return p;}
 static void revision(SceneContext& s){++s.mRevision;}
 static void phase(SceneContext& s,ScenePhase p){s.mPhase=p;}
};
bool SceneContext::ownsCurrentThread()const noexcept{return threadCurrent;}
}
const p2retail::SceneContext* pc_p2_retail_scene_prepared()noexcept{return prepared;}
namespace p2original {namespace piki {
// Test substitute for the only intended production friend; never linked game.
class NativeBodyFactory {public:
 static std::unique_ptr<PikiUpdateSchedule> make(){return std::unique_ptr<PikiUpdateSchedule>(new PikiUpdateSchedule);}
 static bool bind(PikiUpdateSchedule& s,const p2retail::SceneContext& stage,std::string& e){return s.bind(stage,e);}
 static bool init(PikiUpdateSchedule& s,Handle h,std::string& e){return s.initContext(h,e);}
 static bool exit(PikiUpdateSchedule& s,Handle h,std::string& e){return s.exitContext(h,e);}
 static bool update(PikiUpdateSchedule& s,std::string& e){return s.updateFromPikiMgr(e);}
 static bool release(PikiUpdateSchedule& s,std::string& e){return s.release(e);}
 static void forceControl(PikiUpdateSchedule& s,Handle h,bool f){for(auto& c:s.mContexts)if(c.handle.body==h.body)c.forced=f;}
};
bool nativeSceneBinding(SceneBinding& out,bool,std::string&){if(!sceneCurrent)return false;
 SceneBinding b;b.scene=&scene;b.incarnation=1;b.campaign=campaign;b.fingerprint=session;b.catalog=catalog;
 b.captains[0]=scene.captainAt(0);b.captains[1]=scene.captainAt(1);out=b;return true;}
bool nativeSceneCurrent(const SceneBinding& b,bool,std::string&){return sceneCurrent&&b.scene==&scene&&b.incarnation==1;}
NativeHostPhase nativeHost(Piki* p,NativeHostRead& out,std::string& e){
 if(alterPhase){alterPhase=false;p2retail::SceneRuntime::phase(*const_cast<p2retail::SceneContext*>(prepared),p2retail::ScenePhase::Releasing);}
 if(reenter){auto* s=reenter;reenter=nullptr;bool sentinel=true;check(!s->updatable({p,bodies[p]},sentinel,e)&&sentinel,"nested output unchanged");}
 auto i=bodies.find(p);if(failHost||i==bodies.end())return NativeHostPhase::Unavailable;
 NativeHostRead r;r.handle={p,i->second};r.stage=prepared;r.scene=&scene;r.map=prepared->map();out=r;return NativeHostPhase::Committed;
}
} }
bool pc_p2_original_piki_body_current(const Piki* p,std::uint64_t n)noexcept{
 auto i=bodies.find(const_cast<Piki*>(p));return i!=bodies.end()&&i->second==n;
}
bool pc_p2_original_piki_body_handle(const Piki* p,OriginalPikiBodyHandle& out){
 auto i=bodies.find(const_cast<Piki*>(p));if(i==bodies.end())return false;
 OriginalPikiBodyHandle b;b.nativeLifetime=i->second;b.body.origin.catalogFingerprint=catalog;out=b;return true;
}
const std::string& pc_p2_original_piki_catalog_fingerprint()noexcept{return catalog;}
int main(){
 std::unique_ptr<p2retail::SceneContext> stage(p2retail::SceneRuntime::make());prepared=stage.get();
 auto s=NativeBodyFactory::make();std::string e;check(NativeBodyFactory::bind(*s,*stage,e),"bind actual-header fixture");
 std::array<Handle,101> handles;for(unsigned i=0;i<101;++i){handles[i]={reinterpret_cast<Piki*>(0x4000+i*0x100),1+i};bodies[handles[i].body]=handles[i].lifetime;}
 for(unsigned i=0;i<20;++i)check(NativeBodyFactory::init(*s,handles[i],e),"init20");
 PikiUpdateScheduleState before,after;check(s->state(before,e)&&before.clientCount==20,"registered20");
 for(auto count:before.clients)check(count==2,"balanced first tie scheduling");
 for(unsigned step=0;step<10;++step){unsigned eligible=0;for(unsigned i=0;i<20;++i){bool b=false;check(s->updatable(handles[i],b,e),"gate read");eligible+=b;}
  check(eligible==2,"exactly two of20 each slot");check(NativeBodyFactory::update(*s,e),"one source update");}
 check(s->state(after,e)&&after.currentIndex==0,"ten-slot wraps");
 check(NativeBodyFactory::init(*s,handles[0],e)&&s->state(after,e)&&after.clientCount==20,"reinit removes and rebalances without duplicate");
 check(!NativeBodyFactory::release(*s,e),"live contexts prevent release");
 check(NativeBodyFactory::exit(*s,handles[0],e),"death exit before retirement");bodies.erase(handles[0].body);
 bool gate=true;check(!s->updatable(handles[0],gate,e)&&gate,"dead output unchanged");
 handles[0].lifetime=500;bodies[handles[0].body]=500;check(NativeBodyFactory::init(*s,handles[0],e),"pool address reuse new committed lifetime");
 for(unsigned i=20;i<100;++i)check(NativeBodyFactory::init(*s,handles[i],e),"fill100");
 check(!NativeBodyFactory::init(*s,handles[100],e)&&s->state(after,e)&&after.clientCount==100,"bounded overflow unchanged");
 failHost=true;after.clientCount=999;check(!s->state(after,e)&&after.clientCount==999,"failed retained factory unchanged output");failHost=false;
 threadCurrent=false;check(!NativeBodyFactory::update(*s,e),"creating thread fence");threadCurrent=true;
 alterPhase=true;check(!NativeBodyFactory::update(*s,e),"factory callback phase fence");
 p2retail::SceneRuntime::phase(*stage,p2retail::ScenePhase::Prepared);
 sceneCurrent=false;check(!NativeBodyFactory::exit(*s,handles[0],e),"canonical scene cleanup fence");sceneCurrent=true;
 reenter=s.get();check(!NativeBodyFactory::update(*s,e),"nested read rejects outer update");
 check(s->state(after,e)&&after.currentIndex==0,"failed updates do not advance");
 ++bodies[handles[2].body];check(!NativeBodyFactory::update(*s,e),"stale retained lifetime refuses all-source advancement");
 --bodies[handles[2].body];check(s->state(after,e)&&after.clientCount==100&&after.currentIndex==0,"stale refusal preserves schedule");
 prepared=nullptr;check(!NativeBodyFactory::update(*s,e),"prepared Stage pointer revocation fence");prepared=stage.get();
 // Pure source algorithm forcing control ONLY: genuine Piki context has no
 // SourceFsm force producer. Flying flags must never call this fixture helper.
 NativeBodyFactory::forceControl(*s,handles[1],true);gate=false;
 check(s->updatable(handles[1],gate,e)&&gate,"forced context bypasses current slot");
 check(NativeBodyFactory::exit(*s,handles[1],e)&&s->state(after,e)&&after.clientCount==100,"forced exit source no-op");
 NativeBodyFactory::forceControl(*s,handles[1],false);
 for(unsigned i=0;i<100;++i)check(NativeBodyFactory::exit(*s,handles[i],e),"exact retained cleanup");
 check(s->state(after,e)&&after.clientCount==0&&after.retainedContexts==0,"all counts cleared");
 check(NativeBodyFactory::release(*s,e),"empty exact release");
 auto changed=NativeBodyFactory::make();check(NativeBodyFactory::bind(*changed,*stage,e),"new owner bind");
 p2retail::SceneRuntime::revision(*stage);check(!NativeBodyFactory::update(*changed,e),"selection revision fence");
 std::cout<<checks<<" source fixed10 scheduling controls passed; synthetic authority only, no gameplay\n";
}
