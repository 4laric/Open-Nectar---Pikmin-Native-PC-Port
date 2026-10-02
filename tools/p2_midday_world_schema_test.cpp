#include "pc_midday_world.h"
#include "ObjType.h"
#include <cstdio>
#include <cstdlib>
using namespace pc_midday;
namespace {
int checks=0;void require(bool b,const char*m){++checks;if(!b){std::fprintf(stderr,"FAIL %s\n",m);std::exit(1);}}
struct Resolver:LogicalResolver {
 std::map<u32,std::pair<std::string,ReferenceOwnership>> targets;
 bool identify(const char*,RefKind,const void*,LogicalRef&,std::string&)override{return false;}
 bool validate(const char*,RefKind,const LogicalRef&,std::string&)const override{return false;}
 bool validateTyped(const FieldSchema& d,const LogicalRef& r,std::string& e)const override{auto it=targets.find(r.slot);if(r.owner==17&&r.resource==0&&it!=targets.end()&&it->second.first==d.targetType&&(it->second.second==d.ownership||(d.targetType=="CollInfo"&&it->second.second==ReferenceOwnership::ActorSubobject&&d.ownership==ReferenceOwnership::AnyLive)))return true;e="declared type/owner mismatch";return false;}
 bool resolve(const char*,RefKind,const LogicalRef&,void*&,std::string&)override{return false;}
 bool identifyHandle(const char*,RefKind,u32,LogicalRef&,std::string&)override{return false;}
 bool resolveHandle(const char*,RefKind,const LogicalRef&,u32&,std::string&)override{return false;}
};
void put(ActorFields& f,const std::string& key,ScalarKind kind,u64 bits){ActorField v;v.scalar=kind;v.bits=bits;f[key]=v;}
ActorFields base(int type,WorldKind kind,Resolver& resolver,int state=0){ActorFields f;
put(f,"world.goal.mColourFadeRate",ScalarKind::F32,0);
put(f,"world.goal.mColourAnimProgress",ScalarKind::F32,0);
put(f,"world.goal.mColourAnimationEnabled",ScalarKind::Bool,0);
put(f,"world.goal.mSpotEffectActive",ScalarKind::Bool,0);
put(f,"world.goal.mIsClosing",ScalarKind::Bool,0);
put(f,"world.goal.mConeSizeTimer",ScalarKind::F32,0);
put(f,"world.goal.mIsConeEmit",ScalarKind::Bool,0);
put(f,"world.goal.mIsDispensingPikis",ScalarKind::Bool,0);
put(f,"world.goal.mPikisToExit",ScalarKind::S32,0);
put(f,"world.goal.mPikiSpawnTimer",ScalarKind::F32,0);
put(f,"world.goal.mOnionColour",ScalarKind::U16,0);
put(f,"world.goal.mWaypointIdx",ScalarKind::S16,0);
put(f,"world.goal.mPcOwner",ScalarKind::S32,0);
put(f,"world.building.mNumStages",ScalarKind::S32,0);
put(f,"world.building.mCurrStage",ScalarKind::S32,0);
put(f,"world.head.mSeedColor",ScalarKind::S32,0);
put(f,"world.head.mFlowerStage",ScalarKind::S32,0);
put(f,"world.head.mPcOwner",ScalarKind::S32,0);
put(f,"world.head.mP2Purple",ScalarKind::Bool,0);
put(f,"world.head.mP2White",ScalarKind::Bool,0);
put(f,"world.head.mP2Bulbmin",ScalarKind::Bool,0);
put(f,"world.stickBase.mIsActive",ScalarKind::Bool,0);
put(f,"world.stickBase.mEffectDuration",ScalarKind::S8,0);
put(f,"world.bombGen.mCapacity",ScalarKind::S16,0);
put(f,"world.bombGen.mRemaining",ScalarKind::S16,0);
put(f,"world.weeds.mWeedsCount",ScalarKind::S32,0);
put(f,"world.weed.mIsPulled",ScalarKind::U16,0);
put(f,"world.weed.mPulloutTimer",ScalarKind::U16,0);
put(f,"world.rope.mRopeLength",ScalarKind::F32,0);
put(f,"world.rope._2D0",ScalarKind::S32,0);
put(f,"world.seed.mStateId",ScalarKind::S32,0);
put(f,"world.seed.mGrowthTimer",ScalarKind::F32,0);
put(f,"world.key.mState",ScalarKind::S32,0);
put(f,"world.plant.mPlantType",ScalarKind::U16,0);
put(f,"world.plant.mMotionSpeed",ScalarKind::F32,0);
put(f,"world.plant.mIsCulled",ScalarKind::Bool,0);
put(f,"world.plant._394",ScalarKind::Bool,0);
put(f,"world.bridge.mDoUseJointSegments",ScalarKind::Bool,0);
put(f,"world.bridge._3CA",ScalarKind::S16,0);
put(f,"world.bridge._3CC",ScalarKind::U8,0);
put(f,"world.bridge._400",ScalarKind::U8,0);
put(f,"world.bridge.mStageCount",ScalarKind::S32,0);
put(f,"world.bridge._424",ScalarKind::U8,0);
put(f,"world.hinder.mPushingPikmin",ScalarKind::U16,0);
put(f,"world.hinder.mTotalPushStrength",ScalarKind::S32,0);
put(f,"world.hinder.mAmountPushersToStart",ScalarKind::S32,0);
put(f,"world.hinder.mPushSpeed",ScalarKind::F32,0);
put(f,"world.hinder.mCentreSize",ScalarKind::F32,0);
put(f,"world.hinder.mState",ScalarKind::U8,0);
put(f,"world.hinder.mFxCooldownTimer",ScalarKind::U8,0);
put(f,"world.hinder.mPushMoveTimer",ScalarKind::F32,0);
put(f,"world.hinder.mIsMoving",ScalarKind::Bool,0);
put(f,"world.hinder.mIsSoundPlaying",ScalarKind::Bool,0);
put(f,"world.grass.mWorkingPikis",ScalarKind::S32,0);
put(f,"world.grass.mActiveGrass",ScalarKind::U16,0);
put(f,"world.grass.mTotalGrassCount",ScalarKind::U16,0);
put(f,"world.grass.mSize",ScalarKind::F32,0);
put(f,"world.rocks.mWorkingPikis",ScalarKind::S32,0);
put(f,"world.rocks._3CC",ScalarKind::U8,0);
put(f,"world.rocks.mActivePebbles",ScalarKind::U16,0);
put(f,"world.rocks.mMaxPebbles",ScalarKind::U16,0);
put(f,"world.rocks.mSize",ScalarKind::F32,0);
put(f,"world.fish.mFishCount",ScalarKind::S32,0);
put(f,"world.fish.mMaxFish",ScalarKind::S32,0);
put(f,"world.ship.mIsMenuOpen",ScalarKind::Bool,0);
put(f,"world.ship.mIsLightActive",ScalarKind::Bool,0);
put(f,"world.ship.mShouldLightActivate",ScalarKind::Bool,0);
put(f,"world.ship.mIsTroubleFxEnabled",ScalarKind::Bool,0);
put(f,"world.ship.mTroubleFxTimer",ScalarKind::F32,0);
put(f,"world.ship.mTroubleFxState",ScalarKind::U32,0);
put(f,"world.ship.mJetLevel",ScalarKind::S16,0);
put(f,"world.ship.mShipUpgradeLevel",ScalarKind::U8,0);
put(f,"world.ship.mConeEffectId",ScalarKind::S32,0);
put(f,"world.ship.mIsPca1FxActive",ScalarKind::Bool,0);
put(f,"world.ship.mIsPca2FxActive",ScalarKind::Bool,0);
put(f,"world.ship.mWaypointID",ScalarKind::S32,0);
put(f,"world.ship.mPcOwner",ScalarKind::S32,0);
put(f,"world.ship.mNeedPathfindRefresh",ScalarKind::Bool,0);
put(f,"world.door.mFadeTimer",ScalarKind::F32,0);
put(f,"world.door.mStateId",ScalarKind::S32,0);

 put(f,"creature.objectType",ScalarKind::S32,type);put(f,"creature.search.capacity",ScalarKind::S16,0);put(f,"creature.search.count",ScalarKind::S16,0);put(f,"creature.search.last",ScalarKind::S32,0xffffffff);
 ActorField coll;coll.category=FieldCategory::Reference;coll.reference=RefKind::CollInfo;f["creature.mCollInfo"]=coll;ActorField dyn;dyn.category=FieldCategory::Reference;dyn.reference=RefKind::DynParticle;f["world.pellet.dynamics.head"]=dyn;
 put(f,"world.kind",ScalarKind::S32,static_cast<int>(kind));put(f,"world.collision.present",ScalarKind::Bool,0);
 bool ai=type==OBJTYPE_Goal||type==OBJTYPE_Pikihead||type==OBJTYPE_Water||type==OBJTYPE_FallWater||type==OBJTYPE_Plant||(type>=22&&type<=25);
 put(f,"world.ai.current",ScalarKind::S32,ai?state:0xffffffff);put(f,"world.ai.last",ScalarKind::S32,0xffffffff);put(f,"world.ai.events",ScalarKind::S32,0);
 put(f,"world.item.animation.active",ScalarKind::Bool,0);put(f,"world.plant.animation.active",ScalarKind::Bool,0);
 put(f,"world.bridge.mStageCount",ScalarKind::S32,1);put(f,"world.bridge.allocation",ScalarKind::S32,1);put(f,"world.bridge._3CA",ScalarKind::S16,0xffff);
 put(f,"world.building.mNumStages",ScalarKind::S32,3);
 put(f,"world.grass.allocation",ScalarKind::U16,0);put(f,"world.rocks.allocation",ScalarKind::U16,0);
 put(f,"world.fish.allocation",ScalarKind::S32,32);put(f,"world.fish.mMaxFish",ScalarKind::S32,32);put(f,"world.fish.mFishCount",ScalarKind::S32,32);
 for(int i=0;i<2;++i)put(f,"world.goal.exitFor."+std::to_string(i),ScalarKind::S32,0);
 for(int i=0;i<8;++i)put(f,"world.ship.anim."+std::to_string(i)+".animation.active",ScalarKind::Bool,0);
 put(f,"world.pellet.current",ScalarKind::S32,state);put(f,"world.pellet.last",ScalarKind::S32,0xffffffff);put(f,"world.pellet.shaped",ScalarKind::Bool,0);put(f,"world.pellet.dynamics.count",ScalarKind::U16,0);put(f,"world.pellet.mCarryState",ScalarKind::U16,0);
 for(auto p:{"world.goal.materials.count","world.building.materials.count","world.bridge.materials.count","world.building.platforms.count"})put(f,p,ScalarKind::S32,0);
 for(auto p:{"world.bridge.buildPresent","world.hinder.buildPresent"})put(f,p,ScalarKind::Bool,0);
 std::vector<FieldSchema>s;std::string error;require(world_schema(f,type,kind,s,error),error.c_str());ActorFields complete;
 for(const auto& d:s){ActorField v;v.category=d.category;v.scalar=d.scalar;v.reference=d.reference;auto found=f.find(d.key);if(found!=f.end())v=found->second;
  if(d.category!=FieldCategory::Scalar){require(!d.targetType.empty(),"every reference has declared type");if(!d.nullable){u32 id=static_cast<u32>(resolver.targets.size()+1);resolver.targets[id]={d.targetType,d.ownership};v.target={17,0,id};}}
  require(complete.emplace(d.key,v).second,"no duplicate family fields");
 }return complete;
}
bool valid(const ActorFields& f,int type,WorldKind kind,const Resolver&r){ActorBytes bytes;std::string error;return encode_actor_fields(f,bytes,error)&&validate_world(bytes,r,type,kind,error);}
}
int main(){
 for(int type:{1,6,16,30,15,22,23,24,25,29,35,13,31,32,34,33,37,4,5,17,27,28}){Resolver r;auto f=base(type,WorldKind::Item,r);require(valid(f,type,WorldKind::Item,r),"registered item family roundtrip");f.erase("creature.mHealth");require(!valid(f,type,WorldKind::Item,r),"shared Creature field required");}
 for(auto entry:{std::make_pair(38,WorldKind::Bridge),{38,WorldKind::HinderRock},{51,WorldKind::Plant},{18,WorldKind::Rope},{2,WorldKind::Seed},{3,WorldKind::Key}}){Resolver r;auto f=base(entry.first,entry.second,r);require(valid(f,entry.first,entry.second,r),"structure roundtrip");put(f,"world.kind",ScalarKind::S32,99);require(!valid(f,entry.first,entry.second,r),"foreign structure discriminator refused");}
 for(int state=0;state<6;++state){Resolver r;auto f=base(52,WorldKind::Pellet,r,state);require(valid(f,52,WorldKind::Pellet,r),"all six cargo states");put(f,"world.pellet.current",ScalarKind::S32,6);require(!valid(f,52,WorldKind::Pellet,r),"unknown cargo state refused");}
 Resolver r;auto f=base(16,WorldKind::Item,r);f["world.ai.machine"].target.owner=99;require(!valid(f,16,WorldKind::Item,r),"foreign resource identity refused");
 r={};f=base(16,WorldKind::Item,r);auto wrong=f;wrong["world.ai.machine"].target=f["creature.mProps"].target;require(!valid(wrong,16,WorldKind::Item,r),"wrong declared target type refused");
 put(f,"world.goal.mOnionColour",ScalarKind::U16,3);require(!valid(f,16,WorldKind::Item,r),"invalid Onion colour refused");
 r={};f=base(38,WorldKind::Bridge,r);put(f,"world.bridge.allocation",ScalarKind::S32,2);require(!valid(f,38,WorldKind::Bridge,r),"bridge array capacity mismatch refused");
 r={};f=base(15,WorldKind::Item,r);put(f,"world.head.mP2Purple",ScalarKind::Bool,1);put(f,"world.head.mP2White",ScalarKind::Bool,1);require(!valid(f,15,WorldKind::Item,r),"conflicting seed species refused");
 r={};f=base(52,WorldKind::Pellet,r,1);f["world.pellet.mTargetGoal"].target={};require(!valid(f,52,WorldKind::Pellet,r),"sucking cargo requires target");
 r={};f=base(16,WorldKind::Item,r);put(f,"world.collision.present",ScalarKind::Bool,1);put(f,"world.collision.count",ScalarKind::U16,0);put(f,"world.collision.capacity",ScalarKind::U16,0);put(f,"world.collision.defaultStorage",ScalarKind::Bool,1);
 ActorField identity;identity.category=FieldCategory::Reference;identity.reference=RefKind::CollInfo;u32 cid=static_cast<u32>(r.targets.size()+1);r.targets[cid]={"CollInfo",ReferenceOwnership::ActorSubobject};identity.target={17,0,cid};f["creature.mCollInfo"]=identity;f["world.collision.identity"]=identity;ActorField shape;shape.category=FieldCategory::Reference;shape.reference=RefKind::Shape;f["world.collision.shape"]=shape;
 require(valid(f,16,WorldKind::Item,r),"canonical collider present roundtrip");auto mismatch=f;mismatch["world.collision.identity"].target.slot++;require(!valid(mismatch,16,WorldKind::Item,r),"collider alias mismatch refused");mismatch=f;put(mismatch,"world.collision.present",ScalarKind::Bool,0);require(!valid(mismatch,16,WorldKind::Item,r),"hidden collider presence refused");
 r={};f=base(52,WorldKind::Pellet,r);put(f,"world.pellet.dynamics.count",ScalarKind::U16,1);ActorField particle;particle.category=FieldCategory::Reference;particle.reference=RefKind::DynParticle;u32 pid=static_cast<u32>(r.targets.size()+1);r.targets[pid]={"DynParticle",ReferenceOwnership::ActorSubobject};particle.target={17,0,pid};f["world.pellet.dynamics.head"]=particle;particle.target={};f["world.pellet.dynamics.particle.0.next"]=particle;
 std::vector<FieldSchema> dynamicSchema;std::string dynamicError;require(world_schema(f,52,WorldKind::Pellet,dynamicSchema,dynamicError),"single particle schema");for(auto& d:dynamicSchema)if(!f.count(d.key)){ActorField v;v.category=d.category;v.scalar=d.scalar;v.reference=d.reference;f[d.key]=v;}require(valid(f,52,WorldKind::Pellet,r),"single dynamics body roundtrip");mismatch=f;mismatch["world.pellet.dynamics.particle.0.next"].target=f["world.pellet.dynamics.head"].target;require(!valid(mismatch,52,WorldKind::Pellet,r),"cyclic dynamics tail refused");mismatch=f;mismatch["world.pellet.dynamics.head"].target={};require(!valid(mismatch,52,WorldKind::Pellet,r),"short dynamics chain refused");
 std::printf("PASS world typed schemas %d controls\n",checks);return 0;
}
