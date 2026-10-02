#if defined(PIKI_PC_PORT)
#include "Creature.h"
LifeGauge::LifeGauge(MiddayRestoreTag) {
    mRenderStyle=Bar;mDisplayState=STATE_Hidden;
    mFadeTransitionValue=0.0f;mVisibleHoldTimer=0.0f;mSnapToTargetHealth=false;
    mCurrentDisplayHealthRatio=1.0f;mTargetHealthRatio=1.0f;
    mPosition.set(0,0,0);mOffset.set(0,100,0);mScale=48.0f;mActiveCarryNumber=nullptr;
    // Ordinary construction also resets shared gauge colours; restore must not.
}
// Restore-only construction: no manager, RNG, AIPerf, audio or gameplay calls.
SearchBuffer::SearchBuffer(MiddayRestoreTag)
    : mMaxDistance(-100.0f),mLastEntry(-1),_10(0),mDataList(nullptr),
      mCurrentEntries(0),mMaxEntries(0),_1C(0),_20(0),_24(0) {}
void SearchBuffer::bindMiddayRestoreStorage(SearchData* fresh,int count) {
    // Private callers supply their own fixed-size, freshly constructed inline
    // storage. No SmartPtr reset/add/sub callbacks; all wrappers already null.
    mDataList=fresh;mMaxEntries=static_cast<s16>(count);mCurrentEntries=0;
    mMaxDistance=-100.0f;mLastEntry=-1;_10=0;
    for(int i=0;i<count;++i){fresh[i].mDistance=12800.0f;fresh[i].mSearchIteration=0;}
}
Creature::Creature(MiddayRestoreTag,CreatureProp* props)
    : mLifeGauge(LifeGauge::MiddayRestoreTag{}),
      mSearchBuffer(SearchBuffer::MiddayRestoreTag{}) {
    mObjType=OBJTYPE_INVALID;mSeContext=nullptr;mCreatureFlags=0;
    mCollInfo=nullptr;mGroundTriangle=nullptr;_30=0;mRebirthDay=0;
    mCollNormal=nullptr;mPikiPlatformTriangle=nullptr;mGenerator=nullptr;
    mTargetVelocity.set(0,0,0);_B0.set(0,0,0);mVelocity.set(0,0,0);
    mSRT.r.set(0,0,0);mSRT.t.set(0,0,0);mSRT.s.set(1,1,1);
    mFaceDirection=0;mSize=10;mCollisionRadius=16;mProps=props;mFormPoint=nullptr;
    mRotationQuat.v.set(0,0,0);mRotationQuat.s=1;mPrevAngularVelocity.set(0,0,0);
    mIsBeingDamaged=false;mCollPlatform=nullptr;mPreviousTriangle=nullptr;
    _298=0;mIsFrozen=0;
    // Pointer roots ordinary init normally completes are inert until staged bind.
    mRopeListHead=nullptr;mRope=nullptr;mNextRopeHolder=nullptr;mPrevRopeHolder=nullptr;
    mStickListHead=nullptr;mStickTarget=nullptr;mStickPart=nullptr;
    mNextSticker=nullptr;mPrevSticker=nullptr;mFormMgr=nullptr;mCurrCollisionModel=nullptr;
}
#endif
