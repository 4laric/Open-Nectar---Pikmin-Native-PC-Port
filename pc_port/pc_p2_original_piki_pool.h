#pragma once
#include "pc_p2_original_piki_origin.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
class PikiMgr;
namespace p2original { namespace piki {
// Actual native pool allocation incarnation, distinct from the later committed
// GenPiki association lifetime. Neither value is derived from a pointer/UID.
struct PoolTicket {Piki* body=nullptr;std::uint64_t allocation=0;};
enum class PoolPhase { Allocated, Initializing, Initialized, Associated };
enum class PoolAllocationResult { Refused, Capacity, Retained };
struct PoolAllocation {PoolAllocationResult result=PoolAllocationResult::Refused;PoolTicket ticket;};
struct PoolOwner {PoolTicket ticket;PoolPhase phase=PoolPhase::Allocated;std::uint64_t nativeLifetime=0;bool registration=false,current=false;};
// Fixed twenty-body source bootstrap census is reserved BEFORE pool mutation.
// Retained means actual allocation occurred, even when a subsequent legacy
// callback threw. Caller must retain the ticket and inspect error before init.
PoolAllocation allocatePool(const OriginalPikiBody&,std::string&);
bool initializePool(PoolTicket,const std::array<float,3>&,std::string&);
bool poolCurrent(PoolTicket)noexcept;
// Read-only census includes failed/pending bodies. A stale allocation remains
// owned and blocks teardown; it is never silently erased after pointer reuse.
std::size_t poolOwners(std::array<PoolOwner,20>&)noexcept;
bool poolOwned()noexcept;
} }
// Narrow native manager hooks. Overflow preflight is before actual allocation;
// observe executes immediately after MonoObjectMgr::birth, before callouts.
bool pc_p2_original_piki_pool_can_allocate()noexcept;
void pc_p2_original_piki_pool_observe_birth(PikiMgr*,Piki*)noexcept;
// Native manager implementation inspects its actual object array/entry status.
bool pc_p2_original_piki_pool_slot_live(PikiMgr*,Piki*)noexcept;
