#include "pc_midday_player_resources.h"
#include "pc_midday_world.h"
#include "pc_midday_world_resources.h"
#include "PlayerState.h"
namespace pc_midday {
bool player_resources_fields(PlayerState& p,const std::array<bool,30>& replay,bool preload,ActorArchive& outer){
 if(outer.mode()!=Mode::Capture)return outer.fail("PlayerState resource visitor is read-only");
 if(p.mTotalParts!=30||p.mTotalRegisteredParts<0||p.mTotalRegisteredParts>30||!p.mUfoParts||!p.mOlimarShapeObj||!p.mNaviLightEfx||!p.mNaviLightGlowEfx)return outer.fail("PlayerState initialized resource factory missing");
 int repair=-1;for(int i=0;i<p.mTotalRegisteredParts;++i)if(p.mCurrentRepairingPart==&p.mUfoParts[i])repair=i;
 if(p.mCurrentRepairingPart&&repair<0)return outer.fail("current repair is not a registered UfoParts element");
 PrefixArchive ar(outer,"player.resources");int version=1;
 if(!ar.field("version",version)||!ar.field("total",p.mTotalParts)||!ar.field("registered",p.mTotalRegisteredParts)||!ar.field("repair",repair)||!ar.field("preload",preload))return false;
 for(int i=0;i<30;++i){bool flag=replay[i];if(!ar.field(("replay."+std::to_string(i)).c_str(),flag))return false;PrefixArchive part(ar,("part."+std::to_string(i)).c_str());if(!part.field("visibility",p.mUfoParts[i].mPartVisType))return false;}
 for(int i=0;i<p.mTotalRegisteredParts;++i){auto& p2=p.mUfoParts[i];PrefixArchive part(ar,("part."+std::to_string(i)).c_str());bool shaped=p2.mPelletShape!=nullptr;
  PaniAnimKeyListener* listenerIdentity=&p2;
  if(!part.field("joint",p2.mRepairAnimJointIndex)||!part.field("model",p2.mModelID)||!part.field("pellet",p2.mPelletID)||!part.field("shaped",shaped)||!part.ref("shape",RefKind::ItemShape,p2.mPelletShape)||!part.field("repairPosition",p2.mRepairEffectPosition)||!part.ref("listenerIdentity",RefKind::AnimListener,listenerIdentity))return false;
  // ShapeDynMaterials default construction initializes this wrapper even when
  // initAnim(nullptr) returns. Its canonical allocations are captured separately.
  PrefixArchive material(part,"materials");if(!world_materials_fields(p2.mAnimatedMaterials,material))return false;
  // initAnim(nullptr) never initializes animator context/manager or speed.
  // No consumer reads that alternate animation until initAnim(shape) resets it.
  if(shaped){PrefixArchive lower(part,"lower"),upper(part,"upper");if(!part.field("motionSpeed",p2.mMotionSpeed)||!world_animation_fields(p2.mAnimator.mLowerAnimator,lower)||!world_animation_fields(p2.mAnimator.mUpperAnimator,upper))return false;}
 }
 if(!ar.ref("olimarShape",RefKind::Shape,p.mOlimarShapeObj)||!ar.field("olimarSpeed",p.mOlimarAnimMgr.mAnimSpeed))return false;
 PrefixArchive lower(ar,"olimarLower"),upper(ar,"olimarUpper");
 return world_animation_fields(p.mOlimarAnimMgr.mLowerAnimator,lower)&&world_animation_fields(p.mOlimarAnimMgr.mUpperAnimator,upper)&&ar.ref("light",RefKind::Effect,p.mNaviLightEfx)&&ar.ref("lightGlow",RefKind::Effect,p.mNaviLightGlowEfx)&&ar.field("lightPosition",p.mNaviLightEfxPos);
}
bool capturePlayerResources(PlayerState& p,LogicalResolver& resolver,double now,const PlayerCoreReadFence& fence,ActorBytes& out,std::string& e){
 if(!fence.sceneInitialized||!fence.agreedReadOnlyFence||fence.tickBefore!=fence.tickAfter){e="PlayerState resources require initialized stopped course fence";return false;}
 std::array<bool,30> replay{};PcMiddayPlayerReplayAccess::read(replay);
 ActorFields f;FieldArchive ar(Mode::Capture,f,resolver,e,now);std::vector<FieldSchema> s;ActorBytes bytes;
 if(!player_resources_fields(p,replay,preloadUFO,ar)||!ar.finish()||!player_resources_schema(f,s,e)||!validate_actor_fields(f,s,resolver,e)||!encode_actor_fields(f,bytes,e))return false;
 out=std::move(bytes);e.clear();return true;
}
}
