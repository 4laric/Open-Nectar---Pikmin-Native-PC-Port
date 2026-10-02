#pragma once
#include "pc_midday_codec.h"
#include <map>
#include <set>
struct RefCountable;
namespace pc_midday {
enum class ReferenceOwner { Actor, Global };
struct StrongReference {
    ReferenceOwner ownerKind=ReferenceOwner::Actor;
    uint64_t owner=0,field=0,target=0; // field is a stable typed-schema topology ID
};
// Original observed counts are a coverage check, never reused as restored counts.
bool reconcileReferenceCounts(const std::map<uint64_t,int32_t>& observed,
    const std::set<uint32_t>& globalFamilies,const std::vector<StrongReference>&,
    std::map<uint64_t,int32_t>& reconciled,std::string&);
struct ReferenceCountFence {
    bool freshPausedStage=false,allActorAndGlobalReferencesBound=false;
};
struct ReferenceReadFence {
    bool agreedReadOnlyFence=false;
    uint64_t tickBefore=0,tickAfter=0;
};
// Actual named native count observation at a stopped owner-thread tick. Every
// expected actor ID (including retained-2 pool roots) must occur exactly once.
// Original counts prove strong-root coverage; restore reconstructs them instead.
bool observeReferenceCounts(const std::map<uint64_t,const RefCountable*>&,
    const std::set<uint64_t>& expectedActors,const ReferenceReadFence&,
    std::map<uint64_t,int32_t>&,std::string&);
// Real engine bridge: validates the entire staged inventory before direct writes;
// it never invokes SmartPtr setters or RefCountable callbacks.
bool applyReferenceCounts(const std::map<uint64_t,RefCountable*>&,
    const std::map<uint64_t,int32_t>&,const ReferenceCountFence&,std::string&);
}
