#include "pc_midday_actor_archive.h"
#include "pc_midday_creature.h"
#include "NaviState.h"
#include <cmath>
namespace pc_midday {
// Implemented beside the private PC-only state definitions.
bool demon_drop_state(Navi&, ActorArchive&);
bool navi_runtime_fields(Navi&,ActorArchive&);
bool demon_escape_state(Navi&, ActorArchive&);
namespace {
// Some engine scalars are initialized only on entry to an inner phase. Encode
// a canonical zero outside that phase rather than reading indeterminate memory.
template<class T> bool conditional(ActorArchive& a,const char* key,T& member,bool initialized) {
    T local{};
    if(a.mode()==Mode::Capture && initialized)local=member;
    if(!a.field(key,local))return false;
    if(a.mode()==Mode::Apply)member=local;
    return true;
}
template<class T> AState<T>* registered(StateMachine<T>* machine, int id) {
    if (!machine || !machine->mStates || id < 0) return nullptr;
    for (int i=0; i<machine->mStateCount; ++i)
        if (machine->mStates[i] && machine->mStates[i]->getID()==id) return machine->mStates[i];
    return nullptr;
}
bool sunset(NaviDemoSunsetState& s, ActorArchive& a) {
    if (!a.ref("mNavi",RefKind::Creature,s.mNavi) ||
        !a.field("mStartPos",s.mStartPos) || !a.field("mGoalPos",s.mGoalPos) ||
        !a.field("mGoalDistance",s.mGoalDistance) || !a.field("mSunsetTimer",s.mSunsetTimer) ||
        !a.field("mOpenedAccount",s.mOpenedAccount)) return false;
    int current=a.mode()==Mode::Capture && s.mCurrentState ? s.mCurrentState->getID() : -1;
    int last=a.mode()==Mode::Capture && s.mStateMachine ? s.mStateMachine->mLastStateID : -1;
    if (!a.scalar("current",ScalarKind::S32,&current) || !a.scalar("last",ScalarKind::S32,&last)) return false;
    auto* target=registered(s.mStateMachine,current);
    if (!target || (last!=-1 && !registered(s.mStateMachine,last))) return a.fail("invalid sunset state binding");
    if (current==NaviDemoSunsetState::DEMOSTATE_Go) {
        auto* go=dynamic_cast<NaviDemoSunsetState::GoState*>(target);
        if (!go) return a.fail("sunset Go type mismatch");
        if (!conditional(a,"go.mStumbleLoopCount",go->mStumbleLoopCount,a.mode()!=Mode::Capture || go->mIsStumbling) || !a.field("go.mIsStumbling",go->mIsStumbling)) return false;
    } else if (current==NaviDemoSunsetState::DEMOSTATE_Whistle) {
        auto* whistle=dynamic_cast<NaviDemoSunsetState::WhistleState*>(target);
        if (!whistle) return a.fail("sunset Whistle type mismatch");
        if (!a.field("whistle.mWhistleLoopCount",whistle->mWhistleLoopCount)) return false;
    } else if (current<NaviDemoSunsetState::DEMOSTATE_Go || current>=NaviDemoSunsetState::DEMOSTATE_Count)
        return a.fail("invalid sunset state ID");
    if (a.mode()==Mode::Apply) { s.setCurrState(target); s.mStateMachine->mLastStateID=last; }
    return true;
}
bool payload(Navi& n, AState<Navi>* state, int id, ActorArchive& a) {
    switch (id) {
    case NAVISTATE_Walk: {
        auto* s=dynamic_cast<NaviWalkState*>(state);
        if (!s) return a.fail("Navi Walk type mismatch");
        return a.ref("_10",RefKind::Creature,s->_10) &&
               a.field("_14",s->_14) &&
               a.field("mIsTouchingWall",s->mIsTouchingWall) &&
               a.field("_1C",s->_1C);
    }
    case NAVISTATE_Throw: {
        auto* s=dynamic_cast<NaviThrowState*>(state);
        if (!s) return a.fail("Navi Throw type mismatch");
        return a.field("mHasThrownPiki",s->mHasThrownPiki) &&
               a.field("_11",s->_11) &&
               a.field("mQueuedThrowPress",s->mQueuedThrowPress) &&
               a.ref("mTargetPiki",RefKind::Creature,s->mTargetPiki);
    }
    case NAVISTATE_ThrowWait: {
        auto* s=dynamic_cast<NaviThrowWaitState*>(state);
        if (!s) return a.fail("Navi ThrowWait type mismatch");
        return a.ref("mHeldThrowPiki",RefKind::Creature,s->mHeldThrowPiki) &&
               a.ref("mPendingThrowPiki",RefKind::Creature,s->mPendingThrowPiki) &&
               a.field("mThrowChargeLevel",s->mThrowChargeLevel) &&
               a.field("mIsHoldingThrowPiki",s->mIsHoldingThrowPiki) &&
               a.field("_20",s->_20) &&
               a.field("mPendingThrowPikiTimeout",s->mPendingThrowPikiTimeout) &&
               a.field("mSortDelayTimer",s->mSortDelayTimer);
    }
    case NAVISTATE_Gather: {
        auto* s=dynamic_cast<NaviGatherState*>(state);
        if (!s) return a.fail("Navi Gather type mismatch");
        return a.field("mNextWhistlePluckTime",s->mNextWhistlePluckTime) &&
               a.field("mWhistleAnimPhase",s->mWhistleAnimPhase) &&
               a.field("mWhistleCallRadius",s->mWhistleCallRadius) &&
               a.field("mWhistleEffectsStopped",s->mWhistleEffectsStopped);
    }
    case NAVISTATE_Release: {
        auto* s=dynamic_cast<NaviReleaseState*>(state);
        if (!s) return a.fail("Navi Release type mismatch");
        return a.field("mCanInterruptToGather",s->mCanInterruptToGather);
    }
    case NAVISTATE_Nuku: {
        auto* s=dynamic_cast<NaviNukuState*>(state);
        if (!s) return a.fail("Navi Nuku type mismatch");
        return a.field("mPullCountRemaining",s->mPullCountRemaining) &&
               a.field("_12",s->_12) &&
               a.field("mExtractKeyReleased",s->mExtractKeyReleased) &&
               a.field("mWantsNextPluck",s->mWantsNextPluck) &&
               a.field("_15",s->_15);
    }
    case NAVISTATE_NukuAdjust: {
        auto* s=dynamic_cast<NaviNukuAdjustState*>(state);
        if (!s) return a.fail("Navi NukuAdjust type mismatch");
        return a.field("mTargetFaceDirection",s->mTargetFaceDirection) &&
               a.field("mApproachPosition",s->mApproachPosition) &&
               a.field("_20",s->_20) &&
               a.field("mLastPosition",s->mLastPosition);
    }
    case NAVISTATE_Pressed: {
        auto* s=dynamic_cast<NaviPressedState*>(state);
        if (!s) return a.fail("Navi Pressed type mismatch");
        return true;
    }
    case NAVISTATE_Flick: {
        auto* s=dynamic_cast<NaviFlickState*>(state);
        if (!s) return a.fail("Navi Flick type mismatch");
        return a.field("mFlickState",s->mFlickState) &&
               conditional(a,"mGetupAnimationTimer",s->mGetupAnimationTimer,a.mode()!=Mode::Capture || s->mFlickState==2) &&
               a.field("mDirection",s->mDirection) &&
               a.field("mRandVariation",s->mRandVariation) &&
               a.field("mIntensity",s->mIntensity);
    }
    case NAVISTATE_Funbari: {
        auto* s=dynamic_cast<NaviFunbariState*>(state);
        if (!s) return a.fail("Navi Funbari type mismatch");
        return true;
    }
    case NAVISTATE_Rope: {
        auto* s=dynamic_cast<NaviRopeState*>(state);
        if (!s) return a.fail("Navi Rope type mismatch");
        return true;
    }
    case NAVISTATE_RopeExit: {
        auto* s=dynamic_cast<NaviRopeExitState*>(state);
        if (!s) return a.fail("Navi RopeExit type mismatch");
        return true;
    }
    case NAVISTATE_Container: {
        auto* s=dynamic_cast<NaviContainerState*>(state);
        if (!s) return a.fail("Navi Container type mismatch");
        return a.field("mContainerWinEvent",s->mContainerWinEvent) &&
               a.field("mContainerWinCount",s->mContainerWinCount);
    }
    case NAVISTATE_Ufo: {
        auto* s=dynamic_cast<NaviUfoState*>(state);
        if (!s) return a.fail("Navi Ufo type mismatch");
        return a.field("mState",s->mState) &&
               conditional(a,"mRecoveryTimer",s->mRecoveryTimer,a.mode()!=Mode::Capture || s->mState==2) &&
               a.field("mLastPosition",s->mLastPosition) &&
               a.field("mPunchCooldownTimer",s->mPunchCooldownTimer) &&
               a.field("mHasReachedUfo",s->mHasReachedUfo);
    }
    case NAVISTATE_UfoAccess: {
        auto* s=dynamic_cast<NaviUfoAccessState*>(state);
        if (!s) return a.fail("Navi UfoAccess type mismatch");
        return a.field("mHasShownUfoText",s->mHasShownUfoText);
    }
    case NAVISTATE_PartsAccess: {
        auto* s=dynamic_cast<NaviPartsAccessState*>(state);
        if (!s) return a.fail("Navi PartsAccess type mismatch");
        return a.field("mHasShownPartText",s->mHasShownPartText);
    }
    case NAVISTATE_Pick: {
        auto* s=dynamic_cast<NaviPickState*>(state);
        if (!s) return a.fail("Navi Pick type mismatch");
        return true;
    }
    case NAVISTATE_Idle: {
        auto* s=dynamic_cast<NaviIdleState*>(state);
        if (!s) return a.fail("Navi Idle type mismatch");
        return a.field("mStopBeingIdle",s->mStopBeingIdle);
    }
    case NAVISTATE_Stuck: {
        auto* s=dynamic_cast<NaviStuckState*>(state);
        if (!s) return a.fail("Navi Stuck type mismatch");
        return a.field("mPrevStickDir",s->mPrevStickDir) &&
               a.field("mIdleTimer",s->mIdleTimer) &&
               a.field("mActionCount",s->mActionCount);
    }
    case NAVISTATE_Bury: {
        auto* s=dynamic_cast<NaviBuryState*>(state);
        if (!s) return a.fail("Navi Bury type mismatch");
        return a.field("mPreviousStickInput",s->mPreviousStickInput) &&
               a.field("mBuryState",s->mBuryState) &&
               a.field("mEscapeAttemptCounter",s->mEscapeAttemptCounter) &&
               a.field("mValidEscapeAttempts",s->mValidEscapeAttempts) &&
               a.field("mEscapeTimer",s->mEscapeTimer);
    }
    case NAVISTATE_Geyzer: {
        auto* s=dynamic_cast<NaviGeyzerState*>(state);
        if (!s) return a.fail("Navi Geyzer type mismatch");
        return a.field("mGeyserState",s->mGeyserState) &&
               conditional(a,"mGetupDelayTimer",s->mGetupDelayTimer,a.mode()!=Mode::Capture || s->mGeyserState>=3) &&
               a.field("mPlayerDirection",s->mPlayerDirection) &&
               a.field("mSpinDelta",s->mSpinDelta) &&
               a.field("mLaunchTargetPos",s->mLaunchTargetPos) &&
               a.field("mRiseTargetHeight",s->mRiseTargetHeight) &&
               a.field("mHasAppliedLaunchVelocity",s->mHasAppliedLaunchVelocity);
    }
    case NAVISTATE_DemoWait: {
        auto* s=dynamic_cast<NaviDemoWaitState*>(state);
        if (!s) return a.fail("Navi DemoWait type mismatch");
        return a.field("mLookAtPos",s->mLookAtPos);
    }
    case NAVISTATE_DemoInf: {
        auto* s=dynamic_cast<NaviDemoInfState*>(state);
        if (!s) return a.fail("Navi DemoInf type mismatch");
        return true;
    }
    case NAVISTATE_Starting: {
        auto* s=dynamic_cast<NaviStartingState*>(state);
        if (!s) return a.fail("Navi Starting type mismatch");
        return a.field("mStartDelayTimer",s->mStartDelayTimer) &&
               a.field("mWalkTargetPos",s->mWalkTargetPos) &&
               a.field("mLookAtTargetPos",s->mLookAtTargetPos) &&
               a.field("mStartPhase",s->mStartPhase) &&
               conditional(a,"mIsStartAnimComplete",s->mIsStartAnimComplete,a.mode()!=Mode::Capture || s->mStartPhase==2) &&
               a.field("mLastPosition",s->mLastPosition);
    }
    case NAVISTATE_Pellet: {
        auto* s=dynamic_cast<NaviPelletState*>(state);
        if (!s) return a.fail("Navi Pellet type mismatch");
        return a.field("mIsFinished",s->mIsFinished);
    }
    case NAVISTATE_Sow: {
        auto* s=dynamic_cast<NaviSowState*>(state);
        if (!s) return a.fail("Navi Sow type mismatch");
        return true;
    }
    case NAVISTATE_Water: {
        auto* s=dynamic_cast<NaviWaterState*>(state);
        if (!s) return a.fail("Navi Water type mismatch");
        return true;
    }
    case NAVISTATE_Attack: {
        auto* s=dynamic_cast<NaviAttackState*>(state);
        if (!s) return a.fail("Navi Attack type mismatch");
        return a.field("mAttackPhase",s->mAttackPhase) &&
               a.field("mGatherRequested",s->mGatherRequested) &&
               a.field("_14",s->_14) &&
               a.field("_18",s->_18);
    }
    case NAVISTATE_Dead: {
        auto* s=dynamic_cast<NaviDeadState*>(state);
        if (!s) return a.fail("Navi Dead type mismatch");
        return a.field("mDowned",s->mDowned);
    }
    case NAVISTATE_Push: {
        auto* s=dynamic_cast<NaviPushState*>(state);
        if (!s) return a.fail("Navi Push type mismatch");
        return a.field("mIsFinishing",s->mIsFinishing);
    }
    case NAVISTATE_PushPiki: {
        auto* s=dynamic_cast<NaviPushPikiState*>(state);
        if (!s) return a.fail("Navi PushPiki type mismatch");
        return a.field("mHasPushContact",s->mHasPushContact);
    }
    case NAVISTATE_Lock: {
        auto* s=dynamic_cast<NaviLockState*>(state);
        if (!s) return a.fail("Navi Lock type mismatch");
        return true;
    }
    case NAVISTATE_PikiZero: {
        auto* s=dynamic_cast<NaviPikiZeroState*>(state);
        if (!s) return a.fail("Navi PikiZero type mismatch");
        // Other declared bytes are unused padding, never initialized by engine.
        return a.field("mGameOverCountdown",s->mGameOverCountdown);
    }
    case NAVISTATE_Clear: {
        auto* s=dynamic_cast<NaviClearState*>(state);
        if (!s) return a.fail("Navi Clear type mismatch");
        return true;
    }
    case NAVISTATE_IroIro: {
        auto* s=dynamic_cast<NaviIroIroState*>(state);
        if (!s) return a.fail("Navi IroIro type mismatch");
        return true;
    }
    case NAVISTATE_DemoSunset: {
        auto* s=dynamic_cast<NaviDemoSunsetState*>(state);
        return s ? sunset(*s,a) : a.fail("Navi sunset type mismatch");
    }
    case NAVISTATE_DemonDrop: return true; // initialized persistent payload below
    case NAVISTATE_DemonEscape: return true;
    default: return a.fail("unknown Navi state ID");
    }
}
}
bool navi_states(Navi& n, ActorArchive& a) {
    int current=a.mode()==Mode::Capture && n.getCurrState() ? n.getCurrState()->getID() : -1;
    int last=a.mode()==Mode::Capture && n.mStateMachine ? n.mStateMachine->mLastStateID : -1;
    if (!a.scalar("current",ScalarKind::S32,&current) || !a.scalar("last",ScalarKind::S32,&last)) return false;
    auto* target=registered(n.mStateMachine,current);
    if (!target || (last!=-1 && !registered(n.mStateMachine,last))) return a.fail("invalid Navi state binding");
    if (a.mode()==Mode::Capture && target!=n.getCurrState()) return a.fail("Navi current state is not registered instance");
    // Double-tap history persists outside Gather. Store elapsed age, never a
    // steady_clock epoch that belongs to the old process.
    auto* gather=dynamic_cast<NaviGatherState*>(registered(n.mStateMachine,NAVISTATE_Gather));
    if(!gather)return a.fail("persistent Gather state missing");
    double age=-1;
    if(a.mode()==Mode::Capture && gather->mTapState.lastPress>=0)
        age=a.clock_now()-gather->mTapState.lastPress;
    // Older presses cannot affect the 350ms detector; canonicalize expired history.
    if(a.mode()==Mode::Capture && age>0.35)age=-1;
    if(!a.scalar("whistle.pressAge",ScalarKind::F64,&age) || !std::isfinite(age) || (age<0 && age!=-1) || age>0.35 || !std::isfinite(a.clock_now()) || (age>=0 && a.clock_now()<age))
        return a.fail("invalid whistle clock age");
    if(!a.field("whistle.recallWorkers",gather->mTapState.recallWorkers))return false;
    if(a.mode()==Mode::Apply)gather->mTapState.lastPress=age<0 ? -1 : a.clock_now()-age;
    PrefixArchive drop(a,"demon"), escape(a,"escape");
    if (!demon_drop_state(n,drop) || !demon_escape_state(n,escape)) return false;
    PrefixArchive body(a,"state");
    if (!payload(n,target,current,body)) return false;
    if (a.mode()==Mode::Apply) { n.setCurrState(target); n.mStateMachine->mLastStateID=last; }
    return navi_runtime_fields(n,a);
}
}

namespace pc_midday {
bool capture_navi(Navi& n,LogicalResolver& resolver,double now,ActorBytes& output,std::string& error) {
    ActorFields fields;FieldArchive archive(Mode::Capture,fields,resolver,error,now);
    if(!creature_fields(n,archive)||!navi_states(n,archive)||!archive.finish())return false;
    std::vector<FieldSchema> schema;
    if(!navi_schema(fields,schema,error)||!validate_actor_fields(fields,schema,resolver,error))return false;
    return encode_actor_fields(fields,output,error);
}
bool allocate_navi_subobjects(Navi& n,const ActorBytes& bytes,LogicalResolver& resolver,double now,std::string& error) {
    if(!validate_navi(bytes,resolver,error))return false;
    ActorFields fields;if(!decode_actor_fields(bytes,fields,error))return false;
    FieldArchive archive(Mode::Apply,fields,resolver,error,now);
    return navi_state_subobjects(n,archive);
}
bool bind_navi(Navi& n,const ActorBytes& bytes,LogicalResolver& resolver,double now,std::string& error) {
    if(!validate_navi(bytes,resolver,error))return false;
    ActorFields fields;if(!decode_actor_fields(bytes,fields,error))return false;
    FieldArchive validate(Mode::Validate,fields,resolver,error,now);
    if(!creature_fields(n,validate)||!navi_states(n,validate)||!validate.finish())return false;
    FieldArchive apply(Mode::Apply,fields,resolver,error,now);
    return creature_fields(n,apply)&&navi_states(n,apply)&&apply.finish();
}
}
