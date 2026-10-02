#include "pc_midday_actor_archive.h"
namespace pc_midday {
bool piki_state_schema(int state,std::vector<FieldSchema>& out) { switch(state){
 case 0:
 return true;
 case 1:
 return true;
 case 2:
 return true;
 case 3:
 return true;
 case 4:
 return true;
 case 5:
 out.push_back(FieldSchema::value("mToCreateEffect",ScalarKind::Bool));
 return true;
 case 6:
 return true;
 case 7:
 return true;
 case 8:
 return true;
 case 9:
 out.push_back(FieldSchema::value("mSurvivalTimer",ScalarKind::F32));
 out.push_back(FieldSchema::value("mChangeDirectionTimer",ScalarKind::F32));
 out.push_back(FieldSchema::value("mMoveDirection",ScalarKind::F32));
 out.push_back(FieldSchema::value("mSpeedRatio",ScalarKind::F32));
 return true;
 case 10:
 out.push_back(FieldSchema::value("mSurvivalTimer",ScalarKind::F32));
 out.push_back(FieldSchema::value("mChangeDirectionTimer",ScalarKind::F32));
 out.push_back(FieldSchema::value("mMoveDirection",ScalarKind::F32));
 out.push_back(FieldSchema::value("mSpeedRatio",ScalarKind::F32));
 return true;
 case 11:
 return true;
 case 12:
 return true;
 case 13:
 return true;
 case 14:
 out.push_back(FieldSchema::value("mGlideTimer",ScalarKind::F32));
 out.push_back(FieldSchema::value("mIsFlowerGliding",ScalarKind::Bool));
 out.push_back(FieldSchema::value("mHasBounced",ScalarKind::Bool));
 out.push_back(FieldSchema::value("mHorizontalDirection.x",ScalarKind::F32));
 out.push_back(FieldSchema::value("mHorizontalDirection.y",ScalarKind::F32));
 out.push_back(FieldSchema::value("mHorizontalDirection.z",ScalarKind::F32));
 out.push_back(FieldSchema::value("mInitialHorizontalSpeed",ScalarKind::F32));
 out.push_back(FieldSchema::value("mTargetHorizontalSpeed",ScalarKind::F32));
 out.push_back(FieldSchema::value("mGroundTouchFrames",ScalarKind::S32));
 out.push_back(FieldSchema::value("sparkle.position.x",ScalarKind::F32));
 out.push_back(FieldSchema::value("sparkle.position.y",ScalarKind::F32));
 out.push_back(FieldSchema::value("sparkle.position.z",ScalarKind::F32));
 out.push_back(FieldSchema::ref("sparkle.emitter",RefKind::ParticleGenerator,true,"zen::particleGenerator",ReferenceOwnership::ActorSubobject));
 return true;
 case 15:
 out.push_back(FieldSchema::value("mHasLanded",ScalarKind::Bool));
 return true;
 case 16:
 out.push_back(FieldSchema::value("mState",ScalarKind::U32));
 out.push_back(FieldSchema::value("mLoopCounter",ScalarKind::S32));
 out.push_back(FieldSchema::value("mCliffHangType",ScalarKind::S32));
 out.push_back(FieldSchema::value("mInitialVelocity.x",ScalarKind::F32));
 out.push_back(FieldSchema::value("mInitialVelocity.y",ScalarKind::F32));
 out.push_back(FieldSchema::value("mInitialVelocity.z",ScalarKind::F32));
 out.push_back(FieldSchema::value("mInitialFaceDir",ScalarKind::F32));
 return true;
 case 17:
 out.push_back(FieldSchema::value("mState",ScalarKind::S32));
 return true;
 case 18:
 return true;
 case 19:
 return true;
 case 20:
 out.push_back(FieldSchema::value("mIsFinishing",ScalarKind::Bool));
 return true;
 case 21:
 out.push_back(FieldSchema::value("mCollisionFrameCount",ScalarKind::S32));
 out.push_back(FieldSchema::value("mIsFinishing",ScalarKind::Bool));
 return true;
 case 22:
 out.push_back(FieldSchema::value("mState",ScalarKind::U16));
 out.push_back(FieldSchema::value("mGetUpTimer",ScalarKind::F32));
 out.push_back(FieldSchema::value("mInitialAngle",ScalarKind::F32));
 out.push_back(FieldSchema::value("mRotationDelta",ScalarKind::F32));
 out.push_back(FieldSchema::value("mStrength",ScalarKind::F32));
 return true;
 case 23:
 out.push_back(FieldSchema::value("mWalkTimer",ScalarKind::F32));
 out.push_back(FieldSchema::value("mTargetDir.x",ScalarKind::F32));
 out.push_back(FieldSchema::value("mTargetDir.y",ScalarKind::F32));
 out.push_back(FieldSchema::value("mTargetDir.z",ScalarKind::F32));
 out.push_back(FieldSchema::value("mState",ScalarKind::S32));
 out.push_back(FieldSchema::ref("mTarget",RefKind::Creature,true,"Creature",ReferenceOwnership::AnyLive));
 return true;
 case 24:
 out.push_back(FieldSchema::value("mState",ScalarKind::U16));
 out.push_back(FieldSchema::value("mStruggleDuration",ScalarKind::U16));
 out.push_back(FieldSchema::value("mOutOfWaterFrames",ScalarKind::U16));
 out.push_back(FieldSchema::value("mEscapeVelocity.x",ScalarKind::F32));
 out.push_back(FieldSchema::value("mEscapeVelocity.y",ScalarKind::F32));
 out.push_back(FieldSchema::value("mEscapeVelocity.z",ScalarKind::F32));
 out.push_back(FieldSchema::value("mIsBeingWhistled",ScalarKind::Bool));
 return true;
 case 25:
 out.push_back(FieldSchema::value("mKnockdownTimer",ScalarKind::F32));
 out.push_back(FieldSchema::value("mInitialAngle",ScalarKind::F32));
 out.push_back(FieldSchema::value("mRotationDelta",ScalarKind::F32));
 out.push_back(FieldSchema::value("mFlickIntensity",ScalarKind::F32));
 out.push_back(FieldSchema::value("mState",ScalarKind::U16));
 return true;
 case 26:
 out.push_back(FieldSchema::value("mTimer",ScalarKind::F32));
 out.push_back(FieldSchema::value("mState",ScalarKind::S32));
 out.push_back(FieldSchema::value("mRotationStep",ScalarKind::F32));
 return true;
 case 27:
 out.push_back(FieldSchema::value("mDistanceTravelled",ScalarKind::F32));
 return true;
 case 28:
 out.push_back(FieldSchema::value("mState",ScalarKind::S32));
 out.push_back(FieldSchema::value("mHasAbsorbedNectar",ScalarKind::Bool));
 out.push_back(FieldSchema::ref("mNectar",RefKind::Creature,false,"Creature",ReferenceOwnership::AnyLive));
 return true;
 case 29:
 out.push_back(FieldSchema::value("mDoBecomeKinoko",ScalarKind::Bool));
 return true;
 case 30:
 return true;
 case 31:
 out.push_back(FieldSchema::value("mGazePosition.x",ScalarKind::F32));
 out.push_back(FieldSchema::value("mGazePosition.y",ScalarKind::F32));
 out.push_back(FieldSchema::value("mGazePosition.z",ScalarKind::F32));
 out.push_back(FieldSchema::value("mGazeFlag",ScalarKind::U8));
 out.push_back(FieldSchema::value("mCheerCount",ScalarKind::U8));
 out.push_back(FieldSchema::value("mTimer",ScalarKind::F32));
 return true;
 case 33:
 out.push_back(FieldSchema::value("mStunTimer",ScalarKind::F32));
 out.push_back(FieldSchema::value("mIsInvincible",ScalarKind::Bool));
 return true;
 case 35:
 out.push_back(FieldSchema::value("mWaitTime",ScalarKind::F32));
 return true;
 case 36:
 out.push_back(FieldSchema::value("mSurvivalTimer",ScalarKind::F32));
 out.push_back(FieldSchema::value("mChangeDirectionTimer",ScalarKind::F32));
 out.push_back(FieldSchema::value("mMoveDirection",ScalarKind::F32));
 out.push_back(FieldSchema::value("mSpeedRatio",ScalarKind::F32));
 out.push_back(FieldSchema::value("mAstonish",ScalarKind::Bool));
 out.push_back(FieldSchema::value("mAstonishSubState",ScalarKind::S32));
 return true;
 default:return false;
}}
bool piki_action_schema(int type,std::vector<FieldSchema>& out) { switch(type){
 case 1: // TopAction
 out.push_back(FieldSchema::value("mIsDebugDraw",ScalarKind::Bool));
 out.push_back(FieldSchema::value("mIsSuspended",ScalarKind::Bool));
 out.push_back(FieldSchema::value("mIsAnimating",ScalarKind::Bool));
 out.push_back(FieldSchema::value("_1C",ScalarKind::S32));
 out.push_back(FieldSchema::value("_24",ScalarKind::S32));
 out.push_back(FieldSchema::value("_28",ScalarKind::U32));
 out.push_back(FieldSchema::value("_2C",ScalarKind::F32));
 out.push_back(FieldSchema::ref("mListener",RefKind::AnimListener,false,"MotionListener",ReferenceOwnership::ActorSubobject));
 out.push_back(FieldSchema::ref("mTarget",RefKind::Creature,true,"Creature",ReferenceOwnership::AnyLive));
 return true;
 case 2: // ActAdjust
 out.push_back(FieldSchema::value("mAdjustDistance",ScalarKind::F32));
 out.push_back(FieldSchema::value("mAdjustTimeLimit",ScalarKind::S32));
 out.push_back(FieldSchema::value("mTargetPosition.x",ScalarKind::F32));
 out.push_back(FieldSchema::value("mTargetPosition.y",ScalarKind::F32));
 out.push_back(FieldSchema::value("mTargetPosition.z",ScalarKind::F32));
 out.push_back(FieldSchema::value("mAdjustTimer",ScalarKind::F32));
 out.push_back(FieldSchema::value("mTurnSpeed",ScalarKind::F32));
 out.push_back(FieldSchema::value("mVelocity.x",ScalarKind::F32));
 out.push_back(FieldSchema::value("mVelocity.y",ScalarKind::F32));
 out.push_back(FieldSchema::value("mVelocity.z",ScalarKind::F32));
 out.push_back(FieldSchema::value("mForceFail",ScalarKind::Bool));
 return true;
 case 3: // ActAttack
 out.push_back(FieldSchema::value("mHasLost",ScalarKind::Bool));
 out.push_back(FieldSchema::value("mIsAttackFinished",ScalarKind::Bool));
 out.push_back(FieldSchema::value("mIsCriticalHit",ScalarKind::Bool));
 out.push_back(FieldSchema::value("mTargetIsPlayer",ScalarKind::Bool));
 out.push_back(FieldSchema::ref("mTargetObjectPool",RefKind::Traversable,true,"Traversable",ReferenceOwnership::AnyLive));
 out.push_back(FieldSchema::ref("mOther",RefKind::Creature,true,"Creature",ReferenceOwnership::AnyLive,"",ReferenceStrength::StrongCreature));
 out.push_back(FieldSchema::ref("mPlayerObject",RefKind::Creature,true,"Creature",ReferenceOwnership::AnyLive));
 return true;
 case 4: // ActBoMake
 out.push_back(FieldSchema::value("mState",ScalarKind::U16));
 out.push_back(FieldSchema::ref("mBuildObject",RefKind::Creature,true,"Creature",ReferenceOwnership::AnyLive));
 return true;
 case 5: // ActBoreListen
 return true;
 case 6: // ActBoreOneshot
 out.push_back(FieldSchema::value("mIsAnimFinished",ScalarKind::Bool));
 return true;
 case 7: // ActBoreRest
 out.push_back(FieldSchema::value("mIsFinished",ScalarKind::Bool));
 out.push_back(FieldSchema::value("mRestState",ScalarKind::S32));
 out.push_back(FieldSchema::value("mRestTimer",ScalarKind::F32));
 out.push_back(FieldSchema::value("mIsAnimFinished",ScalarKind::Bool));
 out.push_back(FieldSchema::value("mForceComplete",ScalarKind::Bool));
 return true;
 case 8: // ActBoreSelect
 out.push_back(FieldSchema::value("mActionTimer",ScalarKind::F32));
 out.push_back(FieldSchema::value("mIsTimerActive",ScalarKind::Bool));
 out.push_back(FieldSchema::value("mIsChildActionActive",ScalarKind::Bool));
 out.push_back(FieldSchema::value("mStop",ScalarKind::Bool));
 out.push_back(FieldSchema::value("_1C",ScalarKind::F32));
 return true;
 case 9: // ActBoreTalk
 out.push_back(FieldSchema::value("mIsLookHandledElsewhere",ScalarKind::S32));
 out.push_back(FieldSchema::value("mTalkTimer",ScalarKind::F32));
 out.push_back(FieldSchema::value("mIsAnimFinished",ScalarKind::Bool));
 out.push_back(FieldSchema::ref("mTarget",RefKind::Creature,true,"Creature",ReferenceOwnership::AnyLive));
 return true;
 case 10: // ActBou
 out.push_back(FieldSchema::value("mState",ScalarKind::U16));
 out.push_back(FieldSchema::value("mTimeoutCounter",ScalarKind::S16));
 out.push_back(FieldSchema::value("mClimbDirection.x",ScalarKind::F32));
 out.push_back(FieldSchema::value("mClimbDirection.y",ScalarKind::F32));
 out.push_back(FieldSchema::value("mClimbDirection.z",ScalarKind::F32));
 out.push_back(FieldSchema::value("mLastPosition.x",ScalarKind::F32));
 out.push_back(FieldSchema::value("mLastPosition.y",ScalarKind::F32));
 out.push_back(FieldSchema::value("mLastPosition.z",ScalarKind::F32));
 out.push_back(FieldSchema::ref("mTargetStick",RefKind::Creature,true,"Creature",ReferenceOwnership::AnyLive));
 return true;
 case 11: // ActBreakWall
 out.push_back(FieldSchema::value("mState",ScalarKind::U16));
 out.push_back(FieldSchema::value("mHitPikminPosition.x",ScalarKind::F32));
 out.push_back(FieldSchema::value("mHitPikminPosition.y",ScalarKind::F32));
 out.push_back(FieldSchema::value("mHitPikminPosition.z",ScalarKind::F32));
 out.push_back(FieldSchema::value("mStartAttackTime",ScalarKind::S32));
 out.push_back(FieldSchema::value("mWorkTimer",ScalarKind::U8));
 out.push_back(FieldSchema::value("mFailAttackCounter",ScalarKind::U8));
 out.push_back(FieldSchema::value("mIsAttackReady",ScalarKind::Bool));
 out.push_back(FieldSchema::ref("mWall",RefKind::Creature,true,"BuildingItem",ReferenceOwnership::AnyLive));
 return true;
 case 12: // ActBridge
 out.push_back(FieldSchema::value("mState",ScalarKind::U16));
 out.push_back(FieldSchema::value("mStartWorkTime",ScalarKind::S32));
 out.push_back(FieldSchema::value("mIsAttackReady",ScalarKind::S32));
 out.push_back(FieldSchema::value("mCollisionCount",ScalarKind::U16));
 out.push_back(FieldSchema::value("_2A",ScalarKind::U16));
 out.push_back(FieldSchema::value("mRandomBridgeWidth",ScalarKind::F32));
 out.push_back(FieldSchema::value("mStageID",ScalarKind::S16));
 out.push_back(FieldSchema::value("mClimbingBridge",ScalarKind::Bool));
 out.push_back(FieldSchema::value("_33",ScalarKind::U8));
 out.push_back(FieldSchema::value("mBridgeWallNormal.x",ScalarKind::F32));
 out.push_back(FieldSchema::value("mBridgeWallNormal.y",ScalarKind::F32));
 out.push_back(FieldSchema::value("mBridgeWallNormal.z",ScalarKind::F32));
 out.push_back(FieldSchema::value("mClimbingVelocity.x",ScalarKind::F32));
 out.push_back(FieldSchema::value("mClimbingVelocity.y",ScalarKind::F32));
 out.push_back(FieldSchema::value("mClimbingVelocity.z",ScalarKind::F32));
 out.push_back(FieldSchema::value("mActionCounter",ScalarKind::U8));
 out.push_back(FieldSchema::value("mAnimationFinished",ScalarKind::Bool));
 out.push_back(FieldSchema::ref("mBridge",RefKind::Creature,true,"Bridge",ReferenceOwnership::AnyLive));
 return true;
 case 13: // ActChase
 out.push_back(FieldSchema::value("mChaseTimer",ScalarKind::F32));
 out.push_back(FieldSchema::ref("mTarget",RefKind::Creature,true,"Creature",ReferenceOwnership::AnyLive,"",ReferenceStrength::StrongCreature));
 return true;
 case 14: // ActCrowd
 out.push_back(FieldSchema::value("mState",ScalarKind::U16));
 out.push_back(FieldSchema::value("mMode",ScalarKind::U16));
 out.push_back(FieldSchema::value("mPrevMode",ScalarKind::U16));
 out.push_back(FieldSchema::value("mNearSlotCounter",ScalarKind::U16));
 out.push_back(FieldSchema::value("mHasRequestedNewSlot",ScalarKind::Bool));
 out.push_back(FieldSchema::value("_35",ScalarKind::Bool));
 out.push_back(FieldSchema::value("_36",ScalarKind::Bool));
 out.push_back(FieldSchema::value("mCPlateSlotID",ScalarKind::S32));
 out.push_back(FieldSchema::value("mTripLoopCounter",ScalarKind::S32));
 out.push_back(FieldSchema::value("mTravelDistance",ScalarKind::F32));
 out.push_back(FieldSchema::value("mIsTripping",ScalarKind::Bool));
 out.push_back(FieldSchema::value("mLostChildTimer",ScalarKind::F32));
 out.push_back(FieldSchema::value("mWasWaiting",ScalarKind::Bool));
 out.push_back(FieldSchema::value("mHasRoute",ScalarKind::Bool));
 out.push_back(FieldSchema::value("mPcLastPos.x",ScalarKind::F32));
 out.push_back(FieldSchema::value("mPcLastPos.y",ScalarKind::F32));
 out.push_back(FieldSchema::value("mPcLastPos.z",ScalarKind::F32));
 out.push_back(FieldSchema::value("mPcWindowTimer",ScalarKind::F32));
 out.push_back(FieldSchema::value("mPcWindowProgress",ScalarKind::F32));
 out.push_back(FieldSchema::value("mPcRouteTimer",ScalarKind::F32));
 out.push_back(FieldSchema::value("mPcRouteCooldown",ScalarKind::F32));
 out.push_back(FieldSchema::value("mPcRepathTimer",ScalarKind::F32));
 out.push_back(FieldSchema::value("mPcRouteGoal.x",ScalarKind::F32));
 out.push_back(FieldSchema::value("mPcRouteGoal.y",ScalarKind::F32));
 out.push_back(FieldSchema::value("mPcRouteGoal.z",ScalarKind::F32));
 out.push_back(FieldSchema::value("mPcOwnsRoute",ScalarKind::Bool));
 out.push_back(FieldSchema::ref("mPlateMgr",RefKind::CPlate,false,"CPlate",ReferenceOwnership::ActorSubobject,"piki.runtime.mNavi"));
 out.push_back(FieldSchema::value("mOdometer.distance",ScalarKind::F32));
 out.push_back(FieldSchema::value("mOdometer.remaining",ScalarKind::F32));
 out.push_back(FieldSchema::value("mOdometer.minimum",ScalarKind::F32));
 out.push_back(FieldSchema::value("mOdometer.reset",ScalarKind::F32));
 out.push_back(FieldSchema::value("mIsWaiting",ScalarKind::Bool));
 return true;
 case 15: // ActDecoy
 out.push_back(FieldSchema::value("mState",ScalarKind::S32));
 out.push_back(FieldSchema::value("mDecoyTimer",ScalarKind::F32));
 return true;
 case 16: // ActDeliver
 out.push_back(FieldSchema::ref("mObject",RefKind::Creature,true,"Creature",ReferenceOwnership::AnyLive,"",ReferenceStrength::StrongCreature));
 return true;
 case 17: // ActEnter
 out.push_back(FieldSchema::value("mState",ScalarKind::U16));
 out.push_back(FieldSchema::value("mLastPosition.x",ScalarKind::F32));
 out.push_back(FieldSchema::value("mLastPosition.y",ScalarKind::F32));
 out.push_back(FieldSchema::value("mLastPosition.z",ScalarKind::F32));
 out.push_back(FieldSchema::value("mHasCollided",ScalarKind::Bool));
 out.push_back(FieldSchema::ref("mOnyon",RefKind::Creature,true,"GoalItem",ReferenceOwnership::AnyLive));
 out.push_back(FieldSchema::ref("mLeg",RefKind::CollPart,true,"CollPart",ReferenceOwnership::AnyLive));
 return true;
 case 18: // ActEscape
 out.push_back(FieldSchema::value("mEscapeTimer",ScalarKind::F32));
 out.push_back(FieldSchema::value("mState",ScalarKind::S32));
 out.push_back(FieldSchema::value("mAvoidDirection.x",ScalarKind::F32));
 out.push_back(FieldSchema::value("mAvoidDirection.y",ScalarKind::F32));
 out.push_back(FieldSchema::value("mAvoidDirection.z",ScalarKind::F32));
 out.push_back(FieldSchema::ref("mTarget",RefKind::Creature,true,"Creature",ReferenceOwnership::AnyLive,"",ReferenceStrength::StrongCreature));
 return true;
 case 19: // ActExit
 out.push_back(FieldSchema::value("mPrevPosition.x",ScalarKind::F32));
 out.push_back(FieldSchema::value("mPrevPosition.y",ScalarKind::F32));
 out.push_back(FieldSchema::value("mPrevPosition.z",ScalarKind::F32));
 out.push_back(FieldSchema::value("mHasCollided",ScalarKind::Bool));
 return true;
 case 20: // ActFlower
 out.push_back(FieldSchema::value("mElapsedTime",ScalarKind::F32));
 out.push_back(FieldSchema::value("mIsAnimationComplete",ScalarKind::Bool));
 out.push_back(FieldSchema::value("mIsCarryEmpty",ScalarKind::Bool));
 return true;
 case 21: // ActFormation
 out.push_back(FieldSchema::value("mInFormation",ScalarKind::Bool));
 out.push_back(FieldSchema::value("mIdleTimer",ScalarKind::F32));
 out.push_back(FieldSchema::value("mDistanceToTarget",ScalarKind::F32));
 out.push_back(FieldSchema::value("mUseLastFormationPosition",ScalarKind::Bool));
 out.push_back(FieldSchema::value("mIsIdling",ScalarKind::Bool));
 out.push_back(FieldSchema::value("mHasStartedIdleAnim",ScalarKind::Bool));
 out.push_back(FieldSchema::value("mIsOnFloorTripped",ScalarKind::Bool));
 out.push_back(FieldSchema::value("mHasStartedRunAnim",ScalarKind::Bool));
 out.push_back(FieldSchema::value("mIsTripping",ScalarKind::S32));
 out.push_back(FieldSchema::ref("mFormMgr",RefKind::FormationMgr,false,"FormationMgr",ReferenceOwnership::ActorSubobject,"piki.runtime.mNavi"));
 return true;
 case 22: // ActFreeSelect
 out.push_back(FieldSchema::value("mActionTimer",ScalarKind::F32));
 out.push_back(FieldSchema::value("mIsTimerActive",ScalarKind::Bool));
 out.push_back(FieldSchema::value("mIsChildActionActive",ScalarKind::Bool));
 out.push_back(FieldSchema::value("mIsFinished",ScalarKind::Bool));
 out.push_back(FieldSchema::value("_1C",ScalarKind::F32));
 return true;
 case 23: // ActFree
 out.push_back(FieldSchema::value("_1C",ScalarKind::U16));
 out.push_back(FieldSchema::value("_20",ScalarKind::F32));
 out.push_back(FieldSchema::value("_24",ScalarKind::F32));
 out.push_back(FieldSchema::value("mBoidTimer",ScalarKind::F32));
 out.push_back(FieldSchema::value("mFixedPositionTimer",ScalarKind::F32));
 out.push_back(FieldSchema::value("mTargetPosition.x",ScalarKind::F32));
 out.push_back(FieldSchema::value("mTargetPosition.y",ScalarKind::F32));
 out.push_back(FieldSchema::value("mTargetPosition.z",ScalarKind::F32));
 out.push_back(FieldSchema::value("mArrivalRadius",ScalarKind::F32));
 out.push_back(FieldSchema::value("mCollisionCooldownTimer",ScalarKind::F32));
 out.push_back(FieldSchema::value("mTouchedPlayer",ScalarKind::Bool));
 out.push_back(FieldSchema::value("mIsBoidActive",ScalarKind::Bool));
 return true;
 case 24: // ActGoto
 out.push_back(FieldSchema::value("mMaxDistance",ScalarKind::F32));
 out.push_back(FieldSchema::value("mMinDistance",ScalarKind::F32));
 out.push_back(FieldSchema::value("mTimeoutDuration",ScalarKind::F32));
 out.push_back(FieldSchema::ref("mTarget",RefKind::Creature,true,"Creature",ReferenceOwnership::AnyLive,"",ReferenceStrength::StrongCreature));
 return true;
 case 25: // ActGuard
 out.push_back(FieldSchema::value("mGoalPosition.x",ScalarKind::F32));
 out.push_back(FieldSchema::value("mGoalPosition.y",ScalarKind::F32));
 out.push_back(FieldSchema::value("mGoalPosition.z",ScalarKind::F32));
 out.push_back(FieldSchema::value("mFormationAngle",ScalarKind::F32));
 out.push_back(FieldSchema::value("mLandPosition.x",ScalarKind::F32));
 out.push_back(FieldSchema::value("mLandPosition.y",ScalarKind::F32));
 out.push_back(FieldSchema::value("mLandPosition.z",ScalarKind::F32));
 out.push_back(FieldSchema::value("mFormationSpacing",ScalarKind::F32));
 out.push_back(FieldSchema::value("mTimer",ScalarKind::F32));
 out.push_back(FieldSchema::value("mIsWaiting",ScalarKind::Bool));
 out.push_back(FieldSchema::value("mIsGuardable",ScalarKind::Bool));
 out.push_back(FieldSchema::ref("mTarget",RefKind::Creature,true,"Creature",ReferenceOwnership::AnyLive,"",ReferenceStrength::StrongCreature));
 out.push_back(FieldSchema::ref("mLeftGuard",RefKind::Creature,true,"Creature",ReferenceOwnership::AnyLive,"",ReferenceStrength::StrongCreature));
 out.push_back(FieldSchema::ref("mRightGuard",RefKind::Creature,true,"Creature",ReferenceOwnership::AnyLive,"",ReferenceStrength::StrongCreature));
 out.push_back(FieldSchema::value("mFormationSide",ScalarKind::S32));
 return true;
 case 26: // ActJumpAttack
 out.push_back(FieldSchema::value("mState",ScalarKind::S32));
 out.push_back(FieldSchema::value("mAttackState",ScalarKind::S32));
 out.push_back(FieldSchema::value("_2C",ScalarKind::Bool));
 out.push_back(FieldSchema::value("mIsCriticalHit",ScalarKind::Bool));
 out.push_back(FieldSchema::ref("mTarget",RefKind::Creature,true,"Creature",ReferenceOwnership::AnyLive,"",ReferenceStrength::StrongCreature));
 out.push_back(FieldSchema::ref("mTargetCollider",RefKind::CollPart,true,"CollPart",ReferenceOwnership::AnyLive));
 return true;
 case 27: // ActKinoko
 out.push_back(FieldSchema::value("mState",ScalarKind::S32));
 out.push_back(FieldSchema::value("mStateTimer",ScalarKind::F32));
 out.push_back(FieldSchema::value("mTargetDirection.x",ScalarKind::F32));
 out.push_back(FieldSchema::value("mTargetDirection.y",ScalarKind::F32));
 out.push_back(FieldSchema::value("mTargetDirection.z",ScalarKind::F32));
 out.push_back(FieldSchema::ref("mTarget",RefKind::Creature,true,"Creature",ReferenceOwnership::AnyLive,"",ReferenceStrength::StrongCreature));
 return true;
 case 28: // ActMine
 out.push_back(FieldSchema::value("mState",ScalarKind::U16));
 out.push_back(FieldSchema::value("mIsMineActionReady",ScalarKind::Bool));
 out.push_back(FieldSchema::ref("mBombGen",RefKind::Creature,true,"BombGenItem",ReferenceOwnership::AnyLive));
 return true;
 case 29: // ActPick
 out.push_back(FieldSchema::value("mIsAnimationFinished",ScalarKind::Bool));
 out.push_back(FieldSchema::ref("mObject",RefKind::Creature,true,"Creature",ReferenceOwnership::AnyLive,"",ReferenceStrength::StrongCreature));
 return true;
 case 30: // ActPickCreature
 out.push_back(FieldSchema::ref("_18",RefKind::Creature,true,"Creature",ReferenceOwnership::AnyLive,"",ReferenceStrength::StrongCreature));
 return true;
 case 31: // ActPickItem
 out.push_back(FieldSchema::ref("mTargetItem",RefKind::Creature,true,"Creature",ReferenceOwnership::AnyLive,"",ReferenceStrength::StrongCreature));
 return true;
 case 32: // ActPullout
 out.push_back(FieldSchema::ref("mTarget",RefKind::Creature,true,"Creature",ReferenceOwnership::AnyLive,"",ReferenceStrength::StrongCreature));
 return true;
 case 33: // ActPulloutCreature
 out.push_back(FieldSchema::value("mState",ScalarKind::S32));
 out.push_back(FieldSchema::value("mPulloutTimer",ScalarKind::F32));
 out.push_back(FieldSchema::value("mPulloutSuccess",ScalarKind::Bool));
 out.push_back(FieldSchema::ref("mTarget",RefKind::Creature,true,"Creature",ReferenceOwnership::AnyLive,"",ReferenceStrength::StrongCreature));
 return true;
 case 34: // ActPush
 out.push_back(FieldSchema::value("mPushAnimationState",ScalarKind::U8));
 out.push_back(FieldSchema::value("mState",ScalarKind::U16));
 out.push_back(FieldSchema::value("_24",ScalarKind::S32));
 out.push_back(FieldSchema::value("_38",ScalarKind::F32));
 out.push_back(FieldSchema::value("mPushObjectStopped",ScalarKind::Bool));
 out.push_back(FieldSchema::value("_40",ScalarKind::F32));
 out.push_back(FieldSchema::value("_44",ScalarKind::U8));
 out.push_back(FieldSchema::value("mPushCount",ScalarKind::S8));
 out.push_back(FieldSchema::value("mIsPushReady",ScalarKind::Bool));
 out.push_back(FieldSchema::ref("mHinderRock",RefKind::Creature,true,"HinderRock",ReferenceOwnership::AnyLive));
 return true;
 case 35: // ActPut
 out.push_back(FieldSchema::value("mFailCountdownTimer",ScalarKind::F32));
 return true;
 case 36: // ActPutBomb
 out.push_back(FieldSchema::value("mState",ScalarKind::U16));
 out.push_back(FieldSchema::value("mAnimationFinished",ScalarKind::Bool));
 out.push_back(FieldSchema::value("mTouchedPlayer",ScalarKind::Bool));
 out.push_back(FieldSchema::value("mAimTimer",ScalarKind::F32));
 out.push_back(FieldSchema::value("mPlaceTimer",ScalarKind::F32));
 out.push_back(FieldSchema::ref("mTarget",RefKind::Creature,true,"Creature",ReferenceOwnership::AnyLive));
 return true;
 case 37: // ActPutItem
 out.push_back(FieldSchema::value("mItemPosition.x",ScalarKind::F32));
 out.push_back(FieldSchema::value("mItemPosition.y",ScalarKind::F32));
 out.push_back(FieldSchema::value("mItemPosition.z",ScalarKind::F32));
 out.push_back(FieldSchema::ref("mItem",RefKind::Creature,true,"Creature",ReferenceOwnership::AnyLive,"",ReferenceStrength::StrongCreature));
 return true;
 case 38: // ActRandomBoid
 out.push_back(FieldSchema::value("mState",ScalarKind::S32));
 out.push_back(FieldSchema::value("mStateTimer",ScalarKind::S32));
 out.push_back(FieldSchema::value("mIsAnimFinishing",ScalarKind::Bool));
 out.push_back(FieldSchema::ref("mListener",RefKind::AnimListener,true,"AnimListener",ReferenceOwnership::ActorSubobject));
 return true;
 case 39: // ActRescue
 out.push_back(FieldSchema::value("mState",ScalarKind::U16));
 out.push_back(FieldSchema::value("mTargetSurviveTimer",ScalarKind::U16));
 out.push_back(FieldSchema::value("mRescueTargetPosition.x",ScalarKind::F32));
 out.push_back(FieldSchema::value("mRescueTargetPosition.y",ScalarKind::F32));
 out.push_back(FieldSchema::value("mRescueTargetPosition.z",ScalarKind::F32));
 out.push_back(FieldSchema::value("mGotAnimationAction",ScalarKind::Bool));
 out.push_back(FieldSchema::value("mAnimationFinished",ScalarKind::Bool));
 out.push_back(FieldSchema::value("mThrowReady",ScalarKind::Bool));
 out.push_back(FieldSchema::ref("mDrowningPiki",RefKind::Creature,true,"Piki",ReferenceOwnership::AnyLive));
 return true;
 case 40: // ActRope
 out.push_back(FieldSchema::value("mSpeed",ScalarKind::F32));
 out.push_back(FieldSchema::value("mRopeDirection.x",ScalarKind::F32));
 out.push_back(FieldSchema::value("mRopeDirection.y",ScalarKind::F32));
 out.push_back(FieldSchema::value("mRopeDirection.z",ScalarKind::F32));
 return true;
 case 41: // ActShoot
 out.push_back(FieldSchema::value("mTargetIsPlayer",ScalarKind::Bool));
 out.push_back(FieldSchema::ref("mTargetObjectPool",RefKind::Traversable,true,"Traversable",ReferenceOwnership::AnyLive));
 out.push_back(FieldSchema::ref("mTarget",RefKind::Creature,true,"Creature",ReferenceOwnership::AnyLive,"",ReferenceStrength::StrongCreature));
 out.push_back(FieldSchema::ref("mNavi",RefKind::Creature,false,"Navi",ReferenceOwnership::AnyLive));
 return true;
 case 42: // ActShootCreature
 out.push_back(FieldSchema::value("mState",ScalarKind::S32));
 out.push_back(FieldSchema::value("mChaseTimer",ScalarKind::F32));
 out.push_back(FieldSchema::ref("mTarget",RefKind::Creature,true,"Creature",ReferenceOwnership::AnyLive,"",ReferenceStrength::StrongCreature));
 return true;
 case 43: // ActStone
 out.push_back(FieldSchema::value("mState",ScalarKind::U16));
 out.push_back(FieldSchema::value("mIsAttackReady",ScalarKind::Bool));
 out.push_back(FieldSchema::ref("mCurrPebble",RefKind::Pebble,true,"Pebble",ReferenceOwnership::AnyLive));
 out.push_back(FieldSchema::ref("mRockGen",RefKind::RockGen,true,"RockGen",ReferenceOwnership::AnyLive));
 return true;
 case 44: // ActTransport
 out.push_back(FieldSchema::value("mState",ScalarKind::U16));
 out.push_back(FieldSchema::value("mMoveDir.x",ScalarKind::F32));
 out.push_back(FieldSchema::value("mMoveDir.y",ScalarKind::F32));
 out.push_back(FieldSchema::value("mMoveDir.z",ScalarKind::F32));
 out.push_back(FieldSchema::value("mNumRoutePoints",ScalarKind::U16));
 out.push_back(FieldSchema::value("mJumpRetryTimer",ScalarKind::U8));
 out.push_back(FieldSchema::value("mStateProgress",ScalarKind::U16));
 out.push_back(FieldSchema::value("mNextPathIndex",ScalarKind::S32));
 out.push_back(FieldSchema::value("_48",ScalarKind::U32));
 out.push_back(FieldSchema::value("mRouteStartPos.x",ScalarKind::F32));
 out.push_back(FieldSchema::value("mRouteStartPos.y",ScalarKind::F32));
 out.push_back(FieldSchema::value("mRouteStartPos.z",ScalarKind::F32));
 out.push_back(FieldSchema::value("mPathType",ScalarKind::U8));
 out.push_back(FieldSchema::value("mSlotIndex",ScalarKind::S32));
 out.push_back(FieldSchema::value("mSpinStartPosition.x",ScalarKind::F32));
 out.push_back(FieldSchema::value("mSpinStartPosition.y",ScalarKind::F32));
 out.push_back(FieldSchema::value("mSpinStartPosition.z",ScalarKind::F32));
 out.push_back(FieldSchema::value("mFinishPutting",ScalarKind::Bool));
 out.push_back(FieldSchema::value("mIsLiftActionDone",ScalarKind::Bool));
 out.push_back(FieldSchema::value("mLiftRetryCount",ScalarKind::S32));
 out.push_back(FieldSchema::value("mWaitTimer",ScalarKind::F32));
 out.push_back(FieldSchema::value("mPathIndex",ScalarKind::S32));
 out.push_back(FieldSchema::value("mGoalWPIndex",ScalarKind::S32));
 out.push_back(FieldSchema::value("mCanCarry",ScalarKind::Bool));
 out.push_back(FieldSchema::value("mPcStallCheckPos.x",ScalarKind::F32));
 out.push_back(FieldSchema::value("mPcStallCheckPos.y",ScalarKind::F32));
 out.push_back(FieldSchema::value("mPcStallCheckPos.z",ScalarKind::F32));
 out.push_back(FieldSchema::value("mPcStallTimer",ScalarKind::F32));
 out.push_back(FieldSchema::value("mPcStallArmed",ScalarKind::Bool));
 out.push_back(FieldSchema::ref("mPellet",RefKind::Creature,true,"Pellet",ReferenceOwnership::AnyLive,"",ReferenceStrength::StrongCreature));
 out.push_back(FieldSchema::ref("mGoal",RefKind::Creature,true,"Suckable",ReferenceOwnership::AnyLive));
 out.push_back(FieldSchema::value("mSplineControlPts.0.x",ScalarKind::F32));
 out.push_back(FieldSchema::value("mSplineControlPts.0.y",ScalarKind::F32));
 out.push_back(FieldSchema::value("mSplineControlPts.0.z",ScalarKind::F32));
 out.push_back(FieldSchema::value("mSplineControlPts.1.x",ScalarKind::F32));
 out.push_back(FieldSchema::value("mSplineControlPts.1.y",ScalarKind::F32));
 out.push_back(FieldSchema::value("mSplineControlPts.1.z",ScalarKind::F32));
 out.push_back(FieldSchema::value("mSplineControlPts.2.x",ScalarKind::F32));
 out.push_back(FieldSchema::value("mSplineControlPts.2.y",ScalarKind::F32));
 out.push_back(FieldSchema::value("mSplineControlPts.2.z",ScalarKind::F32));
 out.push_back(FieldSchema::value("mSplineControlPts.3.x",ScalarKind::F32));
 out.push_back(FieldSchema::value("mSplineControlPts.3.y",ScalarKind::F32));
 out.push_back(FieldSchema::value("mSplineControlPts.3.z",ScalarKind::F32));
 out.push_back(FieldSchema::value("mOdometer.distance",ScalarKind::F32));
 out.push_back(FieldSchema::value("mOdometer.remaining",ScalarKind::F32));
 out.push_back(FieldSchema::value("mOdometer.minimum",ScalarKind::F32));
 out.push_back(FieldSchema::value("mOdometer.reset",ScalarKind::F32));
 return true;
 case 45: // ActWatch
 out.push_back(FieldSchema::value("mWatchRetryTimer",ScalarKind::S32));
 out.push_back(FieldSchema::value("mTargetPosition.x",ScalarKind::F32));
 out.push_back(FieldSchema::value("mTargetPosition.y",ScalarKind::F32));
 out.push_back(FieldSchema::value("mTargetPosition.z",ScalarKind::F32));
 out.push_back(FieldSchema::ref("mTarget",RefKind::Creature,true,"Creature",ReferenceOwnership::AnyLive,"",ReferenceStrength::StrongCreature));
 out.push_back(FieldSchema::ref("mListener",RefKind::AnimListener,true,"AnimListener",ReferenceOwnership::ActorSubobject));
 return true;
 case 46: // ActWeed
 out.push_back(FieldSchema::value("mState",ScalarKind::U16));
 out.push_back(FieldSchema::value("_28",ScalarKind::U16));
 out.push_back(FieldSchema::value("mAnimationFinished",ScalarKind::Bool));
 out.push_back(FieldSchema::ref("mCurrGrass",RefKind::Grass,true,"Grass",ReferenceOwnership::AnyLive));
 out.push_back(FieldSchema::ref("mGrassGen",RefKind::GrassGen,false,"GrassGen",ReferenceOwnership::AnyLive));
 return true;
 default:return false;
}}
void piki_runtime_schema(std::vector<FieldSchema>& out) {
 out.push_back(FieldSchema::value("mUseAsyncPathfinding",ScalarKind::Bool));
 out.push_back(FieldSchema::value("mRouteSourceIndex",ScalarKind::S16));
 out.push_back(FieldSchema::value("mRouteDestinationIndex",ScalarKind::S16));
 out.push_back(FieldSchema::value("mIsRetryPathfind",ScalarKind::Bool));
 out.push_back(FieldSchema::value("mCurrRoutePoint",ScalarKind::S16));
 out.push_back(FieldSchema::value("mRouteStartPos.x",ScalarKind::F32));
 out.push_back(FieldSchema::value("mRouteStartPos.y",ScalarKind::F32));
 out.push_back(FieldSchema::value("mRouteStartPos.z",ScalarKind::F32));
 out.push_back(FieldSchema::value("mRouteGoalPos.x",ScalarKind::F32));
 out.push_back(FieldSchema::value("mRouteGoalPos.y",ScalarKind::F32));
 out.push_back(FieldSchema::value("mRouteGoalPos.z",ScalarKind::F32));
 out.push_back(FieldSchema::value("mNumRoutePoints",ScalarKind::S16));
 out.push_back(FieldSchema::value("mIsLooking",ScalarKind::Bool));
 out.push_back(FieldSchema::value("mLookTimer",ScalarKind::U8));
 out.push_back(FieldSchema::value("mHorizontalRotation",ScalarKind::F32));
 out.push_back(FieldSchema::value("mVerticalRotation",ScalarKind::F32));
 out.push_back(FieldSchema::value("mOldFaceDirection",ScalarKind::F32));
 out.push_back(FieldSchema::value("mBlendMotionIdx",ScalarKind::S32));
 out.push_back(FieldSchema::value("mEmotion",ScalarKind::U8));
 out.push_back(FieldSchema::value("mActionState",ScalarKind::U8));
 out.push_back(FieldSchema::value("mIsCallable",ScalarKind::Bool));
 out.push_back(FieldSchema::value("mIsPanicked",ScalarKind::Bool));
 out.push_back(FieldSchema::value("mInWaterTimer",ScalarKind::U16));
 out.push_back(FieldSchema::value("mPlayerId",ScalarKind::S32));
 out.push_back(FieldSchema::value("mPcSieging",ScalarKind::Bool));
 out.push_back(FieldSchema::value("mLastAnimPosition.x",ScalarKind::F32));
 out.push_back(FieldSchema::value("mLastAnimPosition.y",ScalarKind::F32));
 out.push_back(FieldSchema::value("mLastAnimPosition.z",ScalarKind::F32));
 out.push_back(FieldSchema::value("mShadowPos.x",ScalarKind::F32));
 out.push_back(FieldSchema::value("mShadowPos.y",ScalarKind::F32));
 out.push_back(FieldSchema::value("mShadowPos.z",ScalarKind::F32));
 out.push_back(FieldSchema::value("mCatchPos.x",ScalarKind::F32));
 out.push_back(FieldSchema::value("mCatchPos.y",ScalarKind::F32));
 out.push_back(FieldSchema::value("mCatchPos.z",ScalarKind::F32));
 out.push_back(FieldSchema::value("mEffectPos.x",ScalarKind::F32));
 out.push_back(FieldSchema::value("mEffectPos.y",ScalarKind::F32));
 out.push_back(FieldSchema::value("mEffectPos.z",ScalarKind::F32));
 out.push_back(FieldSchema::value("mWantToStick",ScalarKind::Bool));
 out.push_back(FieldSchema::value("mMotionSpeed",ScalarKind::F32));
 out.push_back(FieldSchema::value("mMoveSpeed",ScalarKind::F32));
 out.push_back(FieldSchema::value("mDeathTimer",ScalarKind::F32));
 out.push_back(FieldSchema::value("mFlickIntensity",ScalarKind::F32));
 out.push_back(FieldSchema::value("mRotationAngle",ScalarKind::F32));
 out.push_back(FieldSchema::value("mIsWhistlePending",ScalarKind::Bool));
 out.push_back(FieldSchema::value("mPluckVelocity.x",ScalarKind::F32));
 out.push_back(FieldSchema::value("mPluckVelocity.y",ScalarKind::F32));
 out.push_back(FieldSchema::value("mPluckVelocity.z",ScalarKind::F32));
 out.push_back(FieldSchema::value("mFormationPriority",ScalarKind::S32));
 out.push_back(FieldSchema::value("mPushTargetPos.x",ScalarKind::F32));
 out.push_back(FieldSchema::value("mPushTargetPos.y",ScalarKind::F32));
 out.push_back(FieldSchema::value("mPushTargetPos.z",ScalarKind::F32));
 out.push_back(FieldSchema::value("mPikiSize",ScalarKind::F32));
 out.push_back(FieldSchema::value("mMode",ScalarKind::U16));
 out.push_back(FieldSchema::value("mColor",ScalarKind::U16));
 out.push_back(FieldSchema::value("mFloweringTimer",ScalarKind::S32));
 out.push_back(FieldSchema::value("mHappa",ScalarKind::S32));
 out.push_back(FieldSchema::value("mFiredState",ScalarKind::U16));
 out.push_back(FieldSchema::value("mColourBlendRatio",ScalarKind::F32));
 out.push_back(FieldSchema::value("mEraseOnKill",ScalarKind::Bool));
 out.push_back(FieldSchema::value("mP2Purple",ScalarKind::Bool));
 out.push_back(FieldSchema::value("mP2White",ScalarKind::Bool));
 out.push_back(FieldSchema::value("mP2Bulbmin",ScalarKind::Bool));
 out.push_back(FieldSchema::value("mP2AnimationTime",ScalarKind::F32));
 out.push_back(FieldSchema::value("mGasInvincible",ScalarKind::U8));
 out.push_back(FieldSchema::value("mPcLastDryPos.x",ScalarKind::F32));
 out.push_back(FieldSchema::value("mPcLastDryPos.y",ScalarKind::F32));
 out.push_back(FieldSchema::value("mPcLastDryPos.z",ScalarKind::F32));
 out.push_back(FieldSchema::value("mPcHasDryPos",ScalarKind::Bool));
 out.push_back(FieldSchema::value("_334",ScalarKind::F32));
 out.push_back(FieldSchema::value("_474",ScalarKind::U32));
 out.push_back(FieldSchema::value("_478",ScalarKind::F32));
 out.push_back(FieldSchema::value("_480",ScalarKind::S32));
 out.push_back(FieldSchema::value("_484",ScalarKind::S32));
 out.push_back(FieldSchema::value("_4C8.x",ScalarKind::F32));
 out.push_back(FieldSchema::value("_4C8.y",ScalarKind::F32));
 out.push_back(FieldSchema::value("_4C8.z",ScalarKind::F32));
 out.push_back(FieldSchema::value("_4D8",ScalarKind::S32));
 out.push_back(FieldSchema::value("_4E8",ScalarKind::S32));
 out.push_back(FieldSchema::value("_4EC",ScalarKind::F32));
 out.push_back(FieldSchema::value("_4F0",ScalarKind::F32));
 out.push_back(FieldSchema::value("_518",ScalarKind::Bool));
 out.push_back(FieldSchema::value("_519",ScalarKind::Bool));
 out.push_back(FieldSchema::value("_51C",ScalarKind::U32));
 out.push_back(FieldSchema::value("_528",ScalarKind::F32));
 out.push_back(FieldSchema::value("animationSpeed",ScalarKind::F32));
 out.push_back(FieldSchema::value("mCurrentColour.r",ScalarKind::U8));
 out.push_back(FieldSchema::value("mCurrentColour.g",ScalarKind::U8));
 out.push_back(FieldSchema::value("mCurrentColour.b",ScalarKind::U8));
 out.push_back(FieldSchema::value("mCurrentColour.a",ScalarKind::U8));
 out.push_back(FieldSchema::value("mDefaultColour.r",ScalarKind::U8));
 out.push_back(FieldSchema::value("mDefaultColour.g",ScalarKind::U8));
 out.push_back(FieldSchema::value("mDefaultColour.b",ScalarKind::U8));
 out.push_back(FieldSchema::value("mDefaultColour.a",ScalarKind::U8));
 out.push_back(FieldSchema::value("mStartBlendColour.r",ScalarKind::U8));
 out.push_back(FieldSchema::value("mStartBlendColour.g",ScalarKind::U8));
 out.push_back(FieldSchema::value("mStartBlendColour.b",ScalarKind::U8));
 out.push_back(FieldSchema::value("mStartBlendColour.a",ScalarKind::U8));
 out.push_back(FieldSchema::value("mTargetBlendColour.r",ScalarKind::U8));
 out.push_back(FieldSchema::value("mTargetBlendColour.g",ScalarKind::U8));
 out.push_back(FieldSchema::value("mTargetBlendColour.b",ScalarKind::U8));
 out.push_back(FieldSchema::value("mTargetBlendColour.a",ScalarKind::U8));
 out.push_back(FieldSchema::ref("mPathBuffers",RefKind::Path,true,"PathFinder::Buffer",ReferenceOwnership::ActorSubobject));
 out.push_back(FieldSchema::ref("mRouteTargetCreature",RefKind::Creature,true,"Creature",ReferenceOwnership::AnyLive));
 out.push_back(FieldSchema::ref("mLookatPosPtr",RefKind::Vector3,true,"Vector3f",ReferenceOwnership::AnyLive));
 out.push_back(FieldSchema::ref("mCarryingShipPart",RefKind::Creature,true,"Creature",ReferenceOwnership::AnyLive));
 out.push_back(FieldSchema::ref("mCurrNectar",RefKind::Creature,true,"Creature",ReferenceOwnership::AnyLive));
 out.push_back(FieldSchema::ref("mSwallowMouthPart",RefKind::CollPart,true,"CollPart",ReferenceOwnership::AnyLive));
 out.push_back(FieldSchema::ref("mLeaderCreature",RefKind::Creature,true,"Creature",ReferenceOwnership::AnyLive));
 out.push_back(FieldSchema::ref("mPushTargetPiki",RefKind::Creature,true,"Piki",ReferenceOwnership::AnyLive));
 out.push_back(FieldSchema::ref("mWallPlane",RefKind::Plane,true,"Plane",ReferenceOwnership::AnyLive));
 out.push_back(FieldSchema::ref("mWallObj",RefKind::DynCollObject,true,"DynCollObject",ReferenceOwnership::AnyLive));
 out.push_back(FieldSchema::ref("mNavi",RefKind::Creature,false,"Navi",ReferenceOwnership::AnyLive));
 out.push_back(FieldSchema::ref("mPanickedEffect",RefKind::Effect,true,"PermanentEffect",ReferenceOwnership::ActorSubobject));
 out.push_back(FieldSchema::ref("mBurnEffect",RefKind::Effect,true,"BurnEffect",ReferenceOwnership::ActorSubobject));
 out.push_back(FieldSchema::ref("mRippleEffect",RefKind::Effect,true,"RippleEffect",ReferenceOwnership::ActorSubobject));
 out.push_back(FieldSchema::ref("mFreeLightEffect",RefKind::Effect,true,"FreeLightEffect",ReferenceOwnership::ActorSubobject));
 out.push_back(FieldSchema::ref("mSlimeEffect",RefKind::Effect,true,"SlimeEffect",ReferenceOwnership::ActorSubobject));
 out.push_back(FieldSchema::ref("mLookAtCreature",RefKind::Creature,true,"Creature",ReferenceOwnership::AnyLive,"",ReferenceStrength::StrongCreature));
 out.push_back(FieldSchema::ref("_500",RefKind::Creature,true,"Creature",ReferenceOwnership::AnyLive,"",ReferenceStrength::StrongCreature));
 out.push_back(FieldSchema::value("path.capacity",ScalarKind::S32));
 out.push_back(FieldSchema::ref("update.manager",RefKind::UpdateMgr,true,"UpdateMgr",ReferenceOwnership::Content));
 out.push_back(FieldSchema::value("update.slot",ScalarKind::S32));
 out.push_back(FieldSchema::value("update.piki",ScalarKind::Bool));
 out.push_back(FieldSchema::ref("lookUpdate.manager",RefKind::UpdateMgr,true,"UpdateMgr",ReferenceOwnership::Content));
 out.push_back(FieldSchema::value("lookUpdate.slot",ScalarKind::S32));
 out.push_back(FieldSchema::value("lookUpdate.piki",ScalarKind::Bool));
 out.push_back(FieldSchema::handle("routeHandle",RefKind::Path,true,"PathFinder::Client",ReferenceOwnership::ActorSubobject));
 out.push_back(FieldSchema::value("spline.0.position.x",ScalarKind::F32));
 out.push_back(FieldSchema::value("spline.0.position.y",ScalarKind::F32));
 out.push_back(FieldSchema::value("spline.0.position.z",ScalarKind::F32));
 out.push_back(FieldSchema::value("spline.1.position.x",ScalarKind::F32));
 out.push_back(FieldSchema::value("spline.1.position.y",ScalarKind::F32));
 out.push_back(FieldSchema::value("spline.1.position.z",ScalarKind::F32));
 out.push_back(FieldSchema::value("spline.2.position.x",ScalarKind::F32));
 out.push_back(FieldSchema::value("spline.2.position.y",ScalarKind::F32));
 out.push_back(FieldSchema::value("spline.2.position.z",ScalarKind::F32));
 out.push_back(FieldSchema::value("spline.3.position.x",ScalarKind::F32));
 out.push_back(FieldSchema::value("spline.3.position.y",ScalarKind::F32));
 out.push_back(FieldSchema::value("spline.3.position.z",ScalarKind::F32));
 out.push_back(FieldSchema::value("odometer.distance",ScalarKind::F32));
 out.push_back(FieldSchema::value("odometer.remaining",ScalarKind::F32));
 out.push_back(FieldSchema::value("odometer.minimum",ScalarKind::F32));
 out.push_back(FieldSchema::value("odometer.reset",ScalarKind::F32));
 out.push_back(FieldSchema::value("upperAnimation.mPlayState",ScalarKind::S32));
 out.push_back(FieldSchema::value("upperAnimation.mCurrentAnimID",ScalarKind::S32));
 out.push_back(FieldSchema::value("upperAnimation.mStartKeyIndex",ScalarKind::S32));
 out.push_back(FieldSchema::value("upperAnimation.mEndKeyIndex",ScalarKind::S32));
 out.push_back(FieldSchema::value("upperAnimation.mCurrentKeyIndex",ScalarKind::S32));
 out.push_back(FieldSchema::value("upperAnimation.mMotionIdx",ScalarKind::S32));
 out.push_back(FieldSchema::value("upperAnimation.mPreviousKeyIndex",ScalarKind::U32));
 out.push_back(FieldSchema::value("upperAnimation.mAnimationCounter",ScalarKind::F32));
 out.push_back(FieldSchema::value("upperAnimation.mIsFinished",ScalarKind::Bool));
 out.push_back(FieldSchema::ref("upperAnimation.mMgr",RefKind::Animation,false,"AnimMgr",ReferenceOwnership::Content));
 out.push_back(FieldSchema::ref("upperAnimation.mContext",RefKind::Animation,false,"AnimContext",ReferenceOwnership::Content));
 out.push_back(FieldSchema::ref("upperAnimation.mAnimInfo",RefKind::Animation,false,"AnimInfo",ReferenceOwnership::Content));
 out.push_back(FieldSchema::ref("upperAnimation.mMotionTable",RefKind::Animation,false,"PaniMotionTable",ReferenceOwnership::Content));
 out.push_back(FieldSchema::ref("upperAnimation.mListener",RefKind::AnimListener,true,"PaniAnimKeyListener",ReferenceOwnership::ActorSubobject));
 out.push_back(FieldSchema::value("lowerAnimation.mPlayState",ScalarKind::S32));
 out.push_back(FieldSchema::value("lowerAnimation.mCurrentAnimID",ScalarKind::S32));
 out.push_back(FieldSchema::value("lowerAnimation.mStartKeyIndex",ScalarKind::S32));
 out.push_back(FieldSchema::value("lowerAnimation.mEndKeyIndex",ScalarKind::S32));
 out.push_back(FieldSchema::value("lowerAnimation.mCurrentKeyIndex",ScalarKind::S32));
 out.push_back(FieldSchema::value("lowerAnimation.mMotionIdx",ScalarKind::S32));
 out.push_back(FieldSchema::value("lowerAnimation.mPreviousKeyIndex",ScalarKind::U32));
 out.push_back(FieldSchema::value("lowerAnimation.mAnimationCounter",ScalarKind::F32));
 out.push_back(FieldSchema::value("lowerAnimation.mIsFinished",ScalarKind::Bool));
 out.push_back(FieldSchema::ref("lowerAnimation.mMgr",RefKind::Animation,false,"AnimMgr",ReferenceOwnership::Content));
 out.push_back(FieldSchema::ref("lowerAnimation.mContext",RefKind::Animation,false,"AnimContext",ReferenceOwnership::Content));
 out.push_back(FieldSchema::ref("lowerAnimation.mAnimInfo",RefKind::Animation,false,"AnimInfo",ReferenceOwnership::Content));
 out.push_back(FieldSchema::ref("lowerAnimation.mMotionTable",RefKind::Animation,false,"PaniMotionTable",ReferenceOwnership::Content));
 out.push_back(FieldSchema::ref("lowerAnimation.mListener",RefKind::AnimListener,true,"PaniAnimKeyListener",ReferenceOwnership::ActorSubobject));
}
void animation_schema(const std::string& prefix,std::vector<FieldSchema>& out) {
 std::vector<FieldSchema> runtime;piki_runtime_schema(runtime);
 const std::string source="upperAnimation.";
 for(auto field:runtime)if(field.key.compare(0,source.size(),source)==0){field.key=prefix+field.key.substr(source.size());out.push_back(field);}
}
}
