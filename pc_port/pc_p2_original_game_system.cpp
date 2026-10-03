#include "pc_p2_original_game_system.h"
#include "pc_p2_retail_scene.h"
namespace p2original {
namespace {bool fail(std::string& e,const char* t){e=t;return false;}}
class GameSystem::Operation {
 const GameSystem& o;bool entered=false;
public:
 explicit Operation(const GameSystem& owner):o(owner){if(o.mBusy){o.mReentered=true;return;}o.mBusy=true;o.mReentered=false;entered=true;}
 ~Operation(){if(entered)o.mBusy=false;}
 bool ok()const{return entered&&!o.mReentered;}
};
bool GameSystem::exact(std::string& e)const{
 if(!mStage||pc_p2_retail_scene_prepared()!=mStage||!mStage->ownsCurrentThread()
 ||mStage->stage()!=mStageInfo||mStage->map()!=mMap||mStage->nativeSerial()!=mSerial
 ||mStage->selectionRevision()!=mRevision||mStage->campaignSha256()!=mCampaign
 ||mStage->sessionSha256()!=mSession||mReentered)return fail(e,"source GameSystem lost actual Stage/thread/selection");
 if(mBinding.scene){const auto phase=mStage->phase();
  if(!piki::nativeSceneCurrent(mBinding,true,e)||pc_p2_retail_scene_prepared()!=mStage
  ||!mStage->ownsCurrentThread()||mStage->stage()!=mStageInfo||mStage->map()!=mMap
  ||mStage->nativeSerial()!=mSerial||mStage->selectionRevision()!=mRevision
  ||mStage->phase()!=phase||mStage->campaignSha256()!=mCampaign||mStage->sessionSha256()!=mSession||mReentered)
   return fail(e,"source GameSystem scene changed during observation");
 }
 return true;
}
bool GameSystem::sourceInit(const p2retail::SceneContext& stage,std::string& e){
 Operation op(*this);if(!op.ok()||mStage)return fail(e,"source GameSystem init requires fresh retained owner");
 if(pc_p2_retail_scene_prepared()!=&stage||!stage.ownsCurrentThread()||!stage.stage()||!stage.map()
 ||stage.phase()!=p2retail::ScenePhase::Prepared||!stage.nativeSerial()||!stage.selectionRevision()||stage.campaignSha256().empty()||stage.sessionSha256().empty())
  return fail(e,"source GameSystem init requires actual selected Stage ownership");
 // Port only scalar init. TimeMgr/resource initialization is intentionally
 // unavailable: selected time.ini cannot be replaced with retail defaults.
 const auto campaign=stage.campaignSha256(),session=stage.sessionSha256();
 mCampaign=campaign;mSession=session;mSerial=stage.nativeSerial();mRevision=stage.selectionRevision();
 mStageInfo=stage.stage();mMap=stage.map();mState={};mStage=&stage;return true;
}
bool GameSystem::readState(GameSystemState& out,std::string& e)const{
 Operation op(*this);if(!op.ok()||!exact(e)||!op.ok())return false;out=mState;return true;
}
bool GameSystem::inCave(bool& out,std::string& e)const{
 Operation op(*this);if(!op.ok()||!exact(e)||!mSectionKnown||!op.ok())return fail(e,"source section has not initialized mIsInCave");out=mInCave;return true;
}
bool GameSystem::readTime(std::string& e)const{Operation op(*this);return fail(e,"actual selected TimeMgr/time.ini producer unavailable");}
bool GameSystem::readMovie(std::string& e)const{Operation op(*this);return fail(e,"actual source MoviePlayer state producer unavailable");}
bool GameSystem::readMovieDraw(std::string& e)const{Operation op(*this);return fail(e,"actual selected source MovieConfig draw producer unavailable");}
bool GameSystem::sourceStartFrame(std::string& e){
 Operation op(*this);if(!op.ok()||!exact(e)||!op.ok())return false;
 auto next=mState;if(next.pauseCountdown)--next.pauseCountdown;
 if(++next.frameTimer>0x40000000u)next.frameTimer=0;
 // Actual source event owner additionally orders cellMgr initFrame,
 // collisionUpdateMgr update, then TimeMgr update under retail guards.
 // This scalar leaf cannot invoke absent source cell/time/collision producers.
 mState=next;return true;
}
bool GameSystem::sourceEndFrame(std::string& e){
 Operation op(*this);if(!op.ok()||!exact(e)||!op.ok())return false;
 if(mState.pauseCountdown)--mState.pauseCountdown;return true;
}
bool GameSystem::sectionInit(bool cave,std::string& e){
 Operation op(*this);if(!op.ok()||!exact(e)||mContainer)return false;
 piki::SceneBinding next;const auto phase=mStage->phase();
 if(pc_p2_retail_scene_committed()!=mStage||phase!=p2retail::ScenePhase::Committed||!piki::nativeSceneBinding(next,false,e)
 ||next.campaign!=mCampaign||next.fingerprint!=mSession||!piki::nativeSceneCurrent(next,false,e)
 ||!exact(e)||mStage->phase()!=phase||!op.ok())return fail(e,"actual source section init requires committed owned floor/scene");
 if(mBinding.scene&&(next.scene!=mBinding.scene||next.incarnation!=mBinding.incarnation))
  return fail(e,"source GameSystem retains earlier canonical scene");
 mBinding=std::move(next);mSectionKnown=true;mInCave=cave;mState.flags|=0x20;mState.flags&=~0x02;
 return true;
}
bool GameSystem::sourceGameStateInit(std::string& e){return sectionInit(false,e);}
bool GameSystem::sourceCaveStateInit(std::string& e){return sectionInit(true,e);}
bool GameSystem::sourceGameStart(std::string& e){
 Operation op(*this);if(!op.ok()||!exact(e)||!mSectionKnown||!(mState.flags&0x20)||!op.ok())return false;
 mState.flags|=0x02;return true;
}
bool GameSystem::sourceSectionLeft(std::string& e){
 Operation op(*this);if(!op.ok()||!exact(e)||!mSectionKnown||!op.ok())return false;mState.flags&=~0x20;return true;
}
bool GameSystem::pause(bool soft,std::string& e){
 Operation op(*this);if(!op.ok()||!exact(e)||!op.ok())return false;
 // Literal startPause early return: it does not refresh an existing pause.
 if(soft&&mState.paused())return true;mState.pauseCountdown=3;mState.softPause=soft;return true;
}
bool GameSystem::sourceWaitSyncLoadPause(std::string& e){return pause(true,e);}
bool GameSystem::sourceWaitSyncLoadComplete(std::string& e){return pause(false,e);}
bool GameSystem::sourceContainerScreenOpened(Navi* n,std::string& e){
 Operation op(*this);if(!op.ok()||!exact(e)||!mBinding.scene||mContainer
 ||!piki::nativeCaptainFacts(mBinding,n,false,e)||!exact(e)||!op.ok())return false;
 mContainer=n;mState.frozen=true;mState.moviePause=true;return true;
}
bool GameSystem::sourceContainerCleanup(Navi* n,std::string& e){
 Operation op(*this);if(!op.ok()||!exact(e)||!mBinding.scene||(mContainer&&mContainer!=n)
 ||!piki::nativeCaptainFacts(mBinding,n,true,e)||!exact(e)||!op.ok())return false;
 mContainer=nullptr;mState.frozen=false;mState.moviePause=false;return true;
}
bool GameSystem::sourceRelease(std::string& e){
 Operation op(*this);if(!op.ok()||!exact(e)||mContainer||(mState.flags&0x20)||!op.ok())
  return fail(e,"source GameSystem release requires left section and retired container");
 mStage=nullptr;mStageInfo=nullptr;mMap=nullptr;mSerial=0;mRevision=0;mCampaign.clear();mSession.clear();
 mBinding={};mState={};mSectionKnown=false;mInCave=false;return true;
}
}
