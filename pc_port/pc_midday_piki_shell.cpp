#if defined(PIKI_PC_PORT)
#include "ViewPiki.h"
// Inert root only. The owning factory must keep this object unpublished and
// ledger-allocate every action/FSM/effect/path/collision subobject before bind.
// Ordinary Piki/ViewPiki constructors and gameplay initialization are bypassed.
Piki::Piki(MiddayRestoreTag, CreatureProp* props) : Creature(Creature::MiddayRestoreTag{},props) {
    mPathBuffers=nullptr;
    mRouteHandle=0;mUseAsyncPathfinding=false;
    mRouteSourceIndex=-1;mRouteDestinationIndex=-1;
    mRouteTargetCreature=nullptr;mLookatPosPtr=nullptr;
    mCarryingShipPart=nullptr;mPanickedEffect=nullptr;
    mBurnEffect=nullptr;mRippleEffect=nullptr;mFreeLightEffect=nullptr;mSlimeEffect=nullptr;
    mFSM=nullptr;mActiveAction=nullptr;mCurrentState=nullptr;
    mCurrNectar=nullptr;mSwallowMouthPart=nullptr;mLeaderCreature=nullptr;
    mPushTargetPiki=nullptr;mWallPlane=nullptr;mWallObj=nullptr;mNavi=nullptr;
    mRopeListHead=nullptr;mRope=nullptr;mNextRopeHolder=nullptr;mPrevRopeHolder=nullptr;
    mStickListHead=nullptr;mStickTarget=nullptr;mStickPart=nullptr;
    mNextSticker=nullptr;mPrevSticker=nullptr;mFormMgr=nullptr;mCurrCollisionModel=nullptr;
    mObjType=OBJTYPE_Piki;mMode=PikiMode::FormationMode;
    mIsLooking=false;mIsCallable=false;mIsPanicked=false;mWantToStick=false;
    mIsWhistlePending=false;mDeathTimer=0;mHappa=0;
    mSearchBuffer.bindMiddayRestoreStorage(mPikiSearchData,6);
    // PaniAnimator's ordinary default constructor leaves these roots unset.
    mPikiAnimMgr.mUpperAnimator.mMgr=nullptr;
    mPikiAnimMgr.mUpperAnimator.mContext=nullptr;
    mPikiAnimMgr.mLowerAnimator.mMgr=nullptr;
    mPikiAnimMgr.mLowerAnimator.mContext=nullptr;
}
ViewPiki::ViewPiki(MiddayRestoreTag, CreatureProp* props)
    : Piki(Piki::MiddayRestoreTag{},props) {
    mPikiShape=nullptr;mHappaModel=nullptr;
    mLastEffectPosition.set(0,0,0);
}
#endif
