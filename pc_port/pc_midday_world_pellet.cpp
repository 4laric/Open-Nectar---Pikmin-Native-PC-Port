#if defined(PIKI_PC_PORT)
#include "pc_midday_world.h"
#include "pc_midday_world_resources.h"
#include "Pellet.h"
#include "PelletState.h"
#include "DynParticle.h"
#include <set>
using namespace pc_midday;
// Inherited dynamics flags need exact private access without exposing setters.
struct PcMiddayPelletAccess {static bool flags(Pellet& s,ActorArchive& outer){PrefixArchive ar(outer,"world.pellet.dynamics");return ar.field("real",s.mIsRealDynamics)&&ar.field("collisionReady",s.mIsCollisionInitialised)&&ar.field("changed",s._43E)&&ar.field("simpleFixed",s.mIsDynamicsSimpleFixed);}};
namespace {
bool matrix(ActorArchive& ar,Matrix4f& m){for(int i=0;i<4;++i)for(int j=0;j<4;++j)if(!ar.field((std::to_string(i)+"."+std::to_string(j)).c_str(),m.mMtx[i][j]))return false;return true;}
bool particles(Pellet& s,ActorArchive& outer){
 PrefixArchive ar(outer,"world.pellet.dynamics");
 u16 count=outer.mode()==Mode::Capture?s.mParticleCount:0;
 if(!ar.scalar("count",ScalarKind::U16,&count)||count>128||count!=s.mParticleCount)return ar.fail("pellet particle allocation mismatch");
 if(!ar.field("angularMomentum",s.mAngularMomentum)||!ar.field("angularVelocity",s.mAngularVelocity)||!ar.field("pickOffset",s.mPickOffset)||!ar.field("impulse",s.mAngularImpulseAccum)||!ar.field("centre",s.mCenterOfMass)||!ar.field("mass",s.mMass)||!ar.field("flags",s.mDynFlag)||!ar.field("ground",s.mGroundFlag))return false;
 if(count){Matrix4f* matrices[]={&s.mWorldInertiaTensor,&s.mWorldInvInertiaTensor,&s.mInertiaTensor,&s.mInvInertiaTensor};for(int i=0;i<4;++i){PrefixArchive part(ar,("matrix."+std::to_string(i)).c_str());if(!matrix(part,*matrices[i]))return false;}}
 if(!ar.ref("head",RefKind::DynParticle,s.mParticleList))return false;
 std::set<DynParticle*> seen;auto* p=s.mParticleList;
 for(int i=0;i<count;++i){if(!p||!seen.insert(p).second)return ar.fail("pellet particle list missing/cyclic");PrefixArchive part(ar,("particle."+std::to_string(i)).c_str());
 if(!part.field("mass",p->mMass)||!part.field("initial",p->mInitialPosition)||!part.field("local",p->mLocalPosition)||!part.field("preCollision",p->mPreCollisionVelocity)||!part.field("free",p->mIsFree)||!part.field("radius",p->mCollisionRadius)||!part.field("position",p->mWorldPosition)||!part.field("velocity",p->mWorldVelocity))return false;
 PrefixArchive inv(part,"inverse");if(!matrix(inv,p->mInvCrossMatrix)||!part.ref("next",RefKind::DynParticle,p->mNextParticle))return false;p=p->mNextParticle;
 }return !p||ar.fail("pellet particle list exceeds count");
}
}
namespace pc_midday {
bool world_pellet_flags(Pellet& s,ActorArchive& ar){return PcMiddayPelletAccess::flags(s,ar);}
bool world_pellet_fields(Pellet& s,ActorArchive& outer){
 PrefixArchive ar(outer,"world.pellet");
 if(!s.mStateMachine)return ar.fail("missing pellet state table");
 int current=ar.mode()==Mode::Capture&&s.mCurrentState?s.mCurrentState->getID():-1;
 int last=ar.mode()==Mode::Capture?s.mStateMachine->mLastStateID:-1;
 if(!ar.scalar("current",ScalarKind::S32,&current)||!ar.scalar("last",ScalarKind::S32,&last)||current<0||current>=6||last< -1||last>=6)return ar.fail("invalid pellet state");
 AState<Pellet>* selected=nullptr;for(int i=0;i<s.mStateMachine->mStateCount;++i)if(s.mStateMachine->mStates[i]&&s.mStateMachine->mStates[i]->getID()==current)selected=s.mStateMachine->mStates[i];if(!selected)return ar.fail("unregistered pellet state");
#define F(name) if(!ar.field(#name,s.name))return false
#define R(name,kind) if(!ar.ref(#name,RefKind::kind,s.name))return false
 F(mSpawnPosition);F(mUseSpawnPosition);F(mIsPlayTrySound);F(mMotionFlag);F(mStuckAngle);F(mLastPosition);F(mCarryDirection);F(mCarrierCount);F(mTransitionTimer);F(mCarryState);F(mCurrentPelletPosition);F(_4A0);F(mCurrentPelletHeight);F(mMotionSpeed);F(mCarrierCounter);F(mIsAlive);F(mIsAIActive);
 R(mRippleEffect,Effect);R(mTargetGoal,Creature);R(mStuckMouthPart,CollPart);R(mPikiCarrier,Creature);R(mPelletView,PelletView);R(mShapeObject,ItemShape);R(mConfig,PelletConfig);R(mPelletCollInfo,CollInfo);
#undef F
#undef R
 for(int i=0;i<4;++i)if(!ar.field(("slotFlags."+std::to_string(i)).c_str(),s.mSlotFlags[i]))return false;
 // Shape-backed cargo has real animators; corpse-backed PelletView uses its
 // owner's animation and never initializes this alternate animator storage.
 bool shaped=ar.mode()==Mode::Capture&&s.mShapeObject;
 if(!ar.scalar("shaped",ScalarKind::Bool,&shaped))return false;
 if(shaped){PrefixArchive material(ar,"materials");if(!world_materials_fields(s.mAnimatedMaterials,material))return false;PrefixArchive lower(ar,"lower"),upper(ar,"upper");if(!world_animation_fields(s.mAnimator.mLowerAnimator,lower)||!world_animation_fields(s.mAnimator.mUpperAnimator,upper))return false;}
 if(current==PELSTATE_Goal){auto* p=dynamic_cast<PelletGoalState*>(selected);if(!p)return ar.fail("pellet goal state type");PrefixArchive state(ar,"state");if(!state.field("progress",p->_10)||!state.field("distance",p->mDistanceToTarget)||!state.field("wait",p->mWaitTimer)||!state.field("startScale",p->mStartScaleX)||!state.field("suckProgress",p->mSuckProgress)||!state.field("start",p->mStartPosition)||!state.field("firstMove",p->mIsFirstMove)||!state.field("ship",p->mTargetIsShip)||!state.field("speed",p->mSuckSpeed))return false;}
 if(current==PELSTATE_Appear){auto* p=dynamic_cast<PelletAppearState*>(selected);if(!p)return ar.fail("pellet appear type");PrefixArchive state(ar,"state");if(!state.field("scale",p->mCurrentScale)||!state.field("timer",p->mTransitionTimer))return false;}
 if(current==PELSTATE_UfoLoad){auto* p=dynamic_cast<PelletUfoLoadState*>(selected);if(!p||!ar.field("state.wait",p->mWaitTime))return false;}
 if(!particles(s,outer)||!world_pellet_flags(s,outer))return false;
 if(ar.mode()==Mode::Apply){s.mCurrentState=selected;s.mStateMachine->mLastStateID=last;}
 return true;
}
}
#endif
