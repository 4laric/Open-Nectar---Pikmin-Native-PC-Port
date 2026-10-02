#if defined(PIKI_PC_PORT)
#include "Navi.h"
#include <initializer_list>

ShadowCaster::ShadowCaster(MiddayRestoreTag) : CoreNode("") {
    // Camera/CullFrustum/LightCamera constructors initialize only local fields.
    // Deliberately bypass the ordinary ShadowCaster's gsys print-toggle access.
    mLightCamera.mPosition.set(0.0f, 10.0f, 0.0f);
    mLightCamera.mFocus.set(0.0f, 0.0f, 0.00001f);
    mLightCamera.mRotation.set(0.0f, 0.0f, 0.0f);
    mLightCamera.mFov = 90.0f;
    mLightCamera.mNear = 1.0f;
    mLightCamera.mFar = 3000.0f;
    mShadowDrawer = nullptr;
    mLightCamera.mTotalPlaneCount = 0;
    mLightCamera.mActivePlaneCount = 0;
    for (auto& plane : mLightCamera.mPlanePointers) plane = nullptr;
}

Navi::Navi(MiddayRestoreTag, CreatureProp* props, int slot)
    : Creature(Creature::MiddayRestoreTag{}, props), mShadowCaster(ShadowCaster::MiddayRestoreTag{}) {
    // No ordinary Navi constructor, reset, animation init, motion, controller,
    // collision resource walk, state registration, effects or heap allocations.
    mNaviID = slot;
    mObjType = OBJTYPE_Navi;
    mCurrState = nullptr;
    mStateMachine = nullptr;
    mNaviShapeObject = nullptr;
    mCollInfo = nullptr;
    mKontroller = nullptr;
    mNaviCamera = nullptr;
    mControlCamera = nullptr;
    mPlateMgr = nullptr;
    mGoalItem = nullptr;
    mLookAtPosPtr = nullptr;
    mWallPlane = nullptr;
    mWallCollObj = nullptr;
    mDamageEfxA = mDamageEfxB = mDamageEfxC = nullptr;
    mBurnEffect = nullptr;
    mRippleEffect = nullptr;
    mSlimeEffect = nullptr;
    mNaviLightEfx = mNaviLightGlowEfx = mCursorTrailEfx = _780 = nullptr;
    mLoci = nullptr;
    mLociCount = 0;
    _AD0 = nullptr;
    // Animator base constructors leave these pointers uninitialized; shell
    // disposal/identity inspection must not mistake them for owned resources.
    for (PaniPikiAnimMgr* manager : {&mNaviAnimMgr, &mPcPikiAnimMgr}) {
        manager->mAnimSpeed = 0.0f;
        for (PaniPikiAnimator* animator : {&manager->mUpperAnimator, &manager->mLowerAnimator}) {
            animator->mMgr = nullptr;
            animator->mContext = nullptr;
        }
    }
    // Bind only freshly constructed inline storage; the restore-only helper
    // never consults AIPerf, invalidates searches or invokes reference callbacks.
    mSearchBuffer.bindMiddayRestoreStorage(mNaviSearchData, 6);
}
#endif
