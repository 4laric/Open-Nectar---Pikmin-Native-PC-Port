#if defined(PIKI_PC_PORT)
#include "pc_midday_actor_shell.h"
#include "pc_midday_constructor.h"
#include "pc_midday_view_piki.h"
#include "pc_midday_allocation_owner.h"
#include "NaviMgr.h"
#include "PikiMgr.h"
#include <new>
#include <typeinfo>

struct PcMiddayActorShellAccess {
 static Navi* navi(CreatureProp* props,int slot){return new Navi(Navi::MiddayRestoreTag{},props,slot);}
 static ViewPiki* piki(CreatureProp* props){return new ViewPiki(ViewPiki::MiddayRestoreTag{},props);}
 static Navi* navi(pc_midday::AllocationOwner& owner,CreatureProp* props,int slot){return owner.construct<Navi>([&](void* at){new(at) Navi(Navi::MiddayRestoreTag{},props,slot);});}
 static ViewPiki* piki(pc_midday::AllocationOwner& owner,CreatureProp* props){return owner.construct<ViewPiki>([&](void* at){new(at) ViewPiki(ViewPiki::MiddayRestoreTag{},props);});}
};
namespace pc_midday {
namespace {
bool owner(ConstructorFence& fence,std::string& e){
 if(!fence.held()){e="actor shell requires actual constructor fence";return false;}
 // This existing owner-thread check cannot be substituted with a caller flag.
 return pc_sim_rng_constructor_suppression(true,e);
}
template<class T> T* properties(const ActorFields& fields,LogicalResolver& resolver,std::string& e){
 auto found=fields.find("creature.mProps");void* address=nullptr;
 if(found==fields.end()||!resolver.resolve("creature.mProps",RefKind::CreatureProp,found->second.target,address,e)||!address){
  if(e.empty())e="canonical actor properties unavailable";return nullptr;
 }
 // Resolver returns the adjusted CreatureProp interface for this compiled key.
 auto* base=static_cast<CreatureProp*>(address);
 if(typeid(*base)!=typeid(T)){e="actor properties concrete type mismatch";return nullptr;}
 return static_cast<T*>(base);
}
}
bool create_navi_shell(const ActorBytes& bytes,LogicalResolver& resolver,ConstructorFence& fence,
 std::unique_ptr<Navi>& output,std::string& e){
 if(output){e="Navi shell output already occupied";return false;}
 if(!owner(fence,e)||!validate_navi(bytes,resolver,e))return false;
 ActorFields fields;if(!decode_actor_fields(bytes,fields,e))return false;
 int slot=-1;if(!actor_i32(fields,"navi.runtime.mNaviID",slot,e)||slot<0||slot>1){e="Navi shell slot invalid";return false;}
 auto* props=properties<NaviProp>(fields,resolver,e);if(!props)return false;
 try{std::unique_ptr<Navi> staged(PcMiddayActorShellAccess::navi(props,slot));output=std::move(staged);}
 catch(const std::bad_alloc&){e="Navi shell allocation failed";return false;}
 e.clear();return true;
}
bool create_view_piki_shell(const ActorBytes& bytes,const ActorBytes& subtype,LogicalResolver& resolver,
 ConstructorFence& fence,std::unique_ptr<ViewPiki>& output,std::string& e){
 if(output){e="ViewPiki shell output already occupied";return false;}
 if(!owner(fence,e)||!validate_piki(bytes,resolver,e)||!validate_view_piki(subtype,resolver,e))return false;
 ActorFields fields;if(!decode_actor_fields(bytes,fields,e))return false;
 auto* props=properties<PikiProp>(fields,resolver,e);if(!props)return false;
 try{std::unique_ptr<ViewPiki> staged(PcMiddayActorShellAccess::piki(props));output=std::move(staged);}
 catch(const std::bad_alloc&){e="ViewPiki shell allocation failed";return false;}
 e.clear();return true;
}
bool allocate_owned_navi_shell(const ActorBytes& bytes,LogicalResolver& resolver,ConstructorFence& fence,
 AllocationOwner& allocation,Navi*& output,std::string& e){
 if(output){e="owned Navi shell output occupied";return false;}
 if(!owner(fence,e)||!validate_navi(bytes,resolver,e))return false;
 ActorFields fields;if(!decode_actor_fields(bytes,fields,e))return false;
 int slot=-1;if(!actor_i32(fields,"navi.runtime.mNaviID",slot,e)||slot<0||slot>1){e="owned Navi slot invalid";return false;}
 auto* props=properties<NaviProp>(fields,resolver,e);if(!props)return false;
 output=PcMiddayActorShellAccess::navi(allocation,props,slot);return true;
}
bool allocate_owned_view_piki_shell(const ActorBytes& bytes,const ActorBytes& subtype,LogicalResolver& resolver,
 ConstructorFence& fence,AllocationOwner& allocation,ViewPiki*& output,std::string& e){
 if(output){e="owned ViewPiki shell output occupied";return false;}
 if(!owner(fence,e)||!validate_piki(bytes,resolver,e)||!validate_view_piki(subtype,resolver,e))return false;
 ActorFields fields;if(!decode_actor_fields(bytes,fields,e))return false;
 auto* props=properties<PikiProp>(fields,resolver,e);if(!props)return false;
 output=PcMiddayActorShellAccess::piki(allocation,props);return true;
}
}
#endif
