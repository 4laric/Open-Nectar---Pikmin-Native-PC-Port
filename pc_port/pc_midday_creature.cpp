#if defined(PIKI_PC_PORT)
#include "pc_midday_creature.h"
#include "Creature.h"
#include <string>
using namespace pc_midday;
namespace {
bool quat(ActorArchive& a,const char* key,Quat& q){PrefixArchive p(a,key);return p.field("v",q.v)&&p.field("s",q.s);}
bool matrix(ActorArchive& a,const char* key,Matrix4f& m){PrefixArchive p(a,key);for(int i=0;i<4;++i)for(int j=0;j<4;++j)if(!p.field((std::to_string(i)+"."+std::to_string(j)).c_str(),m.mMtx[i][j]))return false;return true;}
bool context(ActorArchive& a,const char* key,UpdateContext& c){PrefixArchive p(a,key);return p.ref("manager",RefKind::UpdateMgr,c.mMgr)&&p.field("slot",c.mMgrSlotIndex)&&p.field("piki",c.mIsPiki);}
}
struct PcMiddayCreatureAccess {
 static bool search(Creature& c,ActorArchive& a){
  auto& s=c.mSearchBuffer;PrefixArchive p(a,"search");
  s16 capacity=a.mode()==Mode::Capture?s.mMaxEntries:0,count=a.mode()==Mode::Capture?s.mCurrentEntries:0;
  if(!p.scalar("capacity",ScalarKind::S16,&capacity)||!p.scalar("count",ScalarKind::S16,&count))return false;
  if(capacity<0||capacity>4096||count<0||count>capacity||capacity!=s.mMaxEntries||(capacity&&!s.mDataList))return p.fail("search allocation/count mismatch");
  if(!p.field("last",s.mLastEntry)||!p.field("maxDistance",s.mMaxDistance))return false;
  // All slots were initialized by SearchBuffer::init; inactive SmartPtrs also
  // participate in scene-wide reference reconciliation.
  for(int i=0;i<capacity;++i){PrefixArchive e(p,std::to_string(i).c_str());auto& d=s.mDataList[i];if(!e.ref("target",RefKind::Creature,d.mTargetCreature.mPtr)||!e.field("distance",d.mDistance)||!e.field("iteration",d.mSearchIteration))return false;}
  if(a.mode()==Mode::Apply)s.mCurrentEntries=count;
  return true;
 }
};
namespace pc_midday {
bool creature_fields(Creature& c,ActorArchive& outer){
 PrefixArchive a(outer,"creature");
 int type=a.mode()==Mode::Capture?int(c.mObjType):0;
 if(!a.scalar("objectType",ScalarKind::S32,&type)||type!=int(c.mObjType))return a.fail("Creature concrete type mismatch");
 if(!a.field("mRebirthDay",c.mRebirthDay))return false;
 if(!a.field("mHealth",c.mHealth))return false;
 if(!a.field("mMaxHealth",c.mMaxHealth))return false;
 if(!a.field("mWaterFxTimer",c.mWaterFxTimer))return false;
 if(!a.field("mFaceDirection",c.mFaceDirection))return false;
 if(!a.field("mCreatureFlags",c.mCreatureFlags))return false;
 if(!a.field("mGroundOffset",c.mGroundOffset))return false;
 if(!a.field("mRopePosRatio",c.mRopePosRatio))return false;
 if(!a.field("mPelletStickSlot",c.mPelletStickSlot))return false;
 if(!a.field("mHasCollChangedVelocity",c.mHasCollChangedVelocity))return false;
 if(!a.field("mCollisionOccurred",c.mCollisionOccurred))return false;
 if(!a.field("mSize",c.mSize))return false;
 if(!a.field("mCollisionRadius",c.mCollisionRadius))return false;
 if(!a.field("mIsFrozen",c.mIsFrozen))return false;
 if(!a.field("mIsBeingDamaged",c.mIsBeingDamaged))return false;
 if(!a.field("_30",c._30))return false;
 if(!a.field("_298",c._298))return false;
 if(!a.field("mGrid.mGridPositionX",c.mGrid.mGridPositionX))return false;
 if(!a.field("mGrid.mGridPositionY",c.mGrid.mGridPositionY))return false;
 if(!a.field("mGrid.mGridPositionZ",c.mGrid.mGridPositionZ))return false;
 if(!a.field("mGrid.mWidth",c.mGrid.mWidth))return false;
 if(!a.field("mGrid.mHeight",c.mGrid.mHeight))return false;
 if(!a.field("mGrid.mNeighbourSize",c.mGrid.mNeighbourSize))return false;
 if(!a.field("mFixedPosition",c.mFixedPosition))return false;
 if(!a.field("mVelocity",c.mVelocity))return false;
 if(!a.field("mSRT.s",c.mSRT.s))return false;
 if(!a.field("mSRT.r",c.mSRT.r))return false;
 if(!a.field("mSRT.t",c.mSRT.t))return false;
 if(!a.field("mTargetVelocity",c.mTargetVelocity))return false;
 if(!a.field("_B0",c._B0))return false;
 if(!a.field("mVolatileVelocity",c.mVolatileVelocity))return false;
 if(!a.field("mPrevAngularVelocity",c.mPrevAngularVelocity))return false;
 if(!a.field("mAttachPosition",c.mAttachPosition))return false;
 if(!a.field("mLastPosition",c.mLastPosition))return false;
 if(!a.field("mCollAttachment.mCollSpacePosition",c.mCollAttachment.mCollSpacePosition))return false;
 if(!a.field("mPlatformAdjustDelta",c.mPlatformAdjustDelta))return false;
 if(!a.ref("mFormPoint",RefKind::FormPoint,c.mFormPoint))return false;
 if(!a.ref("mGenerator",RefKind::Generator,c.mGenerator))return false;
 if(!a.ref("mRopeListHead",RefKind::Creature,c.mRopeListHead))return false;
 if(!a.ref("mRope",RefKind::Creature,c.mRope))return false;
 if(!a.ref("mNextRopeHolder",RefKind::Creature,c.mNextRopeHolder))return false;
 if(!a.ref("mPrevRopeHolder",RefKind::Creature,c.mPrevRopeHolder))return false;
 if(!a.ref("mStickListHead",RefKind::Creature,c.mStickListHead))return false;
 if(!a.ref("mStickTarget",RefKind::Creature,c.mStickTarget))return false;
 if(!a.ref("mStickPart",RefKind::CollPart,c.mStickPart))return false;
 if(!a.ref("mNextSticker",RefKind::Creature,c.mNextSticker))return false;
 if(!a.ref("mPrevSticker",RefKind::Creature,c.mPrevSticker))return false;
 if(!a.ref("mFormMgr",RefKind::FormationMgr,c.mFormMgr))return false;
 if(!a.ref("mCollInfo",RefKind::CollInfo,c.mCollInfo))return false;
 if(!a.ref("mProps",RefKind::CreatureProp,c.mProps))return false;
 if(!a.ref("mCollPlatform",RefKind::DynCollObject,c.mCollPlatform))return false;
 if(!a.ref("mCollNormal",RefKind::Vector3,c.mCollNormal))return false;
 if(!a.ref("mPikiPlatformTriangle",RefKind::CollTriInfo,c.mPikiPlatformTriangle))return false;
 if(!a.ref("mGroundTriangle",RefKind::CollTriInfo,c.mGroundTriangle))return false;
 if(!a.ref("mPreviousTriangle",RefKind::CollTriInfo,c.mPreviousTriangle))return false;
 if(!a.ref("mHoldingCreature.mPtr",RefKind::Creature,c.mHoldingCreature.mPtr))return false;
 if(!a.ref("mGrabbedCreature.mPtr",RefKind::Creature,c.mGrabbedCreature.mPtr))return false;
 if(!quat(a,"rotation",c.mRotationQuat)||!matrix(a,"constraint",c.mConstrainedMoveMtx)||!matrix(a,"world",c.mWorldMtx)||!context(a,"searchUpdate",c.mSearchContext)||!context(a,"optUpdate",c.mOptUpdateContext))return false;
 // Conditional payloads are not initialized before their first native use.
 float drag=a.mode()==Mode::Capture&&(c.mCreatureFlags&CF_EnableAirDrag)?c.mAirResistance:0;
 if(!a.scalar("airResistance",ScalarKind::F32,&drag))return false;
 if(a.mode()==Mode::Apply)c.mAirResistance=drag;
 Quat previous(0,0,0,1),delta(0,0,0,1);float progress=0;
 if(a.mode()==Mode::Capture&&c.mHoldingCreature.mPtr){previous=c.mPreGrabRotation;delta=c._100;progress=c._110;}
 if(!quat(a,"preGrab",previous)||!quat(a,"grabDelta",delta)||!a.scalar("grabProgress",ScalarKind::F32,&progress))return false;
 if(a.mode()==Mode::Apply){c.mPreGrabRotation=previous;c._100=delta;c._110=progress;}
 Shape* collision=a.mode()==Mode::Capture&&c.mGroundTriangle?c.mCurrCollisionModel:nullptr;
 if(!a.ref("collisionModel",RefKind::Shape,collision))return false;
 if(a.mode()==Mode::Apply)c.mCurrCollisionModel=collision;
 return PcMiddayCreatureAccess::search(c,a);
}
bool capture_creature(Creature& c,LogicalResolver& r,double now,ActorBytes& out,std::string& e){ActorFields f;FieldArchive a(Mode::Capture,f,r,e,now);std::vector<FieldSchema>s;return creature_fields(c,a)&&a.finish()&&creature_schema(f,s,e)&&validate_actor_fields(f,s,r,e)&&encode_actor_fields(f,out,e);}
bool bind_creature(Creature& c,const ActorBytes& bytes,LogicalResolver& r,double now,std::string& e){if(!validate_creature(bytes,r,int(c.mObjType),e))return false;ActorFields f;if(!decode_actor_fields(bytes,f,e))return false;FieldArchive v(Mode::Validate,f,r,e,now);if(!creature_fields(c,v)||!v.finish())return false;FieldArchive a(Mode::Apply,f,r,e,now);return creature_fields(c,a)&&a.finish();}
}
#endif
