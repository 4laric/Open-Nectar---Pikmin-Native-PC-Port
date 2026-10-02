#if defined(PIKI_PC_PORT)
#include "pc_midday_actor_states.h"
#include "Piki.h"
#include <string>
namespace pc_midday {
namespace {
bool odometer(ActorArchive& a,OdoMeter& o){return a.field("distance",o.mTotalDistance)&&a.field("remaining",o.mRemainingTime)&&a.field("minimum",o.mMinAllowedDistance)&&a.field("reset",o.mResetTimeValue);}
bool context(ActorArchive& a,const char* key,UpdateContext& c){PrefixArchive p(a,key);return p.ref("manager",RefKind::UpdateMgr,c.mMgr)&&p.field("slot",c.mMgrSlotIndex)&&p.field("piki",c.mIsPiki);}
}
bool animation_fields(PaniPikiAnimator& s,ActorArchive& ar) {
 // A constructed but never-started animator has indeterminate key/frame data.
 // Check the constructor-initialized resource pointers before reading it.
 if(ar.mode()==Mode::Capture&&(!s.mMgr||!s.mContext||!s.mMotionTable||!s.mAnimInfo))return ar.fail("animator has not entered an initialized motion");
 if(!ar.field("mPlayState",s.mPlayState))return false;
 if(!ar.field("mCurrentAnimID",s.mCurrentAnimID))return false;
 if(!ar.field("mStartKeyIndex",s.mStartKeyIndex))return false;
 if(!ar.field("mEndKeyIndex",s.mEndKeyIndex))return false;
 if(!ar.field("mAnimationCounter",s.mAnimationCounter))return false;
 if(!ar.field("mCurrentKeyIndex",s.mCurrentKeyIndex))return false;
 if(!ar.field("mPreviousKeyIndex",s.mPreviousKeyIndex))return false;
 if(!ar.field("mMotionIdx",s.mMotionIdx))return false;
 if(!ar.field("mIsFinished",s.mIsFinished))return false;
 if(!ar.ref("mMgr",RefKind::Animation,s.mMgr))return false;
 if(!ar.ref("mContext",RefKind::Animation,s.mContext))return false;
 if(!ar.ref("mAnimInfo",RefKind::Animation,s.mAnimInfo))return false;
 if(!ar.ref("mMotionTable",RefKind::Animation,s.mMotionTable))return false;
 if(!ar.ref("mListener",RefKind::AnimListener,s.mListener))return false;
 return true;
}
bool piki_runtime_fields(Piki& s,ActorArchive& outer) {
 PrefixArchive ar(outer,"piki.runtime");
 if(!ar.ref("mPathBuffers",RefKind::Path,s.mPathBuffers))return false;
 int capacity=routeMgr?routeMgr->getNumWayPoints('test'):0;
 const int allocated=capacity;
 if(!ar.scalar("path.capacity",ScalarKind::S32,&capacity)||capacity<0||capacity>32767||capacity!=allocated||(capacity&&!s.mPathBuffers))return ar.fail("Piki path allocation/content mismatch");
 for(int i=0;i<capacity;++i){PrefixArchive item(ar,("path."+std::to_string(i)).c_str());if(!item.field("waypoint",s.mPathBuffers[i].mWayPointIdx)||!item.field("direction",s.mPathBuffers[i].mDirection))return false;}
 if(!context(ar,"update",s.mPikiUpdateContext)||!context(ar,"lookUpdate",s.mPikiLookUpdateContext))return false;
 if(!ar.field("mUseAsyncPathfinding",s.mUseAsyncPathfinding))return false;
 if(!ar.field("mRouteSourceIndex",s.mRouteSourceIndex))return false;
 if(!ar.field("mRouteDestinationIndex",s.mRouteDestinationIndex))return false;
 if(!ar.field("mIsRetryPathfind",s.mIsRetryPathfind))return false;
 if(!ar.field("mCurrRoutePoint",s.mCurrRoutePoint))return false;
 if(!ar.field("mRouteStartPos",s.mRouteStartPos))return false;
 if(!ar.field("mRouteGoalPos",s.mRouteGoalPos))return false;
 if(!ar.field("mNumRoutePoints",s.mNumRoutePoints))return false;
 if(!ar.field("mIsLooking",s.mIsLooking))return false;
 if(!ar.field("mLookTimer",s.mLookTimer))return false;
 if(!ar.field("mHorizontalRotation",s.mHorizontalRotation))return false;
 if(!ar.field("mVerticalRotation",s.mVerticalRotation))return false;
 if(!ar.field("mOldFaceDirection",s.mOldFaceDirection))return false;
 if(!ar.field("mBlendMotionIdx",s.mBlendMotionIdx))return false;
 if(!ar.field("mEmotion",s.mEmotion))return false;
 if(!ar.field("mActionState",s.mActionState))return false;
 if(!ar.field("mIsCallable",s.mIsCallable))return false;
 if(!ar.field("mIsPanicked",s.mIsPanicked))return false;
 if(!ar.field("mInWaterTimer",s.mInWaterTimer))return false;
 if(!ar.field("mPlayerId",s.mPlayerId))return false;
 if(!ar.field("mPcSieging",s.mPcSieging))return false;
 if(!ar.field("mLastAnimPosition",s.mLastAnimPosition))return false;
 if(!ar.field("mShadowPos",s.mShadowPos))return false;
 if(!ar.field("mCatchPos",s.mCatchPos))return false;
 if(!ar.field("mEffectPos",s.mEffectPos))return false;
 if(!ar.field("mWantToStick",s.mWantToStick))return false;
 if(!ar.field("mMotionSpeed",s.mMotionSpeed))return false;
 if(!ar.field("mMoveSpeed",s.mMoveSpeed))return false;
 if(!ar.field("mDeathTimer",s.mDeathTimer))return false;
 if(!ar.field("mFlickIntensity",s.mFlickIntensity))return false;
 if(!ar.field("mRotationAngle",s.mRotationAngle))return false;
 if(!ar.field("mIsWhistlePending",s.mIsWhistlePending))return false;
 if(!ar.field("mPluckVelocity",s.mPluckVelocity))return false;
 if(!ar.field("mFormationPriority",s.mFormationPriority))return false;
 if(!ar.field("mPushTargetPos",s.mPushTargetPos))return false;
 if(!ar.field("mPikiSize",s.mPikiSize))return false;
 if(!ar.field("mMode",s.mMode))return false;
 if(!ar.field("mColor",s.mColor))return false;
 if(!ar.field("mFloweringTimer",s.mFloweringTimer))return false;
 if(!ar.field("mHappa",s.mHappa))return false;
 if(!ar.field("mFiredState",s.mFiredState))return false;
 if(!ar.field("mColourBlendRatio",s.mColourBlendRatio))return false;
 if(!ar.field("mEraseOnKill",s.mEraseOnKill))return false;
 if(!ar.field("mP2Purple",s.mP2Purple))return false;
 if(!ar.field("mP2White",s.mP2White))return false;
 if(!ar.field("mP2Bulbmin",s.mP2Bulbmin))return false;
 if(!ar.field("mP2AnimationTime",s.mP2AnimationTime))return false;
 if(!ar.field("mGasInvincible",s.mGasInvincible))return false;
 if(!ar.field("mPcLastDryPos",s.mPcLastDryPos))return false;
 if(!ar.field("mPcHasDryPos",s.mPcHasDryPos))return false;
 if(!ar.field("_334",s._334))return false;
 if(!ar.field("_474",s._474))return false;
 if(!ar.field("_478",s._478))return false;
 if(!ar.field("_480",s._480))return false;
 if(!ar.field("_484",s._484))return false;
 if(!ar.field("_4C8",s._4C8))return false;
 if(!ar.field("_4D8",s._4D8))return false;
 if(!ar.field("_4E8",s._4E8))return false;
 if(!ar.field("_4EC",s._4EC))return false;
 if(!ar.field("_4F0",s._4F0))return false;
 if(!ar.field("_518",s._518))return false;
 if(!ar.field("_519",s._519))return false;
 if(!ar.field("_51C",s._51C))return false;
 if(!ar.field("_528",s._528))return false;
 if(!ar.ref("mRouteTargetCreature",RefKind::Creature,s.mRouteTargetCreature))return false;
 if(!ar.ref("mLookatPosPtr",RefKind::Vector3,s.mLookatPosPtr))return false;
 if(!ar.ref("mCarryingShipPart",RefKind::Creature,s.mCarryingShipPart))return false;
 if(!ar.ref("mCurrNectar",RefKind::Creature,s.mCurrNectar))return false;
 if(!ar.ref("mSwallowMouthPart",RefKind::CollPart,s.mSwallowMouthPart))return false;
 if(!ar.ref("mLeaderCreature",RefKind::Creature,s.mLeaderCreature))return false;
 if(!ar.ref("mPushTargetPiki",RefKind::Creature,s.mPushTargetPiki))return false;
 if(!ar.ref("mWallPlane",RefKind::Plane,s.mWallPlane))return false;
 if(!ar.ref("mWallObj",RefKind::DynCollObject,s.mWallObj))return false;
 if(!ar.ref("mNavi",RefKind::Creature,s.mNavi))return false;
 if(!ar.ref("mPanickedEffect",RefKind::Effect,s.mPanickedEffect))return false;
 if(!ar.ref("mBurnEffect",RefKind::Effect,s.mBurnEffect))return false;
 if(!ar.ref("mRippleEffect",RefKind::Effect,s.mRippleEffect))return false;
 if(!ar.ref("mFreeLightEffect",RefKind::Effect,s.mFreeLightEffect))return false;
 if(!ar.ref("mSlimeEffect",RefKind::Effect,s.mSlimeEffect))return false;
 if(!ar.ref("mLookAtCreature",RefKind::Creature,s.mLookAtCreature.mPtr))return false;
 if(!ar.ref("_500",RefKind::Creature,s._500.mPtr))return false;
 u32 route=ar.mode()==Mode::Capture?s.mRouteHandle:0;
 if(!ar.handle("routeHandle",RefKind::Path,route))return false;
 if(ar.mode()==Mode::Apply)s.mRouteHandle=route;
 for(unsigned i=0;i<4;++i){PrefixArchive point(ar,("spline."+std::to_string(i)).c_str());if(!point.field("position",s.mSplineControlPts[i]))return false;}
 PrefixArchive odo(ar,"odometer");if(!odometer(odo,s.mOdometer))return false;
 if(!ar.field("animationSpeed",s.mPikiAnimMgr.mAnimSpeed))return false;
 PrefixArchive upper(ar,"upperAnimation"),lower(ar,"lowerAnimation");
 if(!animation_fields(s.mPikiAnimMgr.mUpperAnimator,upper)||!animation_fields(s.mPikiAnimMgr.mLowerAnimator,lower))return false;
 if(!ar.field("mCurrentColour.r",s.mCurrentColour.r))return false;
 if(!ar.field("mCurrentColour.g",s.mCurrentColour.g))return false;
 if(!ar.field("mCurrentColour.b",s.mCurrentColour.b))return false;
 if(!ar.field("mCurrentColour.a",s.mCurrentColour.a))return false;
 if(!ar.field("mDefaultColour.r",s.mDefaultColour.r))return false;
 if(!ar.field("mDefaultColour.g",s.mDefaultColour.g))return false;
 if(!ar.field("mDefaultColour.b",s.mDefaultColour.b))return false;
 if(!ar.field("mDefaultColour.a",s.mDefaultColour.a))return false;
 if(!ar.field("mStartBlendColour.r",s.mStartBlendColour.r))return false;
 if(!ar.field("mStartBlendColour.g",s.mStartBlendColour.g))return false;
 if(!ar.field("mStartBlendColour.b",s.mStartBlendColour.b))return false;
 if(!ar.field("mStartBlendColour.a",s.mStartBlendColour.a))return false;
 if(!ar.field("mTargetBlendColour.r",s.mTargetBlendColour.r))return false;
 if(!ar.field("mTargetBlendColour.g",s.mTargetBlendColour.g))return false;
 if(!ar.field("mTargetBlendColour.b",s.mTargetBlendColour.b))return false;
 if(!ar.field("mTargetBlendColour.a",s.mTargetBlendColour.a))return false;
 return true;
}
}
#endif
