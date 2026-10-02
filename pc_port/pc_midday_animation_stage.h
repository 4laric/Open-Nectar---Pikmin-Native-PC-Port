#pragma once
#include "pc_midday_animation_context.h"
#include "pc_midday_constructor.h"
#include "pc_midday_resolved_fields.h"
#include <memory>
namespace pc_midday {
struct CanonicalContextRecord {u64 id=0;ActorBytes payload;};
using ContextResolverFactory=std::function<LogicalResolver*(u64)>;
struct PlannedContext {u64 id=0;AnimationContextRecord state;ResolvedFields pointers;};
// Required IDs come from the actual canonical source/factory inventory, not an
// input boolean. Exact set equality prevents dropping or introducing contexts.
bool planCanonicalContexts(const std::vector<CanonicalContextRecord>&,const std::set<u64>& required,
 const ContextResolverFactory&,const AnimationContentCheck&,std::vector<PlannedContext>&,std::string&);
class IsolatedAnimationContexts {
 struct Impl;std::unique_ptr<Impl>impl_;
public:
 IsolatedAnimationContexts();~IsolatedAnimationContexts();
 bool prepare(const std::vector<CanonicalContextRecord>&,const std::set<u64>& required,
 const ContextResolverFactory&,const AnimationContentCheck&,const RestoreGate&,ConstructorFence&,std::string&);
 AnimContext* context(u64)const;
 bool heldBy(const ConstructorFence&)const;
 // One allocation per ID, shared aliases are installed through the canonical
 // catalog. Never animate/updateContext/startAnim. Retain owner until consumers
 // are disposed; native cleanup requires this same physical fence held again.
};
}
