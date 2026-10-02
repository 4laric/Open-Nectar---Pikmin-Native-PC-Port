#if defined(PIKI_PC_PORT)
#include "pc_midday_enemy.h"
#include "teki.h"
#include "TAI/Action.h"
#include "YaiStrategy.h"
#include "nlib/Function.h"
namespace pc_midday {
bool enemy_animation_fields(PaniAnimator& s,ActorArchive& ar) {
 if(ar.mode()==Mode::Capture&&(!s.mMgr||!s.mContext||!s.mMotionTable||!s.mAnimInfo))return ar.fail("enemy animator is not initialized");
#define A(x) if(!ar.field(#x,s.x))return false;
 A(mPlayState) A(mCurrentAnimID) A(mStartKeyIndex) A(mEndKeyIndex) A(mAnimationCounter)
 A(mCurrentKeyIndex) A(mPreviousKeyIndex) A(mMotionIdx) A(mIsFinished)
#undef A
#define R(x,k) if(!ar.ref(#x,RefKind::k,s.x))return false;
 R(mMgr,Animation) R(mContext,Animation) R(mAnimInfo,Animation) R(mMotionTable,Animation) R(mListener,AnimListener)
#undef R
 return true;
}
bool enemy_base_fields(Teki& s,ActorArchive& outer) {
 PrefixArchive ar(outer,"enemy.base");
 int type=ar.mode()==Mode::Capture?s.mTekiType:0;
 if(!ar.scalar("type",ScalarKind::S32,&type)||!enemy_registration(type)||type!=s.mTekiType)return ar.fail("enemy concrete allocation mismatch");
 int state=ar.mode()==Mode::Capture?s.mStateID:0;
 if(!ar.scalar("state",ScalarKind::S32,&state))return false;
 auto* tai=dynamic_cast<TaiStrategy*>(s.getStrategy());auto* yai=dynamic_cast<YaiStrategy*>(s.getStrategy());
 int countStates=tai?tai->mStateCount:yai?yai->mStateCount:0;TaiState** states=tai?tai->mStateList:yai?yai->mStateList:nullptr;
 if(!states||state<0||state>=countStates||!states[state])return ar.fail("unregistered enemy strategy state");
 // Both registered strategy implementations use the same immutable TaiState
 // catalog; restore binds the actor index without calling either transition.
 TaiState* stateObject=states[state];
 if(!ar.ref("strategyState",RefKind::Action,stateObject)||(ar.mode()==Mode::Apply&&stateObject!=states[state]))return ar.fail("enemy strategy catalog mismatch");
 if(ar.mode()==Mode::Apply)s.mStateID=state;
#define FIELD(k,x) if(!ar.field(#x,s.x))return false;
#define VECTOR(x) if(!ar.field(#x,static_cast<Vector3f&>(s.x)))return false;
#include "pc_midday_enemy_base_fields.inc"
#undef FIELD
#undef VECTOR
 int capacity=ar.mode()==Mode::Capture?s.mRouteWayPointMax:0,count=ar.mode()==Mode::Capture?s.mRouteWayPointCount:0;
 if(!ar.scalar("mRouteWayPointMax",ScalarKind::S32,&capacity)||!ar.scalar("mRouteWayPointCount",ScalarKind::S32,&count)||capacity<0||capacity>4096||capacity!=s.mRouteWayPointMax||count<0||count>capacity||(count&&!s.mRouteWayPoints))return ar.fail("enemy route allocation bounds");
 if(ar.mode()==Mode::Apply)s.mRouteWayPointCount=count;
 for(int i=0;i<count;++i)if(!ar.ref(("route."+std::to_string(i)).c_str(),RefKind::WayPoint,s.mRouteWayPoints[i]))return false;
 // This is a four-character graph ID, not the Piki asynchronous route token.
 if(!ar.field("routeGraph",s.mPathHandle))return false;
 for(int i=0;i<5;++i)if(!ar.field(("timer."+std::to_string(i)).c_str(),s.mTimers[i]))return false;
 for(int i=0;i<4;++i)if(!ar.ref(("target."+std::to_string(i)).c_str(),RefKind::Creature,s.mTargetCreatures[i].mPtr)||!ar.ref(("particle."+std::to_string(i)).c_str(),RefKind::ParticleGenerator,s.mParticleGenerators[i]))return false;
 for(int i=0;i<8;++i)if(!ar.field(("corpseJoint."+std::to_string(i)).c_str(),s.mCorpsePartJoints[i]))return false;
 if(!ar.ref("pellet",RefKind::Creature,s.mPellet))return false;
 if(!s.mTekiAnimator||!s.mPersonality||!s.mVibrationController)return ar.fail("missing enemy owned subobject");
 PrefixArchive animation(ar,"animation");if(!enemy_animation_fields(*s.mTekiAnimator,animation))return false;
 if(!ar.field("vibration.phase",s.mVibrationController->mPhase)||!ar.field("vibration.frequency",s.mVibrationController->mAngularFreq)||!ar.field("vibration.amplitude",s.mVibrationController->mAmplitude))return false;
 auto& p=*s.mPersonality;
 if(!ar.field("personality.position",p.mPosition)||!ar.field("personality.nest",p.mNestPosition)||!ar.field("personality.facing",p.mFaceDirection)||!ar.field("personality.pelletKind",p.mPelletKind)||!ar.field("personality.pelletColor",p.mPelletColor))return false;
 u32 id=ar.mode()==Mode::Capture?p.mID.mId:0;if(!ar.scalar("personality.id",ScalarKind::U32,&id))return false;if(ar.mode()==Mode::Apply)p.mID.setID(id);
 for(int i=0;i<5;++i){int v=ar.mode()==Mode::Capture?p.getI(i):0;float f=ar.mode()==Mode::Capture?p.getF(i):0;if(!ar.scalar(("personality.int."+std::to_string(i)).c_str(),ScalarKind::S32,&v)||!ar.scalar(("personality.float."+std::to_string(i)).c_str(),ScalarKind::F32,&f))return false;if(ar.mode()==Mode::Apply){p.setI(i,v);p.setF(i,f);}}
 return true;
}
}
#endif
