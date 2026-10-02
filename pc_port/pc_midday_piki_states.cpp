#if defined(PIKI_PC_PORT)
#include "pc_midday_actor_states.h"
#include "PikiState.h"
using namespace pc_midday;

// Access only declared runtime members: no layout copies, padding, vtables or
// transition callbacks. The scene transaction validates all actors before Apply.
struct PcMiddayPikiStateAccess {
 static bool payload(PikiState& base, int id, ActorArchive& ar) {
  switch (id) {
  case PIKISTATE_Normal: {
   auto& s = static_cast<PikiNormalState&>(base);
   return true;
  }
  case PIKISTATE_Grow: {
   auto& s = static_cast<PikiGrowState&>(base);
   return true;
  }
  case PIKISTATE_Bury: {
   auto& s = static_cast<PikiBuryState&>(base);
   return true;
  }
  case PIKISTATE_Nukare: {
   auto& s = static_cast<PikiNukareState&>(base);
   return true;
  }
  case PIKISTATE_NukareWait: {
   auto& s = static_cast<PikiNukareWaitState&>(base);
   return true;
  }
  case PIKISTATE_AutoNuki: {
   auto& s = static_cast<PikiAutoNukiState&>(base);
   if (!ar.field("mToCreateEffect", s.mToCreateEffect)) return false;
   return true;
  }
  case PIKISTATE_Dying: {
   auto& s = static_cast<PikiDyingState&>(base);
   return true;
  }
  case PIKISTATE_Dead: {
   auto& s = static_cast<PikiDeadState&>(base);
   return true;
  }
  case PIKISTATE_Swallowed: {
   auto& s = static_cast<PikiSwallowedState&>(base);
   return true;
  }
  case PIKISTATE_Fired: {
   auto& s = static_cast<PikiFiredState&>(base);
   if (!ar.field("mSurvivalTimer", s.mSurvivalTimer)) return false;
   if (!ar.field("mChangeDirectionTimer", s.mChangeDirectionTimer)) return false;
   if (!ar.field("mMoveDirection", s.mMoveDirection)) return false;
   if (!ar.field("mSpeedRatio", s.mSpeedRatio)) return false;
   return true;
  }
  case PIKISTATE_Bubble: {
   auto& s = static_cast<PikiBubbleState&>(base);
   if (!ar.field("mSurvivalTimer", s.mSurvivalTimer)) return false;
   if (!ar.field("mChangeDirectionTimer", s.mChangeDirectionTimer)) return false;
   if (!ar.field("mMoveDirection", s.mMoveDirection)) return false;
   if (!ar.field("mSpeedRatio", s.mSpeedRatio)) return false;
   return true;
  }
  case PIKISTATE_GoHang: {
   auto& s = static_cast<PikiGoHangState&>(base);
   return true;
  }
  case PIKISTATE_Hanged: {
   auto& s = static_cast<PikiHangedState&>(base);
   return true;
  }
  case PIKISTATE_WaterHanged: {
   auto& s = static_cast<PikiWaterHangedState&>(base);
   return true;
  }
  case PIKISTATE_Flying: {
   auto& s = static_cast<PikiFlyingState&>(base);
   if (!ar.field("mGlideTimer", s.mGlideTimer)) return false;
   if (!ar.field("mIsFlowerGliding", s.mIsFlowerGliding)) return false;
   if (!ar.field("mHasBounced", s.mHasBounced)) return false;
   if (!ar.field("mHorizontalDirection", s.mHorizontalDirection)) return false;
   if (!ar.field("mInitialHorizontalSpeed", s.mInitialHorizontalSpeed)) return false;
   if (!ar.field("mTargetHorizontalSpeed", s.mTargetHorizontalSpeed)) return false;
   if (!ar.field("mGroundTouchFrames", s.mGroundTouchFrames)) return false;
   if (!ar.field("sparkle.position", s.mSparkleEffect.mPosition) || !ar.ref("sparkle.emitter", RefKind::ParticleGenerator, s.mSparkleEffect.mPtclGen)) return false;
   return true;
  }
  case PIKISTATE_Emit: {
   auto& s = static_cast<PikiEmitState&>(base);
   if (!ar.field("mHasLanded", s.mHasLanded)) return false;
   return true;
  }
  case PIKISTATE_Cliff: {
   auto& s = static_cast<PikiCliffState&>(base);
   if (!ar.field("mState", s.mState)) return false;
   if (!ar.field("mLoopCounter", s.mLoopCounter)) return false;
   if (!ar.field("mCliffHangType", s.mCliffHangType)) return false;
   if (!ar.field("mInitialVelocity", s.mInitialVelocity)) return false;
   if (!ar.field("mInitialFaceDir", s.mInitialFaceDir)) return false;
   return true;
  }
  case PIKISTATE_Fall: {
   auto& s = static_cast<PikiFallState&>(base);
   if (!ar.field("mState", s.mState)) return false;
   return true;
  }
  case PIKISTATE_Wave: {
   auto& s = static_cast<PikiWaveState&>(base);
   return true;
  }
  case PIKISTATE_GrowUp: {
   auto& s = static_cast<PikiGrowupState&>(base);
   return true;
  }
  case PIKISTATE_Push: {
   auto& s = static_cast<PikiPushState&>(base);
   if (!ar.field("mIsFinishing", s.mIsFinishing)) return false;
   return true;
  }
  case PIKISTATE_PushPiki: {
   auto& s = static_cast<PikiPushPikiState&>(base);
   if (!ar.field("mCollisionFrameCount", s.mCollisionFrameCount)) return false;
   if (!ar.field("mIsFinishing", s.mIsFinishing)) return false;
   return true;
  }
  case PIKISTATE_Flick: {
   auto& s = static_cast<PikiFlickState&>(base);
   if (!ar.field("mState", s.mState)) return false;
   if (!ar.field("mGetUpTimer", s.mGetUpTimer)) return false;
   if (!ar.field("mInitialAngle", s.mInitialAngle)) return false;
   if (!ar.field("mRotationDelta", s.mRotationDelta)) return false;
   if (!ar.field("mStrength", s.mStrength)) return false;
   return true;
  }
  case PIKISTATE_Kinoko: {
   auto& s = static_cast<PikiKinokoState&>(base);
   if (!ar.field("mWalkTimer", s.mWalkTimer)) return false;
   if (!ar.field("mTargetDir", s.mTargetDir)) return false;
   if (!ar.field("mState", s.mState)) return false;
   if (!ar.ref("mTarget", RefKind::Creature, s.mTarget)) return false;
   return true;
  }
  case PIKISTATE_Drown: {
   auto& s = static_cast<PikiDrownState&>(base);
   if (!ar.field("mState", s.mState)) return false;
   if (!ar.field("mStruggleDuration", s.mStruggleDuration)) return false;
   if (!ar.field("mOutOfWaterFrames", s.mOutOfWaterFrames)) return false;
   if (!ar.field("mEscapeVelocity", s.mEscapeVelocity)) return false;
   if (!ar.field("mIsBeingWhistled", s.mIsBeingWhistled)) return false;
   return true;
  }
  case PIKISTATE_Flown: {
   auto& s = static_cast<PikiFlownState&>(base);
   if (!ar.field("mKnockdownTimer", s.mKnockdownTimer)) return false;
   if (!ar.field("mInitialAngle", s.mInitialAngle)) return false;
   if (!ar.field("mRotationDelta", s.mRotationDelta)) return false;
   if (!ar.field("mFlickIntensity", s.mFlickIntensity)) return false;
   if (!ar.field("mState", s.mState)) return false;
   return true;
  }
  case PIKISTATE_LookAt: {
   auto& s = static_cast<PikiLookAtState&>(base);
   if (!ar.field("mTimer", s.mTimer)) return false;
   if (!ar.field("mState", s.mState)) return false;
   if (!ar.field("mRotationStep", s.mRotationStep)) return false;
   return true;
  }
  case PIKISTATE_Bullet: {
   auto& s = static_cast<PikiBulletState&>(base);
   if (!ar.field("mDistanceTravelled", s.mDistanceTravelled)) return false;
   return true;
  }
  case PIKISTATE_Absorb: {
   auto& s = static_cast<PikiAbsorbState&>(base);
   if (!ar.field("mState", s.mState)) return false;
   if (!ar.field("mHasAbsorbedNectar", s.mHasAbsorbedNectar)) return false;
   if (!ar.ref("mNectar", RefKind::Creature, s.mNectar)) return false;
   return true;
  }
  case PIKISTATE_KinokoChange: {
   auto& s = static_cast<PikiKinokoChangeState&>(base);
   if (!ar.field("mDoBecomeKinoko", s.mDoBecomeKinoko)) return false;
   return true;
  }
  case PIKISTATE_FallMeck: {
   auto& s = static_cast<PikiFallMeckState&>(base);
   return true;
  }
  case PIKISTATE_Emotion: {
   auto& s = static_cast<PikiEmotionState&>(base);
   if (!ar.field("mGazePosition", s.mGazePosition)) return false;
   if (!ar.field("mGazeFlag", s.mGazeFlag)) return false;
   if (!ar.field("mCheerCount", s.mCheerCount)) return false;
   if (!ar.field("mTimer", s.mTimer)) return false;
   return true;
  }
  case PIKISTATE_Pressed: {
   auto& s = static_cast<PikiPressedState&>(base);
   if (!ar.field("mStunTimer", s.mStunTimer)) return false;
   if (!ar.field("mIsInvincible", s.mIsInvincible)) return false;
   return true;
  }
  case PIKISTATE_DenkiDying: {
   auto& s = static_cast<PikiDenkiDyingState&>(base);
   if (!ar.field("mWaitTime", s.mWaitTime)) return false;
   return true;
  }
  case PIKISTATE_Panic: {
   auto& s = static_cast<PikiPanicState&>(base);
   if (!ar.field("mSurvivalTimer", s.mSurvivalTimer)) return false;
   if (!ar.field("mChangeDirectionTimer", s.mChangeDirectionTimer)) return false;
   if (!ar.field("mMoveDirection", s.mMoveDirection)) return false;
   if (!ar.field("mSpeedRatio", s.mSpeedRatio)) return false;
   if (!ar.field("mAstonish", s.mAstonish)) return false;
   if (!ar.field("mAstonishSubState", s.mAstonishSubState)) return false;
   return true;
  }
  default: return ar.fail("unregistered Piki state ID");
  }
 }
};
namespace pc_midday {
namespace {
AState<Piki>* stateByID(PikiStateMachine& fsm, int id) {
 if (id < 0 || id >= PIKISTATE_Count || id == PIKISTATE_UNUSED32 || id == PIKISTATE_Unk34) return nullptr;
 if (!fsm.mStates || !fsm.mStateIDs || fsm.mStateCount != PIKISTATE_Count - 2 || fsm.mStateLimit != PIKISTATE_Count) return nullptr;
 AState<Piki>* result = nullptr;
 for (int i=0; i<fsm.mStateCount; ++i) {
  if (!fsm.mStates[i] || fsm.mStates[i]->getID()!=fsm.mStateIDs[i]) return nullptr;
  if (fsm.mStateIDs[i]==id) { if (result) return nullptr; result=fsm.mStates[i]; }
 }
 return result;
}
}
bool piki_states(Piki& piki, ActorArchive& outer) {
 PrefixArchive ar(outer,"piki.fsm");
 if (!piki.mFSM) return ar.fail("Piki FSM not allocated");
 int current=-1, last=-1;
 if (ar.mode()==Mode::Capture) {
  // Prove membership before dereferencing the current pointer.
  for (int i=0;i<piki.mFSM->mStateCount && i<PIKISTATE_Count;++i)
   if (piki.mFSM->mStates && piki.mFSM->mStates[i]==piki.mCurrentState && piki.mCurrentState) current=piki.mCurrentState->getID();
  last=piki.mFSM->mLastStateID;
 }
 if (!ar.scalar("current",ScalarKind::S32,&current) || !ar.scalar("last",ScalarKind::S32,&last)) return false;
 auto* selected=stateByID(*piki.mFSM,current);
 if (!selected || (last!=-1 && !stateByID(*piki.mFSM,last))) return ar.fail("invalid Piki current/last state");
 if (!PcMiddayPikiStateAccess::payload(*static_cast<PikiState*>(selected),current,ar)) return false;
 if (ar.mode()==Mode::Apply) { piki.mCurrentState=selected; piki.mFSM->mLastStateID=last; }
 return true;
}
}
#endif
