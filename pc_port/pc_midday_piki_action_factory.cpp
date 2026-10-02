#if defined(PIKI_PC_PORT)
#include "PikiAI.h"
#include "pc_midday_piki_action_factory.h"
#include "pc_midday_allocation_owner.h"
#include <new>

#include <stdexcept>
#include <set>
#include <typeinfo>
TopAction::ObjBore::ObjBore(MiddayRestoreTag):mBoredomLevels(nullptr),mObjectIds(nullptr),mIsMaxBored(nullptr),mCurrentCount(0),mMaxCount(5){}
TopAction::Boredom::Boredom(MiddayRestoreTag):mBoredomCollectors(nullptr),mBoredomIds(nullptr),mCurrentBoredomCount(0),mMaxBoredomCollectors(30),mNextAvailableIndex(0){}
Action::Action(MiddayRestoreTag,Piki* piki) : mChildActions{},mCurrActionIdx{},mChildCount{},mPiki{},mName{} {mPiki=piki;mCurrActionIdx=-1;mName="restore staged action";}
AndAction::AndAction(MiddayRestoreTag,Piki* piki) : Action(Action::MiddayRestoreTag{},piki),mOtherCreature{} {}
TopAction::TopAction(MiddayRestoreTag,Piki* piki) : Action(Action::MiddayRestoreTag{},piki),mListener{},mIsDebugDraw{},mIsSuspended{},mIsAnimating{},_1C{},mTarget{},_24{},_28{},_2C{},mBoredom(Boredom::MiddayRestoreTag{}) {}
ActAdjust::ActAdjust(MiddayRestoreTag,Piki* piki) : Action(Action::MiddayRestoreTag{},piki),mAdjustDistance{},mAdjustTimeLimit{},mTargetPosition{},mAdjustTimer{},mTurnSpeed{},mVelocity{},mForceFail{} {}
ActAttack::ActAttack(MiddayRestoreTag,Piki* piki) : AndAction(AndAction::MiddayRestoreTag{},piki),mHasLost{},mIsAttackFinished{},mIsCriticalHit{},mTargetIsPlayer{},mTargetObjectPool{},mOther{},mPlayerObject{} {}
ActBoMake::ActBoMake(MiddayRestoreTag,Piki* piki) : Action(Action::MiddayRestoreTag{},piki),mState{},mBuildObject{},_20{} {}
ActBoreListen::ActBoreListen(MiddayRestoreTag,Piki* piki) : Action(Action::MiddayRestoreTag{},piki) {}
ActBoreOneshot::ActBoreOneshot(MiddayRestoreTag,Piki* piki) : Action(Action::MiddayRestoreTag{},piki),mIsAnimFinished{} {}
ActBoreRest::ActBoreRest(MiddayRestoreTag,Piki* piki) : Action(Action::MiddayRestoreTag{},piki),mIsFinished{},mRestState{},mRestTimer{},mIsAnimFinished{},mForceComplete{} {}
ActBoreSelect::ActBoreSelect(MiddayRestoreTag,Piki* piki) : Action(Action::MiddayRestoreTag{},piki),mActionTimer{},mIsTimerActive{},mIsChildActionActive{},mStop{},_1C{} {}
ActBoreTalk::ActBoreTalk(MiddayRestoreTag,Piki* piki) : Action(Action::MiddayRestoreTag{},piki),mIsLookHandledElsewhere{},mTarget{},mTalkTimer{},mIsAnimFinished{} {}
ActBou::ActBou(MiddayRestoreTag,Piki* piki) : Action(Action::MiddayRestoreTag{},piki),mState{},mTimeoutCounter{},mClimbDirection{},mTargetStick{},mLastPosition{} {}
ActBreakWall::ActBreakWall(MiddayRestoreTag,Piki* piki) : Action(Action::MiddayRestoreTag{},piki),mWall{},mState{},mHitPikminPosition{},mStartAttackTime{},mWorkTimer{},mFailAttackCounter{},mIsAttackReady{} {}
ActBridge::ActBridge(MiddayRestoreTag,Piki* piki) : Action(Action::MiddayRestoreTag{},piki),mBridge{},mState{},mStartWorkTime{},mIsAttackReady{},mCollisionCount{},_2A{},mRandomBridgeWidth{},mStageID{},mClimbingBridge{},_33{},mBridgeWallNormal{},mClimbingVelocity{},mActionCounter{},mAnimationFinished{} {}
ActChase::ActChase(MiddayRestoreTag,Piki* piki) : Action(Action::MiddayRestoreTag{},piki),mTarget{},mChaseTimer{} {}
ActCrowd::ActCrowd(MiddayRestoreTag,Piki* piki) : Action(Action::MiddayRestoreTag{},piki),mOdometer{},mSelectAction{},mState{},mMode{},mPrevMode{},mNearSlotCounter{},mHasRequestedNewSlot{},_35{},_36{},_38{},_3C{},_48{},_54{},mCPlateSlotID{},mTripLoopCounter{},mTravelDistance{},mIsTripping{},mLostChildTimer{},mPlateMgr{},mWallNormal{},mBoredomMotion{},mIsWaiting{},mWasWaiting{},mHasRoute{},mPcLastPos{},mPcWindowTimer{},mPcWindowProgress{},mPcRouteTimer{},mPcRouteCooldown{},mPcRepathTimer{},mPcRouteGoal{},mPcOwnsRoute{} {}
ActDecoy::ActDecoy(MiddayRestoreTag,Piki* piki) : Action(Action::MiddayRestoreTag{},piki),mState{},mDecoyTimer{} {}
ActDeliver::ActDeliver(MiddayRestoreTag,Piki* piki) : AndAction(AndAction::MiddayRestoreTag{},piki),mObject{} {}
ActEnter::ActEnter(MiddayRestoreTag,Piki* piki) : Action(Action::MiddayRestoreTag{},piki),mState{},mOnyon{},mLeg{},mLastPosition{},mHasCollided{} {}
ActEscape::ActEscape(MiddayRestoreTag,Piki* piki) : Action(Action::MiddayRestoreTag{},piki),mTarget{},mEscapeTimer{},_1C{},mState{},mAvoidDirection{} {}
ActExit::ActExit(MiddayRestoreTag,Piki* piki) : Action(Action::MiddayRestoreTag{},piki),mPrevPosition{},mHasCollided{} {}
ActFlower::ActFlower(MiddayRestoreTag,Piki* piki) : Action(Action::MiddayRestoreTag{},piki),mElapsedTime{},mIsAnimationComplete{},mIsCarryEmpty{} {}
ActFormation::ActFormation(MiddayRestoreTag,Piki* piki) : Action(Action::MiddayRestoreTag{},piki),mInFormation{},mIdleTimer{},mFormMgr{},mDistanceToTarget{},mUseLastFormationPosition{},mIsIdling{},mHasStartedIdleAnim{},mIsOnFloorTripped{},mHasStartedRunAnim{},mIsTripping{} {}
ActFreeSelect::ActFreeSelect(MiddayRestoreTag,Piki* piki) : Action(Action::MiddayRestoreTag{},piki),mActionTimer{},mIsTimerActive{},mIsChildActionActive{},mIsFinished{},_1C{} {}
ActFree::ActFree(MiddayRestoreTag,Piki* piki) : Action(Action::MiddayRestoreTag{},piki),mSelectAction{},_1C{},_20{},_24{},mBoidTimer{},mFixedPositionTimer{},mTargetPosition{},mArrivalRadius{},mCollisionCooldownTimer{},_44{},mTouchedPlayer{},mIsBoidActive{} {}
ActGoto::ActGoto(MiddayRestoreTag,Piki* piki) : Action(Action::MiddayRestoreTag{},piki),mMaxDistance{},mMinDistance{},mTarget{},mTimeoutDuration{} {}
ActGuard::ActGuard(MiddayRestoreTag,Piki* piki) : Action(Action::MiddayRestoreTag{},piki),mTarget{},mLeftGuard{},mRightGuard{},mGoalPosition{},mFormationAngle{},mLandPosition{},mFormationSpacing{},mTimer{},mFormationSide{},mIsWaiting{},mIsGuardable{} {}
ActJumpAttack::ActJumpAttack(MiddayRestoreTag,Piki* piki) : Action(Action::MiddayRestoreTag{},piki),mState{},_1C{},mAttackState{},mTarget{},mTargetCollider{},_2C{},mIsCriticalHit{} {}
ActKinoko::ActKinoko(MiddayRestoreTag,Piki* piki) : Action(Action::MiddayRestoreTag{},piki),mTarget{},mState{},mStateTimer{},mTargetDirection{} {}
ActMine::ActMine(MiddayRestoreTag,Piki* piki) : Action(Action::MiddayRestoreTag{},piki),mBombGen{},mState{},_20{},mIsMineActionReady{} {}
ActPick::ActPick(MiddayRestoreTag,Piki* piki) : Action(Action::MiddayRestoreTag{},piki),mObject{},mIsAnimationFinished{} {}
ActPickCreature::ActPickCreature(MiddayRestoreTag,Piki* piki) : AndAction(AndAction::MiddayRestoreTag{},piki),_18{} {}
ActPickItem::ActPickItem(MiddayRestoreTag,Piki* piki) : AndAction(AndAction::MiddayRestoreTag{},piki),mTargetItem{} {}
ActPullout::ActPullout(MiddayRestoreTag,Piki* piki) : AndAction(AndAction::MiddayRestoreTag{},piki),mTarget{} {}
ActPulloutCreature::ActPulloutCreature(MiddayRestoreTag,Piki* piki) : Action(Action::MiddayRestoreTag{},piki),mState{},mPulloutTimer{},mTarget{},mPulloutSuccess{} {}
ActPush::ActPush(MiddayRestoreTag,Piki* piki) : Action(Action::MiddayRestoreTag{},piki),mHinderRock{},mPushAnimationState{},mState{},_20{},_24{},_28{},_2C{},_38{},mPushObjectStopped{},_40{},_44{},mPushCount{},mIsPushReady{} {}
ActPut::ActPut(MiddayRestoreTag,Piki* piki) : Action(Action::MiddayRestoreTag{},piki),mFailCountdownTimer{} {}
ActPutBomb::ActPutBomb(MiddayRestoreTag,Piki* piki) : Action(Action::MiddayRestoreTag{},piki),mState{},mAnimationFinished{},mTouchedPlayer{},mAimTimer{},mPlaceTimer{},mTarget{} {}
ActPutItem::ActPutItem(MiddayRestoreTag,Piki* piki) : Action(Action::MiddayRestoreTag{},piki),mItemPosition{},mItem{} {}
ActRandomBoid::ActRandomBoid(MiddayRestoreTag,Piki* piki) : Action(Action::MiddayRestoreTag{},piki),mState{},mStateTimer{},mIsAnimFinishing{},_20{},mListener{} {}
ActRescue::ActRescue(MiddayRestoreTag,Piki* piki) : Action(Action::MiddayRestoreTag{},piki),mState{},mDrowningPiki{},mTargetSurviveTimer{},mRescueTargetPosition{},mGotAnimationAction{},mAnimationFinished{},mThrowReady{} {}
ActRope::ActRope(MiddayRestoreTag,Piki* piki) : Action(Action::MiddayRestoreTag{},piki),mSpeed{},mRopeDirection{} {}
ActShoot::ActShoot(MiddayRestoreTag,Piki* piki) : AndAction(AndAction::MiddayRestoreTag{},piki),mTargetIsPlayer{},mTargetObjectPool{},mTarget{},mNavi{} {}
ActShootCreature::ActShootCreature(MiddayRestoreTag,Piki* piki) : Action(Action::MiddayRestoreTag{},piki),mState{},mChaseTimer{},mTarget{} {}
ActStone::ActStone(MiddayRestoreTag,Piki* piki) : Action(Action::MiddayRestoreTag{},piki),mState{},_1A{},mCurrPebble{},mRockGen{},mIsAttackReady{} {}
ActTransport::ActTransport(MiddayRestoreTag,Piki* piki) : Action(Action::MiddayRestoreTag{},piki),mPellet{},mState{},mOdometer{},mMoveDir{},mNumRoutePoints{},mJumpRetryTimer{},mStateProgress{},mNextPathIndex{},_48{},mSplineControlPts{},mRouteStartPos{},mPathType{},mSlotIndex{},mSpinStartPosition{},mFinishPutting{},mIsLiftActionDone{},mLiftRetryCount{},mWaitTimer{},mPathIndex{},mGoalWPIndex{},mGoal{},mCanCarry{},mPcStallCheckPos{} {}
ActWatch::ActWatch(MiddayRestoreTag,Piki* piki) : Action(Action::MiddayRestoreTag{},piki),mTarget{},mWatchRetryTimer{},mListener{},mTargetPosition{} {}
ActWeed::ActWeed(MiddayRestoreTag,Piki* piki) : Action(Action::MiddayRestoreTag{},piki),mState{},_1A{},mCurrGrass{},mGrassGen{},_28{},mAnimationFinished{} {}


struct PcMiddayPikiActionFactoryAccess {
 template<class T> static void detachAction(void* p,size_t) noexcept {static_cast<T*>(p)->mChildActions=nullptr;}
 static void detachChildren(void* p,size_t count) noexcept {auto* c=static_cast<Action::Child*>(p);for(size_t i=0;i<count;++i){c[i].mAction=nullptr;c[i].mInitialiser=nullptr;}}
 template<class T> static T* shell(Piki& p,pc_midday::AllocationOwner& owner){
  T* object=owner.construct<T>([&](void* memory){return new(memory)T(typename T::MiddayRestoreTag{},&p);});
  owner.setCleanup(object,&detachAction<T>);return object;
 }
 static bool inspect(Action* a,int type,Piki& p,std::set<const void*>& seen,std::string& e){
  auto fail=[&](const char*m){e=m;return false;};
  if(!a||!seen.insert(a).second||a->mPiki!=&p||a->mCurrActionIdx!=-1)return fail("action owner/identity/selection");
  switch(type){
 case 1:if(typeid(*a)!=typeid(TopAction))return fail("action concrete type");break;
 case 2:if(typeid(*a)!=typeid(ActAdjust))return fail("action concrete type");break;
 case 3:if(typeid(*a)!=typeid(ActAttack))return fail("action concrete type");break;
 case 4:if(typeid(*a)!=typeid(ActBoMake))return fail("action concrete type");break;
 case 5:if(typeid(*a)!=typeid(ActBoreListen))return fail("action concrete type");break;
 case 6:if(typeid(*a)!=typeid(ActBoreOneshot))return fail("action concrete type");break;
 case 7:if(typeid(*a)!=typeid(ActBoreRest))return fail("action concrete type");break;
 case 8:if(typeid(*a)!=typeid(ActBoreSelect))return fail("action concrete type");break;
 case 9:if(typeid(*a)!=typeid(ActBoreTalk))return fail("action concrete type");break;
 case 10:if(typeid(*a)!=typeid(ActBou))return fail("action concrete type");break;
 case 11:if(typeid(*a)!=typeid(ActBreakWall))return fail("action concrete type");break;
 case 12:if(typeid(*a)!=typeid(ActBridge))return fail("action concrete type");break;
 case 13:if(typeid(*a)!=typeid(ActChase))return fail("action concrete type");break;
 case 14:if(typeid(*a)!=typeid(ActCrowd))return fail("action concrete type");break;
 case 15:if(typeid(*a)!=typeid(ActDecoy))return fail("action concrete type");break;
 case 16:if(typeid(*a)!=typeid(ActDeliver))return fail("action concrete type");break;
 case 17:if(typeid(*a)!=typeid(ActEnter))return fail("action concrete type");break;
 case 18:if(typeid(*a)!=typeid(ActEscape))return fail("action concrete type");break;
 case 19:if(typeid(*a)!=typeid(ActExit))return fail("action concrete type");break;
 case 20:if(typeid(*a)!=typeid(ActFlower))return fail("action concrete type");break;
 case 21:if(typeid(*a)!=typeid(ActFormation))return fail("action concrete type");break;
 case 22:if(typeid(*a)!=typeid(ActFreeSelect))return fail("action concrete type");break;
 case 23:if(typeid(*a)!=typeid(ActFree))return fail("action concrete type");break;
 case 24:if(typeid(*a)!=typeid(ActGoto))return fail("action concrete type");break;
 case 25:if(typeid(*a)!=typeid(ActGuard))return fail("action concrete type");break;
 case 26:if(typeid(*a)!=typeid(ActJumpAttack))return fail("action concrete type");break;
 case 27:if(typeid(*a)!=typeid(ActKinoko))return fail("action concrete type");break;
 case 28:if(typeid(*a)!=typeid(ActMine))return fail("action concrete type");break;
 case 29:if(typeid(*a)!=typeid(ActPick))return fail("action concrete type");break;
 case 30:if(typeid(*a)!=typeid(ActPickCreature))return fail("action concrete type");break;
 case 31:if(typeid(*a)!=typeid(ActPickItem))return fail("action concrete type");break;
 case 32:if(typeid(*a)!=typeid(ActPullout))return fail("action concrete type");break;
 case 33:if(typeid(*a)!=typeid(ActPulloutCreature))return fail("action concrete type");break;
 case 34:if(typeid(*a)!=typeid(ActPush))return fail("action concrete type");break;
 case 35:if(typeid(*a)!=typeid(ActPut))return fail("action concrete type");break;
 case 36:if(typeid(*a)!=typeid(ActPutBomb))return fail("action concrete type");break;
 case 37:if(typeid(*a)!=typeid(ActPutItem))return fail("action concrete type");break;
 case 38:if(typeid(*a)!=typeid(ActRandomBoid))return fail("action concrete type");break;
 case 39:if(typeid(*a)!=typeid(ActRescue))return fail("action concrete type");break;
 case 40:if(typeid(*a)!=typeid(ActRope))return fail("action concrete type");break;
 case 41:if(typeid(*a)!=typeid(ActShoot))return fail("action concrete type");break;
 case 42:if(typeid(*a)!=typeid(ActShootCreature))return fail("action concrete type");break;
 case 43:if(typeid(*a)!=typeid(ActStone))return fail("action concrete type");break;
 case 44:if(typeid(*a)!=typeid(ActTransport))return fail("action concrete type");break;
 case 45:if(typeid(*a)!=typeid(ActWatch))return fail("action concrete type");break;
 case 46:if(typeid(*a)!=typeid(ActWeed))return fail("action concrete type");break;
 default:return fail("unknown action type");}
  static const int root[]={38,45,18,13,24,30,37,21,3,41,25,32,31,15,14,23,40,17,19,11,28,44,27,12,34,36,39,46,43,4,10};
  static const int attack[]={26},bore[]={45,9,6,7},deliver[]={30,24,35},pick[]={24,29},pull[]={24,2,33},shoot[]={24,42};
  const int* children=nullptr;size_t count=0;
  switch(type){case 1:children=root;count=31;break;case 3:children=attack;count=1;break;
  case 8:case 22:children=bore;count=4;break;case 16:children=deliver;count=3;break;
  case 30:case 31:children=pick;count=2;break;case 32:children=pull;count=3;break;
  case 41:children=shoot;count=2;break;default:break;}
  if(a->mChildCount!=int(count)||(count==0&&a->mChildActions)||(count&&!a->mChildActions))return fail("action child topology");
  if(count&&!seen.insert(a->mChildActions).second)return fail("duplicate child storage");
  for(size_t i=0;i<count;++i){auto& c=a->mChildActions[i];if(!inspect(c.mAction,children[i],p,seen,e))return false;
   bool expected=(type==16&&i==1)||(type==41&&i==0);
   if(expected){auto* init=dynamic_cast<ActGoto::Initialiser*>(c.mInitialiser);if(!init||!seen.insert(init).second||init->mTarget||init->mMaxDistance!=(type==16?50.f:220.f)||init->mMinDistance!=(type==16?0.f:100.f))return fail("Goto initializer topology");}
   else if(c.mInitialiser)return fail("unexpected child initializer");
  }
  if(type==1){auto*t=static_cast<TopAction*>(a);if(!t->mListener||t->mListener->mAction!=t||!seen.insert(t->mListener).second)return fail("top listener");auto&b=t->mBoredom;
   if(b.mMaxBoredomCollectors!=30||b.mCurrentBoredomCount||b.mNextAvailableIndex||!b.mBoredomCollectors||!b.mBoredomIds||!seen.insert(b.mBoredomCollectors).second||!seen.insert(b.mBoredomIds).second)return fail("boredom topology");
   for(int i=0;i<30;++i){auto&c=b.mBoredomCollectors[i];if(c.mMaxCount!=5||c.mCurrentCount||!c.mBoredomLevels||!c.mObjectIds||!c.mIsMaxBored||!seen.insert(c.mBoredomLevels).second||!seen.insert(c.mObjectIds).second||!seen.insert(c.mIsMaxBored).second)return fail("boredom collector topology");}
  }
  if(type==38){auto*t=static_cast<ActRandomBoid*>(a);if(!t->mListener||t->mListener->mAction!=t||t->mListener->mPiki!=&p||!seen.insert(t->mListener).second)return fail("boid listener");}
  if(type==45){auto*t=static_cast<ActWatch*>(a);if(!t->mListener||t->mListener->mOwnerAction!=t||t->mListener->mActor!=&p||!seen.insert(t->mListener).second)return fail("watch listener");}
  if(type==14&&!inspect(static_cast<ActCrowd*>(a)->mSelectAction,8,p,seen,e))return false;
  if(type==23&&!inspect(static_cast<ActFree*>(a)->mSelectAction,22,p,seen,e))return false;
  return true;
 }
 static Action* build(int type,Piki& p,pc_midday::AllocationOwner& owner){
  Action* a=nullptr;switch(type){
 case 1:a=shell<TopAction>(p,owner);break;
 case 2:a=shell<ActAdjust>(p,owner);break;
 case 3:a=shell<ActAttack>(p,owner);break;
 case 4:a=shell<ActBoMake>(p,owner);break;
 case 5:a=shell<ActBoreListen>(p,owner);break;
 case 6:a=shell<ActBoreOneshot>(p,owner);break;
 case 7:a=shell<ActBoreRest>(p,owner);break;
 case 8:a=shell<ActBoreSelect>(p,owner);break;
 case 9:a=shell<ActBoreTalk>(p,owner);break;
 case 10:a=shell<ActBou>(p,owner);break;
 case 11:a=shell<ActBreakWall>(p,owner);break;
 case 12:a=shell<ActBridge>(p,owner);break;
 case 13:a=shell<ActChase>(p,owner);break;
 case 14:a=shell<ActCrowd>(p,owner);break;
 case 15:a=shell<ActDecoy>(p,owner);break;
 case 16:a=shell<ActDeliver>(p,owner);break;
 case 17:a=shell<ActEnter>(p,owner);break;
 case 18:a=shell<ActEscape>(p,owner);break;
 case 19:a=shell<ActExit>(p,owner);break;
 case 20:a=shell<ActFlower>(p,owner);break;
 case 21:a=shell<ActFormation>(p,owner);break;
 case 22:a=shell<ActFreeSelect>(p,owner);break;
 case 23:a=shell<ActFree>(p,owner);break;
 case 24:a=shell<ActGoto>(p,owner);break;
 case 25:a=shell<ActGuard>(p,owner);break;
 case 26:a=shell<ActJumpAttack>(p,owner);break;
 case 27:a=shell<ActKinoko>(p,owner);break;
 case 28:a=shell<ActMine>(p,owner);break;
 case 29:a=shell<ActPick>(p,owner);break;
 case 30:a=shell<ActPickCreature>(p,owner);break;
 case 31:a=shell<ActPickItem>(p,owner);break;
 case 32:a=shell<ActPullout>(p,owner);break;
 case 33:a=shell<ActPulloutCreature>(p,owner);break;
 case 34:a=shell<ActPush>(p,owner);break;
 case 35:a=shell<ActPut>(p,owner);break;
 case 36:a=shell<ActPutBomb>(p,owner);break;
 case 37:a=shell<ActPutItem>(p,owner);break;
 case 38:a=shell<ActRandomBoid>(p,owner);break;
 case 39:a=shell<ActRescue>(p,owner);break;
 case 40:a=shell<ActRope>(p,owner);break;
 case 41:a=shell<ActShoot>(p,owner);break;
 case 42:a=shell<ActShootCreature>(p,owner);break;
 case 43:a=shell<ActStone>(p,owner);break;
 case 44:a=shell<ActTransport>(p,owner);break;
 case 45:a=shell<ActWatch>(p,owner);break;
 case 46:a=shell<ActWeed>(p,owner);break;
 default:throw std::logic_error("unknown restore Action concrete type");}
  static const int root[]={38,45,18,13,24,30,37,21,3,41,25,32,31,15,14,23,40,17,19,11,28,44,27,12,34,36,39,46,43,4,10};
  static const int attack[]={26},bore[]={45,9,6,7},deliver[]={30,24,35},pick[]={24,29},pull[]={24,2,33},shoot[]={24,42};
  const int* children=nullptr;size_t count=0;
  switch(type){case 1:children=root;count=31;break;case 3:children=attack;count=1;break;
  case 8:case 22:children=bore;count=4;break;case 16:children=deliver;count=3;break;
  case 30:case 31:children=pick;count=2;break;case 32:children=pull;count=3;break;
  case 41:children=shoot;count=2;break;default:break;}
  if(count){
   auto* slots=owner.array<Action::Child>(count);owner.setCleanup(slots,&detachChildren);
   a->mChildActions=slots;a->mChildCount=static_cast<s16>(count);
   for(size_t i=0;i<count;++i)slots[i].mAction=build(children[i],p,owner);
  }
  if(type==16)a->mChildActions[1].mInitialiser=owner.make<ActGoto::Initialiser>(50.0f,0.0f,nullptr);
  if(type==41)a->mChildActions[0].mInitialiser=owner.make<ActGoto::Initialiser>(220.0f,100.0f,nullptr);
  if(type==1){auto* t=static_cast<TopAction*>(a);t->mListener=owner.make<TopAction::MotionListener>(t);
   auto& b=t->mBoredom;
   b.mBoredomCollectors=owner.arrayConstruct<TopAction::ObjBore>(30,[](void* slot,size_t){return new(slot)TopAction::ObjBore(TopAction::ObjBore::MiddayRestoreTag{});});
   b.mBoredomIds=owner.array<int>(30);
   for(int i=0;i<30;++i){auto& c=b.mBoredomCollectors[i];c.mBoredomLevels=owner.array<float>(5);c.mObjectIds=owner.array<int>(5);c.mIsMaxBored=owner.array<bool>(5);}
  }
  if(type==38){auto* t=static_cast<ActRandomBoid*>(a);t->mListener=owner.make<ActRandomBoid::AnimListener>(t,&p);t->mListener->_0C=false;}
  if(type==45){auto* t=static_cast<ActWatch*>(a);t->mListener=owner.make<ActWatch::AnimListener>(t,&p);}
  if(type==14)static_cast<ActCrowd*>(a)->mSelectAction=static_cast<ActBoreSelect*>(build(8,p,owner));
  if(type==23)static_cast<ActFree*>(a)->mSelectAction=static_cast<ActFreeSelect*>(build(22,p,owner));
  return a;
 }
};
namespace pc_midday {
bool inspect_piki_action_graph(Piki& p,TopAction& root,std::string& e){std::set<const void*> seen;return PcMiddayPikiActionFactoryAccess::inspect(&root,1,p,seen,e);}
TopAction* allocate_piki_action_graph(Piki& p,AllocationOwner& owner){return static_cast<TopAction*>(PcMiddayPikiActionFactoryAccess::build(1,p,owner));}
}

#endif
