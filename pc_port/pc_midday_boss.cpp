#if defined(PIKI_PC_PORT)
#include "pc_midday_enemy.h"
#include "Boss.h"
#include "Kogane.h"
#include "Pom.h"
#include "Mizu.h"
struct PcMiddayEnemyAccess {
 static bool family(Boss& body,int family,pc_midday::ActorArchive& outer) {
 using namespace pc_midday;PrefixArchive ar(outer,"boss.family");int saved=ar.mode()==Mode::Capture?family:0;
 if(!ar.scalar("id",ScalarKind::S32,&saved)||saved!=family||body.mObjType!=boss_object_type(family))return ar.fail("boss family allocation mismatch");
#define F(key,x) if(!ar.field(key,x))return false;
#define R(key,kind,x) if(!ar.ref(key,RefKind::kind,x))return false;
 if(family==4){
  auto* s=dynamic_cast<Kogane*>(&body);if(!s||!s->mKoganeAi||!s->mKoganeAi->mRippleCallBack)return ar.fail("missing Kogane constructor topology");auto& a=*s->mKoganeAi;
  F("appear",s->mIsAppear) F("pelletPending",s->mCreatePelletPending) F("inWater",a.mInWater) F("dropCount",a.mDropCount) F("appearTimer",a.mAppearTimer) F("idleDuration",a.mIdleDuration)
  int effect=ar.mode()==Mode::Capture?a.mEffectType:0;if(!ar.scalar("effectType",ScalarKind::S32,&effect))return false;if(ar.mode()==Mode::Apply)a.mEffectType=static_cast<EffectMgr::effTypeTable>(effect);
  R("owner",Creature,a.mKogane) R("rippleOwner",Creature,a.mRippleCallBack->mKogane)
 }else if(family==5){
  auto* s=dynamic_cast<Pom*>(&body);if(!s||!s->mPomAi||!s->mPomAi->mOpenStarCallBack)return ar.fail("missing Pom constructor topology");auto& a=*s->mPomAi;
  F("touching",s->mIsPikiOrPlayerTouching) F("color",s->mColor) F("collided",a.mHasCollided) F("playSound",a.mPlaySound) F("opening",a.mIsOpening) F("previousPiki",a.mPrevStickPikiCount) F("released",a.mReleasedSeedCount) F("maximum",a.mMaxSeedCount) F("deformAmount",a.mDeformAmount) F("currentDeform",a.mCurrentDeform) R("owner",Creature,a.mPom)
  bool callbackBound=ar.mode()==Mode::Capture&&a.mOpenStarCallBack->mIsActive;
  if(ar.mode()==Mode::Capture&&callbackBound&&a.mOpenStarCallBack->mIsActive!=&a.mIsOpening)return ar.fail("foreign Pom callback member");
  if(!ar.scalar("callbackBound",ScalarKind::Bool,&callbackBound))return false;if(ar.mode()==Mode::Apply)a.mOpenStarCallBack->mIsActive=callbackBound?&a.mIsOpening:nullptr;
 }else if(family==10||family==11){
  auto* s=dynamic_cast<Mizu*>(&body);if(!s||!s->mMizuAi||!s->mMizuAi->mPuffCallBack)return ar.fail("missing Mizu constructor topology");auto& a=*s->mMizuAi;
  F("visible",s->mIsVisible) R("owner",Creature,a.mMizu) R("bubble",ParticleGenerator,a._08) R("mist",ParticleGenerator,a._0C) R("puff",ParticleGenerator,a.mPuffCallBack->mPtcl)
 }else return ar.fail("boss family payload not implemented");
#undef F
#undef R
 return true;
 }
 static bool boss(Boss& s,pc_midday::ActorArchive& outer) {
 using namespace pc_midday;PrefixArchive ar(outer,"boss.base");
#define FIELD(k,x) if(!ar.field(#x,s.x))return false;
#define VECTOR(x) if(!ar.field(#x,s.x))return false;
#include "pc_midday_boss_base_fields.inc"
#undef FIELD
#undef VECTOR
 if(!ar.ref("target",RefKind::Creature,s.mTargetCreature))return false;
 // Wall members are initialized only by a collision. Inactive values are not
 // observed by the engine; canonicalize them without reading indeterminate data.
 DynCollObject* wall=ar.mode()==Mode::Capture&&s.mIsOnWall?s.mWallCollObject:nullptr;
 float offset=ar.mode()==Mode::Capture&&s.mIsOnWall?s.mWallPlane.mOffset:0;
 Vector3f normal;if(ar.mode()==Mode::Capture&&s.mIsOnWall)normal=s.mWallPlane.mNormal;
 if(!ar.ref("wallObject",RefKind::DynCollObject,wall)||!ar.field("wall.normal",normal)||!ar.scalar("wall.offset",ScalarKind::F32,&offset))return false;
 if(ar.mode()==Mode::Apply){s.mWallCollObject=wall;s.mWallPlane.mNormal=normal;s.mWallPlane.mOffset=offset;}
 u32 id=ar.mode()==Mode::Capture?s.mPelletID.mId:0;if(!ar.scalar("pelletID",ScalarKind::U32,&id))return false;if(ar.mode()==Mode::Apply)s.mPelletID.setID(id);
 PrefixArchive animation(ar,"animation");return enemy_animation_fields(s.mAnimator,animation);
 }
};
namespace pc_midday { bool boss_small_family_fields(Boss& s,int id,ActorArchive& ar){return PcMiddayEnemyAccess::family(s,id,ar);}
bool boss_base_fields(Boss& s,ActorArchive& ar){return PcMiddayEnemyAccess::boss(s,ar);} }
#endif
