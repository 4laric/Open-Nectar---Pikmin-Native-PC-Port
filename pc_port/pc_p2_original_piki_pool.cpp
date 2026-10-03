#include "pc_p2_original_piki_pool.h"
#include "pc_p2_original_piki_init.h"
#include "pc_p2_species.h"
#include "Piki.h"
#include "PikiMgr.h"
#include "GameStat.h"
#include <limits>
#include <cmath>
namespace {
using namespace p2original::piki;
struct Owner {
 bool reserved=false,reused=false,registration=false;
 PoolTicket ticket;PikiMgr* manager=nullptr;
 PoolPhase phase=PoolPhase::Allocated;
 OriginalPikiBody source;std::uint64_t nativeLifetime=0;
};
std::array<Owner,20> owners;
Owner* capture=nullptr;
std::uint64_t allocationClock=0;
bool busy=false;
Owner* find(PoolTicket t)noexcept {
 if(!t.body||!t.allocation)return nullptr;
 for(auto& o:owners)if(o.reserved&&o.ticket.body==t.body&&o.ticket.allocation==t.allocation)return &o;
 return nullptr;
}
bool current(const Owner& o)noexcept {
 return o.ticket.body&&!o.reused&&o.manager==pikiMgr
  &&pc_p2_original_piki_pool_slot_live(o.manager,o.ticket.body)
  &&(!o.nativeLifetime||pc_p2_original_piki_body_current(o.ticket.body,o.nativeLifetime));
}
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
bool poolCurrent(PoolTicket t)noexcept {auto* o=find(t);return o&&current(*o);}
std::size_t poolOwners(std::array<PoolOwner,20>& out)noexcept {
 std::size_t count=0;
 for(const auto& o:owners)if(o.reserved)out[count++]={o.ticket,o.phase,o.nativeLifetime,o.registration,current(o)};
 return count;
}
bool poolOwned()noexcept {if(busy||capture)return true;for(const auto& o:owners)if(o.reserved)return true;return false;}
} }
