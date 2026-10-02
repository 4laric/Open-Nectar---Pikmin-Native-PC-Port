#include "pc_midday_world.h"
#include "pc_midday_world_resources.h"
#include "pc_midday_particle.h"
#include "ObjType.h"
#include "pc_midday_collision.h"
namespace pc_midday {
namespace {
bool bad(std::string&e,const char*m){if(e.empty())e=m;return false;}
void scalar(std::vector<FieldSchema>&s,const std::string&k,ScalarKind t){s.push_back(FieldSchema::value(k.c_str(),t));}
void ref(std::vector<FieldSchema>&s,const std::string&k,RefKind t,const char* target,ReferenceOwnership own,bool n=true){s.push_back(FieldSchema::ref(k.c_str(),t,n,target,own));}
void vector(std::vector<FieldSchema>&s,const std::string&k){for(auto x:{".x",".y",".z"})scalar(s,k+x,ScalarKind::F32);}
bool boolean(const ActorFields&f,const std::string&k,bool&v,std::string&e){auto it=f.find(k);if(it==f.end()||it->second.category!=FieldCategory::Scalar||it->second.scalar!=ScalarKind::Bool||it->second.bits>1)return bad(e,"invalid world boolean");v=it->second.bits!=0;return true;}
bool number(const ActorFields& f,const std::string& key,ScalarKind kind,int& out,std::string& e){auto it=f.find(key);if(it==f.end()||it->second.category!=FieldCategory::Scalar||it->second.scalar!=kind)return bad(e,"missing typed world count");switch(kind){case ScalarKind::S32:out=static_cast<s32>(it->second.bits);break;case ScalarKind::S16:out=static_cast<s16>(it->second.bits);break;case ScalarKind::U16:out=static_cast<u16>(it->second.bits);break;case ScalarKind::U8:out=static_cast<u8>(it->second.bits);break;case ScalarKind::Bool:out=it->second.bits?1:0;break;default:return bad(e,"invalid world count kind");}return true;}
bool registered(int type,int state){if(state==-1)return true;switch(type){case OBJTYPE_Goal:return state>=0&&state<7;case OBJTYPE_Pikihead:return state>=0&&state<15&&state!=1;case OBJTYPE_SluiceSoft:case OBJTYPE_SluiceHard:case OBJTYPE_SluiceBomb:case OBJTYPE_SluiceBombHard:case OBJTYPE_Bomb:return state>=0&&state<6;case OBJTYPE_Water:case OBJTYPE_FallWater:return state>=0&&state<5;case OBJTYPE_Plant:return state>=0&&state<2;default:return false;}}
}
bool world_animation_schema(const ActorFields&f,const std::string&p,std::vector<FieldSchema>&s,std::string&e){
 bool active=false;if(!boolean(f,p+"active",active,e))return false;scalar(s,p+"active",ScalarKind::Bool);
 const char* names[]={"mMgr","mContext","mMotionTable","mAnimInfo"};const char* types[]={"AnimMgr","AnimContext","PaniMotionTable","AnimInfo"};for(int i=0;i<4;++i)ref(s,p+names[i],RefKind::Animation,types[i],ReferenceOwnership::Content,!active);
 if(!active){auto it=f.find(p+"mAnimInfo");if(it!=f.end()&&(it->second.target.owner||it->second.target.resource||it->second.target.slot))return bad(e,"inactive animation has active info");return true;}
 for(auto name:{"mPlayState","mCurrentAnimID","mStartKeyIndex","mEndKeyIndex","mCurrentKeyIndex","mMotionIdx"})scalar(s,p+name,ScalarKind::S32);
 scalar(s,p+"mPreviousKeyIndex",ScalarKind::U32);scalar(s,p+"mAnimationCounter",ScalarKind::F32);scalar(s,p+"mIsFinished",ScalarKind::Bool);ref(s,p+"mListener",RefKind::AnimListener,"PaniAnimKeyListener",ReferenceOwnership::ActorSubobject);
 int play=0;if(!actor_i32(f,(p+"mPlayState").c_str(),play,e)||play<0||play>2)return bad(e,"invalid world animation play state");return true;
}
bool world_ai_schema(const ActorFields&f,std::vector<FieldSchema>&s,std::string&e){
 int type=0,current=0,last=0,events=0;
 if(!actor_i32(f,"creature.objectType",type,e)||!actor_i32(f,"world.ai.current",current,e)||!actor_i32(f,"world.ai.last",last,e)||!actor_i32(f,"world.ai.events",events,e)||events<0||events>16||!registered(type,current)||!registered(type,last)||(current==-1&&last!=-1))return bad(e,"world SAI discriminator invalid");
 ref(s,"world.ai.machine",RefKind::SAIStateMachine,"StateMachine<AICreature>",ReferenceOwnership::Content,current==-1);scalar(s,"world.ai.current",ScalarKind::S32);scalar(s,"world.ai.last",ScalarKind::S32);ref(s,"world.ai.collision",RefKind::Creature,"Creature",ReferenceOwnership::AnyLive);vector(s,"world.ai.vector");scalar(s,"world.ai.animation",ScalarKind::S32);scalar(s,"world.ai.counter",ScalarKind::S32);scalar(s,"world.ai.health",ScalarKind::F32);scalar(s,"world.ai.events",ScalarKind::S32);
 for(int i=0;i<16;++i)scalar(s,"world.ai.event."+std::to_string(i),ScalarKind::Bool);
 if(type==OBJTYPE_Bomb)scalar(s,"world.ai.maxHealth",ScalarKind::F32);
 return true;
}
bool world_item_schema(const ActorFields&f,std::vector<FieldSchema>&s,std::string&e){
 if(!world_ai_schema(f,s,e))return false;scalar(s,"world.item.motionSpeed",ScalarKind::F32);scalar(s,"world.item.setup",ScalarKind::Bool);ref(s,"world.item.shape",RefKind::Shape,"Shape",ReferenceOwnership::Content);ref(s,"world.item.shapeObject",RefKind::ItemShape,"ItemShapeObject",ReferenceOwnership::Content);
 return world_animation_schema(f,"world.item.animation.",s,e);
}
}

namespace pc_midday {
// Family-specific typed fields. Generated from the explicitly audited member inventory.
static bool schema_goal(const ActorFields& f,std::vector<FieldSchema>& s,std::string& e){
 scalar(s,"world.goal.mColourFadeRate",ScalarKind::F32);
 scalar(s,"world.goal.mColourAnimProgress",ScalarKind::F32);
 scalar(s,"world.goal.mColourAnimationEnabled",ScalarKind::Bool);
 scalar(s,"world.goal.mSpotEffectActive",ScalarKind::Bool);
 scalar(s,"world.goal.mIsClosing",ScalarKind::Bool);
 scalar(s,"world.goal.mConeSizeTimer",ScalarKind::F32);
 vector(s,"world.goal._3FC");
 scalar(s,"world.goal.mIsConeEmit",ScalarKind::Bool);
 scalar(s,"world.goal.mIsDispensingPikis",ScalarKind::Bool);
 scalar(s,"world.goal.mPikisToExit",ScalarKind::S32);
 scalar(s,"world.goal.mPikiSpawnTimer",ScalarKind::F32);
 vector(s,"world.goal._41C");
 scalar(s,"world.goal.mOnionColour",ScalarKind::U16);
 scalar(s,"world.goal.mWaypointIdx",ScalarKind::S16);
 scalar(s,"world.goal.mPcOwner",ScalarKind::S32);
 ref(s,"world.goal.mSpotEfx",RefKind::ParticleGenerator,"zen::particleGenerator",ReferenceOwnership::ActorSubobject);
 ref(s,"world.goal.mHaloEfx",RefKind::ParticleGenerator,"zen::particleGenerator",ReferenceOwnership::ActorSubobject);
 ref(s,"world.goal.mSuckEfx",RefKind::ParticleGenerator,"zen::particleGenerator",ReferenceOwnership::ActorSubobject);
 ref(s,"world.goal.mSpotModelEff",RefKind::Effect,"EffShpInst",ReferenceOwnership::ActorSubobject);
 scalar(s,"world.goal.slot.0.held",ScalarKind::U32);
 ref(s,"world.goal.slot.0.shape",RefKind::ItemShape,"ItemShapeObject",ReferenceOwnership::Content);
 ref(s,"world.goal.slot.0.fulcrum",RefKind::Creature,"Fulcrum",ReferenceOwnership::AnyLive);
 ref(s,"world.goal.slot.0.rope",RefKind::Creature,"RopeItem",ReferenceOwnership::AnyLive);
 scalar(s,"world.goal.slot.1.held",ScalarKind::U32);
 ref(s,"world.goal.slot.1.shape",RefKind::ItemShape,"ItemShapeObject",ReferenceOwnership::Content);
 ref(s,"world.goal.slot.1.fulcrum",RefKind::Creature,"Fulcrum",ReferenceOwnership::AnyLive);
 ref(s,"world.goal.slot.1.rope",RefKind::Creature,"RopeItem",ReferenceOwnership::AnyLive);
 scalar(s,"world.goal.slot.2.held",ScalarKind::U32);
 ref(s,"world.goal.slot.2.shape",RefKind::ItemShape,"ItemShapeObject",ReferenceOwnership::Content);
 ref(s,"world.goal.slot.2.fulcrum",RefKind::Creature,"Fulcrum",ReferenceOwnership::AnyLive);
 ref(s,"world.goal.slot.2.rope",RefKind::Creature,"RopeItem",ReferenceOwnership::AnyLive);
 scalar(s,"world.goal.exitFor.0",ScalarKind::S32);
 scalar(s,"world.goal.exitFor.1",ScalarKind::S32);
 if(!world_materials_schema(f,"world.goal.materials",s,e))return false;

 return true;
}
static bool schema_building(const ActorFields& f,std::vector<FieldSchema>& s,std::string& e){
 scalar(s,"world.building.mNumStages",ScalarKind::S32);
 scalar(s,"world.building.mCurrStage",ScalarKind::S32);
 vector(s,"world.building._448");
 ref(s,"world.building.mWayPoint",RefKind::WayPoint,"WayPoint",ReferenceOwnership::AnyLive);
 if(!world_materials_schema(f,"world.building.materials",s,e))return false;
 if(!world_platform_schema(f,"world.building.platforms",s,e))return false;
 if(!particle_schema(f,ParticleRecordKind::Permanent,"world.building.effect0",s,e))return false;
 if(!particle_schema(f,ParticleRecordKind::Permanent,"world.building.effect1",s,e))return false;

 return true;
}
static bool schema_head(const ActorFields& f,std::vector<FieldSchema>& s,std::string& e){
 scalar(s,"world.head.mSeedColor",ScalarKind::S32);
 scalar(s,"world.head.mFlowerStage",ScalarKind::S32);
 vector(s,"world.head.mGlowEffectPos");
 scalar(s,"world.head.mPcOwner",ScalarKind::S32);
 scalar(s,"world.head.mP2Purple",ScalarKind::Bool);
 scalar(s,"world.head.mP2White",ScalarKind::Bool);
 scalar(s,"world.head.mP2Bulbmin",ScalarKind::Bool);
 ref(s,"world.head.mFreeLightEfx",RefKind::Effect,"FreeLightEffect",ReferenceOwnership::ActorSubobject);
 ref(s,"world.head.mParentOnion",RefKind::Creature,"GoalItem",ReferenceOwnership::AnyLive);
 ref(s,"world.head.mRippleEfx",RefKind::Effect,"RippleEffect",ReferenceOwnership::ActorSubobject);
 if(!particle_schema(f,ParticleRecordKind::Permanent,"world.head.sparkle",s,e))return false;

 return true;
}
static bool schema_stick(const ActorFields& f,std::vector<FieldSchema>& s,std::string& e){
 vector(s,"world.stick.mGroundPosition");
 ref(s,"world.stick.mBaseItem",RefKind::Creature,"BoBaseItem",ReferenceOwnership::AnyLive);
 return true;
}
static bool schema_stickBase(const ActorFields& f,std::vector<FieldSchema>& s,std::string& e){
 vector(s,"world.stickBase.mGroundPosition");
 scalar(s,"world.stickBase.mIsActive",ScalarKind::Bool);
 scalar(s,"world.stickBase.mEffectDuration",ScalarKind::S8);
 ref(s,"world.stickBase.mStickItem",RefKind::Creature,"KusaItem",ReferenceOwnership::AnyLive);
 ref(s,"world.stickBase.mParticleGenerator",RefKind::ParticleGenerator,"zen::particleGenerator",ReferenceOwnership::ActorSubobject);
 return true;
}
static bool schema_bombGen(const ActorFields& f,std::vector<FieldSchema>& s,std::string& e){
 scalar(s,"world.bombGen.mCapacity",ScalarKind::S16);
 scalar(s,"world.bombGen.mRemaining",ScalarKind::S16);
 return true;
}
static bool schema_weeds(const ActorFields& f,std::vector<FieldSchema>& s,std::string& e){
 scalar(s,"world.weeds.mWeedsCount",ScalarKind::S32);
 ref(s,"world.weeds.mWeedShape",RefKind::Shape,"Shape",ReferenceOwnership::Content);
 ref(s,"world.weeds.mWeedsGenProps",RefKind::CreatureProp,"CreatureProp",ReferenceOwnership::Content);
 return true;
}
static bool schema_weed(const ActorFields& f,std::vector<FieldSchema>& s,std::string& e){
 scalar(s,"world.weed.mIsPulled",ScalarKind::U16);
 scalar(s,"world.weed.mPulloutTimer",ScalarKind::U16);
 ref(s,"world.weed.mGen",RefKind::Creature,"WeedsGen",ReferenceOwnership::AnyLive);
 return true;
}
static bool schema_rope(const ActorFields& f,std::vector<FieldSchema>& s,std::string& e){
 scalar(s,"world.rope.mRopeLength",ScalarKind::F32);
 vector(s,"world.rope.mRopeDirection");
 scalar(s,"world.rope._2D0",ScalarKind::S32);
 ref(s,"world.rope.mParentRope",RefKind::Creature,"Creature",ReferenceOwnership::AnyLive);
 ref(s,"world.rope.mAttachedObj",RefKind::Creature,"RopeCreature",ReferenceOwnership::AnyLive);
 ref(s,"world.rope.mModel",RefKind::Shape,"Shape",ReferenceOwnership::Content);
 ref(s,"world.rope.mOwner",RefKind::Creature,"Creature",ReferenceOwnership::AnyLive);
 return true;
}
static bool schema_seed(const ActorFields& f,std::vector<FieldSchema>& s,std::string& e){
 scalar(s,"world.seed.mStateId",ScalarKind::S32);
 scalar(s,"world.seed.mGrowthTimer",ScalarKind::F32);
 ref(s,"world.seed.mCurrentShape",RefKind::Shape,"Shape",ReferenceOwnership::Content);
 ref(s,"world.seed.mSeedShape",RefKind::Shape,"Shape",ReferenceOwnership::Content);
 ref(s,"world.seed.mPlantedShape",RefKind::Shape,"Shape",ReferenceOwnership::Content);
 return true;
}
static bool schema_key(const ActorFields& f,std::vector<FieldSchema>& s,std::string& e){
 scalar(s,"world.key.mState",ScalarKind::S32);
 ref(s,"world.key.mModel",RefKind::Shape,"Shape",ReferenceOwnership::Content);
 return true;
}
static bool schema_plant(const ActorFields& f,std::vector<FieldSchema>& s,std::string& e){
 scalar(s,"world.plant.mPlantType",ScalarKind::U16);
 scalar(s,"world.plant.mMotionSpeed",ScalarKind::F32);
 scalar(s,"world.plant.mIsCulled",ScalarKind::Bool);
 scalar(s,"world.plant._394",ScalarKind::Bool);
 if(!world_ai_schema(f,s,e)||!world_animation_schema(f,"world.plant.animation.",s,e))return false;
 return true;
}
static bool schema_bridge(const ActorFields& f,std::vector<FieldSchema>& s,std::string& e){
 scalar(s,"world.bridge.mDoUseJointSegments",ScalarKind::Bool);
 scalar(s,"world.bridge._3CA",ScalarKind::S16);
 scalar(s,"world.bridge._3CC",ScalarKind::U8);
 scalar(s,"world.bridge._400",ScalarKind::U8);
 scalar(s,"world.bridge.mStageCount",ScalarKind::S32);
 scalar(s,"world.bridge._424",ScalarKind::U8);
 ref(s,"world.bridge.mStartWaypoint",RefKind::WayPoint,"WayPoint",ReferenceOwnership::AnyLive);
 ref(s,"world.bridge.mEndWaypoint",RefKind::WayPoint,"WayPoint",ReferenceOwnership::AnyLive);
 ref(s,"world.bridge.mBuildShape",RefKind::DynBuildShape,"DynBuildShape",ReferenceOwnership::AnyLive);
 ref(s,"world.bridge.mBridgeShape",RefKind::Shape,"Shape",ReferenceOwnership::Content);
 ref(s,"world.bridge._410",RefKind::CollPart,"CollPart",ReferenceOwnership::AnyLive);
 int count=0,allocation=0;if(!number(f,"world.bridge.mStageCount",ScalarKind::S32,count,e)||!number(f,"world.bridge.allocation",ScalarKind::S32,allocation,e)||count!=allocation||count<0||count>4096)return bad(e,"world array capacity mismatch");scalar(s,"world.bridge.allocation",ScalarKind::S32);
 for(int i=0;i<count;++i)scalar(s,"world.bridge.stage."+std::to_string(i)+".progress",ScalarKind::F32);bool joints=false;if(!boolean(f,"world.bridge.mDoUseJointSegments",joints,e))return false;if(joints)for(int i=0;i<count*2;++i)ref(s,"world.bridge.joint."+std::to_string(i),RefKind::Joint,"Joint",ReferenceOwnership::Content,false);
 if(!world_materials_schema(f,"world.bridge.materials",s,e))return false;
 if(!particle_schema(f,ParticleRecordKind::Permanent,"world.bridge.effect0",s,e))return false;
 if(!particle_schema(f,ParticleRecordKind::Permanent,"world.bridge.effect1",s,e))return false;
 bool build=false;if(!boolean(f,"world.bridge.buildPresent",build,e))return false;scalar(s,"world.bridge.buildPresent",ScalarKind::Bool);if(build&&!world_dyn_shape_schema(f,"world.bridge.build",s,e))return false;

 return true;
}
static bool schema_hinder(const ActorFields& f,std::vector<FieldSchema>& s,std::string& e){
 scalar(s,"world.hinder.mPushingPikmin",ScalarKind::U16);
 vector(s,"world.hinder.mDestinationPosition");
 scalar(s,"world.hinder.mTotalPushStrength",ScalarKind::S32);
 scalar(s,"world.hinder.mAmountPushersToStart",ScalarKind::S32);
 scalar(s,"world.hinder.mPushSpeed",ScalarKind::F32);
 scalar(s,"world.hinder.mCentreSize",ScalarKind::F32);
 scalar(s,"world.hinder.mState",ScalarKind::U8);
 scalar(s,"world.hinder.mFxCooldownTimer",ScalarKind::U8);
 scalar(s,"world.hinder.mPushMoveTimer",ScalarKind::F32);
 scalar(s,"world.hinder.mIsMoving",ScalarKind::Bool);
 scalar(s,"world.hinder.mIsSoundPlaying",ScalarKind::Bool);
 vector(s,"world.hinder.mMoveFrontEfxPos");
 ref(s,"world.hinder.mWayPoint",RefKind::WayPoint,"WayPoint",ReferenceOwnership::AnyLive);
 ref(s,"world.hinder.mBuildShape",RefKind::DynBuildShape,"DynBuildShape",ReferenceOwnership::AnyLive);
 ref(s,"world.hinder.mBoxShape",RefKind::Shape,"Shape",ReferenceOwnership::Content);
 ref(s,"world.hinder.mEfxA",RefKind::ParticleGenerator,"zen::particleGenerator",ReferenceOwnership::ActorSubobject);
 ref(s,"world.hinder.mEfxB",RefKind::ParticleGenerator,"zen::particleGenerator",ReferenceOwnership::ActorSubobject);
 ref(s,"world.hinder.mEfxC",RefKind::ParticleGenerator,"zen::particleGenerator",ReferenceOwnership::ActorSubobject);
 vector(s,"world.hinder.plane.0.normal");scalar(s,"world.hinder.plane.0.offset",ScalarKind::F32);
 vector(s,"world.hinder.plane.1.normal");scalar(s,"world.hinder.plane.1.offset",ScalarKind::F32);
 vector(s,"world.hinder.plane.2.normal");scalar(s,"world.hinder.plane.2.offset",ScalarKind::F32);
 vector(s,"world.hinder.plane.3.normal");scalar(s,"world.hinder.plane.3.offset",ScalarKind::F32);
 vector(s,"world.hinder.side.0");
 vector(s,"world.hinder.side.1");
 bool build=false;if(!boolean(f,"world.hinder.buildPresent",build,e))return false;scalar(s,"world.hinder.buildPresent",ScalarKind::Bool);if(build&&!world_dyn_shape_schema(f,"world.hinder.build",s,e))return false;

 return true;
}
static bool schema_grass(const ActorFields& f,std::vector<FieldSchema>& s,std::string& e){
 scalar(s,"world.grass.mWorkingPikis",ScalarKind::S32);
 scalar(s,"world.grass.mActiveGrass",ScalarKind::U16);
 scalar(s,"world.grass.mTotalGrassCount",ScalarKind::U16);
 vector(s,"world.grass._3D4");
 scalar(s,"world.grass.mSize",ScalarKind::F32);
 ref(s,"world.grass.mGrass",RefKind::Grass,"Grass",ReferenceOwnership::ActorSubobject);
 int count=0,allocation=0;if(!number(f,"world.grass.mTotalGrassCount",ScalarKind::U16,count,e)||!number(f,"world.grass.allocation",ScalarKind::U16,allocation,e)||count!=allocation||count<0||count>4096)return bad(e,"world array capacity mismatch");scalar(s,"world.grass.allocation",ScalarKind::U16);
 for(int i=0;i<count;++i){const auto p=std::string("world.grass.entry.")+std::to_string(i)+".";vector(s,p+"position");scalar(s,p+"health",ScalarKind::U8);scalar(s,p+"shape",ScalarKind::U8);scalar(s,p+"rotation",ScalarKind::U8);}
 return true;
}
static bool schema_rocks(const ActorFields& f,std::vector<FieldSchema>& s,std::string& e){
 scalar(s,"world.rocks.mWorkingPikis",ScalarKind::S32);
 scalar(s,"world.rocks._3CC",ScalarKind::U8);
 scalar(s,"world.rocks.mActivePebbles",ScalarKind::U16);
 scalar(s,"world.rocks.mMaxPebbles",ScalarKind::U16);
 vector(s,"world.rocks._3D8");
 scalar(s,"world.rocks.mSize",ScalarKind::F32);
 ref(s,"world.rocks.mPebbles",RefKind::Pebble,"Pebble",ReferenceOwnership::ActorSubobject);
 int count=0,allocation=0;if(!number(f,"world.rocks.mMaxPebbles",ScalarKind::U16,count,e)||!number(f,"world.rocks.allocation",ScalarKind::U16,allocation,e)||count!=allocation||count<0||count>4096)return bad(e,"world array capacity mismatch");scalar(s,"world.rocks.allocation",ScalarKind::U16);
 for(int i=0;i<count;++i){const auto p=std::string("world.rocks.entry.")+std::to_string(i)+".";vector(s,p+"position");scalar(s,p+"health",ScalarKind::U8);scalar(s,p+"shape",ScalarKind::U8);scalar(s,p+"rotation",ScalarKind::U8);}
 return true;
}
static bool schema_fish(const ActorFields& f,std::vector<FieldSchema>& s,std::string& e){
 scalar(s,"world.fish.mFishCount",ScalarKind::S32);
 scalar(s,"world.fish.mMaxFish",ScalarKind::S32);
 vector(s,"world.fish.mSchoolCentre");
 int count=0,allocation=0;if(!number(f,"world.fish.mMaxFish",ScalarKind::S32,count,e)||!number(f,"world.fish.allocation",ScalarKind::S32,allocation,e)||count!=allocation||count<0||count>4096)return bad(e,"world array capacity mismatch");scalar(s,"world.fish.allocation",ScalarKind::S32);
 for(int i=0;i<count;++i){const auto p=std::string("world.fish.entry.")+std::to_string(i)+".";vector(s,p+"position");vector(s,p+"velocity");scalar(s,p+"direction",ScalarKind::F32);}
 return true;
}
static bool schema_ship(const ActorFields& f,std::vector<FieldSchema>& s,std::string& e){
 scalar(s,"world.ship.mIsMenuOpen",ScalarKind::Bool);
 scalar(s,"world.ship.mIsLightActive",ScalarKind::Bool);
 scalar(s,"world.ship.mShouldLightActivate",ScalarKind::Bool);
 scalar(s,"world.ship.mIsTroubleFxEnabled",ScalarKind::Bool);
 scalar(s,"world.ship.mTroubleFxTimer",ScalarKind::F32);
 scalar(s,"world.ship.mTroubleFxState",ScalarKind::U32);
 scalar(s,"world.ship.mJetLevel",ScalarKind::S16);
 scalar(s,"world.ship.mShipUpgradeLevel",ScalarKind::U8);
 scalar(s,"world.ship.mConeEffectId",ScalarKind::S32);
 vector(s,"world.ship.mPca1FxPosition");
 vector(s,"world.ship.mPca2FxPosition");
 scalar(s,"world.ship.mIsPca1FxActive",ScalarKind::Bool);
 scalar(s,"world.ship.mIsPca2FxActive",ScalarKind::Bool);
 vector(s,"world.ship.mSpotlightPosition");
 scalar(s,"world.ship.mWaypointID",ScalarKind::S32);
 scalar(s,"world.ship.mPcOwner",ScalarKind::S32);
 scalar(s,"world.ship.mNeedPathfindRefresh",ScalarKind::Bool);
 ref(s,"world.ship.mRingFx",RefKind::ParticleGenerator,"zen::particleGenerator",ReferenceOwnership::ActorSubobject);
 ref(s,"world.ship.mSparkleFx",RefKind::ParticleGenerator,"zen::particleGenerator",ReferenceOwnership::ActorSubobject);
 ref(s,"world.ship.mShipModel",RefKind::UfoShape,"UfoShapeObject",ReferenceOwnership::Content);
 scalar(s,"world.ship.anim.0.speed",ScalarKind::F32);if(!world_animation_schema(f,"world.ship.anim.0.animation.",s,e))return false;
 scalar(s,"world.ship.anim.1.speed",ScalarKind::F32);if(!world_animation_schema(f,"world.ship.anim.1.animation.",s,e))return false;
 scalar(s,"world.ship.anim.2.speed",ScalarKind::F32);if(!world_animation_schema(f,"world.ship.anim.2.animation.",s,e))return false;
 scalar(s,"world.ship.anim.3.speed",ScalarKind::F32);if(!world_animation_schema(f,"world.ship.anim.3.animation.",s,e))return false;
 scalar(s,"world.ship.anim.4.speed",ScalarKind::F32);if(!world_animation_schema(f,"world.ship.anim.4.animation.",s,e))return false;
 scalar(s,"world.ship.anim.5.speed",ScalarKind::F32);if(!world_animation_schema(f,"world.ship.anim.5.animation.",s,e))return false;
 scalar(s,"world.ship.anim.6.speed",ScalarKind::F32);if(!world_animation_schema(f,"world.ship.anim.6.animation.",s,e))return false;
 scalar(s,"world.ship.anim.7.speed",ScalarKind::F32);if(!world_animation_schema(f,"world.ship.anim.7.animation.",s,e))return false;
 vector(s,"world.ship.spot.0.position");
 scalar(s,"world.ship.spot.0.radius",ScalarKind::F32);
 scalar(s,"world.ship.spot.0.angle",ScalarKind::F32);
 scalar(s,"world.ship.spot.0.rotation",ScalarKind::F32);
 vector(s,"world.ship.spot.1.position");
 scalar(s,"world.ship.spot.1.radius",ScalarKind::F32);
 scalar(s,"world.ship.spot.1.angle",ScalarKind::F32);
 scalar(s,"world.ship.spot.1.rotation",ScalarKind::F32);
 vector(s,"world.ship.spot.2.position");
 scalar(s,"world.ship.spot.2.radius",ScalarKind::F32);
 scalar(s,"world.ship.spot.2.angle",ScalarKind::F32);
 scalar(s,"world.ship.spot.2.rotation",ScalarKind::F32);
 vector(s,"world.ship.trouble.0.position");ref(s,"world.ship.trouble.0.emitter",RefKind::ParticleGenerator,"zen::particleGenerator",ReferenceOwnership::ActorSubobject);
 vector(s,"world.ship.trouble.1.position");ref(s,"world.ship.trouble.1.emitter",RefKind::ParticleGenerator,"zen::particleGenerator",ReferenceOwnership::ActorSubobject);
 vector(s,"world.ship.trouble.2.position");ref(s,"world.ship.trouble.2.emitter",RefKind::ParticleGenerator,"zen::particleGenerator",ReferenceOwnership::ActorSubobject);
 vector(s,"world.ship.trouble.3.position");ref(s,"world.ship.trouble.3.emitter",RefKind::ParticleGenerator,"zen::particleGenerator",ReferenceOwnership::ActorSubobject);
 vector(s,"world.ship.trouble.4.position");ref(s,"world.ship.trouble.4.emitter",RefKind::ParticleGenerator,"zen::particleGenerator",ReferenceOwnership::ActorSubobject);
 vector(s,"world.ship.trouble.5.position");ref(s,"world.ship.trouble.5.emitter",RefKind::ParticleGenerator,"zen::particleGenerator",ReferenceOwnership::ActorSubobject);
 scalar(s,"world.ship.light.0.frame",ScalarKind::F32);scalar(s,"world.ship.light.0.speed",ScalarKind::F32);scalar(s,"world.ship.light.0.type",ScalarKind::U16);
 ref(s,"world.ship.light.0.engine.0",RefKind::ParticleGenerator,"zen::particleGenerator",ReferenceOwnership::ActorSubobject);
 ref(s,"world.ship.light.0.engine.1",RefKind::ParticleGenerator,"zen::particleGenerator",ReferenceOwnership::ActorSubobject);
 ref(s,"world.ship.light.0.engine.2",RefKind::ParticleGenerator,"zen::particleGenerator",ReferenceOwnership::ActorSubobject);
 ref(s,"world.ship.light.0.engine.3",RefKind::ParticleGenerator,"zen::particleGenerator",ReferenceOwnership::ActorSubobject);
 scalar(s,"world.ship.light.1.frame",ScalarKind::F32);scalar(s,"world.ship.light.1.speed",ScalarKind::F32);scalar(s,"world.ship.light.1.type",ScalarKind::U16);
 ref(s,"world.ship.light.1.engine.0",RefKind::ParticleGenerator,"zen::particleGenerator",ReferenceOwnership::ActorSubobject);
 ref(s,"world.ship.light.1.engine.1",RefKind::ParticleGenerator,"zen::particleGenerator",ReferenceOwnership::ActorSubobject);
 ref(s,"world.ship.light.1.engine.2",RefKind::ParticleGenerator,"zen::particleGenerator",ReferenceOwnership::ActorSubobject);
 ref(s,"world.ship.light.1.engine.3",RefKind::ParticleGenerator,"zen::particleGenerator",ReferenceOwnership::ActorSubobject);
 scalar(s,"world.ship.light.2.frame",ScalarKind::F32);scalar(s,"world.ship.light.2.speed",ScalarKind::F32);scalar(s,"world.ship.light.2.type",ScalarKind::U16);
 ref(s,"world.ship.light.2.engine.0",RefKind::ParticleGenerator,"zen::particleGenerator",ReferenceOwnership::ActorSubobject);
 ref(s,"world.ship.light.2.engine.1",RefKind::ParticleGenerator,"zen::particleGenerator",ReferenceOwnership::ActorSubobject);
 ref(s,"world.ship.light.2.engine.2",RefKind::ParticleGenerator,"zen::particleGenerator",ReferenceOwnership::ActorSubobject);
 ref(s,"world.ship.light.2.engine.3",RefKind::ParticleGenerator,"zen::particleGenerator",ReferenceOwnership::ActorSubobject);
 scalar(s,"world.ship.light.3.frame",ScalarKind::F32);scalar(s,"world.ship.light.3.speed",ScalarKind::F32);scalar(s,"world.ship.light.3.type",ScalarKind::U16);
 ref(s,"world.ship.light.3.engine.0",RefKind::ParticleGenerator,"zen::particleGenerator",ReferenceOwnership::ActorSubobject);
 ref(s,"world.ship.light.3.engine.1",RefKind::ParticleGenerator,"zen::particleGenerator",ReferenceOwnership::ActorSubobject);
 ref(s,"world.ship.light.3.engine.2",RefKind::ParticleGenerator,"zen::particleGenerator",ReferenceOwnership::ActorSubobject);
 ref(s,"world.ship.light.3.engine.3",RefKind::ParticleGenerator,"zen::particleGenerator",ReferenceOwnership::ActorSubobject);
 for(int i=0;i<4;++i)ref(s,"world.ship.light."+std::to_string(i)+".materials",RefKind::ShapeDynMaterials,"ShapeDynMaterials",ReferenceOwnership::AnyLive,true);
 ref(s,"world.ship.materials",RefKind::ShapeDynMaterials,"ShapeDynMaterials",ReferenceOwnership::AnyLive,true);

 return true;
}
static bool schema_door(const ActorFields& f,std::vector<FieldSchema>& s,std::string& e){
 scalar(s,"world.door.mFadeTimer",ScalarKind::F32);
 scalar(s,"world.door.mStateId",ScalarKind::S32);
 ref(s,"world.door.mDestinationStagePath",RefKind::StaticText,"char",ReferenceOwnership::Content);
 ref(s,"world.door.mLabelText",RefKind::StaticText,"char",ReferenceOwnership::Content);
 return true;
}
bool world_derived_schema(const ActorFields&f,int type,WorldKind kind,std::vector<FieldSchema>&s,std::string&e){
 if(kind==WorldKind::Bridge)return schema_bridge(f,s,e);
 if(kind==WorldKind::HinderRock)return schema_hinder(f,s,e);
 if(kind==WorldKind::Plant)return schema_plant(f,s,e);
 if(kind==WorldKind::Rope)return schema_rope(f,s,e);
 if(kind==WorldKind::Seed)return schema_seed(f,s,e);
 if(kind==WorldKind::Key)return schema_key(f,s,e);
 if(kind!=WorldKind::Item)return bad(e,"invalid derived world family");switch(type){
 case OBJTYPE_Goal:return schema_goal(f,s,e);
 case OBJTYPE_Ufo:return schema_ship(f,s,e);
 case OBJTYPE_Pikihead:return schema_head(f,s,e);
 case OBJTYPE_SluiceSoft:return schema_building(f,s,e);
 case OBJTYPE_SluiceHard:return schema_building(f,s,e);
 case OBJTYPE_SluiceBomb:return schema_building(f,s,e);
 case OBJTYPE_SluiceBombHard:return schema_building(f,s,e);
 case OBJTYPE_Kusa:return schema_stick(f,s,e);
 case OBJTYPE_BoBase:return schema_stickBase(f,s,e);
 case OBJTYPE_BombGen:return schema_bombGen(f,s,e);
 case OBJTYPE_Weeds:return schema_weeds(f,s,e);
 case OBJTYPE_Weed:return schema_weed(f,s,e);
 case OBJTYPE_GrassGen:return schema_grass(f,s,e);
 case OBJTYPE_RockGen:return schema_rocks(f,s,e);
 case OBJTYPE_Fish:return schema_fish(f,s,e);
 case OBJTYPE_Door:return schema_door(f,s,e);
 case OBJTYPE_Gate:return schema_door(f,s,e);
 case OBJTYPE_Water:case OBJTYPE_FallWater:case OBJTYPE_Fulcrum:case OBJTYPE_SunsetStart:case OBJTYPE_SunsetGoal:return true;default:return bad(e,"unregistered world item type");}}
}

namespace pc_midday {
bool world_pellet_schema(const ActorFields&f,std::vector<FieldSchema>&s,std::string&e){
 int current=0,last=0,count=0;bool shaped=false;if(!actor_i32(f,"world.pellet.current",current,e)||!actor_i32(f,"world.pellet.last",last,e)||current<0||current>5||last< -1||last>5||!number(f,"world.pellet.dynamics.count",ScalarKind::U16,count,e)||count<0||count>128||!boolean(f,"world.pellet.shaped",shaped,e))return bad(e,"invalid pellet state/dynamics discriminator");
 scalar(s,"world.pellet.current",ScalarKind::S32);scalar(s,"world.pellet.last",ScalarKind::S32);scalar(s,"world.pellet.shaped",ScalarKind::Bool);
 vector(s,"world.pellet.mSpawnPosition");
 scalar(s,"world.pellet.mUseSpawnPosition",ScalarKind::Bool);
 scalar(s,"world.pellet.mIsPlayTrySound",ScalarKind::Bool);
 scalar(s,"world.pellet.mMotionFlag",ScalarKind::U8);
 scalar(s,"world.pellet.mStuckAngle",ScalarKind::F32);
 vector(s,"world.pellet.mLastPosition");
 vector(s,"world.pellet.mCarryDirection");
 scalar(s,"world.pellet.mCarrierCount",ScalarKind::U16);
 scalar(s,"world.pellet.mTransitionTimer",ScalarKind::F32);
 scalar(s,"world.pellet.mCarryState",ScalarKind::U16);
 vector(s,"world.pellet.mCurrentPelletPosition");
 scalar(s,"world.pellet._4A0",ScalarKind::U16);
 scalar(s,"world.pellet.mCurrentPelletHeight",ScalarKind::F32);
 scalar(s,"world.pellet.mMotionSpeed",ScalarKind::F32);
 scalar(s,"world.pellet.mCarrierCounter",ScalarKind::U16);
 scalar(s,"world.pellet.mIsAlive",ScalarKind::Bool);
 scalar(s,"world.pellet.mIsAIActive",ScalarKind::Bool);
 ref(s,"world.pellet.mRippleEffect",RefKind::Effect,"RippleEffect",ReferenceOwnership::ActorSubobject,true);
 ref(s,"world.pellet.mTargetGoal",RefKind::Creature,"Suckable",ReferenceOwnership::AnyLive,current!=1);
 ref(s,"world.pellet.mStuckMouthPart",RefKind::CollPart,"CollPart",ReferenceOwnership::AnyLive,true);
 ref(s,"world.pellet.mPikiCarrier",RefKind::Creature,"Creature",ReferenceOwnership::AnyLive,true);
 ref(s,"world.pellet.mPelletView",RefKind::PelletView,"PelletView",ReferenceOwnership::ActorSubobject,shaped);
 ref(s,"world.pellet.mShapeObject",RefKind::ItemShape,"PelletShapeObject",ReferenceOwnership::Content,!shaped);
 ref(s,"world.pellet.mConfig",RefKind::PelletConfig,"PelletConfig",ReferenceOwnership::Content,false);
 ref(s,"world.pellet.mPelletCollInfo",RefKind::CollInfo,"CollInfo",ReferenceOwnership::ActorSubobject,true);
 for(int i=0;i<4;++i)scalar(s,"world.pellet.slotFlags."+std::to_string(i),ScalarKind::S32);
 if(shaped&&!world_materials_schema(f,"world.pellet.materials",s,e))return false;
 if(shaped&&(!world_animation_schema(f,"world.pellet.lower.",s,e)||!world_animation_schema(f,"world.pellet.upper.",s,e)))return false;
 if(current==1){for(auto key:{"progress","distance","wait","startScale","suckProgress","speed"})scalar(s,std::string("world.pellet.state.")+key,ScalarKind::F32);vector(s,"world.pellet.state.start");scalar(s,"world.pellet.state.firstMove",ScalarKind::U8);scalar(s,"world.pellet.state.ship",ScalarKind::U8);}
 if(current==2){scalar(s,"world.pellet.state.scale",ScalarKind::F32);scalar(s,"world.pellet.state.timer",ScalarKind::F32);}
 if(current==5)scalar(s,"world.pellet.state.wait",ScalarKind::U8);
 scalar(s,"world.pellet.dynamics.count",ScalarKind::U16);for(auto key:{"angularMomentum","angularVelocity","impulse","centre"})vector(s,std::string("world.pellet.dynamics.")+key);for(auto key:{"pickOffset","mass"})scalar(s,std::string("world.pellet.dynamics.")+key,ScalarKind::F32);for(auto key:{"flags","ground"})scalar(s,std::string("world.pellet.dynamics.")+key,ScalarKind::U8);for(auto key:{"real","collisionReady","changed","simpleFixed"})scalar(s,std::string("world.pellet.dynamics.")+key,ScalarKind::Bool);
 ref(s,"world.pellet.dynamics.head",RefKind::DynParticle,"DynParticle",ReferenceOwnership::ActorSubobject,count==0);
 if(count)for(int m=0;m<4;++m)for(int i=0;i<4;++i)for(int j=0;j<4;++j)scalar(s,"world.pellet.dynamics.matrix."+std::to_string(m)+"."+std::to_string(i)+"."+std::to_string(j),ScalarKind::F32);
 for(int n=0;n<count;++n){std::string p="world.pellet.dynamics.particle."+std::to_string(n)+".";for(auto key:{"mass","radius"})scalar(s,p+key,ScalarKind::F32);for(auto key:{"initial","local","preCollision","position","velocity"})vector(s,p+key);scalar(s,p+"free",ScalarKind::S32);for(int i=0;i<4;++i)for(int j=0;j<4;++j)scalar(s,p+"inverse."+std::to_string(i)+"."+std::to_string(j),ScalarKind::F32);ref(s,p+"next",RefKind::DynParticle,"DynParticle",ReferenceOwnership::ActorSubobject,n==count-1);}
 // Before allocation, the linked allocation must name exactly count distinct bodies.
 auto head=f.find("world.pellet.dynamics.head");if(head==f.end())return bad(e,"missing dynamics head");auto next=head->second.target;std::set<std::string> bodies;
 for(int i=0;i<count;++i){if(!next.owner&&!next.resource&&!next.slot)return bad(e,"short dynamics chain");auto key=std::to_string(next.owner)+":"+std::to_string(next.resource)+":"+std::to_string(next.slot);if(!bodies.insert(key).second)return bad(e,"cyclic dynamics chain");auto edge=f.find("world.pellet.dynamics.particle."+std::to_string(i)+".next");if(edge==f.end())return bad(e,"missing dynamics next");next=edge->second.target;}
 if(next.owner||next.resource||next.slot)return bad(e,"dynamics chain exceeds count");
 return true;
}
}

namespace pc_midday {
static bool world_ranges(const ActorFields& f,int type,WorldKind kind,std::string& e){
 auto range=[&](const char* key,ScalarKind k,int lo,int hi){int v=0;return number(f,key,k,v,e)&&((v>=lo&&v<=hi)||bad(e,"world scalar outside native range"));};
 if(kind==WorldKind::Plant&&!range("world.plant.mPlantType",ScalarKind::U16,0,11))return false;
 if(kind==WorldKind::Key&&!range("world.key.mState",ScalarKind::S32,0,2))return false;
 if(kind==WorldKind::Seed&&!range("world.seed.mStateId",ScalarKind::S32,0,3))return false;
 if(kind==WorldKind::Bridge){int count=0,stage=0;if(!number(f,"world.bridge.mStageCount",ScalarKind::S32,count,e)||!number(f,"world.bridge._3CA",ScalarKind::S16,stage,e)||stage< -1||stage>=count||!range("world.bridge._400",ScalarKind::U8,0,2))return bad(e,"invalid bridge stage index/shape");}
 if(kind==WorldKind::Pellet&&!range("world.pellet.mCarryState",ScalarKind::U16,0,2))return false;
 if(kind!=WorldKind::Item)return true;
 if(type==OBJTYPE_Goal){if(!range("world.goal.mOnionColour",ScalarKind::U16,0,2)||!range("world.goal.mPcOwner",ScalarKind::S32,-1,1)||!range("world.goal.mPikisToExit",ScalarKind::S32,0,100))return false;for(int i=0;i<2;++i)if(!range(("world.goal.exitFor."+std::to_string(i)).c_str(),ScalarKind::S32,0,100))return false;}
 if(type==OBJTYPE_Pikihead){int purple=0,white=0,bulbmin=0;if(!range("world.head.mSeedColor",ScalarKind::S32,0,2)||!range("world.head.mFlowerStage",ScalarKind::S32,0,2)||!range("world.head.mPcOwner",ScalarKind::S32,-1,1)||!number(f,"world.head.mP2Purple",ScalarKind::Bool,purple,e)||!number(f,"world.head.mP2White",ScalarKind::Bool,white,e)||!number(f,"world.head.mP2Bulbmin",ScalarKind::Bool,bulbmin,e)||purple+white+bulbmin>1)return bad(e,"invalid seed species/maturity");}
 if(type>=OBJTYPE_SluiceSoft&&type<=OBJTYPE_SluiceBombHard){int count=0,current=0;if(!actor_i32(f,"world.building.mNumStages",count,e)||!actor_i32(f,"world.building.mCurrStage",current,e)||count<1||count>4096||current<0||current>count)return bad(e,"invalid gate damage stage");}
 if(type==OBJTYPE_Door||type==OBJTYPE_Gate)if(!range("world.door.mStateId",ScalarKind::S32,0,3))return false;
 if(type==OBJTYPE_GrassGen||type==OBJTYPE_RockGen){const bool grass=type==OBJTYPE_GrassGen;const std::string p=grass?"world.grass.":"world.rocks.";int count=0,active=0,workers=0;if(!number(f,p+(grass?"mTotalGrassCount":"mMaxPebbles"),ScalarKind::U16,count,e)||!number(f,p+(grass?"mActiveGrass":"mActivePebbles"),ScalarKind::U16,active,e)||!number(f,p+"mWorkingPikis",ScalarKind::S32,workers,e)||workers<0||workers>100||active<0||active>count)return bad(e,"invalid foliage population");int alive=0;for(int i=0;i<count;++i){int health=0,shape=0;if(!number(f,p+"entry."+std::to_string(i)+".health",ScalarKind::U8,health,e)||!number(f,p+"entry."+std::to_string(i)+".shape",ScalarKind::U8,shape,e)||shape>2)return bad(e,"invalid foliage element");alive+=health!=0;}if(alive!=active)return bad(e,"foliage active count mismatch");}
 if(type==OBJTYPE_Fish){int count=0,capacity=0;if(!actor_i32(f,"world.fish.mFishCount",count,e)||!actor_i32(f,"world.fish.mMaxFish",capacity,e)||count<0||count>capacity)return bad(e,"fish population bounds");}
 return true;
}
bool world_schema(const ActorFields& f,int expectedType,WorldKind kind,std::vector<FieldSchema>& out,std::string& e){
 int type=0,k=0;if(!actor_i32(f,"creature.objectType",type,e)||type!=expectedType||!actor_i32(f,"world.kind",k,e)||k!=static_cast<int>(kind))return bad(e,"world factory discriminator mismatch");
 switch(kind){case WorldKind::Pellet:if(type!=OBJTYPE_Pellet)return bad(e,"cargo object type");break;case WorldKind::Bridge:case WorldKind::HinderRock:if(type!=OBJTYPE_WorkObject)return bad(e,"work structure object type");break;case WorldKind::Plant:if(type!=OBJTYPE_Plant)return bad(e,"plant object type");break;case WorldKind::Rope:if(type!=OBJTYPE_Rope)return bad(e,"rope object type");break;case WorldKind::Seed:if(type!=OBJTYPE_Seed)return bad(e,"seed object type");break;case WorldKind::Key:if(type!=OBJTYPE_Key)return bad(e,"key object type");break;case WorldKind::Item:break;default:return bad(e,"unknown world family");}
 std::vector<FieldSchema>s;if(!creature_schema(f,s,e))return false;scalar(s,"world.kind",ScalarKind::S32);
 bool collision=false;if(!boolean(f,"world.collision.present",collision,e))return false;auto collider=f.find("creature.mCollInfo");if(collider==f.end())return bad(e,"missing canonical world collider reference");const auto& id=collider->second.target;bool has=id.owner||id.resource||id.slot;if(has!=collision)return bad(e,"world collider presence/identity mismatch");if(collision){auto owned=f.find("world.collision.identity");if(owned==f.end()||owned->second.target.owner!=id.owner||owned->second.target.resource!=id.resource||owned->second.target.slot!=id.slot)return bad(e,"world collider alias mismatch");}scalar(s,"world.collision.present",ScalarKind::Bool);if(collision&&!collision_schema(f,"world.collision",s,e))return false;
 if(kind==WorldKind::Pellet){if(!world_pellet_schema(f,s,e))return false;}
 else {if((kind==WorldKind::Item||kind==WorldKind::Bridge||kind==WorldKind::HinderRock)&&!world_item_schema(f,s,e))return false;if(!world_derived_schema(f,type,kind,s,e))return false;}
 if(!world_ranges(f,type,kind,e))return false;out.swap(s);return true;
}
bool validate_world(const ActorBytes& bytes,const LogicalResolver& resolver,int expectedType,WorldKind kind,std::string& e){ActorFields f;std::vector<FieldSchema>s;return decode_actor_fields(bytes,f,e)&&world_schema(f,expectedType,kind,s,e)&&validate_actor_fields(f,s,resolver,e);}
}
