#include "pc_midday_actor_archive.h"
namespace pc_midday {
namespace {
void scalar(std::vector<FieldSchema>& out,const std::string& key,ScalarKind kind){out.push_back(FieldSchema::value(key.c_str(),kind));}
void ref(std::vector<FieldSchema>& out,const std::string& key,RefKind kind,bool nullable,const char* type,ReferenceOwnership owner=ReferenceOwnership::AnyLive){out.push_back(FieldSchema::ref(key.c_str(),kind,nullable,type,owner));}
void vector(std::vector<FieldSchema>& out,const std::string& key){for(const char* c:{".x",".y",".z"})scalar(out,key+c,ScalarKind::F32);}

}
bool navi_runtime_schema(const ActorFields& fields,std::vector<FieldSchema>& out,std::string& error) {
    const std::string p="navi.runtime.";int capacity=0,color=-1;
    if(!actor_i32(fields,(p+"plate.capacity").c_str(),capacity,error)||capacity<1||capacity>4096||!actor_i32(fields,(p+"mPcPikiAnimColor").c_str(),color,error)||color< -1||color>2){error="invalid Navi runtime topology";return false;}
    int used=0,total=0;
    if(fields.count(p+"plate.mUsedSlotCount")&&(!actor_i32(fields,(p+"plate.mUsedSlotCount").c_str(),used,error)||used<0||used>capacity)){error="invalid CPlate used count";return false;}
    if(fields.count(p+"plate.mTotalSlotCount")&&(!actor_i32(fields,(p+"plate.mTotalSlotCount").c_str(),total,error)||total<0||total>capacity)){error="invalid CPlate total count";return false;}
    u32 plateCount=0;
    if(fields.count(p+"plate.mPlatePikiCount")&&(!actor_u32(fields,(p+"plate.mPlatePikiCount").c_str(),plateCount,error)||plateCount>static_cast<u32>(capacity))){error="invalid CPlate Piki count";return false;}
    // refresh can reduce used slots before other counters settle. Bound the
    // counters, but do not invent equality with the legacy plate/total counts.
    int happaSum=0;
    for(int i=0;i<3;++i) {
        const auto key=p+"plate.happa."+std::to_string(i);int count=0;
        if(fields.count(key)&&(!actor_i32(fields,key.c_str(),count,error)||count<0||count>capacity)){error="invalid CPlate happa count";return false;}
        happaSum+=count;
    }
    if(happaSum>capacity){error="CPlate happa population exceeds capacity";return false;}
    int owner=0;if(fields.count(p+"mNaviID")&&(!actor_i32(fields,(p+"mNaviID").c_str(),owner,error)||owner<0||owner>1)){error="invalid Navi slot";return false;}
    scalar(out,p+"mIsRidingUfo",ScalarKind::Bool);
    scalar(out,p+"mIsPellet",ScalarKind::Bool);
    scalar(out,p+"mLookTimer",ScalarKind::U8);
    scalar(out,p+"mHeadYawOffsetRel",ScalarKind::F32);
    scalar(out,p+"mHeadPitchOffset",ScalarKind::F32);
    scalar(out,p+"mCollidedWorkObjTimer",ScalarKind::F32);
    scalar(out,p+"mIsInWater",ScalarKind::Bool);
    scalar(out,p+"mPluckCursorVisibilityTimer",ScalarKind::S32);
    scalar(out,p+"mIsCursorVisible",ScalarKind::S32);
    scalar(out,p+"mMotionSpeed",ScalarKind::F32);
    scalar(out,p+"mIsDayEnd",ScalarKind::S32);
    scalar(out,p+"mCursorNaviDist",ScalarKind::F32);
    scalar(out,p+"mPendingLowerMotionId",ScalarKind::S32);
    scalar(out,p+"mLowerMotionCooldown",ScalarKind::S32);
    scalar(out,p+"mFlickIntensity",ScalarKind::F32);
    scalar(out,p+"mPlateYaw",ScalarKind::F32);
    scalar(out,p+"mPlateDirLocked",ScalarKind::Bool);
    scalar(out,p+"mRearrangePending",ScalarKind::Bool);
    scalar(out,p+"mFormationBand",ScalarKind::S32);
    scalar(out,p+"mFormationBandStableTimer",ScalarKind::S32);
    scalar(out,p+"mIsCStickNeutral",ScalarKind::Bool);
    scalar(out,p+"mSeedCollectionCount",ScalarKind::U32);
    scalar(out,p+"mCurrKeyCount",ScalarKind::S32);
    scalar(out,p+"mNeutralTime",ScalarKind::F32);
    scalar(out,p+"mAiTickTimer",ScalarKind::F32);
    scalar(out,p+"mAiHitWall",ScalarKind::S32);
    scalar(out,p+"mWalkAnimPrevDir",ScalarKind::F32);
    scalar(out,p+"mPreBlendLowerMotionID",ScalarKind::S32);
    scalar(out,p+"mIsPlucking",ScalarKind::Bool);
    scalar(out,p+"mFastPluckKeyTaps",ScalarKind::U8);
    scalar(out,p+"mNoPluckTimer",ScalarKind::U8);
    scalar(out,p+"mThrowHoldTime",ScalarKind::F32);
    scalar(out,p+"mThrowDistance",ScalarKind::F32);
    scalar(out,p+"mThrowHeight",ScalarKind::F32);
    scalar(out,p+"mFormationPriMode",ScalarKind::S32);
    scalar(out,p+"mPressedTimer",ScalarKind::F32);
    scalar(out,p+"mForcePikiDistCheck",ScalarKind::Bool);
    scalar(out,p+"mNaviID",ScalarKind::S32);
    scalar(out,p+"mWhistleTimer",ScalarKind::F32);
    scalar(out,p+"mWhistleCircleMode",ScalarKind::S32);
    scalar(out,p+"mWhistleRadiusFrac",ScalarKind::F32);
    scalar(out,p+"mWhistleCircleRadius",ScalarKind::F32);
    scalar(out,p+"_AC4",ScalarKind::F32);
    scalar(out,p+"_AD8",ScalarKind::F32);
    vector(out,p+"mPcPikiLeafTip");
    vector(out,p+"mCursorPosition");
    vector(out,p+"mCursorTargetPosition");
    vector(out,p+"mCursorWorldPos");
    vector(out,p+"mPrevMainStick");
    vector(out,p+"mMainStick");
    vector(out,p+"mPrevCStick");
    vector(out,p+"mCStick");
    vector(out,p+"mCursorTrailLastPos");
    vector(out,p+"mNaviLightPosition");
    vector(out,p+"mDayEndPosition");
    vector(out,p+"mWalkAnimPrevPos");
    ref(out,p+"mDamageEfxA",RefKind::ParticleGenerator,true,"zen::particleGenerator",ReferenceOwnership::ActorSubobject);
    ref(out,p+"mDamageEfxB",RefKind::ParticleGenerator,true,"zen::particleGenerator",ReferenceOwnership::ActorSubobject);
    ref(out,p+"mDamageEfxC",RefKind::ParticleGenerator,true,"zen::particleGenerator",ReferenceOwnership::ActorSubobject);
    ref(out,p+"mKontroller",RefKind::Controller,false,"Kontroller");
    ref(out,p+"mNaviCamera",RefKind::Camera,false,"Camera");
    ref(out,p+"mControlCamera",RefKind::Camera,true,"Camera");
    ref(out,p+"mLookAtPosPtr",RefKind::Vector3,true,"Vector3f");
    ref(out,p+"mCollidedWorkObj",RefKind::Creature,true,"Creature");
    ref(out,p+"mSelectedShipPart",RefKind::Creature,true,"Pellet");
    ref(out,p+"mPcLockTarget",RefKind::Creature,true,"Creature");
    ref(out,p+"mGoalItem",RefKind::Creature,true,"GoalItem");
    ref(out,p+"mPikiToPluck",RefKind::Creature,true,"Piki");
    ref(out,p+"mSproutToPluck",RefKind::Creature,true,"PikiHeadItem");
    ref(out,p+"mNextThrowPiki",RefKind::Creature,true,"Piki");
    ref(out,p+"mPellet",RefKind::Creature,true,"Pellet");
    ref(out,p+"mBurnEffect",RefKind::Effect,true,"BurnEffect",ReferenceOwnership::ActorSubobject);
    ref(out,p+"mRippleEffect",RefKind::Effect,true,"RippleEffect",ReferenceOwnership::ActorSubobject);
    ref(out,p+"mSlimeEffect",RefKind::Effect,true,"SlimeEffect",ReferenceOwnership::ActorSubobject);
    ref(out,p+"mNaviLightEfx",RefKind::Effect,true,"PermanentEffect",ReferenceOwnership::ActorSubobject);
    ref(out,p+"mNaviLightGlowEfx",RefKind::Effect,true,"PermanentEffect",ReferenceOwnership::ActorSubobject);
    ref(out,p+"mCursorTrailEfx",RefKind::Effect,true,"PermanentEffect",ReferenceOwnership::ActorSubobject);
    ref(out,p+"mWallPlane",RefKind::Plane,true,"Plane");
    ref(out,p+"mWallCollObj",RefKind::DynCollObject,true,"DynCollObject");
    ref(out,p+"mNaviShapeObject",RefKind::Shape,false,"PikiShapeObject",ReferenceOwnership::Content);
    ref(out,p+"attackTarget",RefKind::Creature,true,"Creature");
    for(const char* key:{"distance","remaining","minimum","reset"})scalar(out,p+"odometer."+key,ScalarKind::F32);
    for(int i=0;i<32;++i)vector(out,p+"whistleFx."+std::to_string(i));
    scalar(out,p+"animationSpeed",ScalarKind::F32);animation_schema(p+"upperAnimation.",out);animation_schema(p+"lowerAnimation.",out);
    scalar(out,p+"mPcPikiAnimColor",ScalarKind::S32);
    if(color>=0){scalar(out,p+"pcAnimationSpeed",ScalarKind::F32);animation_schema(p+"pcUpperAnimation.",out);animation_schema(p+"pcLowerAnimation.",out);}
    scalar(out,p+"plate.capacity",ScalarKind::S32);
    vector(out,p+"plate.mPlateOffset");
    scalar(out,p+"plate.mPlateLength",ScalarKind::F32);
    scalar(out,p+"plate.mPlateSize",ScalarKind::F32);
    scalar(out,p+"plate.mInnerRadius",ScalarKind::F32);
    scalar(out,p+"plate.mTotalSlotCount",ScalarKind::S32);
    scalar(out,p+"plate.mPlatePikiCount",ScalarKind::U32);
    scalar(out,p+"plate.mUsedSlotCount",ScalarKind::S32);
    vector(out,p+"plate.mOriginPosition");
    vector(out,p+"plate.mPlateCenter");
    vector(out,p+"plate.mCurrentVelocity");
    scalar(out,p+"plate.mDirectionAngle",ScalarKind::F32);
    scalar(out,p+"plate._C8",ScalarKind::Bool);
    scalar(out,p+"plate.mIsNeutral",ScalarKind::Bool);
    for(const char* key:{"startOffset","lengthLimit","maxPosSize"})scalar(out,p+"plate.params."+key,ScalarKind::F32);
    for(int i=0;i<3;++i)scalar(out,p+"plate.happa."+std::to_string(i),ScalarKind::S32);
    for(int i=0;i<capacity;++i) {
        auto slot=p+"plate.slot."+std::to_string(i)+".";
        vector(out,slot+"position");vector(out,slot+"offset");ref(out,slot+"occupant",RefKind::Creature,i>=used,"Creature");ref(out,slot+"listener",RefKind::SlotListener,i>=used,"SlotChangeListner",ReferenceOwnership::ActorSubobject);
    }
    return true;
}
}
