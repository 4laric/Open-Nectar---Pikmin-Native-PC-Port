#include "pc_p2_original_piki_physical_bootstrap.h"
#include "pc_p2_retail_scene.h"
#include "pc_p2_retail_starting_piki.h"
#include "Piki.h"
#include "GameStat.h"
#include "UpdateMgr.h"
#include <cmath>
#include <exception>
#include <limits>
namespace p2original {namespace piki {
namespace {
bool same(const OriginalPikiBody& a,const OriginalPikiBody& b){
 return a.origin.sourceKey==b.origin.sourceKey&&a.origin.recordUid==b.origin.recordUid
  &&a.origin.attempt==b.origin.attempt&&a.origin.activation==b.origin.activation
  &&a.origin.catalogFingerprint==b.origin.catalogFingerprint
  &&a.state.species==b.state.species&&a.state.wild==b.state.wild&&a.state.wasWild==b.state.wasWild;
}
bool fail(std::string& e,const char* s){e=s;return false;}
}
NativePhysicalBootstrap::~NativePhysicalBootstrap(){if(owned()||ticket.body)std::terminate();}
void NativePhysicalBootstrap::poolRetired()noexcept{stage=nullptr;ticket={};serial=revision=0;source={};}
bool NativePhysicalBootstrap::physicalResourcesEmpty()const noexcept{
 if(counter||complete)return false;
 for(const auto& c:contexts)if(c.owned())return false;
 return true;
}
bool NativePhysicalBootstrap::retainedStageCurrent(std::string& e)const{
 if(!stage||pc_p2_retail_scene_prepared()!=stage||!stage->ownsCurrentThread()
  ||stage->nativeSerial()!=serial||stage->selectionRevision()!=revision)
  return fail(e,"physical bootstrap lost exact Stage/thread/pool ownership");
 e.clear();return true;
}
bool NativePhysicalBootstrap::current(std::string& e)const{
 if(!retainedStageCurrent(e)||!poolCurrent(ticket))return false;
 OriginalPikiBody actual;
 if(!poolSource(ticket,actual,e)||!same(source,actual))return fail(e,"physical bootstrap source changed");
 for(const auto& c:contexts)if(!c.current(e))return false;
 e.clear();return true;
}
bool NativePhysicalBootstrap::initialize(const p2retail::SceneContext& ctx,PoolTicket t,std::string& e){
 if(owned()||ticket.body||pc_p2_retail_scene_prepared()!=&ctx||!ctx.ownsCurrentThread()
  ||ctx.phase()!=p2retail::ScenePhase::Prepared||!poolCurrent(t))
  return fail(e,"physical bootstrap requires exact fresh prepared allocation");
 OriginalPikiBody actual;if(!poolSource(t,actual,e))return false;
 p2retail::StartingPikiInputs inputs;if(!p2retail::selectedStartingPiki(ctx,inputs,e))return false;
 const PikiSourceRecord* row=nullptr;
 for(const auto& r:inputs.manifest.rows)if(r.sourceKey==actual.origin.sourceKey){if(row)return fail(e,"duplicate selected startup body");row=&r;}
 if(!row||inputs.manifest.campaign!=ctx.campaignSha256()||inputs.manifest.catalog!=actual.origin.catalogFingerprint
  ||row->spawn.uid!=actual.origin.recordUid||row->spawn.count!=1||actual.origin.attempt!=0
  ||row->spawn.species!=1||actual.state.species!=1||actual.state.wild||actual.state.wasWild)
  return fail(e,"physical bootstrap lacks exact selected engineered Red row");
 if(pc_p2_retail_scene_prepared()!=&ctx||!ctx.ownsCurrentThread()
  ||ctx.phase()!=p2retail::ScenePhase::Prepared||!poolCurrent(t))
  return fail(e,"selected startup read changed physical allocation authority");
 if(t.body->mGenerator||t.body->getCnt()!=0||t.body->mStickListHead
  ||t.body->mStickTarget||t.body->mNextRopeHolder||t.body->mPrevRopeHolder)
  return fail(e,"startup pool root retains foreign native consumers");
 for(float v:row->spawn.position)if(!std::isfinite(v))return fail(e,"nonfinite selected startup position");
 if(GameStat::workPikis[Red]<0||GameStat::workPikis[Red]==std::numeric_limits<int>::max())
  return fail(e,"native source work counter cannot register");
 // Fallible source storage precedes handoff and all physical writes.
 source=actual;
 if(!poolBeginPhysicalInitialization(t,actual,e))return false;
 stage=&ctx;ticket=t;serial=ctx.nativeSerial();revision=ctx.selectionRevision();
 try{
  auto& p=*t.body;
  if(!contexts[0].acquire(p.mPikiUpdateContext,pikiUpdateMgr,false,e)
   ||!contexts[1].acquire(p.mPikiLookUpdateContext,pikiLookUpdateMgr,false,e)
   ||!contexts[2].acquire(p.mOptUpdateContext,pikiOptUpdateMgr,false,e)
   ||!contexts[3].acquire(p.mSearchContext,searchUpdateMgr,true,e))return false;
  if(!current(e))return false;
  counter=true;GameStat::workPikis.inc(Red);GameStat::update();
  // Only physical host fields. Source CF flags/Brain/animator are separate
  // genuine constructors; dormant native P1 state is never executed here.
  p.mSRT.t.set(row->spawn.position[0],row->spawn.position[1],row->spawn.position[2]);
  p.mSRT.s.set(1.0f,1.0f,1.0f);p.mSRT.r.set(0.0f,0.0f,0.0f);
  p.mVelocity.set(0.0f,0.0f,0.0f);p.mTargetVelocity.set(0.0f,0.0f,0.0f);
  p.mFaceDirection=0.0f;p.mColor=Red;p.mHappa=Leaf;
  if(!current(e))return false;
  complete=true;e.clear();return true;
 }catch(...){return fail(e,"physical bootstrap threw; partial owner retained");}
}
bool NativePhysicalBootstrap::dispose(std::string& e){
 if(!owned()){e.clear();return true;}
 if(!current(e))return false;
 OriginalPikiBodyHandle h;
 if(pc_p2_original_piki_body_handle(ticket.body,h))return fail(e,"physical cleanup still has canonical body consumers");
 for(const auto& c:contexts)if(!c.canRelease(e))return false;
 if(counter&&GameStat::workPikis[Red]<=0)return fail(e,"native source work counter lost its contribution");
 // Checked reverse prefix disposal is retryable: released leases stay Empty.
 for(std::size_t i=contexts.size();i>0;--i)if(!contexts[i-1].release(e))return false;
 if(counter){GameStat::workPikis.dec(Red);counter=false;GameStat::update();}
 // Physical resources are gone; the root allocation is STILL retained until
 // the Pool owner independently observes raw -1 (not deferred -2).
 complete=false;e.clear();return true;
}
}}

