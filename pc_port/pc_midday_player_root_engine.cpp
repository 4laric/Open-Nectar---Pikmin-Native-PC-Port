#include "pc_midday_player_root.h"
#include "PlayerState.h"
#include <algorithm>
#include <exception>
// Constructor allocates nothing. Inline Demo/Result use their inert constructors;
// ordinary PlayerState's global/content/card/setup paths are never invoked.
PlayerState::PlayerState(const pc_midday::PlayerRootStageTag&)
 :mDemoFlags(pc_midday::DemoStageTag{}),mResultFlags(pc_midday::ResultStageTag{}){
 mSproutedNum=mLostBattlePikis=mLeftBehindPikis=mTotalPluckedPikiCount=0;
 mShipUpgradeLevel=mShipEffectPartFlag=mContainerFlag=mDisplayPikiFlag=0;
 mCurrentRepairingPart=nullptr;for(auto&v:mPartsCollectedByDay)v=0;for(auto&v:mPartsToNextByDay)v=0;
 mHasExtinctionDemoPlayed=false;mOlimarShapeObj=nullptr;mTotalRegisteredParts=0;mTotalParts=30;mUfoParts=nullptr;
 mCurrParts=mRequiredUfoPartCount=mTotalDeadPikiNum=mTotalBornPikiNum=mLivingPikiNum=0;
 mIsTutorialMode=_186=mIsNaviPilot=mInDayEnd=mIsChallengeMode=false;mLastUpdatedTime=0;
 for(auto&v:mStagePartsCollected)v=0;mCourseFlags=nullptr;mNaviLightEfx=mNaviLightGlowEfx=nullptr;
 mOlimarAnimMgr.mAnimSpeed=0;
 for(PaniAnimator*a:{&mOlimarAnimMgr.mLowerAnimator,&mOlimarAnimMgr.mUpperAnimator}){
  a->mMgr=nullptr;a->mContext=nullptr;a->mAnimInfo=nullptr;a->mMotionTable=nullptr;a->mListener=nullptr;
  a->mPostOneShotPlayState=a->mPostOneShotAnimID=a->mPostOneShotStartKeyIndex=a->mPostOneShotEndKeyIndex=0;
  a->mPlayState=ANIMSTATE_Inactive;a->mCurrentAnimID=a->mStartKeyIndex=a->mEndKeyIndex=0;
  a->mAnimationCounter=0;a->mCurrentKeyIndex=a->mMotionIdx=-1;a->mPreviousKeyIndex=0;a->mIsFinished=false;
 }
}
struct PcMiddayPlayerRootAccess {
 static void result(ResultFlags& to,const ResultFlags& from){
  to.mLength=from.mLength;to.mActiveCount=from.mActiveCount;to.mTableSize=from.mTableSize;
  to.mStates=from.mStates;to.mScreenToTableList=from.mScreenToTableList;
  std::copy(std::begin(from.mDaysSeen),std::end(from.mDaysSeen),std::begin(to.mDaysSeen));
 }
};
namespace pc_midday {
struct IsolatedPlayerRoot::Impl {
 ConstructorFence* fence=nullptr;
 std::vector<TimeGraph::PikiNum>hour,day;
 std::array<std::vector<uint8_t>,5>courseBytes;
 std::array<BitFlags,5>courseFlags;
 std::array<BitFlags*,5>coursePointers{};
 IsolatedDemo demo;IsolatedResult result;
 // Last member is disposed first, before every backing/resource owner.
 std::unique_ptr<PlayerState>root;
 ~Impl(){if(root){std::string e;if(!fence||!fence->held()||!pc_sim_rng_constructor_suppression(true,e))std::terminate();}}
};
IsolatedPlayerRoot::IsolatedPlayerRoot()=default;IsolatedPlayerRoot::~IsolatedPlayerRoot()=default;
PlayerState*IsolatedPlayerRoot::root()const{return impl_?impl_->root.get():nullptr;}
bool IsolatedPlayerRoot::heldBy(const ConstructorFence& f)const{return impl_&&impl_->fence==&f&&f.held();}
namespace {
void graph(TimeGraph& to,const PlayerGraph& from,std::vector<TimeGraph::PikiNum>& backing){
 backing.resize(from.entries.size());for(size_t i=0;i<backing.size();++i)for(size_t c=0;c<3;++c)backing[i].mNum[c]=from.entries[i][c];
 to.mStartTime=from.start;to.mEndTime=from.end;to.mEntries=backing.data();
}
void core(PlayerState&p,const PlayerCoreFields&v){
 p.mSproutedNum=v.sprouted;p.mLostBattlePikis=v.lostBattle;p.mLeftBehindPikis=v.leftBehind;p.mTotalPluckedPikiCount=v.totalPlucked;
 p.mTotalRegisteredParts=v.totalRegisteredParts;p.mTotalParts=v.totalParts;p.mCurrParts=v.currentParts;p.mRequiredUfoPartCount=v.requiredParts;
 p.mTotalDeadPikiNum=v.totalDead;p.mTotalBornPikiNum=v.totalBorn;p.mLivingPikiNum=v.living;
 p.mShipUpgradeLevel=v.shipUpgrade;p.mShipEffectPartFlag=v.shipEffect;p.mContainerFlag=v.container;p.mDisplayPikiFlag=v.displayPiki;
 p.mHasExtinctionDemoPlayed=v.extinctionPlayed;p.mIsTutorialMode=v.tutorial;p._186=v.unused186!=0;p.mIsNaviPilot=v.naviPilot;p.mInDayEnd=v.dayEnd;p.mIsChallengeMode=v.challenge;p.mLastUpdatedTime=v.lastUpdated;
 std::copy(v.collectedByDay.begin(),v.collectedByDay.end(),p.mPartsCollectedByDay);std::copy(v.partsToNext.begin(),v.partsToNext.end(),p.mPartsToNextByDay);std::copy(v.stageParts.begin(),v.stageParts.end(),p.mStagePartsCollected);
}
void demo(DemoFlags&to,const DemoFlags&from){
 to.mFlagCount=from.mFlagCount;to.mCurrentDataIndex=from.mCurrentDataIndex;to.mFlagDataNum=from.mFlagDataNum;to.mStoredFlags=from.mStoredFlags;to.mFlagDataList=from.mFlagDataList;to.mTargetCreature=from.mTargetCreature;to.mWaitTimer=from.mWaitTimer;to.mCurrentDemoIndex=from.mCurrentDemoIndex;
}
}
bool IsolatedPlayerRoot::prepare(const PlayerState& source,const Bytes& c,const Bytes& d,const Bytes& r,const PlayerCoreTopology& topology,const std::map<uint64_t,Creature*>& actors,const RestoreGate& gate,ConstructorFence& fence,std::string& e){
 if(impl_||!fence.held()||!pc_sim_rng_constructor_suppression(true,e)){if(e.empty())e="PlayerState root requires new stage and physical constructor fence";return false;}
 PlayerCoreFields fields;if(!preparePlayerCore(fields,c,topology,gate,e)||!validatePlayerRootCore(fields,topology,e))return false;
 DemoFields demoFields;ResultFields resultFields;if(!decodeDemo(d,demoFields,e)||!decodeResult(r,resultFields,e))return false;
 try {
  auto staged=std::make_unique<Impl>();staged->fence=&fence;
  if(!staged->demo.prepare(source.mDemoFlags,d,actors,gate,fence,e)||!staged->result.prepare(r,gate,fence,e))return false;
  staged->root=std::make_unique<PlayerState>(PlayerRootStageTag{});auto&p=*staged->root;core(p,fields);
  graph(p.mPerHourGraph,fields.hour,staged->hour);graph(p.mPerDayGraph,fields.day,staged->day);
  for(size_t i=0;i<5;++i){auto& flags=staged->courseFlags[i];flags.mEntryCount=0;flags.mSize=0;flags.mFlags=nullptr;
   const auto& saved=fields.courses[i];if(!saved.entries)continue;
   staged->courseBytes[i]=saved.bits;flags.mEntryCount=saved.entries;flags.mSize=uint16_t(saved.bits.size());flags.mFlags=staged->courseBytes[i].data();staged->coursePointers[i]=&flags;
  }
  p.mCourseFlags=staged->coursePointers.data();demo(p.mDemoFlags,*staged->demo.resource());PcMiddayPlayerRootAccess::result(p.mResultFlags,*staged->result.resource());
  // No UfoParts/Olimar/light reset masquerades as restoration. These currently
  // absent roots keep this private stage ineligible for whole-world publication.
  impl_=std::move(staged);e.clear();return true;
 }catch(const std::exception& x){e=std::string("PlayerState root staging allocation failed: ")+x.what();return false;}
}
}
