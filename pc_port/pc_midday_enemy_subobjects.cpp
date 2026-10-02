#if defined(PIKI_PC_PORT)
#include "pc_midday_enemy.h"
#include "teki.h"
#include "Peve/MotionEvents.h"
namespace {
using namespace pc_midday;
bool event(PeveEvent& s,ActorArchive& a){if(s.getChildCount()!=0)return a.fail("unexpected actor motion child graph");return a.field("options",s.mEventOptions)&&a.ref("condition",RefKind::Action,s.mCondition);}
bool acceleration(PeveAccelerationEvent& s,ActorArchive& a){return event(s,a)&&a.ref("position",RefKind::Action,s.mPositionIO)&&a.ref("velocity",RefKind::Action,s.mVelocityIO)&&a.ref("acceleration",RefKind::Action,s.mAccelIO);}
}
struct PcMiddayTekiAccess {
 static bool fields(Teki& s,pc_midday::ActorArchive& outer){
 using namespace pc_midday;PrefixArchive a(outer,"enemy.subobjects");
#define F(k,x) if(!a.field(k,x))return false;
#define R(k,t,x) if(!a.ref(k,RefKind::t,x))return false;
 F("status",s.mStatus) F("tableIndex",s.mTableIndex) F("mapCode",s.mMapCode) F("frame",s.mFrameCounter) F("frameMax",s.mFrameCounterMax) F("speed",s.mSpeed) F("turnAngle",s.mTurnAngle) F("dororoGravity",s.mDororoGravity) F("dororoBark",s.mDororoBarkDesire)
 R("work",Creature,s.mWorkObject)
 for(int i=0;i<4;++i)if(!a.field(("foot."+std::to_string(i)).c_str(),s.mFootPosY[i]))return false;
 for(int i=0;i<8;++i)if(!a.ref(("particle."+std::to_string(i)).c_str(),RefKind::ParticleGenerator,s.mPtclGenPtrs[i]))return false;
#define BIT(k,x,max) {u32 v=a.mode()==Mode::Capture?s.mTekiSwitches.x:0;if(!a.scalar(k,ScalarKind::U32,&v)||v>max)return a.fail("invalid Teki switch");if(a.mode()==Mode::Apply)s.mTekiSwitches.x=v;}
 BIT("bite",mBite,1) BIT("runAway",mRunAway,1) BIT("stay",mStay,1) BIT("flying",mFlying,1) BIT("timer",mTimer,1) BIT("choke",mChoke,1) BIT("footEffect",mFootEffect,15)
#undef BIT
 auto& e=s.mEffectAttackParam;
 F("attack.time",e.mCurrentTime) F("attack.duration",e.mDuration) F("attack.radius",e.mRadius) F("attack.range",e.mMaxRange) F("attack.position",e.mPosition) F("attack.velocity",e.mVelocity) F("attack.direction",e.mDirection) F("attack.damage",e.mDamage)
 R("attack.owner",Creature,e.mTeki) R("attack.emitter1",ParticleGenerator,e.mSubEmitter1) R("attack.emitter2",ParticleGenerator,e.mSubEmitter2) R("attack.callback",Action,e.mCallBackRef)
 u32 moving=a.mode()==Mode::Capture?e.mState.mIsMoving:0;if(!a.scalar("attack.moving",ScalarKind::U32,&moving)||moving>1)return a.fail("invalid attack motion flag");if(a.mode()==Mode::Apply)e.mState.mIsMoving=moving;
 R("cone.param",Action,s.mConeCallBack.mParam) F("cone.angle",s.mConeCallBack.mConeHalfAngle) R("cylinder.param",Action,s.mCylinderCallBack.mParam) R("event.param",Action,s.mEventCallBack.mParam)
 if(!s.mAccelEvent||!s.mParabolaEvent||!s.mCircleMoveEvent||!s.mSinWaveEvent)return a.fail("missing motion event topology");
 PrefixArchive accel(a,"accel");if(!acceleration(*s.mAccelEvent,accel))return false;
 PrefixArchive parabola(a,"parabola");if(!acceleration(*s.mParabolaEvent,parabola)||!parabola.field("gravity",static_cast<Vector3f&>(s.mParabolaEvent->mGravityAccelIO._04))||!parabola.field("velocityValue",static_cast<Vector3f&>(s.mParabolaEvent->mClampedVelocityIO._04))||!parabola.field("velocityLimit",s.mParabolaEvent->mClampedVelocityIO.mMaxLength))return false;
 auto& c=*s.mCircleMoveEvent;PrefixArchive circle(a,"circle");if(!event(c,circle))return false;
#define CF(x) if(!circle.field(#x,c.x))return false;
 CF(mAngle) CF(mPositionLerpFactor) CF(mRadius) CF(mHeightOffset) CF(mAngularSpeed) CF(mTimeCondition.mCurrTime) CF(mTimeCondition.mLimit)
#undef CF
 if(!circle.ref("position",RefKind::Action,c.mPositionIO)||!circle.ref("center",RefKind::Action,c.mCenterPositionIO))return false;
 auto& w=*s.mSinWaveEvent;PrefixArchive wave(a,"wave");if(!event(w,wave)||!wave.ref("position",RefKind::Action,w.mPositionIO)||!wave.field("velocity",static_cast<Vector3f&>(w.mLinearVelocity)))return false;
#define WF(x) if(!wave.field(#x,w.x))return false;
 WF(mOffset) WF(mAmplitude) WF(mStartingTheta) WF(mAngularVelocity) WF(mTheta)
#undef WF
#undef F
#undef R
 return true;
 }
};
namespace pc_midday {bool enemy_subobject_fields(Teki& s,ActorArchive& a){return PcMiddayTekiAccess::fields(s,a);}}
#endif
