#include "pc_midday_actor_archive.h"
#include "pc_midday_creature.h"
#include "ObjType.h"
#include <cstring>
#include <cmath>
namespace pc_midday {
namespace {
void scalar(std::vector<FieldSchema>& out,const std::string& key,ScalarKind kind) {out.push_back(FieldSchema::value(key.c_str(),kind));}
void ref(std::vector<FieldSchema>& out,const std::string& key,RefKind kind,bool nullable,const char* type,ReferenceOwnership owner=ReferenceOwnership::AnyLive) {out.push_back(FieldSchema::ref(key.c_str(),kind,nullable,type,owner));}
void vector(std::vector<FieldSchema>& out,const std::string& key) {for(const char* v:{".x",".y",".z"})scalar(out,key+v,ScalarKind::F32);}
}
bool navi_schema(const ActorFields& fields,std::vector<FieldSchema>& out,std::string& error) {
    int current=-1,last=-1;
    if(!actor_i32(fields,"current",current,error)||!actor_i32(fields,"last",last,error))return false;
    if(current<0||current>=38||last< -1||last>=38){error="invalid Navi discriminator";return false;}
    auto ageIt=fields.find("whistle.pressAge");
    if(ageIt!=fields.end()) {
        double age=0;auto bits=ageIt->second.bits;std::memcpy(&age,&bits,8);
        if(ageIt->second.scalar!=ScalarKind::F64 || !std::isfinite(age) || (age<0 && age!=-1) || age>0.35) {error="invalid whistle age";return false;}
    }
    struct Bound {int state;const char* key;u64 maximum;};
    for(const auto& bound: {Bound{3,"state.mWhistleAnimPhase",2},Bound{8,"state.mFlickState",3},Bound{13,"state.mState",3},Bound{19,"state.mBuryState",3},Bound{20,"state.mGeyserState",4},Bound{23,"state.mStartPhase",2},Bound{28,"state.mAttackPhase",2}}) {
        auto it=fields.find(bound.key);
        if(current==bound.state && it!=fields.end() && it->second.bits>bound.maximum){error="invalid Navi subphase";return false;}
    }
    out.clear();scalar(out,"current",ScalarKind::S32);scalar(out,"last",ScalarKind::S32);
    scalar(out,"whistle.pressAge",ScalarKind::F64);scalar(out,"whistle.recallWorkers",ScalarKind::Bool);
    scalar(out,"demon.policy.phase",ScalarKind::S32);scalar(out,"demon.policy.generation",ScalarKind::U64);
    scalar(out,"demon.policy.pendingDamage",ScalarKind::F32);scalar(out,"demon.policy.recovery",ScalarKind::F32);
    ref(out,"demon.captain",RefKind::Creature,current!=36,"Navi",ReferenceOwnership::Self);scalar(out,"demon.generation",ScalarKind::U64);scalar(out,"demon.serial",ScalarKind::U64);
    scalar(out,"demon.retired",ScalarKind::Bool);scalar(out,"demon.frame",ScalarKind::U32);scalar(out,"demon.expected",ScalarKind::S32);
    scalar(out,"demon.listeners.count",ScalarKind::U32);
    u32 tokens=0;int phase=-1;
    if(!actor_u32(fields,"demon.listeners.count",tokens,error)||tokens>4096||!actor_i32(fields,"demon.policy.phase",phase,error)||phase<0||phase>4){error="invalid Demon state payload";return false;}
    if(current==36) {
        auto policyGeneration=fields.find("demon.policy.generation");
        auto generation=fields.find("demon.generation");
        if(policyGeneration!=fields.end() && generation!=fields.end() && (!policyGeneration->second.bits || policyGeneration->second.bits!=generation->second.bits)){error="Demon ownership generation mismatch";return false;}
    }
    if(current==36 && phase==0){error="active DemonDrop has idle policy";return false;}
    for(u32 i=0;i<tokens;++i) {
        auto p=std::string("demon.listeners.")+std::to_string(i);
        ref(out,p+".captain",RefKind::Creature,false,"Navi",ReferenceOwnership::Self);scalar(out,p+".generation",ScalarKind::U64);scalar(out,p+".serial",ScalarKind::U64);
    }
    ref(out,"escape.captain",RefKind::Creature,current!=37,"Navi",ReferenceOwnership::Self);
    switch(current) {
    case 0: // Walk
        ref(out,"state._10",RefKind::Creature,true,"Creature");
        scalar(out,"state._14",ScalarKind::F32);
        scalar(out,"state.mIsTouchingWall",ScalarKind::S32);
        scalar(out,"state._1C",ScalarKind::F32);
        break;
    case 1: // Throw
        scalar(out,"state.mHasThrownPiki",ScalarKind::Bool);
        scalar(out,"state._11",ScalarKind::Bool);
        scalar(out,"state.mQueuedThrowPress",ScalarKind::Bool);
        ref(out,"state.mTargetPiki",RefKind::Creature,true,"Piki");
        break;
    case 2: // ThrowWait
        ref(out,"state.mHeldThrowPiki",RefKind::Creature,true,"Piki");
        ref(out,"state.mPendingThrowPiki",RefKind::Creature,true,"Piki");
        scalar(out,"state.mThrowChargeLevel",ScalarKind::S32);
        scalar(out,"state.mIsHoldingThrowPiki",ScalarKind::Bool);
        scalar(out,"state._20",ScalarKind::U32);
        scalar(out,"state.mPendingThrowPikiTimeout",ScalarKind::F32);
        scalar(out,"state.mSortDelayTimer",ScalarKind::F32);
        break;
    case 3: // Gather
        scalar(out,"state.mNextWhistlePluckTime",ScalarKind::F32);
        scalar(out,"state.mWhistleAnimPhase",ScalarKind::U16);
        scalar(out,"state.mWhistleCallRadius",ScalarKind::F32);
        scalar(out,"state.mWhistleEffectsStopped",ScalarKind::Bool);
        break;
    case 4: // Release
        scalar(out,"state.mCanInterruptToGather",ScalarKind::Bool);
        break;
    case 5: // Nuku
        scalar(out,"state.mPullCountRemaining",ScalarKind::U16);
        scalar(out,"state._12",ScalarKind::Bool);
        scalar(out,"state.mExtractKeyReleased",ScalarKind::Bool);
        scalar(out,"state.mWantsNextPluck",ScalarKind::Bool);
        scalar(out,"state._15",ScalarKind::Bool);
        break;
    case 6: // NukuAdjust
        scalar(out,"state.mTargetFaceDirection",ScalarKind::F32);
        vector(out,"state.mApproachPosition");
        scalar(out,"state._20",ScalarKind::Bool);
        vector(out,"state.mLastPosition");
        break;
    case 7: // Pressed
        break;
    case 8: // Flick
        scalar(out,"state.mFlickState",ScalarKind::U16);
        scalar(out,"state.mGetupAnimationTimer",ScalarKind::F32);
        scalar(out,"state.mDirection",ScalarKind::F32);
        scalar(out,"state.mRandVariation",ScalarKind::F32);
        scalar(out,"state.mIntensity",ScalarKind::F32);
        break;
    case 9: // Funbari
        break;
    case 10: // Rope
        break;
    case 11: // RopeExit
        break;
    case 12: // Container
        scalar(out,"state.mContainerWinEvent",ScalarKind::S32);
        scalar(out,"state.mContainerWinCount",ScalarKind::S32);
        break;
    case 13: // Ufo
        scalar(out,"state.mState",ScalarKind::U16);
        scalar(out,"state.mRecoveryTimer",ScalarKind::U16);
        vector(out,"state.mLastPosition");
        scalar(out,"state.mPunchCooldownTimer",ScalarKind::S8);
        scalar(out,"state.mHasReachedUfo",ScalarKind::Bool);
        break;
    case 14: // UfoAccess
        scalar(out,"state.mHasShownUfoText",ScalarKind::Bool);
        break;
    case 15: // PartsAccess
        scalar(out,"state.mHasShownPartText",ScalarKind::Bool);
        break;
    case 16: // Pick
        break;
    case 17: // Idle
        scalar(out,"state.mStopBeingIdle",ScalarKind::Bool);
        break;
    case 18: // Stuck
        vector(out,"state.mPrevStickDir");
        scalar(out,"state.mIdleTimer",ScalarKind::F32);
        scalar(out,"state.mActionCount",ScalarKind::S32);
        break;
    case 19: // Bury
        vector(out,"state.mPreviousStickInput");
        scalar(out,"state.mBuryState",ScalarKind::U8);
        scalar(out,"state.mEscapeAttemptCounter",ScalarKind::U8);
        scalar(out,"state.mValidEscapeAttempts",ScalarKind::U8);
        scalar(out,"state.mEscapeTimer",ScalarKind::U8);
        break;
    case 20: // Geyzer
        scalar(out,"state.mGeyserState",ScalarKind::U16);
        scalar(out,"state.mGetupDelayTimer",ScalarKind::F32);
        scalar(out,"state.mPlayerDirection",ScalarKind::F32);
        scalar(out,"state.mSpinDelta",ScalarKind::F32);
        vector(out,"state.mLaunchTargetPos");
        scalar(out,"state.mRiseTargetHeight",ScalarKind::F32);
        scalar(out,"state.mHasAppliedLaunchVelocity",ScalarKind::Bool);
        break;
    case 21: // DemoWait
        vector(out,"state.mLookAtPos");
        break;
    case 22: // DemoInf
        break;
    case 23: // Starting
        scalar(out,"state.mStartDelayTimer",ScalarKind::F32);
        vector(out,"state.mWalkTargetPos");
        vector(out,"state.mLookAtTargetPos");
        scalar(out,"state.mStartPhase",ScalarKind::U16);
        scalar(out,"state.mIsStartAnimComplete",ScalarKind::Bool);
        vector(out,"state.mLastPosition");
        break;
    case 24: // Pellet
        scalar(out,"state.mIsFinished",ScalarKind::Bool);
        break;
    case 26: // Sow
        break;
    case 27: // Water
        break;
    case 28: // Attack
        scalar(out,"state.mAttackPhase",ScalarKind::U16);
        scalar(out,"state.mGatherRequested",ScalarKind::Bool);
        scalar(out,"state._14",ScalarKind::F32);
        scalar(out,"state._18",ScalarKind::F32);
        break;
    case 29: // Dead
        scalar(out,"state.mDowned",ScalarKind::Bool);
        break;
    case 30: // Push
        scalar(out,"state.mIsFinishing",ScalarKind::Bool);
        break;
    case 31: // PushPiki
        scalar(out,"state.mHasPushContact",ScalarKind::S32);
        break;
    case 32: // Lock
        break;
    case 33: // PikiZero
        scalar(out,"state.mGameOverCountdown",ScalarKind::U16);
        break;
    case 34: // Clear
        break;
    case 35: // IroIro
        break;
    case 25: {
        ref(out,"state.mNavi",RefKind::Creature,false,"Navi",ReferenceOwnership::Self);vector(out,"state.mStartPos");vector(out,"state.mGoalPos");
        scalar(out,"state.mGoalDistance",ScalarKind::F32);scalar(out,"state.mSunsetTimer",ScalarKind::F32);scalar(out,"state.mOpenedAccount",ScalarKind::Bool);
        scalar(out,"state.current",ScalarKind::S32);scalar(out,"state.last",ScalarKind::S32);
        int sub=-1,previous=-1;
        if(!actor_i32(fields,"state.current",sub,error)||!actor_i32(fields,"state.last",previous,error)||sub<0||sub>=5||previous< -1||previous>=5){error="invalid sunset discriminator";return false;}
        if(sub==0){scalar(out,"state.go.mStumbleLoopCount",ScalarKind::S32);scalar(out,"state.go.mIsStumbling",ScalarKind::Bool);}
        if(sub==2)scalar(out,"state.whistle.mWhistleLoopCount",ScalarKind::S32);
        break;
    }
    case 36: case 37: break;
    default:error="unknown Navi schema";return false;
    }
    int objectType=-1;
    if(!actor_i32(fields,"creature.objectType",objectType,error)||objectType!=OBJTYPE_Navi){error="Navi factory object type mismatch";return false;}
    std::vector<FieldSchema> common;
    if(!creature_schema(fields,common,error))return false;
    out.insert(out.end(),common.begin(),common.end());
    return navi_runtime_schema(fields,out,error);
}
bool validate_navi(const ActorBytes& bytes,const LogicalResolver& resolver,std::string& error) {
    ActorFields fields;std::vector<FieldSchema> schema;
    return decode_actor_fields(bytes,fields,error) && navi_schema(fields,schema,error) && validate_actor_fields(fields,schema,resolver,error);
}
}
