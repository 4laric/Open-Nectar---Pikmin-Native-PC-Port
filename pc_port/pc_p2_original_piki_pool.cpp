#include "pc_p2_original_piki_pool.h"
#include "pc_p2_original_piki_init.h"
#include "pc_p2_original_piki_physical_bootstrap.h"
#include "pc_p2_species.h"
#include "Piki.h"
#include "PikiMgr.h"
#include "GameStat.h"
#include <limits>
#include <cmath>
namespace {
using namespace p2original::piki;
struct Owner {
 bool reserved=false,reused=false,registration=false,releasePending=false,physicalReleased=false;
 PoolTicket ticket;PikiMgr* manager=nullptr;
 PoolPhase phase=PoolPhase::Allocated;
 OriginalPikiBody source;std::uint64_t nativeLifetime=0;
};
std::array<Owner,20> owners;
Owner* capture=nullptr;
std::uint64_t allocationClock=0;
bool busy=false,physicalCallback=false;
Owner* find(PoolTicket t)noexcept {
 if(!t.body||!t.allocation)return nullptr;
 for(auto& o:owners)if(o.reserved&&o.ticket.body==t.body&&o.ticket.allocation==t.allocation)return &o;
 return nullptr;
}
bool allocationCurrent(const Owner& o)noexcept {
 return o.ticket.body&&!o.reused&&!o.releasePending&&o.manager==pikiMgr
  &&pc_p2_original_piki_pool_slot_live(o.manager,o.ticket.body);
}
bool current(const Owner& o)noexcept {
 return allocationCurrent(o)&&(!o.nativeLifetime||pc_p2_original_piki_body_current(o.ticket.body,o.nativeLifetime));
}
bool same(const OriginalPikiBody& a,const OriginalPikiBody& b){
 return a.origin.sourceKey==b.origin.sourceKey&&a.origin.recordUid==b.origin.recordUid
  &&a.origin.attempt==b.origin.attempt&&a.origin.activation==b.origin.activation
  &&a.origin.catalogFingerprint==b.origin.catalogFingerprint
  &&a.state.species==b.state.species&&a.state.wild==b.state.wild&&a.state.wasWild==b.state.wasWild;
}
struct PhysicalCallback {PhysicalCallback(){physicalCallback=true;}~PhysicalCallback(){physicalCallback=false;}};
struct QueryOperation {bool admitted=false,owns=false;QueryOperation(){if(!busy){busy=true;admitted=owns=true;}else if(physicalCallback)admitted=true;}~QueryOperation(){if(owns)busy=false;}};
struct Operation {bool admitted=false;Operation(){if(!busy){busy=true;admitted=true;}}~Operation(){if(admitted)busy=false;}};
}
bool pc_p2_original_piki_pool_can_allocate()noexcept {return allocationClock!=std::numeric_limits<std::uint64_t>::max();}
void pc_p2_original_piki_pool_observe_birth(PikiMgr* manager,Piki* p)noexcept {
 if(!p)return;
 // Every actual native allocation (including ordinary births) invalidates any
 // retained old occupant. Ownership is retained so premature reuse is visible.
 for(auto& o:owners)if(o.reserved&&o.ticket.body==p)o.reused=true;
 if(!pc_p2_original_piki_pool_can_allocate())return; // caller preflights
 const auto incarnation=++allocationClock;
 if(capture){
  capture->ticket={p,incarnation};capture->manager=manager;
  capture->phase=PoolPhase::Allocated;capture=nullptr;
 }
}
namespace p2original { namespace piki {
PoolAllocation allocatePool(const OriginalPikiBody& source,std::string& e){
 Operation operation;
 if(!operation.admitted||capture||!pikiMgr||!pikiMgr->mPikiParms
  ||pc_p2_original_piki_init_busy()||!pc_p2_original_piki_body_birth_admit(source)
  ||!pc_p2_original_piki_pool_can_allocate()){
  e="source pool allocation lacks exact source/manager ownership";return {};
 }
 if(GameStat::mapPikis<0){e="invalid native field population";return {};}
 if(GameStat::mapPikis>=100){e.clear();return {PoolAllocationResult::Capacity,{}};}
 Owner* pending=nullptr;
 for(auto& o:owners)if(!o.reserved){pending=&o;break;}
 if(!pending){e="source pool twenty-body owner census is full";return {};}
 // Fallible strings are copied before reservation or any physical mutation.
 OriginalPikiBody copied=source;
 *pending=Owner{};pending->source=std::move(copied);pending->reserved=true;
 capture=pending;
 try {
  auto* born=static_cast<Piki*>(pikiMgr->birthOriginalP2());
  if(!pending->ticket.body){
   capture=nullptr;
   if(born){
    // Broken mandatory hook cannot be repaired with a synthesized token. Keep
    // observed physical ownership visible and block all initialization/release.
    pending->ticket.body=born;pending->manager=pikiMgr;pending->reused=true;
    e="native source pool birth omitted mandatory pre-init observer";
    return {PoolAllocationResult::Retained,pending->ticket};
   }
   *pending=Owner{};
   e.clear();return {PoolAllocationResult::Capacity,{}};
  }
  if(born!=pending->ticket.body||!current(*pending)||born->mGenerator){
   e="retained source allocation failed post-birth native ownership check";
   return {PoolAllocationResult::Retained,pending->ticket};
  }
  e.clear();return {PoolAllocationResult::Retained,pending->ticket};
 }catch(...){
  capture=nullptr;
  if(pending->ticket.body){e="native birth callback threw after captured source allocation";return {PoolAllocationResult::Retained,pending->ticket};}
  *pending=Owner{};e="native birth failed before physical allocation";return {};
 }
}
bool initializePool(PoolTicket ticket,const std::array<float,3>& xyz,std::string& e){
 Operation operation;auto* o=find(ticket);
 if(!operation.admitted||!o||!current(*o)||o->phase!=PoolPhase::Allocated
  ||!pc_p2_original_piki_body_birth_admit(o->source)||pc_p2_original_piki_init_busy()){
  e="source pool initialization lacks current uninitialized allocation";return false;
 }
 for(float v:xyz)if(!std::isfinite(v)){e="nonfinite source pool birth position";return false;}
 auto* p=ticket.body;
 const int color=o->source.state.species<=P2SpeciesYellow?o->source.state.species:Red;
 // Every phase is recorded before writes. Exceptions retain this fixed owner;
 // no catch wrapper calls Creature::kill on an incompletely initialized body.
 try {
  o->registration=true;GameStat::workPikis.inc(color);GameStat::update();
  PcOriginalPikiInitScope scope(p);
  if(!scope.valid()){e="source pool init scope refused; allocation retained";return false;}
  o->phase=PoolPhase::Initializing;p->init(nullptr);
  o->phase=PoolPhase::Initialized;
  if(!current(*o)){e="source pool allocation changed during initializer";return false;}
  p->resetPosition(Vector3f(xyz[0],xyz[1],xyz[2]));
  if(!pc_p2_set_species(p,o->source.state.species)){e="source pool species refused";return false;}
  p->mHappa=Leaf;p->changeMode(PikiMode::FreeMode,nullptr);
  if(!current(*o)||pc_p2_species(p)!=o->source.state.species||!p->isAlive()){
   e="source pool physical initialization mismatch";return false;
  }
  if(!pc_p2_original_piki_body_associate_birth(p,o->source)){
   // Association can commit before a later callback throws/refuses. Preserve
   // the actual committed lifetime if one exists, never manufacture token0.
   OriginalPikiBodyHandle h;if(pc_p2_original_piki_body_handle(p,h))o->nativeLifetime=h.nativeLifetime;
   e="source pool canonical association refused; body retained";return false;
  }
  OriginalPikiBodyHandle h;
  if(!pc_p2_original_piki_body_handle(p,h)||!h.nativeLifetime){e="source pool association has no actual lifetime";return false;}
  o->nativeLifetime=h.nativeLifetime;o->phase=PoolPhase::Associated;
  e.clear();return current(*o);
 }catch(...){
  OriginalPikiBodyHandle h;if(pc_p2_original_piki_body_handle(p,h))o->nativeLifetime=h.nativeLifetime;
  e="source pool initializer threw; exact partial allocation retained";return false;
 }
}
bool poolSource(PoolTicket t,OriginalPikiBody& out,std::string& e){
 QueryOperation operation;auto* o=find(t);
 if(!operation.admitted||!o||o->releasePending||!allocationCurrent(*o)){
  e="source pool immutable query lacks current retained allocation";return false;
 }
 OriginalPikiBody copied=o->source;
 if(find(t)!=o||!allocationCurrent(*o)){
  e="source pool immutable query allocation changed";return false;
 }
 out=std::move(copied);e.clear();return true;
}
bool poolBeginPhysicalInitialization(PoolTicket t,OriginalPikiBody& out,std::string& e){
 Operation operation;auto* o=find(t);OriginalPikiBodyHandle associated;
 if(!operation.admitted||!o||o->releasePending||!current(*o)||
    o->phase!=PoolPhase::Allocated||o->registration||o->nativeLifetime||
    pc_p2_original_piki_body_handle(t.body,associated)){
  e="source physical handoff requires current unassociated allocation";return false;
 }
 OriginalPikiBody copied=o->source;
 if(find(t)!=o||!current(*o)||pc_p2_original_piki_body_handle(t.body,associated)||!current(*o)){
  e="source physical handoff allocation changed before registration";return false;
 }
 // No foreign callback or fallible copy follows this ownership transition.
 // All potential partial registration is retained, even if caller's first
 // native write subsequently fails. Allocation-only release now always refuses.
 o->registration=true;o->phase=PoolPhase::Initializing;
 out=std::move(copied);e.clear();return true;
}
bool poolReleaseAllocation(PoolTicket t,std::string& e){
 Operation operation;auto* o=find(t);
 if(!operation.admitted||!o||o->reused||o->manager!=pikiMgr||
    o->phase!=PoolPhase::Allocated||o->registration||o->nativeLifetime){
  e="source pool release requires exact unregistered allocation";return false;
 }
 OriginalPikiBodyHandle associated;
 if(pc_p2_original_piki_body_handle(t.body,associated)){
  e="source allocation already has actual canonical association";return false;
 }
 if(o->reused||o->manager!=pikiMgr){e="source allocation changed during association inspection";return false;}
 if(o->releasePending){
  if(!pc_p2_original_piki_pool_slot_retired(o->manager,t.body)){
   e="source pool allocation release remains pending native retirement";return false;
  }
 }else{
  if(!current(*o)){e="source pool allocation release is stale";return false;}
  // Ownership of the attempt is retained before native pool mutation. Raw
  // manager kill consults actual reference count and may defer with status -2.
  o->releasePending=true;
  try{o->manager->MonoObjectMgr::kill(t.body);}
  catch(...){e="source native allocator kill threw; ticket retained";return false;}
 }
 if(find(t)!=o||o->reused||o->manager!=pikiMgr||
    !pc_p2_original_piki_pool_slot_retired(o->manager,t.body)){
  e="source pool allocation has not reached exact native retirement";return false;
 }
 *o=Owner{};e.clear();return true;
}
bool poolFinishPhysicalInitialization(PoolTicket t,const NativePhysicalBootstrap& physical,std::string& e){
 Operation operation;auto* o=find(t);
 if(!operation.admitted||!o||!allocationCurrent(*o)||o->phase!=PoolPhase::Initializing
  ||!o->registration||o->physicalReleased||!physical.matchesTicket(t)||!physical.initialized()){
  e="source completion lacks exact initialized physical owner";return false;
 }
 {PhysicalCallback callback;if(!physical.current(e))return false;}
 if(find(t)!=o||!allocationCurrent(*o)||!physical.matchesTicket(t)||!physical.initialized()){
  e="source physical completion allocation changed";return false;
 }
 OriginalPikiBodyHandle associated;
 if(!pc_p2_original_piki_body_handle(t.body,associated)||!associated.nativeLifetime){
  e="source physical completion lacks actual canonical association";return false;
 }
 // Retain any actual association observed after external registration, even
 // when its immutable source mismatch prevents successful completion.
 o->nativeLifetime=associated.nativeLifetime;
 if(!same(o->source,associated.body)||!allocationCurrent(*o)
  ||!pc_p2_original_piki_body_current(t.body,associated.nativeLifetime)
  ||!physical.matchesTicket(t)||!physical.initialized()){
  e="source physical completion canonical source changed";return false;
 }
 {PhysicalCallback callback;if(!physical.current(e))return false;}
 if(find(t)!=o||!allocationCurrent(*o)||!pc_p2_original_piki_body_current(t.body,associated.nativeLifetime)
  ||!physical.matchesTicket(t)||!physical.initialized()){e="source completion authority changed after owner query";return false;}
 o->phase=PoolPhase::Associated;e.clear();return true;
}
bool poolReleasePhysical(PoolTicket t,NativePhysicalBootstrap& physical,std::string& e){
 Operation operation;auto* o=find(t);OriginalPikiBodyHandle associated;
 if(!operation.admitted||!o||o->reused||o->manager!=pikiMgr||!o->registration
  ||o->phase==PoolPhase::Allocated||!physical.matchesTicket(t)){
  e="source physical release lacks exact retained initializer";return false;
 }
 {PhysicalCallback callback;if(!physical.retainedStageCurrent(e))return false;}
 if(pc_p2_original_piki_body_handle(t.body,associated)){
  e="source physical release still has canonical consumers";return false;
 }
 if(find(t)!=o||o->reused||o->manager!=pikiMgr){e="source physical release allocation changed";return false;}
 if(!o->physicalReleased){
  if(!allocationCurrent(*o)){e="source physical release has no live allocation";return false;}
  {PhysicalCallback callback;if(!physical.dispose(e))return false;}
  if(find(t)!=o||!allocationCurrent(*o)||!physical.matchesTicket(t)||physical.initialized()||!physical.physicalResourcesEmpty()
   ||pc_p2_original_piki_body_handle(t.body,associated)||!allocationCurrent(*o)){
   e="source physical disposal changed retained allocation";return false;
  }
  o->physicalReleased=true;
 }
 {PhysicalCallback callback;if(!physical.retainedStageCurrent(e))return false;}
 if(!physical.physicalResourcesEmpty()){e="source physical leases/counter remain owned";return false;}
 if(!o->releasePending){
  if(!allocationCurrent(*o)){e="source physical allocator retirement is stale";return false;}
  o->releasePending=true;
  try{o->manager->MonoObjectMgr::kill(t.body);}
  catch(...){e="source physical allocator kill threw; owner retained";return false;}
 }
 {PhysicalCallback callback;if(!physical.retainedStageCurrent(e))return false;}
 if(find(t)!=o||o->reused||o->manager!=pikiMgr||!physical.matchesTicket(t)||physical.initialized()||!physical.physicalResourcesEmpty()
  ||pc_p2_original_piki_body_handle(t.body,associated)
  ||!pc_p2_original_piki_pool_slot_retired(o->manager,t.body)){
  e="source physical retirement remains pending exact native slot release";return false;
 }
 {PhysicalCallback callback;if(!physical.retainedStageCurrent(e))return false;}
 if(find(t)!=o||o->reused||o->manager!=pikiMgr||!physical.matchesTicket(t)||!physical.physicalResourcesEmpty())
  {e="source physical retirement owner changed during final inspection";return false;}
 // No callback follows exact native retirement. The concrete initializer keeps
 // its ticket until this checked pool census removal, then its friend resets it.
 *o=Owner{};physical.poolRetired();e.clear();return true;
}
bool poolCurrent(PoolTicket t)noexcept {auto* o=find(t);return o&&allocationCurrent(*o);}
bool poolAssociatedCurrent(PoolTicket t)noexcept {auto* o=find(t);return o&&o->nativeLifetime&&current(*o);}
std::size_t poolOwners(std::array<PoolOwner,20>& out)noexcept {
 std::size_t count=0;
 for(const auto& o:owners)if(o.reserved)out[count++]={o.ticket,o.phase,o.nativeLifetime,o.registration,current(o),allocationCurrent(o),o.nativeLifetime&&current(o)};
 return count;
}
bool poolOwned()noexcept {if(busy||capture)return true;for(const auto& o:owners)if(o.reserved)return true;return false;}
} }
