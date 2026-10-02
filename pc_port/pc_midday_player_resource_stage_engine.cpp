#include "pc_midday_player_resource_stage.h"
#include "PlayerState.h"
#include "Material.h"
#include "UtEffect.h"
#include <exception>
namespace pc_midday {
struct IsolatedPlayerResources::Impl {
 ConstructorFence* fence=nullptr;PlayerResourcePlan plan;
 std::array<std::unique_ptr<Material[]>,30> materials;
 std::array<std::unique_ptr<PermanentEffect>,2> lights;
 // Destroy the containing native wrappers before their material backing.
 std::unique_ptr<PlayerState::UfoParts[]> parts;
 ~Impl(){if(parts){std::string e;if(!fence||!fence->held()||!pc_sim_rng_constructor_suppression(true,e))std::terminate();}}
};
IsolatedPlayerResources::IsolatedPlayerResources()=default;
IsolatedPlayerResources::~IsolatedPlayerResources()=default;
bool IsolatedPlayerResources::heldBy(const ConstructorFence& f)const{return impl_&&impl_->fence==&f&&f.held();}
void* IsolatedPlayerResources::parts()const{return impl_?impl_->parts.get():nullptr;}
Material* IsolatedPlayerResources::materials(size_t part)const{return impl_&&part<30?impl_->materials[part].get():nullptr;}
PermanentEffect* IsolatedPlayerResources::light(bool glow)const{return impl_?impl_->lights[glow?1:0].get():nullptr;}
const PlayerResourcePlan* IsolatedPlayerResources::plan()const{return impl_?&impl_->plan:nullptr;}
bool IsolatedPlayerResources::allocate(const ActorBytes& bytes,const PlayerCoreFields& core,const LogicalResolver& resolver,const RestoreGate& gate,ConstructorFence& fence,std::string& e){
 if(impl_||!fence.held()||!pc_sim_rng_constructor_suppression(true,e)){if(e.empty())e="course resources require new owner and physical constructor fence";return false;}
 if(!gate.freshProcess||!gate.paused||!gate.zeroInput||!gate.birthEffectsSuppressed||!gate.rewardsSuppressed||!gate.rngDrawsSuppressed||!gate.audioVoicesSuppressed){e="course resources require all paused restore gates";return false;}
 PlayerResourcePlan plan;if(!planPlayerResources(bytes,core,resolver,plan,e))return false;
 try{
  auto staged=std::make_unique<Impl>();staged->fence=&fence;staged->plan=plan;
  // These default constructors allocate no nested graph and invoke no game
  // callbacks. Never use registerUfoParts/initAnim/changeEffect here.
  staged->parts=std::make_unique<PlayerState::UfoParts[]>(30);
  for(size_t i=0;i<30;++i){auto& part=staged->parts[i];part.mRepairAnimJointIndex=-1;part.mModelID=part.mPelletID=0;part.mPelletShape=nullptr;part.mMotionSpeed=0;part.mPartVisType=plan.visibility[i];
   if(plan.materials[i]){staged->materials[i]=std::make_unique<Material[]>(plan.materials[i]);part.mAnimatedMaterials.mMatCount=plan.materials[i];part.mAnimatedMaterials.mMaterials=staged->materials[i].get();}
  }
  for(auto& light:staged->lights)light=std::make_unique<PermanentEffect>();
  impl_=std::move(staged);e.clear();return true;
 }catch(const std::exception& x){e=std::string("course resource allocation failed: ")+x.what();return false;}
}
}
