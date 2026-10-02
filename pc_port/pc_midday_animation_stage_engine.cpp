#include "pc_midday_animation_stage.h"
#include "Animator.h"
#include <exception>
namespace pc_midday {
struct IsolatedAnimationContexts::Impl {
 ConstructorFence* fence=nullptr;std::map<u64,std::unique_ptr<AnimContext>> contexts;
 ~Impl(){if(!contexts.empty()){std::string e;if(!fence||!fence->held()||!pc_sim_rng_constructor_suppression(true,e))std::terminate();}}
};
IsolatedAnimationContexts::IsolatedAnimationContexts()=default;
IsolatedAnimationContexts::~IsolatedAnimationContexts()=default;
AnimContext* IsolatedAnimationContexts::context(u64 id)const{if(!impl_)return nullptr;auto it=impl_->contexts.find(id);return it==impl_->contexts.end()?nullptr:it->second.get();}
bool IsolatedAnimationContexts::heldBy(const ConstructorFence& fence)const{return impl_&&impl_->fence==&fence&&fence.held();}
bool IsolatedAnimationContexts::prepare(const std::vector<CanonicalContextRecord>& records,const std::set<u64>& required,const ContextResolverFactory& factory,const AnimationContentCheck& content,const RestoreGate& gate,ConstructorFence& fence,std::string& e){
 if(impl_||!fence.held()||!pc_sim_rng_constructor_suppression(true,e)){if(e.empty())e="canonical contexts require new owner and physical constructor fence";return false;}
 if(!gate.freshProcess||!gate.paused||!gate.zeroInput||!gate.birthEffectsSuppressed||!gate.rewardsSuppressed||!gate.rngDrawsSuppressed||!gate.audioVoicesSuppressed){e="canonical contexts require all paused restore gates";return false;}
 std::vector<PlannedContext> plan;if(!planCanonicalContexts(records,required,factory,content,plan,e))return false;
 try{auto stage=std::make_unique<Impl>();stage->fence=&fence;
  // Complete native allocations before the first named resource field bind.
  for(const auto& entry:plan)stage->contexts.emplace(entry.id,std::make_unique<AnimContext>());
  for(auto& entry:plan){void* data=nullptr;if(!entry.pointers.resolve("context.data",RefKind::Animation,entry.state.data,data,e))return false;auto& native=*stage->contexts.at(entry.id);native.mData=static_cast<AnimData*>(data);native.mCurrentFrame=entry.state.frame;native.mAnimSpeed=entry.state.speed;}
  impl_=std::move(stage);e.clear();return true;
 }catch(const std::exception& x){e=std::string("canonical animation context staging failed: ")+x.what();return false;}
}
}
