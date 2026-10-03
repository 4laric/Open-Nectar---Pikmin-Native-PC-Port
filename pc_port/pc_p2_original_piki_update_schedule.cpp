#include "pc_p2_original_piki_update_schedule.h"
#include "pc_p2_original_piki_host.h"
#include "pc_p2_retail_scene.h"
namespace p2original {namespace piki {
namespace {
bool fail(std::string& e,const char* text){e=text;return false;}
bool same(Handle a,Handle b){return a.body==b.body&&a.lifetime==b.lifetime;}
}
class PikiUpdateSchedule::Operation {
 const PikiUpdateSchedule& owner;bool entered=false;
public:
 explicit Operation(const PikiUpdateSchedule& value):owner(value){
  if(owner.mBusy){owner.mReentered=true;return;}
  owner.mBusy=true;owner.mReentered=false;entered=true;
 }
 ~Operation(){if(entered)owner.mBusy=false;}
 bool ok()const{return entered&&!owner.mReentered;}
};
bool PikiUpdateSchedule::exact(bool cleanup,std::string& e)const{
 // Compare borrowed identity before dereferencing the retained Stage.
 if(!mStage||pc_p2_retail_scene_prepared()!=mStage||!mStage->ownsCurrentThread()
 ||!mStageInfo||mStage->stage()!=mStageInfo||!mMap||mStage->map()!=mMap||mStage->nativeSerial()!=mSerial
 ||mStage->selectionRevision()!=mRevision||mStage->campaignSha256()!=mBinding.campaign
 ||mStage->sessionSha256()!=mBinding.fingerprint)
  return fail(e,"source Piki update scheduler lost actual Stage/thread/scene identity");
 const auto phase=mStage->phase();
 if(!nativeSceneCurrent(mBinding,cleanup,e)||pc_p2_retail_scene_prepared()!=mStage||!mStage->ownsCurrentThread()
 ||mStage->stage()!=mStageInfo||mStage->map()!=mMap||mStage->nativeSerial()!=mSerial
 ||mStage->selectionRevision()!=mRevision||mStage->campaignSha256()!=mBinding.campaign
 ||mStage->sessionSha256()!=mBinding.fingerprint||mStage->phase()!=phase||mReentered)
  return fail(e,"source Piki update scheduler lost actual Stage/thread/scene identity");
 return true;
}
bool PikiUpdateSchedule::body(Handle h,bool cleanup,std::string& e)const{
 if(!exact(cleanup,e)||!h.body||!h.lifetime)return false;
 const auto phase=mStage->phase();NativeHostRead host;
 if(nativeHost(h.body,host,e)!=NativeHostPhase::Committed||!same(host.handle,h)
 ||host.stage!=mStage||host.scene!=mBinding.scene||host.map!=mStage->map()
 ||!pc_p2_original_piki_body_current(h.body,h.lifetime)||!exact(cleanup,e)
 ||mStage->phase()!=phase)return fail(e,"source Piki update context lacks exact committed factory lifetime");
 OriginalPikiBodyHandle committed;
 if(!pc_p2_original_piki_body_handle(h.body,committed)||committed.nativeLifetime!=h.lifetime
 ||committed.body.origin.catalogFingerprint!=pc_p2_original_piki_catalog_fingerprint()
 ||!exact(cleanup,e))return fail(e,"source Piki update context association changed");
 return true;
}
bool PikiUpdateSchedule::validate(bool cleanup,std::string& e)const{
 if(!exact(cleanup,e))return false;
 const auto phase=mStage->phase();
 for(const auto& c:mContexts)if(c.handle.body&&!body(c.handle,cleanup,e))return false;
 return exact(cleanup,e)&&mStage->phase()==phase;
}
bool PikiUpdateSchedule::bind(const p2retail::SceneContext& stage,std::string& e){
 Operation op(*this);if(!op.ok())return fail(e,"reentrant source Piki scheduler bind");
 if(mStage)return mStage==&stage&&validate(false,e);
 if(pc_p2_retail_scene_prepared()!=&stage||!stage.ownsCurrentThread())return false;
 const auto serial=stage.nativeSerial(),revision=stage.selectionRevision();
 const auto phase=stage.phase();auto* info=stage.stage();auto* map=stage.map();
 SceneBinding next;
 if(pc_p2_retail_scene_prepared()!=&stage||!stage.ownsCurrentThread()||!stage.stage()||!stage.map()
 ||!stage.nativeSerial()||!stage.selectionRevision()||!nativeSceneBinding(next,false,e)
 ||next.campaign!=stage.campaignSha256()||next.fingerprint!=stage.sessionSha256()
 ||!nativeSceneCurrent(next,false,e)||pc_p2_retail_scene_prepared()!=&stage||!stage.ownsCurrentThread()
 ||stage.nativeSerial()!=serial||stage.selectionRevision()!=revision||stage.phase()!=phase
 ||stage.stage()!=info||stage.map()!=map||!op.ok())
  return fail(e,"source Piki scheduler requires actual retained selected Stage");
 mBinding=std::move(next);mSerial=serial;mRevision=revision;mStageInfo=info;mMap=map;mStage=&stage;
 return true;
}
bool PikiUpdateSchedule::initContext(Handle h,std::string& e){
 Operation op(*this);if(!op.ok()||!validate(false,e)||!body(h,false,e))return false;
 auto contexts=mContexts;auto state=mState;Context* target=nullptr;
 for(auto& c:contexts)if(c.handle.body==h.body){
  if(!same(c.handle,h))return fail(e,"source Piki scheduler retains earlier body lifetime");
  target=&c;break;
 }
 if(!target){for(auto& c:contexts)if(!c.handle.body){target=&c;break;}
  if(!target)return fail(e,"source Piki scheduler capacity100 exceeded");
  target->handle=h;++state.retainedContexts;
 }
 // Source init sets manager; force-active skips addClient. No retail Piki FSM
 // writes this flag: Flying force belongs to the independent Creature context.
 if(!target->forced){
  if(target->clientIndex!=-1){--state.clients[target->clientIndex];
   if(target->active)--state.activeClients[target->clientIndex];--state.clientCount;}
  unsigned smallest=0;for(unsigned i=1;i<10;++i)if(state.clients[i]<state.clients[smallest])smallest=i;
  target->clientIndex=static_cast<int>(smallest);++state.clients[smallest];
  if(target->active)++state.activeClients[smallest];++state.clientCount;
 }
 if(!validate(false,e)||!body(h,false,e)||!op.ok())return false;
 mContexts=contexts;mState=state;return true;
}
bool PikiUpdateSchedule::exitContext(Handle h,std::string& e){
 Operation op(*this);if(!op.ok()||!validate(true,e)||!body(h,true,e))return false;
 auto contexts=mContexts;auto state=mState;Context* target=nullptr;
 for(auto& c:contexts)if(same(c.handle,h)){target=&c;break;}
 if(!target)return fail(e,"source Piki scheduler exit lacks exact retained lifetime");
 // Literal UpdateContext::exit is a no-op when forced active.
 if(!target->forced){if(target->clientIndex!=-1){--state.clients[target->clientIndex];
  if(target->active)--state.activeClients[target->clientIndex];--state.clientCount;}
  *target=Context{};--state.retainedContexts;
 }
 if(!validate(true,e)||!op.ok())return false;
 mContexts=contexts;mState=state;return true;
}
bool PikiUpdateSchedule::updateFromPikiMgr(std::string& e){
 Operation op(*this);if(!op.ok()||!validate(false,e)||!op.ok())return false;
 mState.currentIndex=(mState.currentIndex+1)%10;return true;
}
bool PikiUpdateSchedule::updatable(Handle h,bool& out,std::string& e)const{
 Operation op(*this);if(!op.ok()||!validate(false,e)||!body(h,false,e))return false;
 const Context* found=nullptr;for(const auto& c:mContexts)if(same(c.handle,h)){found=&c;break;}
 if(!found)return fail(e,"source Piki scheduler query lacks registered lifetime");
 const bool next=found->forced||found->clientIndex==static_cast<int>(mState.currentIndex);
 if(!validate(false,e)||!op.ok())return false;out=next;return true;
}
bool PikiUpdateSchedule::state(PikiUpdateScheduleState& out,std::string& e)const{
 Operation op(*this);if(!op.ok()||!validate(true,e)||!op.ok())return false;out=mState;return true;
}
bool PikiUpdateSchedule::release(std::string& e){
 Operation op(*this);if(!op.ok()||!validate(true,e)||mState.retainedContexts||!op.ok())
  return fail(e,"source Piki scheduler release requires exact empty owner");
 mStage=nullptr;mStageInfo=nullptr;mMap=nullptr;mBinding={};mSerial=0;mRevision=0;mState={};return true;
}
} }
