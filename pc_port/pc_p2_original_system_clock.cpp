#include "pc_p2_original_system_clock.h"
#include "pc_p2_retail_scene.h"
#include "pc_p2_original_captain_damage.h"
#include "pc_randomizer.h"
#include <exception>
namespace p2original {
namespace {
bool fail(std::string& e,const char* why){e=why;return false;}
bool phase(p2retail::ScenePhase p,bool cleanup){
 return p==p2retail::ScenePhase::Prepared||p==p2retail::ScenePhase::Installing
  ||p==p2retail::ScenePhase::Committed||(cleanup&&p==p2retail::ScenePhase::Releasing);
}
}
class SystemClock::Operation {
 const SystemClock& o;bool entered=false;
public:
 explicit Operation(const SystemClock& clock):o(clock){
  if(o.mBusy)o.mReentered=true;else{o.mBusy=true;o.mReentered=false;entered=true;}
 }
 ~Operation(){if(entered)o.mBusy=false;}
 bool ok()const{return entered&&!o.mReentered;}
};
bool SystemClock::stageCurrent(bool cleanup,std::string& e)const{
 if(!mStage||mReentered||std::this_thread::get_id()!=mThread||pc_p2_retail_scene_prepared()!=mStage)
  return fail(e,"Source SystemClock actual Stage/thread expired or reentered");
 if(!mStage->ownsCurrentThread()||!phase(mStage->phase(),cleanup)||mStage->stage()!=mStageInfo
  ||mStage->map()!=mMap||mStage->routes()!=mRoutes||mStage->nativeSerial()!=mSerial
  ||mStage->selectionRevision()!=mRevision||mStage->campaignSha256()!=mCampaign
  ||mStage->sessionSha256()!=mSession||mStage->plan().layoutSha256!=mLayout)
  return fail(e,"Source SystemClock retained Stage/selection differs");
 const auto& s=mStage->snapshot().scene;
 if(s.seed!=mSession||s.visit!=mVisit||s.layoutSha256!=mLayout||s.serial!=mSerial)
  return fail(e,"Source SystemClock full native scene identity differs");
 return true;
}
bool SystemClock::exact(bool cleanup,std::string& e)const{
 try{
 if(!stageCurrent(cleanup,e))return false;
 const auto before=mStage->phase();
 const bool original=pc_randomizer_original_session();
 const auto campaign=pc_randomizer_original_campaign();
 const auto session=pc_randomizer_session_fingerprint();
 const auto revision=pc_randomizer_original_selection_revision();
 if(!original||campaign!=mCampaign||session!=mSession||revision!=mRevision
  ||!stageCurrent(cleanup,e)||mStage->phase()!=before)
  return fail(e,"Source SystemClock selected SDK/Stage changed");
 if(mScene){
  if(!sceneCurrent(e)||!stageCurrent(cleanup,e)||mStage->phase()!=before)
   return fail(e,"Source SystemClock retained canonical descriptor changed");
 }
 return true;
 }catch(const std::exception& x){e=std::string("Source SystemClock observation exception: ")+x.what();return false;}
}
bool SystemClock::sceneCurrent(std::string& e)const{
 if(!mScene||pc_p2_original_captain_loaded_scene()!=mScene)
  return fail(e,"Source SystemClock canonical descriptor expired");
 if(mScene->incarnation()!=mSerial||mScene->selectedCampaign()!=mCampaign
  ||mScene->selectedFingerprint()!=mSession||mScene->sourceCatalog()!=mLayout
  ||mScene->captainAt(0)!=mCaptains[0]||mScene->captainAt(1)!=mCaptains[1]
  ||pc_p2_original_captain_loaded_scene()!=mScene||mReentered)
  return fail(e,"Source SystemClock canonical descriptor/roster differs");
 return true;
}
bool SystemClock::sourceStageOwned(const p2retail::SceneContext& s,std::string& e){
 if(std::this_thread::get_id()!=mThread)return fail(e,"Source SystemClock event requires constructor thread");
 Operation op(*this);
 if(!op.ok()||mStage)return fail(e,"Source SystemClock requires fresh Stage binding");
 try{
  if(pc_p2_retail_scene_prepared()!=&s)return fail(e,"Source SystemClock requires actual prepared Stage");
  if(!s.ownsCurrentThread()||s.phase()!=p2retail::ScenePhase::Prepared||!s.stage()||!s.map()
   ||!s.nativeSerial()||!s.selectionRevision())return fail(e,"Source SystemClock Stage initialization unavailable");
  const auto campaign=s.campaignSha256(),session=s.sessionSha256(),layout=s.plan().layoutSha256;
  const auto identity=s.snapshot().scene;const auto serial=s.nativeSerial(),revision=s.selectionRevision();
  auto* stage=s.stage();auto* map=s.map();auto* routes=s.routes();
  if(!p2retail::hex64(campaign)||!p2retail::hex64(session)||!p2retail::hex64(layout)
   ||identity.seed!=session||identity.visit.empty()||identity.layoutSha256!=layout||identity.serial!=serial)
   return fail(e,"Source SystemClock full Stage identity invalid");
  const bool original=pc_randomizer_original_session();
  const auto selectedCampaign=pc_randomizer_original_campaign(),selectedSession=pc_randomizer_session_fingerprint();
  const auto selectedRevision=pc_randomizer_original_selection_revision();
  if(!original||selectedCampaign!=campaign||selectedSession!=session||selectedRevision!=revision
   ||pc_p2_retail_scene_prepared()!=&s||!op.ok())return fail(e,"Source SystemClock selected SDK changed during bind");
  if(!s.ownsCurrentThread()||s.phase()!=p2retail::ScenePhase::Prepared||s.stage()!=stage||s.map()!=map
   ||s.routes()!=routes||s.nativeSerial()!=serial||s.selectionRevision()!=revision
   ||s.campaignSha256()!=campaign||s.sessionSha256()!=session||s.plan().layoutSha256!=layout
   ||!(s.snapshot().scene==identity))return fail(e,"Source SystemClock Stage changed during bind");
  // All allocating copies precede the pointer commit. Construction already
  // supplied literal factor1/dt1/60; Stage rebinding never resets that history.
  auto nextCampaign=campaign,nextSession=session,nextVisit=identity.visit,nextLayout=layout;
  mCampaign=std::move(nextCampaign);mSession=std::move(nextSession);
  mVisit=std::move(nextVisit);mLayout=std::move(nextLayout);
  mSerial=serial;mRevision=revision;mStageInfo=stage;mMap=map;mRoutes=routes;
  mStage=&s;e.clear();return true;
 }catch(const std::exception& x){e=std::string("Source SystemClock binding exception: ")+x.what();return false;}
}
bool SystemClock::sourceBaseGameSectionInit(std::string& e){
 if(std::this_thread::get_id()!=mThread)return fail(e,"Source SystemClock event requires constructor thread");
 Operation op(*this);
 if(!op.ok()||!exact(false,e)||mSectionInitialized||mFrameOpen||!op.ok())
  return fail(e,"Source SystemClock requires matching first BaseGameSection init event");
 // System::setFrameRate(2), literal BaseGameSection::init call before initJ3D
 // and onInit. No Captain roster, MoviePlayer permission or host dt is queried.
 mFrameRate=2.0f;mDeltaTime=mFrameRate/60.0f;mSectionInitialized=true;e.clear();return true;
}
bool SystemClock::sourceLoadedScene(std::string& e){
 if(std::this_thread::get_id()!=mThread)return fail(e,"Source SystemClock event requires constructor thread");
 Operation op(*this);
 try{
 if(!op.ok()||!exact(false,e)||!mSectionInitialized||mScene||mFrameOpen)
  return fail(e,"Source SystemClock loaded descriptor event out of order");
 const auto before=mStage->phase();const auto* scene=pc_p2_original_captain_loaded_scene();
 if(!scene)return fail(e,"Source SystemClock actual loaded descriptor absent");
 auto* a=scene->captainAt(0);auto* b=scene->captainAt(1);
 if(!a||!b||a==b||scene->incarnation()!=mSerial||scene->selectedCampaign()!=mCampaign
  ||scene->selectedFingerprint()!=mSession||scene->sourceCatalog()!=mLayout
  ||pc_p2_original_captain_loaded_scene()!=scene||!exact(false,e)||mStage->phase()!=before||!op.ok())
  return fail(e,"Source SystemClock actual loaded descriptor differs");
 mScene=scene;mCaptains[0]=a;mCaptains[1]=b;e.clear();return true;
 }catch(const std::exception& x){e=std::string("Source SystemClock loaded scene exception: ")+x.what();return false;}
}
bool SystemClock::readDeltaTime(float& out,std::string& e)const{
 // Reject other threads before touching mutable operation bookkeeping.
 if(std::this_thread::get_id()!=mThread)return fail(e,"Source SystemClock read requires constructor thread");
 Operation op(*this);
 if(!op.ok()||!exact(false,e)||!mSectionInitialized||!op.ok())
  return fail(e,"Source SystemClock dt unavailable before actual section init");
 out=mDeltaTime;e.clear();return true;
}
bool SystemClock::sourceBaseGameSectionUpdateBegin(std::string& e){
 if(std::this_thread::get_id()!=mThread)return fail(e,"Source SystemClock event requires constructor thread");
 Operation op(*this);
 if(!op.ok()||!exact(false,e)||!mSectionInitialized||mFrameOpen||!op.ok())
  return fail(e,"Source SystemClock unmatched BaseGameSection update begin");
 mFrameOpen=true;e.clear();return true;
}
bool SystemClock::sourceBaseGameSectionUpdateEnd(std::string& e){
 if(std::this_thread::get_id()!=mThread)return fail(e,"Source SystemClock event requires constructor thread");
 Operation op(*this);
 if(!op.ok()||!exact(false,e)||!mSectionInitialized||!mFrameOpen||!op.ok())
  return fail(e,"Source SystemClock unmatched BaseGameSection update end");
 mFrameOpen=false;e.clear();return true;
}
bool SystemClock::sourceStageReleased(std::string& e){
 if(std::this_thread::get_id()!=mThread)return fail(e,"Source SystemClock event requires constructor thread");
 Operation op(*this);
 if(!op.ok()||!exact(true,e)||mFrameOpen||!op.ok())
  return fail(e,"Source SystemClock release retains in-flight update or stale Stage");
 mStage=nullptr;mStageInfo=nullptr;mMap=nullptr;mRoutes=nullptr;mSerial=0;mRevision=0;
 mCampaign.clear();mSession.clear();mVisit.clear();mLayout.clear();mScene=nullptr;mCaptains[0]=mCaptains[1]=nullptr;
 mSectionInitialized=false;
 // Actual System survives section retirement. Do not invent setFrameRate(1).
 e.clear();return true;
}
}
