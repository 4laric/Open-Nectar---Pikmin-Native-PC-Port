#if defined(PIKI_PC_PORT)
#include "pc_midday_projectile.h"
#include "pc_p2_cannon_stone.h"
#include "pc_p2_rock_hazard.h"
#include "pc_p2_kabuto_stone_fleet.h"
#include "pc_p2_groink_volley.h"
#include "pc_p2_bombsarai_bomb.h"
using namespace pc_midday;
namespace {
template<class E>bool enumeration(ActorArchive& a,const char* key,E& field,int max){int n=a.mode()==Mode::Capture?int(field):0;if(!a.scalar(key,ScalarKind::S32,&n)||n<0||n>max)return a.fail("projectile enum outside domain");if(a.mode()==Mode::Apply)field=static_cast<E>(n);return true;}
template<class V>bool vector(ActorArchive& a,const char* key,V& v){PrefixArchive p(a,key);return p.field("x",v.x)&&p.field("y",v.y)&&p.field("z",v.z);}
template<class T>bool token(ActorArchive& a,const char* key,T& live){u64 v=a.mode()==Mode::Capture?live:0;if(!a.token64(key,RefKind::ProjectileToken,v))return false;if(a.mode()==Mode::Apply)live=v;return true;}
}
struct PcMiddayProjectileAccess {
 static bool fields(P2CannonStone& o,ActorArchive& a){
 if(!enumeration(a,"phase",o.mPhase,3))return false;
 if(!enumeration(a,"config.variant",o.mConfig.variant,1))return false;
 if(!a.field("config.moveSpeed",o.mConfig.moveSpeed))return false;
 if(!a.field("config.searchRumbleSpeed",o.mConfig.searchRumbleSpeed))return false;
 if(!a.field("config.turnSpeed",o.mConfig.turnSpeed))return false;
 if(!a.field("config.maxTurnAngle",o.mConfig.maxTurnAngle))return false;
 if(!a.field("config.attackDamage",o.mConfig.attackDamage))return false;
 if(!a.field("config.sightRadius",o.mConfig.sightRadius))return false;
 if(!a.field("config.collisionRadius",o.mConfig.collisionRadius))return false;
 if(!a.field("config.health",o.mConfig.health))return false;
 if(!a.field("mFaceDir",o.mFaceDir))return false;
 if(!a.field("mTimer",o.mTimer))return false;
 if(!a.field("mScale",o.mScale))return false;
 if(!a.field("mHealth",o.mHealth))return false;
 if(!a.field("mHoming",o.mHoming))return false;
 if(!a.field("mHealthZeroed",o.mHealthZeroed))return false;
 if(!vector(a,"mPosition",o.mPosition))return false;
 if(!vector(a,"mVelocity",o.mVelocity))return false;
 if(!vector(a,"mTargetVelocity",o.mTargetVelocity))return false;
 return token(a,"source",o.mSourceToken)&&token(a,"self",o.mSelfToken);
 }
 static bool fields(P2CannonStonePool& o,ActorArchive& a){
 int capacity=a.mode()==Mode::Capture?o.mCapacity:0;
 if(!a.scalar("capacity",ScalarKind::S32,&capacity)||capacity<0||capacity>16)return a.fail("projectile pool capacity");
 if(a.mode()!=Mode::Capture&&capacity!=o.mCapacity)return a.fail("factory pool capacity mismatch");
 for(int i=0;i<16;++i){PrefixArchive p(a,("slot."+std::to_string(i)).c_str());if(!p.field("used",o.mUsed[i])||!fields(o.mStones[i],p))return false;}
 return true;
 }
 static bool fields(P2RockHazard& o,ActorArchive& a){
 if(!enumeration(a,"phase",o.mPhase,6))return false;
 if(!enumeration(a,"motion",o.mMotion,2))return false;
 if(!a.field("config.fallSpeed",o.mConfig.fallSpeed))return false;
 if(!a.field("config.fallOffset",o.mConfig.fallOffset))return false;
 if(!a.field("config.scaleUpRate",o.mConfig.scaleUpRate))return false;
 if(!a.field("config.sightRadius",o.mConfig.sightRadius))return false;
 if(!a.field("config.attackDamage",o.mConfig.attackDamage))return false;
 if(!a.field("config.collisionRadius",o.mConfig.collisionRadius))return false;
 if(!a.field("config.health",o.mConfig.health))return false;
 if(!a.field("mScale",o.mScale))return false;
 if(!a.field("mTimer",o.mTimer))return false;
 if(!a.field("mHealth",o.mHealth))return false;
 if(!a.field("mTimedAppear",o.mTimedAppear))return false;
 if(!a.field("mAtari",o.mAtari))return false;
 if(!a.field("mUntargetable",o.mUntargetable))return false;
 if(!a.field("mHardConstrained",o.mHardConstrained))return false;
 if(!a.field("mAnimating",o.mAnimating))return false;
 if(!a.field("mModelHidden",o.mModelHidden))return false;
 if(!a.field("mCullable",o.mCullable))return false;
 if(!a.field("mCullSound",o.mCullSound))return false;
 if(!a.field("mShadow",o.mShadow))return false;
 if(!a.field("mShadowForced",o.mShadowForced))return false;
 if(!a.field("mFallEffect",o.mFallEffect))return false;
 if(!a.field("mDeadEffect",o.mDeadEffect))return false;
 if(!a.field("mAnimationCullingOff",o.mAnimationCullingOff))return false;
 if(!a.field("mColliding",o.mColliding))return false;
 if(!a.field("mMotionStopped",o.mMotionStopped))return false;
 if(!vector(a,"mPosition",o.mPosition))return false;
 if(!vector(a,"mVelocity",o.mVelocity))return false;
 if(!vector(a,"mTargetVelocity",o.mTargetVelocity))return false;
 return token(a,"source",o.mSourceToken)&&token(a,"self",o.mSelfToken);
 }
 static bool fields(P2RockHazardPool& o,ActorArchive& a){
 int capacity=a.mode()==Mode::Capture?o.mCapacity:0;
 if(!a.scalar("capacity",ScalarKind::S32,&capacity)||capacity<0||capacity>16)return a.fail("projectile pool capacity");
 if(a.mode()!=Mode::Capture&&capacity!=o.mCapacity)return a.fail("factory pool capacity mismatch");
 for(int i=0;i<16;++i){PrefixArchive p(a,("slot."+std::to_string(i)).c_str());if(!p.field("used",o.mUsed[i])||!fields(o.mRocks[i],p))return false;}
 return true;
 }
 static bool fields(P2BombSaraiBomb& o,ActorArchive& a){
 if(!enumeration(a,"phase",o.mPhase,5))return false;
 if(!a.field("config.gravityPerTick",o.mConfig.gravityPerTick))return false;
 if(!a.field("config.fuseHealth",o.mConfig.fuseHealth))return false;
 if(!a.field("config.armLoopTicks",o.mConfig.armLoopTicks))return false;
 if(!a.field("config.bombRadius",o.mConfig.bombRadius))return false;
 if(!a.field("config.blastRadius",o.mConfig.blastRadius))return false;
 if(!a.field("config.blastHalfHeight",o.mConfig.blastHalfHeight))return false;
 if(!a.field("config.tekiDamage",o.mConfig.tekiDamage))return false;
 if(!a.field("config.naviPikiDamage",o.mConfig.naviPikiDamage))return false;
 if(!a.field("config.ip02TriggerLimit",o.mConfig.ip02TriggerLimit))return false;
 if(!a.field("mEscapeTicks",o.mEscapeTicks))return false;
 if(!a.field("mArmTicksRemaining",o.mArmTicksRemaining))return false;
 if(!a.field("mFuseHealthRemaining",o.mFuseHealthRemaining))return false;
 if(!a.field("mDetonateDelayTicks",o.mDetonateDelayTicks))return false;
 if(!a.field("mInductionCounter",o.mInductionCounter))return false;
 if(!a.field("mHasBlast",o.mHasBlast))return false;
 if(!vector(a,"position",o.mPosition)||!vector(a,"velocity",o.mVelocity)||!token(a,"carrier",o.mCarrierToken))return false;
 PrefixArchive b(a,"blast");
 if(!b.field("radius",o.mBlast.radius))return false;
 if(!b.field("halfHeight",o.mBlast.halfHeight))return false;
 if(!b.field("tekiDamage",o.mBlast.tekiDamage))return false;
 if(!b.field("naviPikiDamage",o.mBlast.naviPikiDamage))return false;
 if(!b.field("knockbackNavi",o.mBlast.knockbackNavi))return false;
 if(!b.field("knockbackPiki",o.mBlast.knockbackPiki))return false;
 if(!b.field("hasCarrier",o.mBlast.hasCarrier))return false;
 if(!b.field("carrierValid",o.mBlast.carrierValid))return false;
 return vector(b,"center",o.mBlast.center)&&token(b,"carrier",o.mBlast.carrierToken);
 }
 static bool fields(P2BombSaraiBombPool& o,ActorArchive& a){
 int capacity=a.mode()==Mode::Capture?o.mCapacity:0;if(!a.scalar("capacity",ScalarKind::S32,&capacity)||capacity<0||capacity>16)return a.fail("bomb pool capacity");
 if(a.mode()!=Mode::Capture&&capacity!=o.mCapacity)return a.fail("bomb factory capacity mismatch");
 for(int i=0;i<16;++i){PrefixArchive p(a,("slot."+std::to_string(i)).c_str());if(!p.field("used",o.mUsed[i])||!fields(o.mBombs[i],p))return false;}return true;
 }
 static bool terminal(P2GroinkTerminalStep& o,ActorArchive& a){return a.field("valid",o.valid)&&enumeration(a,"reason",o.reason,4)&&vector(a,"start",o.start)&&vector(a,"end",o.end);}
 static bool fields(P2GroinkPolicy& o,ActorArchive& a){PrefixArchive term(a,"terminal");return a.field("active",o.mShell.active)&&a.field("primary",o.mShell.primary)&&vector(a,"position",o.mShell.position)&&vector(a,"velocity",o.mShell.velocity)&&terminal(o.mLastTerminalStep,term);}
 template<class T>static bool size(ActorArchive&a,const char* key,T& live,u32 limit){u32 n=a.mode()==Mode::Capture?u32(live):0;if(!a.scalar(key,ScalarKind::U32,&n)||n>limit)return a.fail("projectile list bounds");if(a.mode()==Mode::Apply)live=n;return true;}
 static bool fields(P2GroinkVolley& o,ActorArchive& a){
 if(!size(a,"activeCount",o.mActiveCount,6)||!size(a,"inactiveCount",o.mInactiveCount,6)||!size(a,"terminalCount",o.mTerminalCount,6)||!size(a,"segmentCount",o.mSegmentCount,6))return false;
 for(int i=0;i<6;++i){PrefixArchive p(a,("slot."+std::to_string(i)).c_str());PrefixArchive node(p,"node"),t(p,"terminal"),step(t,"step"),seg(p,"segment");auto& tr=o.mTerminals[i];auto& sg=o.mSegments[i];
 if(!fields(o.mNodes[i],node)||!p.field("primary",o.mPrimary[i])||!size(p,"activeIndex",o.mActive[i],5)||!size(p,"inactiveIndex",o.mInactive[i],5)||!size(t,"slot",tr.slot,5)||!t.field("primary",tr.primary)||!terminal(tr.step,step)||!size(seg,"slot",sg.slot,5)||!seg.field("primary",sg.primary)||!seg.field("terminal",sg.terminal)||!vector(seg,"start",sg.start)||!vector(seg,"end",sg.end))return false;
 }return true;
 }
 static bool fields(p2kabutostone::Fleet& o,ActorArchive& a){
 if(!a.field("nextId",o.mNextId)||!a.field("graceIgnored",o.mGraceIgnored)||!a.field("strikesDeferred",o.mStrikesDeferred))return false;
 for(int i=0;i<16;++i){auto& slot=o.mSlots[i];PrefixArchive p(a,("slot."+std::to_string(i)).c_str()),stone(p,"stone");
 if(!p.field("used",slot.used)||!fields(slot.stone,stone)||!p.field("id",slot.id)||!token(p,"owner",slot.owner)||!vector(p,"birth",slot.birth)||!p.field("dirX",slot.dirX)||!p.field("dirZ",slot.dirZ)||!p.field("vy",slot.vy)||!p.field("deadHold",slot.deadHold)||!p.field("travel",slot.travel)||!p.field("maxLateral",slot.maxLateral)||!p.field("closest",slot.closest)||!p.field("hits",slot.hits))return false;
 u32 count=a.mode()==Mode::Capture?u32(slot.ledger.size()):0;if(!p.scalar("ledgerCount",ScalarKind::U32,&count)||count>4096)return p.fail("projectile ledger count");
 // Ledger storage carries no separately addressable scene objects. Allocate
 // only on the disposable object, after complete pure validation succeeded.
 if(a.mode()==Mode::Apply)slot.ledger.resize(count);
 for(u32 j=0;j<count;++j){u64 value=a.mode()==Mode::Capture?slot.ledger[j]:0;if(!p.token64(("ledger."+std::to_string(j)).c_str(),RefKind::ProjectileToken,value))return false;if(a.mode()==Mode::Apply)slot.ledger[j]=value;}
 }return true;
 }
};
namespace pc_midday {
#define IMPLEMENT(T,K) \
bool projectile_fields(T& o,ActorArchive& a){int version=1,kind=int(ProjectileType::K);if(!a.scalar("projectile.version",ScalarKind::S32,&version)||!a.scalar("projectile.kind",ScalarKind::S32,&kind)||version!=1||kind!=int(ProjectileType::K))return a.fail("projectile factory/version mismatch");PrefixArchive body(a,"projectile.body");return PcMiddayProjectileAccess::fields(o,body);} \
bool capture_projectile(T& o,LogicalResolver& r,ActorBytes& bytes,std::string& e){ActorFields f;FieldArchive a(Mode::Capture,f,r,e,0);std::vector<FieldSchema>s;return projectile_fields(o,a)&&a.finish()&&projectile_schema(f,ProjectileType::K,s,e)&&validate_actor_fields(f,s,r,e)&&encode_actor_fields(f,bytes,e);} \
bool bind_projectile(T& o,const ActorBytes& bytes,LogicalResolver& r,std::string& e){if(!validate_projectile(bytes,ProjectileType::K,r,e))return false;ActorFields f;if(!decode_actor_fields(bytes,f,e))return false;FieldArchive v(Mode::Validate,f,r,e,0);if(!projectile_fields(o,v)||!v.finish())return false;FieldArchive a(Mode::Apply,f,r,e,0);return projectile_fields(o,a)&&a.finish();}
IMPLEMENT(P2CannonStone,CannonStone)
IMPLEMENT(P2RockHazard,RockHazard)
IMPLEMENT(P2CannonStonePool,CannonPool)
IMPLEMENT(P2RockHazardPool,RockPool)
IMPLEMENT(p2kabutostone::Fleet,KabutoFleet)
IMPLEMENT(P2GroinkPolicy,GroinkShell)
IMPLEMENT(P2GroinkVolley,GroinkVolley)
IMPLEMENT(P2BombSaraiBomb,BombSarai)
IMPLEMENT(P2BombSaraiBombPool,BombSaraiPool)
#undef IMPLEMENT
}
#endif
