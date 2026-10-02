#include "pc_midday_refcounts.h"
#include "RefCountable.h"
namespace pc_midday {
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
