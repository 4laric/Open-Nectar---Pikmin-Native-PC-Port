#include "pc_midday_actor_archive.h"
#include "Navi.h"
#include "CPlate.h"
#include "SlotChangeListner.h"
namespace pc_midday { bool animation_fields(PaniPikiAnimator&,ActorArchive&); }
struct PcMiddayNaviRuntimeAccess {
 static bool plate(CPlate& s,pc_midday::ActorArchive& a) {
    using namespace pc_midday;
    int capacity=a.mode()==Mode::Capture?s.mSlotListSize:0;
    if(!a.scalar("capacity",ScalarKind::S32,&capacity)||capacity<1||capacity>4096||capacity!=s.mSlotListSize||!s.mSlotList)
        return a.fail("CPlate allocated capacity mismatch");
    if(!a.field("mPlateOffset",s.mPlateOffset))return false;
    if(!a.field("mPlateLength",s.mPlateLength))return false;
    if(!a.field("mPlateSize",s.mPlateSize))return false;
    if(!a.field("mInnerRadius",s.mInnerRadius))return false;
    if(!a.field("mTotalSlotCount",s.mTotalSlotCount))return false;
    if(!a.field("mPlatePikiCount",s.mPlatePikiCount))return false;
    if(!a.field("mUsedSlotCount",s.mUsedSlotCount))return false;
    if(!a.field("mOriginPosition",s.mOriginPosition))return false;
    if(!a.field("mPlateCenter",s.mPlateCenter))return false;
    if(!a.field("mCurrentVelocity",s.mCurrentVelocity))return false;
    if(!a.field("mDirectionAngle",s.mDirectionAngle))return false;
    if(!a.field("_C8",s._C8))return false;
    if(!a.field("mIsNeutral",s.mIsNeutral))return false;
    if(!a.field("params.startOffset",s.mCPlateParms.mStartOffset())||!a.field("params.lengthLimit",s.mCPlateParms.mLengthLimit())||!a.field("params.maxPosSize",s.mCPlateParms.mMaxPosSize()))return false;
    for(int i=0;i<3;++i)if(!a.field(("happa."+std::to_string(i)).c_str(),s.mHappaCounts[i]))return false;
    for(int i=0;i<capacity;++i) {
        PrefixArchive slot(a,("slot."+std::to_string(i)).c_str());auto& v=s.mSlotList[i];
        if(!slot.field("position",v.mPosition)||!slot.field("offset",v.mOffsetFromCenter)||
           !slot.ref("occupant",RefKind::Creature,v.mOccupant.mPtr))return false;
        // Releasing the last slot clears its occupant but leaves the listener
        // stale. getSlot overwrites it before reuse; do not root that dead link.
        auto* listener=a.mode()==Mode::Capture && !v.mOccupant.mPtr ? nullptr : v.mListener;
        if(!slot.ref("listener",RefKind::SlotListener,listener))return false;
        if(a.mode()==Mode::Apply)v.mListener=listener;
    }
    return true;
 }
};
namespace pc_midday {
bool navi_runtime_fields(Navi& s,ActorArchive& outer) {
    PrefixArchive a(outer,"navi.runtime");
    if(!a.field("mIsRidingUfo",s.mIsRidingUfo))return false;
    if(!a.field("mIsPellet",s.mIsPellet))return false;
    if(!a.field("mLookTimer",s.mLookTimer))return false;
    if(!a.field("mHeadYawOffsetRel",s.mHeadYawOffsetRel))return false;
    if(!a.field("mHeadPitchOffset",s.mHeadPitchOffset))return false;
    if(!a.field("mCollidedWorkObjTimer",s.mCollidedWorkObjTimer))return false;
    if(!a.field("mIsInWater",s.mIsInWater))return false;
    if(!a.field("mPluckCursorVisibilityTimer",s.mPluckCursorVisibilityTimer))return false;
    if(!a.field("mIsCursorVisible",s.mIsCursorVisible))return false;
    if(!a.field("mMotionSpeed",s.mMotionSpeed))return false;
    if(!a.field("mIsDayEnd",s.mIsDayEnd))return false;
    if(!a.field("mCursorNaviDist",s.mCursorNaviDist))return false;
    if(!a.field("mPendingLowerMotionId",s.mPendingLowerMotionId))return false;
    if(!a.field("mLowerMotionCooldown",s.mLowerMotionCooldown))return false;
    if(!a.field("mFlickIntensity",s.mFlickIntensity))return false;
    if(!a.field("mPlateYaw",s.mPlateYaw))return false;
    if(!a.field("mPlateDirLocked",s.mPlateDirLocked))return false;
    if(!a.field("mRearrangePending",s.mRearrangePending))return false;
    if(!a.field("mFormationBand",s.mFormationBand))return false;
    if(!a.field("mFormationBandStableTimer",s.mFormationBandStableTimer))return false;
    if(!a.field("mIsCStickNeutral",s.mIsCStickNeutral))return false;
    if(!a.field("mSeedCollectionCount",s.mSeedCollectionCount))return false;
    if(!a.field("mCurrKeyCount",s.mCurrKeyCount))return false;
    if(!a.field("mNeutralTime",s.mNeutralTime))return false;
    if(!a.field("mAiTickTimer",s.mAiTickTimer))return false;
    if(!a.field("mAiHitWall",s.mAiHitWall))return false;
    if(!a.field("mWalkAnimPrevDir",s.mWalkAnimPrevDir))return false;
    if(!a.field("mPreBlendLowerMotionID",s.mPreBlendLowerMotionID))return false;
    if(!a.field("mIsPlucking",s.mIsPlucking))return false;
    if(!a.field("mFastPluckKeyTaps",s.mFastPluckKeyTaps))return false;
    if(!a.field("mNoPluckTimer",s.mNoPluckTimer))return false;
    if(!a.field("mThrowHoldTime",s.mThrowHoldTime))return false;
    if(!a.field("mThrowDistance",s.mThrowDistance))return false;
    if(!a.field("mThrowHeight",s.mThrowHeight))return false;
    if(!a.field("mFormationPriMode",s.mFormationPriMode))return false;
    if(!a.field("mPressedTimer",s.mPressedTimer))return false;
    if(!a.field("mForcePikiDistCheck",s.mForcePikiDistCheck))return false;
    if(!a.field("mNaviID",s.mNaviID))return false;
    if(!a.field("mWhistleTimer",s.mWhistleTimer))return false;
    if(!a.field("mWhistleCircleMode",s.mWhistleCircleMode))return false;
    if(!a.field("mWhistleRadiusFrac",s.mWhistleRadiusFrac))return false;
    if(!a.field("mWhistleCircleRadius",s.mWhistleCircleRadius))return false;
    if(!a.field("_AC4",s._AC4))return false;
    if(!a.field("_AD8",s._AD8))return false;
    if(!a.field("mPcPikiLeafTip",s.mPcPikiLeafTip))return false;
    if(!a.field("mCursorPosition",s.mCursorPosition))return false;
    if(!a.field("mCursorTargetPosition",s.mCursorTargetPosition))return false;
    if(!a.field("mCursorWorldPos",s.mCursorWorldPos))return false;
    if(!a.field("mPrevMainStick",s.mPrevMainStick))return false;
    if(!a.field("mMainStick",s.mMainStick))return false;
    if(!a.field("mPrevCStick",s.mPrevCStick))return false;
    if(!a.field("mCStick",s.mCStick))return false;
    if(!a.field("mCursorTrailLastPos",s.mCursorTrailLastPos))return false;
    if(!a.field("mNaviLightPosition",s.mNaviLightPosition))return false;
    if(!a.field("mDayEndPosition",s.mDayEndPosition))return false;
    if(!a.field("mWalkAnimPrevPos",s.mWalkAnimPrevPos))return false;
    if(!a.ref("mDamageEfxA",RefKind::ParticleGenerator,s.mDamageEfxA))return false;
    if(!a.ref("mDamageEfxB",RefKind::ParticleGenerator,s.mDamageEfxB))return false;
    if(!a.ref("mDamageEfxC",RefKind::ParticleGenerator,s.mDamageEfxC))return false;
    if(!a.ref("mKontroller",RefKind::Controller,s.mKontroller))return false;
    if(!a.ref("mNaviCamera",RefKind::Camera,s.mNaviCamera))return false;
    if(!a.ref("mControlCamera",RefKind::Camera,s.mControlCamera))return false;
    if(!a.ref("mLookAtPosPtr",RefKind::Vector3,s.mLookAtPosPtr))return false;
    if(!a.ref("mCollidedWorkObj",RefKind::Creature,s.mCollidedWorkObj))return false;
    if(!a.ref("mSelectedShipPart",RefKind::Creature,s.mSelectedShipPart))return false;
    if(!a.ref("mPcLockTarget",RefKind::Creature,s.mPcLockTarget))return false;
    if(!a.ref("mGoalItem",RefKind::Creature,s.mGoalItem))return false;
    if(!a.ref("mPikiToPluck",RefKind::Creature,s.mPikiToPluck))return false;
    if(!a.ref("mSproutToPluck",RefKind::Creature,s.mSproutToPluck))return false;
    if(!a.ref("mNextThrowPiki",RefKind::Creature,s.mNextThrowPiki))return false;
    if(!a.ref("mPellet",RefKind::Creature,s.mPellet))return false;
    if(!a.ref("mBurnEffect",RefKind::Effect,s.mBurnEffect))return false;
    if(!a.ref("mRippleEffect",RefKind::Effect,s.mRippleEffect))return false;
    if(!a.ref("mSlimeEffect",RefKind::Effect,s.mSlimeEffect))return false;
    if(!a.ref("mNaviLightEfx",RefKind::Effect,s.mNaviLightEfx))return false;
    if(!a.ref("mNaviLightGlowEfx",RefKind::Effect,s.mNaviLightGlowEfx))return false;
    if(!a.ref("mCursorTrailEfx",RefKind::Effect,s.mCursorTrailEfx))return false;
    if(!a.ref("mWallPlane",RefKind::Plane,s.mWallPlane))return false;
    if(!a.ref("mWallCollObj",RefKind::DynCollObject,s.mWallCollObj))return false;
    if(!a.ref("mNaviShapeObject",RefKind::Shape,s.mNaviShapeObject))return false;
    if(!a.ref("attackTarget",RefKind::Creature,s.mAttackTarget.mPtr))return false;
    if(!a.field("odometer.distance",s.mOdoMeter.mTotalDistance)||!a.field("odometer.remaining",s.mOdoMeter.mRemainingTime)||!a.field("odometer.minimum",s.mOdoMeter.mMinAllowedDistance)||!a.field("odometer.reset",s.mOdoMeter.mResetTimeValue))return false;
    for(unsigned i=0;i<32;++i)if(!a.field(("whistleFx."+std::to_string(i)).c_str(),s.mWhistleFxPosArr[i]))return false;
    if(!a.field("animationSpeed",s.mNaviAnimMgr.mAnimSpeed))return false;
    PrefixArchive upper(a,"upperAnimation"),lower(a,"lowerAnimation");
    if(!animation_fields(s.mNaviAnimMgr.mUpperAnimator,upper)||!animation_fields(s.mNaviAnimMgr.mLowerAnimator,lower))return false;
    int color=outer.mode()==Mode::Capture?s.mPcPikiAnimColor:-1;
    if(!a.scalar("mPcPikiAnimColor",ScalarKind::S32,&color)||color< -1||color>2)return a.fail("invalid captain Piki animation color");
    if(color>=0) {
        if(!a.field("pcAnimationSpeed",s.mPcPikiAnimMgr.mAnimSpeed))return false;
        PrefixArchive pcUpper(a,"pcUpperAnimation"),pcLower(a,"pcLowerAnimation");
        if(!animation_fields(s.mPcPikiAnimMgr.mUpperAnimator,pcUpper)||!animation_fields(s.mPcPikiAnimMgr.mLowerAnimator,pcLower))return false;
    }
    if(outer.mode()==Mode::Apply)s.mPcPikiAnimColor=color;
    if(!s.mPlateMgr)return a.fail("Navi CPlate missing");
    PrefixArchive plate(a,"plate");return PcMiddayNaviRuntimeAccess::plate(*s.mPlateMgr,plate);
}
}
