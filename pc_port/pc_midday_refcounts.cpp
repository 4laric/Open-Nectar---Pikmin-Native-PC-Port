#include "pc_midday_refcounts.h"
#include <tuple>
namespace pc_midday {
bool reconcileReferenceCounts(const std::map<uint64_t,int32_t>& observed,
    const std::set<uint32_t>& globals,const std::vector<StrongReference>& references,
    std::map<uint64_t,int32_t>& out,std::string& e){
    constexpr size_t limit=MaxBytes/sizeof(uint64_t)/4;
    if(observed.size()>MaxActors||references.size()>limit||globals.size()>1024||globals.count(0)){e="strong reference census exceeds bound or has invalid globals";return false;}
    std::map<uint64_t,int32_t> counts;
    for(const auto& actor:observed){
        if(!actor.first||actor.second<0||uint32_t(actor.second)>limit){e="invalid observed actor reference count";return false;}
        counts.emplace(actor.first,0);
    }
    std::set<std::tuple<ReferenceOwner,uint64_t,uint64_t>> fields;
    for(const auto& reference:references){
        switch(reference.ownerKind){
        case ReferenceOwner::Actor:if(!counts.count(reference.owner)){e="untracked strong-reference owner";return false;}break;
        case ReferenceOwner::Global:if(reference.owner>UINT32_MAX||!globals.count(uint32_t(reference.owner))){e="unknown global strong-reference owner";return false;}break;
        default:e="unknown strong-reference owner kind";return false;
        }
        if(!reference.field||!fields.emplace(reference.ownerKind,reference.owner,reference.field).second){e="missing or duplicate typed strong-reference field";return false;}
        if(!reference.target)continue;
        auto target=counts.find(reference.target);if(target==counts.end()){e="dangling strong-reference actor ID";return false;}
        ++target->second;
    }
    if(counts!=observed){e="strong-reference census disagrees with native counts; missing root/field coverage";return false;}
    out=std::move(counts);e.clear();return true;
}
}
