#include "pc_midday_actor_graph.h"
#include "pc_midday_actor_shell.h"
#include "pc_midday_actor_ancillary.h"
#include "pc_midday_allocation_owner.h"
#include "pc_midday_state_factory.h"
#include "pc_midday_piki_action_factory.h"
#include "pc_midday_constructor.h"
#include "ViewPiki.h"
#include "Navi.h"
#include <cstring>
#include <exception>
namespace pc_midday {
struct ActorAllocationGraph::Impl {
 ConstructorFence& fence;
 AllocationOwner owner;
 Navi* navi=nullptr;ViewPiki* piki=nullptr;
 Impl(ConstructorFence& f,std::size_t failure):fence(f),owner(failure){}
 ~Impl(){
  if(owner.ownedEntries()){
   std::string error;
   if(!fence.held()||!pc_sim_rng_constructor_suppression(true,error))std::terminate();
   owner.reset();
  }
 }
};
ActorAllocationGraph::ActorAllocationGraph()=default;
ActorAllocationGraph::~ActorAllocationGraph()=default;
Navi* ActorAllocationGraph::stagedNavi()const{return impl_?impl_->navi:nullptr;}
ViewPiki* ActorAllocationGraph::stagedPiki()const{return impl_?impl_->piki:nullptr;}
bool ActorAllocationGraph::empty()const{return !impl_;}
void ActorAllocationGraph::reset(){impl_.reset();}
namespace {
bool number(const ActorFields& fields,const char* key,float& output,std::string& error){
 auto found=fields.find(key);
 if(found==fields.end()||found->second.category!=FieldCategory::Scalar||found->second.scalar!=ScalarKind::F32){error="missing saved allocation parameter";return false;}
 u32 bits=static_cast<u32>(found->second.bits);std::memcpy(&output,&bits,sizeof(output));return true;
}
}
bool ActorAllocationGraph::prepareNavi(const ActorBytes& base,LogicalResolver& resolver,ConstructorFence& fence,
 int port,std::string& error,std::size_t failAt){
 if(impl_){error="actor graph already prepared";return false;}
 attempts_=0;std::unique_ptr<Impl> pending;
 struct Receipt {std::size_t& result;std::unique_ptr<Impl>& stage;~Receipt(){if(stage)result=stage->owner.allocationAttempts();}} receipt{attempts_,pending};
 try {
  pending.reset(new Impl(fence,failAt));
  if(!allocate_owned_navi_shell(base,resolver,fence,pending->owner,pending->navi,error)){attempts_=pending->owner.allocationAttempts();return false;}
  ActorFields fields;if(!decode_actor_fields(base,fields,error))return false;
  NaviAncillaryConfig config{5,0,port,0,0,0};
  if(!actor_i32(fields,"navi.runtime.plate.capacity",config.plateCapacity,error)||
     !number(fields,"navi.runtime.plate.params.startOffset",config.plateStartOffset,error)||
     !number(fields,"navi.runtime.plate.params.lengthLimit",config.plateLengthLimit,error)||
     !number(fields,"navi.runtime.plate.params.maxPosSize",config.plateMaxPosSize,error))return false;
  const bool ok=allocate_navi_state_graph(*pending->navi,pending->owner,error)&&
                allocate_navi_ancillary(*pending->navi,config,pending->owner,error);
  attempts_=pending->owner.allocationAttempts();if(!ok)return false;
  impl_=std::move(pending);error.clear();return true;
 } catch(const std::exception& exception){if(pending)attempts_=pending->owner.allocationAttempts();error=exception.what();return false;}
}
bool ActorAllocationGraph::prepareViewPiki(const ActorBytes& base,const ActorBytes& subtype,LogicalResolver& resolver,
 ConstructorFence& fence,std::string& error,std::size_t failAt){
 if(impl_){error="actor graph already prepared";return false;}
 attempts_=0;std::unique_ptr<Impl> pending;
 struct Receipt {std::size_t& result;std::unique_ptr<Impl>& stage;~Receipt(){if(stage)result=stage->owner.allocationAttempts();}} receipt{attempts_,pending};
 try {
  pending.reset(new Impl(fence,failAt));
  if(!allocate_owned_view_piki_shell(base,subtype,resolver,fence,pending->owner,pending->piki,error)){attempts_=pending->owner.allocationAttempts();return false;}
  ActorFields fields;if(!decode_actor_fields(base,fields,error))return false;
  PikiAncillaryConfig config{4,0};
  if(!actor_i32(fields,"piki.runtime.path.capacity",config.pathCapacity,error))return false;
  if(!allocate_piki_state_graph(*pending->piki,pending->owner,error)||
     !allocate_piki_ancillary(*pending->piki,config,pending->owner,error)){
   attempts_=pending->owner.allocationAttempts();return false;
  }
  pending->piki->mActiveAction=allocate_piki_action_graph(*pending->piki,pending->owner);
  attempts_=pending->owner.allocationAttempts();impl_=std::move(pending);error.clear();return true;
 } catch(const std::exception& exception){if(pending)attempts_=pending->owner.allocationAttempts();error=exception.what();return false;}
}
}
