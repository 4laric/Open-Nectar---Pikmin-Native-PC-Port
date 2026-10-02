#include "pc_midday_refcounts.h"
#include "RefCountable.h"
namespace pc_midday {
bool observeReferenceCounts(const std::map<uint64_t,const RefCountable*>& actors,
    const std::set<uint64_t>& expected,const ReferenceReadFence& fence,
    std::map<uint64_t,int32_t>& out,std::string& e){
    if(!fence.agreedReadOnlyFence||fence.tickBefore!=fence.tickAfter){e="count observation requires stopped read-only tick fence";return false;}
    if(actors.empty()||actors.size()>MaxActors||actors.size()!=expected.size()){e="incomplete native count actor inventory";return false;}
    std::set<const RefCountable*> unique;std::map<uint64_t,int32_t> counts;
    for(const auto& entry:actors){
        if(!entry.first||!expected.count(entry.first)||!entry.second||!unique.insert(entry.second).second){e="untracked/null/aliased native count actor";return false;}
        const int count=entry.second->mCount;
        if(count<0||uint32_t(count)>MaxBytes/sizeof(uint64_t)/4){e="native reference count outside census bounds";return false;}
        counts.emplace(entry.first,count);
    }
    out=std::move(counts);e.clear();return true;
}
bool applyReferenceCounts(const std::map<uint64_t,RefCountable*>& actors,
    const std::map<uint64_t,int32_t>& counts,const ReferenceCountFence& fence,std::string& e){
    if(!fence.freshPausedStage||!fence.allActorAndGlobalReferencesBound){e="reference reconciliation requires complete paused staging fence";return false;}
    if(actors.size()>MaxActors||actors.size()!=counts.size()){e="incomplete staged reference-count inventory";return false;}
    std::set<RefCountable*> objects;
    for(const auto& entry:actors){
        auto count=counts.find(entry.first);
        if(!entry.first||!entry.second||count==counts.end()||count->second<0||uint32_t(count->second)>MaxBytes/sizeof(uint64_t)/4||!objects.insert(entry.second).second){e="invalid/aliased staged reference-count target";return false;}
    }
    // No allocation or callbacks after validation; failure above writes nothing.
    for(const auto& entry:actors)entry.second->mCount=counts.at(entry.first);
    e.clear();return true;
}
}
