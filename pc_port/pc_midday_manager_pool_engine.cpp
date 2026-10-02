#include "pc_midday_manager_pool.h"
#include "ObjectMgr.h"
#include "Creature.h"
struct PcMiddayManagerAccess {
    static bool view(const MonoObjectMgr& m,pc_midday::MonoPoolView& out,std::string& e){
        using namespace pc_midday;
        if(m.mMaxElements<0||m.mMaxElements>int(MaxActors)||m.mNumObjects<0||m.mNumObjects>m.mMaxElements||
           (m.mMaxElements&&(!m.mObjectList||!m.mEntryStatus))){e="uninitialized or invalid native mono pool";return false;}
        MonoPoolView v;v.capacity=uint32_t(m.mMaxElements);v.count=uint32_t(m.mNumObjects);
        for(int i=0;i<m.mMaxElements;++i){v.statuses.push_back(m.mEntryStatus[i]);v.objects.push_back(m.mObjectList[i]);}
        out=std::move(v);return true;
    }
    static bool stage(MonoObjectMgr& m,const pc_midday::MonoPoolPlan& p,const pc_midday::RestoreGate& g,
                      std::map<uint64_t,Creature*>& out,std::string& e){
        using namespace pc_midday;
        if(!g.freshProcess||!g.paused||!g.zeroInput||!g.birthEffectsSuppressed||!g.rewardsSuppressed||!g.rngDrawsSuppressed||!g.audioVoicesSuppressed){e="pool staging requires full fresh constructor fence";return false;}
        if(!out.empty()){e="pool root output is not fresh staging";return false;}
        MonoPoolView v;if(!view(m,v,e))return false;
        if(v.capacity!=p.capacity||v.count){e="native destination pool is not fresh or capacity differs";return false;}
        std::set<uint64_t> ids;for(const auto& s:p.slots)if(s.actor)ids.insert(s.actor);
        if(!validateMonoPool(p,ids,e))return false;
        std::set<const void*> unique;std::map<uint64_t,Creature*> roots;
        for(size_t i=0;i<v.capacity;++i){
            if(v.statuses[i]!=-1||!v.objects[i]||!unique.insert(v.objects[i]).second){e="destination pool slots not fresh/distinct";return false;}
            if(p.slots[i].actor)roots.emplace(p.slots[i].actor,m.mObjectList[i]);
        }
        // All allocations/map validation completed before these simple writes.
        // This is an unpublished fresh scene; no manager callbacks or RNG draws.
        for(size_t i=0;i<v.capacity;++i)m.mEntryStatus[i]=p.slots[i].life==SlotLife::Free?-1:p.slots[i].life==SlotLife::Retained?-2:0;
        m.mNumObjects=int(p.count);out.swap(roots);e.clear();return true;
    }
};
namespace pc_midday {
bool readMonoPoolView(const MonoObjectMgr& m,MonoPoolView& out,std::string& e){
 MonoPoolView before,after;if(!PcMiddayManagerAccess::view(m,before,e)||!PcMiddayManagerAccess::view(m,after,e))return false;
 if(before.capacity!=after.capacity||before.count!=after.count||before.statuses!=after.statuses||before.objects!=after.objects){e="native pool changed during root inventory";return false;}
 out=std::move(before);e.clear();return true;
}
bool captureMonoPool(const MonoObjectMgr& m,const BirthLedger& ledger,MonoPoolPlan& out,std::string& e){
    MonoPoolView before,after;if(!PcMiddayManagerAccess::view(m,before,e))return false;
    MonoPoolPlan p;if(!planMonoPool(before,ledger,p,e)||!PcMiddayManagerAccess::view(m,after,e))return false;
    if(before.capacity!=after.capacity||before.count!=after.count||before.statuses!=after.statuses||before.objects!=after.objects){e="native pool changed during read-only capture";return false;}
    out=std::move(p);e.clear();return true;
}
bool stageMonoPool(MonoObjectMgr& m,const MonoPoolPlan& p,const RestoreGate& g,std::map<uint64_t,Creature*>& out,std::string& e){return PcMiddayManagerAccess::stage(m,p,g,out,e);}
}
