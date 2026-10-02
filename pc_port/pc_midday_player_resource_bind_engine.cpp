#include "pc_midday_player_resource_bind.h"
#include "pc_midday_resolved_fields.h"
#include "pc_midday_world.h"
#include "pc_midday_world_resources.h"
#include "pc_midday_particle.h"
#include "PlayerState.h"
#include "Material.h"
#include "UtEffect.h"
namespace pc_midday {
namespace {
void animator(PaniAnimator&a){a.mMgr=nullptr;a.mContext=nullptr;a.mAnimInfo=nullptr;a.mMotionTable=nullptr;a.mListener=nullptr;a.mPlayState=ANIMSTATE_Inactive;a.mCurrentAnimID=a.mStartKeyIndex=a.mEndKeyIndex=0;a.mAnimationCounter=0;a.mCurrentKeyIndex=a.mMotionIdx=-1;a.mPreviousKeyIndex=0;a.mIsFinished=false;a.mPostOneShotPlayState=a.mPostOneShotAnimID=a.mPostOneShotStartKeyIndex=a.mPostOneShotEndKeyIndex=0;}
bool apply(PlayerState& p,IsolatedPlayerResources& owned,ActorArchive& outer,PlayerReplayStage& flags){
 auto* parts=static_cast<PlayerState::UfoParts*>(owned.parts());const auto& plan=*owned.plan();PrefixArchive ar(outer,"player.resources");int version=0,total=0,registered=0,repair=-1;
 if(!ar.scalar("version",ScalarKind::S32,&version)||version!=1||!ar.scalar("total",ScalarKind::S32,&total)||total!=p.mTotalParts||!ar.scalar("registered",ScalarKind::S32,&registered)||registered!=p.mTotalRegisteredParts||!ar.scalar("repair",ScalarKind::S32,&repair)||repair!=plan.repair||!ar.field("preload",flags.preload))return ar.fail("course resource allocation/core mismatch");
 for(int i=0;i<30;++i){if(!ar.field(("replay."+std::to_string(i)).c_str(),flags.replay[i]))return false;PrefixArchive part(ar,("part."+std::to_string(i)).c_str());if(!part.field("visibility",parts[i].mPartVisType))return false;}
 for(int i=0;i<registered;++i){auto& part=parts[i];PrefixArchive a(ar,("part."+std::to_string(i)).c_str());bool shaped=false;PaniAnimKeyListener* listener=nullptr;
  if(!a.field("joint",part.mRepairAnimJointIndex)||!a.field("model",part.mModelID)||!a.field("pellet",part.mPelletID)||!a.scalar("shaped",ScalarKind::Bool,&shaped)||!a.ref("shape",RefKind::ItemShape,part.mPelletShape)||shaped!=(part.mPelletShape!=nullptr)||!a.field("repairPosition",part.mRepairEffectPosition)||!a.ref("listenerIdentity",RefKind::AnimListener,listener)||listener!=static_cast<PaniAnimKeyListener*>(&part))return a.fail("UFO shape/listener binding mismatch");
  PrefixArchive material(a,"materials");if(!world_materials_fields(part.mAnimatedMaterials,material))return false;
  // initAnim(nullptr) does not initialize native animator fields. These local
  // inert defaults are staging safety only; saved active fields overwrite them.
  animator(part.mAnimator.mLowerAnimator);animator(part.mAnimator.mUpperAnimator);
  if(shaped){PrefixArchive lower(a,"lower"),upper(a,"upper");if(!a.field("motionSpeed",part.mMotionSpeed)||!world_animation_fields(part.mAnimator.mLowerAnimator,lower)||!world_animation_fields(part.mAnimator.mUpperAnimator,upper))return false;}
 }
 if(!ar.ref("olimarShape",RefKind::Shape,p.mOlimarShapeObj)||!ar.field("olimarSpeed",p.mOlimarAnimMgr.mAnimSpeed))return false;
 PrefixArchive lower(ar,"olimarLower"),upper(ar,"olimarUpper");
 if(!world_animation_fields(p.mOlimarAnimMgr.mLowerAnimator,lower)||!world_animation_fields(p.mOlimarAnimMgr.mUpperAnimator,upper)||!ar.ref("light",RefKind::Effect,p.mNaviLightEfx)||!ar.ref("lightGlow",RefKind::Effect,p.mNaviLightGlowEfx)||!ar.field("lightPosition",p.mNaviLightEfxPos))return false;
 p.mUfoParts=parts;p.mCurrentRepairingPart=repair<0?nullptr:&parts[repair];return true;
}
}
bool bindPlayerResources(IsolatedPlayerRoot& root,IsolatedPlayerResources& owned,const ActorBytes& bytes,const std::array<ActorBytes,2>& effects,LogicalResolver& resolver,LogicalResolver& lightResolver,LogicalResolver& glowResolver,ConstructorFence& fence,double now,PlayerReplayStage& out,std::string& e){
 if(!root.heldBy(fence)||!owned.heldBy(fence)||!pc_sim_rng_constructor_suppression(true,e)){if(e.empty())e="resource bind requires exact physically fenced private owners";return false;}
 auto* p=root.root();const auto* plan=owned.plan();if(!p||!plan||p->mUfoParts||p->mNaviLightEfx||p->mNaviLightGlowEfx||p->mTotalRegisteredParts!=plan->registered){e="resource bind requires matching unbound private PlayerState";return false;}
 ActorFields f;std::vector<FieldSchema>s;ResolvedFields resolved;std::map<std::string,const void*> exact;
 if(!decode_actor_fields(bytes,f,e)||!player_resources_schema(f,s,e))return false;
 auto* parts=static_cast<PlayerState::UfoParts*>(owned.parts());
 for(int i=0;i<plan->registered;++i){const auto prefix="player.resources.part."+std::to_string(i)+".";int count=-1;if(!actor_i32(f,(prefix+"materials.count").c_str(),count,e)||count!=plan->materials[i]){e="material descriptor changed since allocation";return false;}
  exact[prefix+"listenerIdentity"]=static_cast<PaniAnimKeyListener*>(&parts[i]);for(int j=0;j<count;++j)exact[prefix+"materials.material."+std::to_string(j)]=&owned.materials(i)[j];
 }
 exact["player.resources.light"]=owned.light(false);exact["player.resources.lightGlow"]=owned.light(true);
 if(!resolved.prepare(f,s,resolver,exact,e))return false;
 std::array<ActorFields,2> ef;std::array<ResolvedFields,2> er;
 LogicalResolver* effectResolvers[2]={&lightResolver,&glowResolver};
 for(int i=0;i<2;++i){std::vector<FieldSchema> schema;if(!decode_actor_fields(effects[i],ef[i],e)||!particle_schema(ef[i],ParticleRecordKind::Permanent,"",schema,e)||!er[i].prepare(ef[i],schema,*effectResolvers[i],{},e))return false;}
 // Every schema/reference/owned allocation is validated and pre-resolved before
 // the first named native write. Any unexpected failure rejects this stage.
 PlayerReplayStage flags;FieldArchive ar(Mode::Apply,f,resolved,e,now);
 if(!apply(*p,owned,ar,flags)||!ar.finish())return false;
 for(int i=0;i<2;++i){FieldArchive effect(Mode::Apply,ef[i],er[i],e,now);if(!particle_fields(*owned.light(i!=0),effect)||!effect.finish())return false;}
 out=flags;e.clear();return true;
}
}
