#if defined(PIKI_PC_PORT)
#include "pc_midday_world.h"
#include "pc_midday_world_resources.h"
#include "pc_midday_particle.h"
#include "BuildingItem.h"
#include "DoorItem.h"
#include "FishItem.h"
#include "GoalItem.h"
#include "ItemObject.h"
#include "KeyItem.h"
#include "KusaItem.h"
#include "MizuItem.h"
#include "PikiHeadItem.h"
#include "Plane.h"
#include "PlantMgr.h"
#include "RopeCreature.h"
#include "SeedItem.h"
#include "UfoItem.h"
#include "WeedsItem.h"
#include "WorkObject.h"
#include <set>
#include <type_traits>
using namespace pc_midday;
template<class T> bool enumField(ActorArchive& ar,const char* key,T& field){if constexpr(!std::is_enum<T>::value)return ar.field(key,field);int value=ar.mode()==Mode::Capture?static_cast<int>(field):0;if(!ar.scalar(key,ScalarKind::S32,&value))return false;if(ar.mode()==Mode::Apply)field=static_cast<T>(value);return true;}
struct PcMiddayWorldAccess {
static bool goal(GoalItem& s,ActorArchive& outer){
 PrefixArchive ar(outer,"world.goal");
 if(!ar.field("mColourFadeRate",s.mColourFadeRate))return false;
 if(!ar.field("mColourAnimProgress",s.mColourAnimProgress))return false;
 if(!ar.field("mColourAnimationEnabled",s.mColourAnimationEnabled))return false;
 if(!ar.field("mSpotEffectActive",s.mSpotEffectActive))return false;
 if(!ar.field("mIsClosing",s.mIsClosing))return false;
 if(!ar.field("mConeSizeTimer",s.mConeSizeTimer))return false;
 if(!ar.field("_3FC",s._3FC))return false;
 if(!ar.field("mIsConeEmit",s.mIsConeEmit))return false;
 if(!ar.field("mIsDispensingPikis",s.mIsDispensingPikis))return false;
 if(!ar.field("mPikisToExit",s.mPikisToExit))return false;
 if(!ar.field("mPikiSpawnTimer",s.mPikiSpawnTimer))return false;
 if(!ar.field("_41C",s._41C))return false;
 if(!ar.field("mOnionColour",s.mOnionColour))return false;
 if(!ar.field("mWaypointIdx",s.mWaypointIdx))return false;
 if(!ar.field("mPcOwner",s.mPcOwner))return false;
 if(!ar.ref("mSpotEfx",RefKind::ParticleGenerator,s.mSpotEfx))return false;
 if(!ar.ref("mHaloEfx",RefKind::ParticleGenerator,s.mHaloEfx))return false;
 if(!ar.ref("mSuckEfx",RefKind::ParticleGenerator,s.mSuckEfx))return false;
 if(!ar.ref("mSpotModelEff",RefKind::Effect,s.mSpotModelEff))return false;
 for(int i=0;i<3;++i){PrefixArchive part(ar,("slot."+std::to_string(i)).c_str());if(!part.field("held",s.mHeldPikis[i])||!part.ref("shape",RefKind::ItemShape,s._438[i])||!part.ref("fulcrum",RefKind::Creature,s.mLegs[i].mFulcrum)||!part.ref("rope",RefKind::Creature,s.mLegs[i].mRope))return false;} for(int i=0;i<PC_COOP_CAPTAINS;++i)if(!ar.field(("exitFor."+std::to_string(i)).c_str(),s.mPcExitFor[i]))return false;
 {PrefixArchive a(ar,"materials");if(!world_materials_fields(s.mAnimatedMaterials,a))return false;}

 return true;
}
static bool building(BuildingItem& s,ActorArchive& outer){
 PrefixArchive ar(outer,"world.building");
 if(!ar.field("mNumStages",s.mNumStages))return false;
 if(!ar.field("mCurrStage",s.mCurrStage))return false;
 if(!ar.field("_448",s._448))return false;
 if(!ar.ref("mWayPoint",RefKind::WayPoint,s.mWayPoint))return false;
 {PrefixArchive a(ar,"materials");if(!world_materials_fields(s.mAnimatedMaterials,a))return false;}
 {PrefixArchive a(ar,"platforms");if(!world_platform_fields(s.mPlatMgr,a))return false;}
 {PrefixArchive a(ar,"effect0");if(!particle_fields(s._3D8,a))return false;}
 {PrefixArchive a(ar,"effect1");if(!particle_fields(s._3E8,a))return false;}

 return true;
}
static bool head(PikiHeadItem& s,ActorArchive& outer){
 PrefixArchive ar(outer,"world.head");
 if(!ar.field("mSeedColor",s.mSeedColor))return false;
 if(!ar.field("mFlowerStage",s.mFlowerStage))return false;
 if(!ar.field("mGlowEffectPos",s.mGlowEffectPos))return false;
 if(!ar.field("mPcOwner",s.mPcOwner))return false;
 if(!ar.field("mP2Purple",s.mP2Purple))return false;
 if(!ar.field("mP2White",s.mP2White))return false;
 if(!ar.field("mP2Bulbmin",s.mP2Bulbmin))return false;
 if(!ar.ref("mFreeLightEfx",RefKind::Effect,s.mFreeLightEfx))return false;
 if(!ar.ref("mParentOnion",RefKind::Creature,s.mParentOnion))return false;
 if(!ar.ref("mRippleEfx",RefKind::Effect,s.mRippleEfx))return false;
 {PrefixArchive a(ar,"sparkle");if(!particle_fields(s.mSparkleEffect,a))return false;}

 return true;
}
static bool stick(KusaItem& s,ActorArchive& outer){
 PrefixArchive ar(outer,"world.stick");
 if(!ar.field("mGroundPosition",s.mGroundPosition))return false;
 if(!ar.ref("mBaseItem",RefKind::Creature,s.mBaseItem))return false;
 return true;
}
static bool stickBase(BoBaseItem& s,ActorArchive& outer){
 PrefixArchive ar(outer,"world.stickBase");
 if(!ar.field("mGroundPosition",s.mGroundPosition))return false;
 if(!ar.field("mIsActive",s.mIsActive))return false;
 if(!ar.field("mEffectDuration",s.mEffectDuration))return false;
 if(!ar.ref("mStickItem",RefKind::Creature,s.mStickItem))return false;
 if(!ar.ref("mParticleGenerator",RefKind::ParticleGenerator,s.mParticleGenerator))return false;
 return true;
}
static bool bombGen(BombGenItem& s,ActorArchive& outer){
 PrefixArchive ar(outer,"world.bombGen");
 if(!ar.field("mCapacity",s.mCapacity))return false;
 if(!ar.field("mRemaining",s.mRemaining))return false;
 return true;
}
static bool weeds(WeedsGen& s,ActorArchive& outer){
 PrefixArchive ar(outer,"world.weeds");
 if(!ar.field("mWeedsCount",s.mWeedsCount))return false;
 if(!ar.ref("mWeedShape",RefKind::Shape,s.mWeedShape))return false;
 if(!ar.ref("mWeedsGenProps",RefKind::CreatureProp,s.mWeedsGenProps))return false;
 return true;
}
static bool weed(Weed& s,ActorArchive& outer){
 PrefixArchive ar(outer,"world.weed");
 if(!ar.field("mIsPulled",s.mIsPulled))return false;
 if(!ar.field("mPulloutTimer",s.mPulloutTimer))return false;
 if(!ar.ref("mGen",RefKind::Creature,s.mGen))return false;
 return true;
}
static bool rope(RopeItem& s,ActorArchive& outer){
 PrefixArchive ar(outer,"world.rope");
 if(!ar.field("mRopeLength",s.mRopeLength))return false;
 if(!ar.field("mRopeDirection",s.mRopeDirection))return false;
 if(!ar.field("_2D0",s._2D0))return false;
 if(!ar.ref("mParentRope",RefKind::Creature,s.mParentRope))return false;
 if(!ar.ref("mAttachedObj",RefKind::Creature,s.mAttachedObj))return false;
 if(!ar.ref("mModel",RefKind::Shape,s.mModel))return false;
 if(!ar.ref("mOwner",RefKind::Creature,s.mOwner))return false;
 return true;
}
static bool seed(SeedItem& s,ActorArchive& outer){
 PrefixArchive ar(outer,"world.seed");
 if(!enumField(ar,"mStateId",s.mStateId))return false;
 if(!ar.field("mGrowthTimer",s.mGrowthTimer))return false;
 if(!ar.ref("mCurrentShape",RefKind::Shape,s.mCurrentShape))return false;
 if(!ar.ref("mSeedShape",RefKind::Shape,s.mSeedShape))return false;
 if(!ar.ref("mPlantedShape",RefKind::Shape,s.mPlantedShape))return false;
 return true;
}
static bool key(KeyItem& s,ActorArchive& outer){
 PrefixArchive ar(outer,"world.key");
 if(!enumField(ar,"mState",s.mState))return false;
 if(!ar.ref("mModel",RefKind::Shape,s.mModel))return false;
 return true;
}
static bool plant(Plant& s,ActorArchive& outer){
 PrefixArchive ar(outer,"world.plant");
 if(!ar.field("mPlantType",s.mPlantType))return false;
 if(!ar.field("mMotionSpeed",s.mMotionSpeed))return false;
 if(!ar.field("mIsCulled",s.mIsCulled))return false;
 if(!ar.field("_394",s._394))return false;
 if(!world_ai_fields(s,outer))return false;PrefixArchive anim(ar,"animation");if(!world_animation_fields(s.mPlantAnimator,anim))return false;
 return true;
}
static bool bridge(Bridge& s,ActorArchive& outer){
 PrefixArchive ar(outer,"world.bridge");
 int allocated=s.mStageCount; int saved=ar.mode()==Mode::Capture?allocated:0; if(!ar.scalar("allocation",ScalarKind::S32,&saved)||saved!=allocated||saved>4096)return ar.fail("world allocation size mismatch");
 if(!ar.field("mDoUseJointSegments",s.mDoUseJointSegments))return false;
 if(!ar.field("_3CA",s._3CA))return false;
 if(!ar.field("_3CC",s._3CC))return false;
 if(!ar.field("_400",s._400))return false;
 if(!ar.field("mStageCount",s.mStageCount))return false;
 if(!ar.field("_424",s._424))return false;
 if(!ar.ref("mStartWaypoint",RefKind::WayPoint,s.mStartWaypoint))return false;
 if(!ar.ref("mEndWaypoint",RefKind::WayPoint,s.mEndWaypoint))return false;
 if(!ar.ref("mBuildShape",RefKind::DynBuildShape,s.mBuildShape))return false;
 if(!ar.ref("mBridgeShape",RefKind::Shape,s.mBridgeShape))return false;
 if(!ar.ref("_410",RefKind::CollPart,s._410))return false;
 if(s.mStageCount<0||s.mStageCount>4096||(s.mStageCount&&!s.mStageProgressList)||(s.mDoUseJointSegments&&s.mStageCount&&!s.mStageJoints))return ar.fail("missing bridge array");for(int i=0;i<s.mStageCount;++i)if(!ar.field(("stage."+std::to_string(i)+".progress").c_str(),s.mStageProgressList[i]))return false;if(s.mDoUseJointSegments)for(int i=0;i<2*s.mStageCount;++i)if(!ar.ref(("joint."+std::to_string(i)).c_str(),RefKind::Joint,s.mStageJoints[i]))return false;
 {PrefixArchive a(ar,"materials");if(!world_materials_fields(s.mAnimatedMaterials,a))return false;}
 {PrefixArchive a(ar,"effect0");if(!particle_fields(s._3D8,a))return false;}
 {PrefixArchive a(ar,"effect1");if(!particle_fields(s._3E8,a))return false;}
 bool build=ar.mode()==Mode::Capture&&s.mBuildShape;if(!ar.scalar("buildPresent",ScalarKind::Bool,&build)||build!=(s.mBuildShape!=nullptr))return ar.fail("build collision allocation mismatch");if(build){PrefixArchive a(ar,"build");if(!world_dyn_shape_fields(*s.mBuildShape,a))return false;}

 return true;
}
static bool hinder(HinderRock& s,ActorArchive& outer){
 PrefixArchive ar(outer,"world.hinder");
 if(!ar.field("mPushingPikmin",s.mPushingPikmin))return false;
 if(!ar.field("mDestinationPosition",s.mDestinationPosition))return false;
 if(!ar.field("mTotalPushStrength",s.mTotalPushStrength))return false;
 if(!ar.field("mAmountPushersToStart",s.mAmountPushersToStart))return false;
 if(!ar.field("mPushSpeed",s.mPushSpeed))return false;
 if(!ar.field("mCentreSize",s.mCentreSize))return false;
 if(!enumField(ar,"mState",s.mState))return false;
 if(!ar.field("mFxCooldownTimer",s.mFxCooldownTimer))return false;
 if(!ar.field("mPushMoveTimer",s.mPushMoveTimer))return false;
 if(!ar.field("mIsMoving",s.mIsMoving))return false;
 if(!ar.field("mIsSoundPlaying",s.mIsSoundPlaying))return false;
 if(!ar.field("mMoveFrontEfxPos",s.mMoveFrontEfxPos))return false;
 if(!ar.ref("mWayPoint",RefKind::WayPoint,s.mWayPoint))return false;
 if(!ar.ref("mBuildShape",RefKind::DynBuildShape,s.mBuildShape))return false;
 if(!ar.ref("mBoxShape",RefKind::Shape,s.mBoxShape))return false;
 if(!ar.ref("mEfxA",RefKind::ParticleGenerator,s.mEfxA))return false;
 if(!ar.ref("mEfxB",RefKind::ParticleGenerator,s.mEfxB))return false;
 if(!ar.ref("mEfxC",RefKind::ParticleGenerator,s.mEfxC))return false;
 for(int i=0;i<4;++i){PrefixArchive p(ar,("plane."+std::to_string(i)).c_str());if(!p.field("normal",s.mPlanes[i].mNormal)||!p.field("offset",s.mPlanes[i].mOffset))return false;}for(int i=0;i<2;++i)if(!ar.field(("side."+std::to_string(i)).c_str(),s.mMoveSideEfxPos[i]))return false;
 bool build=ar.mode()==Mode::Capture&&s.mBuildShape;if(!ar.scalar("buildPresent",ScalarKind::Bool,&build)||build!=(s.mBuildShape!=nullptr))return ar.fail("build collision allocation mismatch");if(build){PrefixArchive a(ar,"build");if(!world_dyn_shape_fields(*s.mBuildShape,a))return false;}

 return true;
}
static bool grass(GrassGen& s,ActorArchive& outer){
 PrefixArchive ar(outer,"world.grass");
 u16 allocated=s.mTotalGrassCount; u16 saved=ar.mode()==Mode::Capture?allocated:0; if(!ar.scalar("allocation",ScalarKind::U16,&saved)||saved!=allocated||saved>4096)return ar.fail("world allocation size mismatch");
 if(!ar.field("mWorkingPikis",s.mWorkingPikis))return false;
 if(!ar.field("mActiveGrass",s.mActiveGrass))return false;
 if(!ar.field("mTotalGrassCount",s.mTotalGrassCount))return false;
 if(!ar.field("_3D4",s._3D4))return false;
 if(!ar.field("mSize",s.mSize))return false;
 if(!ar.ref("mGrass",RefKind::Grass,s.mGrass))return false;
 if(s.mTotalGrassCount&&!s.mGrass)return ar.fail("missing grass array");for(int i=0;i<s.mTotalGrassCount;++i){PrefixArchive part(ar,("entry."+std::to_string(i)).c_str());auto& g=s.mGrass[i];if(!part.field("position",g.mPosition)||!part.field("health",g.mHealth)||!part.field("shape",g.mGrassShapeId)||!part.field("rotation",g.mRotationDegrees))return false;}
 return true;
}
static bool rocks(RockGen& s,ActorArchive& outer){
 PrefixArchive ar(outer,"world.rocks");
 u16 allocated=s.mMaxPebbles; u16 saved=ar.mode()==Mode::Capture?allocated:0; if(!ar.scalar("allocation",ScalarKind::U16,&saved)||saved!=allocated||saved>4096)return ar.fail("world allocation size mismatch");
 if(!ar.field("mWorkingPikis",s.mWorkingPikis))return false;
 if(!ar.field("_3CC",s._3CC))return false;
 if(!ar.field("mActivePebbles",s.mActivePebbles))return false;
 if(!ar.field("mMaxPebbles",s.mMaxPebbles))return false;
 if(!ar.field("_3D8",s._3D8))return false;
 if(!ar.field("mSize",s.mSize))return false;
 if(!ar.ref("mPebbles",RefKind::Pebble,s.mPebbles))return false;
 if(s.mMaxPebbles&&!s.mPebbles)return ar.fail("missing pebble array");for(int i=0;i<s.mMaxPebbles;++i){PrefixArchive part(ar,("entry."+std::to_string(i)).c_str());auto& g=s.mPebbles[i];if(!part.field("position",g.mPosition)||!part.field("health",g.mHealth)||!part.field("shape",g.mShapeIndex)||!part.field("rotation",g.mRotationDegrees))return false;}
 return true;
}
static bool fish(FishGenerator& s,ActorArchive& outer){
 PrefixArchive ar(outer,"world.fish");
 int allocated=s.mMaxFish; int saved=ar.mode()==Mode::Capture?allocated:0; if(!ar.scalar("allocation",ScalarKind::S32,&saved)||saved!=allocated||saved>4096)return ar.fail("world allocation size mismatch");
 if(!ar.field("mFishCount",s.mFishCount))return false;
 if(!ar.field("mMaxFish",s.mMaxFish))return false;
 if(!ar.field("mSchoolCentre",s.mSchoolCentre))return false;
 if(s.mMaxFish<0||s.mMaxFish>4096||(s.mMaxFish&&!s.mFish))return ar.fail("missing fish array");for(int i=0;i<s.mMaxFish;++i){PrefixArchive part(ar,("entry."+std::to_string(i)).c_str());auto& g=s.mFish[i];if(!part.field("position",g.mPosition)||!part.field("velocity",g.mVelocity)||!part.field("direction",g.mDirection))return false;}
 return true;
}
static bool ship(UfoItem& s,ActorArchive& outer){
 PrefixArchive ar(outer,"world.ship");
 if(!ar.field("mIsMenuOpen",s.mIsMenuOpen))return false;
 if(!ar.field("mIsLightActive",s.mIsLightActive))return false;
 if(!ar.field("mShouldLightActivate",s.mShouldLightActivate))return false;
 if(!ar.field("mIsTroubleFxEnabled",s.mIsTroubleFxEnabled))return false;
 if(!ar.field("mTroubleFxTimer",s.mTroubleFxTimer))return false;
 if(!ar.field("mTroubleFxState",s.mTroubleFxState))return false;
 if(!ar.field("mJetLevel",s.mJetLevel))return false;
 if(!ar.field("mShipUpgradeLevel",s.mShipUpgradeLevel))return false;
 if(!ar.field("mConeEffectId",s.mConeEffectId))return false;
 if(!ar.field("mPca1FxPosition",s.mPca1FxPosition))return false;
 if(!ar.field("mPca2FxPosition",s.mPca2FxPosition))return false;
 if(!ar.field("mIsPca1FxActive",s.mIsPca1FxActive))return false;
 if(!ar.field("mIsPca2FxActive",s.mIsPca2FxActive))return false;
 if(!ar.field("mSpotlightPosition",s.mSpotlightPosition))return false;
 if(!ar.field("mWaypointID",s.mWaypointID))return false;
 if(!ar.field("mPcOwner",s.mPcOwner))return false;
 if(!ar.field("mNeedPathfindRefresh",s.mNeedPathfindRefresh))return false;
 if(!ar.ref("mRingFx",RefKind::ParticleGenerator,s.mRingFx))return false;
 if(!ar.ref("mSparkleFx",RefKind::ParticleGenerator,s.mSparkleFx))return false;
 if(!ar.ref("mShipModel",RefKind::UfoShape,s.mShipModel))return false;
 if(!shipAnimations(s,ar))return false;
 if(!ar.ref("materials",RefKind::ShapeDynMaterials,s.mAnimatedMaterialsList))return false;

 return true;
}
static bool door(DoorItem& s,ActorArchive& outer){
 PrefixArchive ar(outer,"world.door");
 if(!ar.field("mFadeTimer",s.mFadeTimer))return false;
 if(!enumField(ar,"mStateId",s.mStateId))return false;
 if(!ar.ref("mDestinationStagePath",RefKind::StaticText,s.mDestinationStagePath))return false;
 if(!ar.ref("mLabelText",RefKind::StaticText,s.mLabelText))return false;
 return true;
}
static bool shipAnimations(UfoItem& s,ActorArchive& ar);
};
bool PcMiddayWorldAccess::shipAnimations(UfoItem& s,ActorArchive& ar){
 if(!s.mAnimator.mAnims||!s.mAnimator.mAnimSpeeds)return ar.fail("missing ship animator allocation");
 for(int i=0;i<8;++i){PrefixArchive part(ar,("anim."+std::to_string(i)).c_str());if(!part.field("speed",s.mAnimator.mAnimSpeeds[i]))return false;PrefixArchive anim(part,"animation");if(!world_animation_fields(s.mAnimator.mAnims[i],anim))return false;}
 for(int i=0;i<3;++i){PrefixArchive part(ar,("spot."+std::to_string(i)).c_str());auto& v=s.mSpots[i];if(!part.field("position",v.mPosition)||!part.field("radius",v.mRadius)||!part.field("angle",v.mAngleOffset)||!part.field("rotation",v.mRotationTime))return false;}
 for(int i=0;i<6;++i){PrefixArchive part(ar,("trouble."+std::to_string(i)).c_str());if(!part.field("position",s.mTroubleFxPositionList[i])||!part.ref("emitter",RefKind::ParticleGenerator,s.mTroubleFxGenList[i]))return false;}
 for(int i=0;i<4;++i){PrefixArchive part(ar,("light."+std::to_string(i)).c_str());auto& v=s.mLightAnims[i];if(!part.ref("materials",RefKind::ShapeDynMaterials,v.mAnimatedMaterials)||!part.field("frame",v.mFrame)||!part.field("speed",v.mSpeed)||!part.field("type",v.mType))return false;for(int j=0;j<4;++j)if(!part.ref(("engine."+std::to_string(j)).c_str(),RefKind::ParticleGenerator,s.mEngineParticleGenList[i][j]))return false;}
 return true;
}
namespace pc_midday {
bool world_structure_fields(Creature& s,WorldKind kind,ActorArchive& ar){
 if(kind==WorldKind::Bridge){auto* p=dynamic_cast<Bridge*>(&s);return p&&PcMiddayWorldAccess::bridge(*p,ar);}
 if(kind==WorldKind::HinderRock){auto* p=dynamic_cast<HinderRock*>(&s);return p&&PcMiddayWorldAccess::hinder(*p,ar);}
 if(kind==WorldKind::Plant){auto* p=dynamic_cast<Plant*>(&s);return p&&PcMiddayWorldAccess::plant(*p,ar);}
 if(kind==WorldKind::Rope){auto* p=dynamic_cast<RopeItem*>(&s);return p&&PcMiddayWorldAccess::rope(*p,ar);}
 if(kind==WorldKind::Seed){auto* p=dynamic_cast<SeedItem*>(&s);return p&&PcMiddayWorldAccess::seed(*p,ar);}
 if(kind==WorldKind::Key){auto* p=dynamic_cast<KeyItem*>(&s);return p&&PcMiddayWorldAccess::key(*p,ar);}
 return ar.fail("invalid structure family");}
bool world_item_derived_fields(ItemCreature& s,ActorArchive& ar){switch(s.mObjType){
 case OBJTYPE_Goal:{auto* p=dynamic_cast<GoalItem*>(&s);return p&&PcMiddayWorldAccess::goal(*p,ar);}
 case OBJTYPE_Ufo:{auto* p=dynamic_cast<UfoItem*>(&s);return p&&PcMiddayWorldAccess::ship(*p,ar);}
 case OBJTYPE_Pikihead:{auto* p=dynamic_cast<PikiHeadItem*>(&s);return p&&PcMiddayWorldAccess::head(*p,ar);}
 case OBJTYPE_SluiceSoft:{auto* p=dynamic_cast<BuildingItem*>(&s);return p&&PcMiddayWorldAccess::building(*p,ar);}
 case OBJTYPE_SluiceHard:{auto* p=dynamic_cast<BuildingItem*>(&s);return p&&PcMiddayWorldAccess::building(*p,ar);}
 case OBJTYPE_SluiceBomb:{auto* p=dynamic_cast<BuildingItem*>(&s);return p&&PcMiddayWorldAccess::building(*p,ar);}
 case OBJTYPE_SluiceBombHard:{auto* p=dynamic_cast<BuildingItem*>(&s);return p&&PcMiddayWorldAccess::building(*p,ar);}
 case OBJTYPE_Kusa:{auto* p=dynamic_cast<KusaItem*>(&s);return p&&PcMiddayWorldAccess::stick(*p,ar);}
 case OBJTYPE_BoBase:{auto* p=dynamic_cast<BoBaseItem*>(&s);return p&&PcMiddayWorldAccess::stickBase(*p,ar);}
 case OBJTYPE_BombGen:{auto* p=dynamic_cast<BombGenItem*>(&s);return p&&PcMiddayWorldAccess::bombGen(*p,ar);}
 case OBJTYPE_Weeds:{auto* p=dynamic_cast<WeedsGen*>(&s);return p&&PcMiddayWorldAccess::weeds(*p,ar);}
 case OBJTYPE_Weed:{auto* p=dynamic_cast<Weed*>(&s);return p&&PcMiddayWorldAccess::weed(*p,ar);}
 case OBJTYPE_GrassGen:{auto* p=dynamic_cast<GrassGen*>(&s);return p&&PcMiddayWorldAccess::grass(*p,ar);}
 case OBJTYPE_RockGen:{auto* p=dynamic_cast<RockGen*>(&s);return p&&PcMiddayWorldAccess::rocks(*p,ar);}
 case OBJTYPE_Fish:{auto* p=dynamic_cast<FishGenerator*>(&s);return p&&PcMiddayWorldAccess::fish(*p,ar);}
 case OBJTYPE_Door:{auto* p=dynamic_cast<DoorItem*>(&s);return p&&PcMiddayWorldAccess::door(*p,ar);}
 case OBJTYPE_Gate:{auto* p=dynamic_cast<DoorItem*>(&s);return p&&PcMiddayWorldAccess::door(*p,ar);}
 case OBJTYPE_SunsetStart:return dynamic_cast<NaviDemoSunsetStart*>(&s)!=nullptr;case OBJTYPE_SunsetGoal:return dynamic_cast<NaviDemoSunsetGoal*>(&s)!=nullptr;case OBJTYPE_Water:case OBJTYPE_FallWater:return dynamic_cast<MizuItem*>(&s)!=nullptr;case OBJTYPE_Fulcrum:return dynamic_cast<Fulcrum*>(&s)!=nullptr;default:return ar.fail("world item type not registered in this adapter");}}
}
#endif
