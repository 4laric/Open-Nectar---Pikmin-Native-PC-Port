#include "pc_midday_animation_context.h"
#include "Animator.h"
namespace pc_midday {
bool declareAnimationData(AnimationContentIndex& index,const LogicalRef& id,const AnimData& native,std::string& e){
 if(native.mTotalFrameCount<=0){e="installed AnimData has invalid frame count";return false;}
 return index.declare(id,u32(native.mTotalFrameCount),e);
}
bool captureAnimationContext(const AnimContext& native,LogicalResolver& resolver,const AnimationContentCheck& content,ActorBytes& out,std::string& e){
 AnimationContextRecord r;r.frame=native.mCurrentFrame;r.speed=native.mAnimSpeed;
 if(native.mData&&!resolver.identify("context.data",RefKind::Animation,native.mData,r.data,e))return false;
 return encodeAnimationContext(r,resolver,content,out,e);
}
bool stageAnimationContext(AnimContext& native,const ActorBytes& bytes,LogicalResolver& resolver,const AnimationContentCheck& content,const RestoreGate& gate,std::string& e){
 if(!gate.freshProcess||!gate.paused||!gate.zeroInput||!gate.birthEffectsSuppressed||!gate.rewardsSuppressed||!gate.rngDrawsSuppressed||!gate.audioVoicesSuppressed){e="animation context requires full fresh restore fence";return false;}
 AnimationContextRecord r;if(!decodeAnimationContext(bytes,resolver,content,r,e))return false;
 void* data=nullptr;if(!resolver.resolve("context.data",RefKind::Animation,r.data,data,e))return false;
 // Exact adjusted AnimData pointer comes from the compiled schema/catalog.
 // No animate/updateContext/startAnim call, allocation or callback after this.
 native.mData=static_cast<AnimData*>(data);native.mCurrentFrame=r.frame;native.mAnimSpeed=r.speed;
 e.clear();return true;
}
}
