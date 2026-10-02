#include "pc_midday_manager_pool.h"
#include <algorithm>
namespace pc_midday {
namespace {
bool valid(const MonoPoolPlan& p,std::set<uint64_t>& ids,std::string& e){
    if(p.capacity>MaxActors||p.slots.size()!=p.capacity||p.count>p.capacity){e="invalid mono pool capacity/count";return false;}
    uint32_t count=0;
    for(const auto& s:p.slots){
        switch(s.life){
        case SlotLife::Free:if(s.actor){e="free slot has live identity";return false;}break;
        case SlotLife::Active:case SlotLife::Retained:
            if(!s.actor||!ids.insert(s.actor).second){e="duplicate/absent pool actor identity";return false;}++count;break;
        default:e="unknown native slot lifecycle";return false;
        }
    }
    if(count!=p.count){e="native pool retained/active count mismatch";return false;}return true;
}
void put(Bytes& b,uint64_t n,unsigned width){for(unsigned i=0;i<width;++i)b.push_back(uint8_t(n>>(8*i)));}
uint64_t get(const Bytes& b,size_t at,unsigned width){uint64_t n=0;for(unsigned i=0;i<width;++i)n|=uint64_t(b[at+i])<<(8*i);return n;}
}
bool planMonoPool(const MonoPoolView& v,const BirthLedger& ledger,MonoPoolPlan& out,std::string& e){
    if(v.capacity>MaxActors||v.count>v.capacity||v.statuses.size()!=v.capacity||v.objects.size()!=v.capacity){e="incomplete native mono pool view";return false;}
    MonoPoolPlan p;p.capacity=v.capacity;p.count=v.count;std::set<const void*> unique;
    for(size_t i=0;i<v.capacity;++i){PoolSlot s;
        if(!v.objects[i]||!unique.insert(v.objects[i]).second){e="native pool backing object missing/aliased";return false;}
        if(v.statuses[i]==-1){if(ledger.lookup(v.objects[i])){e="free pool slot retains a live incarnation";return false;}s.life=SlotLife::Free;}
        else if(v.statuses[i]==0||v.statuses[i]==-2){
            s.life=v.statuses[i]==0?SlotLife::Active:SlotLife::Retained;
            auto life=ledger.lookup(v.objects[i]);
            if(!life){e="untracked/duplicate active or retained native root";return false;}s.actor=life->id;
        }else{e="unknown native mono pool status";return false;}
        p.slots.push_back(s);
    }
    std::set<uint64_t> ids;if(!valid(p,ids,e))return false;out=std::move(p);e.clear();return true;
}
bool validateMonoPool(const MonoPoolPlan& p,const std::set<uint64_t>& expected,std::string& e){std::set<uint64_t> actual;if(!valid(p,actual,e))return false;if(actual!=expected){e="saved pool and typed actor inventory disagree";return false;}e.clear();return true;}
bool encodeMonoPool(const MonoPoolPlan& p,Bytes& out,std::string& e){
    std::set<uint64_t> ids;if(!valid(p,ids,e))return false;
    Bytes b={'P','C','M','O','N','O','0','1'};put(b,1,4);put(b,p.capacity,4);put(b,p.count,4);
    for(const auto& s:p.slots){put(b,uint8_t(s.life),1);put(b,s.actor,8);}out=std::move(b);e.clear();return true;
}
bool decodeMonoPool(const Bytes& b,MonoPoolPlan& out,std::string& e){
    const Bytes magic={'P','C','M','O','N','O','0','1'};
    if(b.size()<20||!std::equal(magic.begin(),magic.end(),b.begin())||get(b,8,4)!=1){e="mono pool framing/version mismatch";return false;}
    MonoPoolPlan p;p.capacity=uint32_t(get(b,12,4));p.count=uint32_t(get(b,16,4));
    if(p.capacity>MaxActors||b.size()!=20+size_t(p.capacity)*9){e="mono pool payload outside bounds";return false;}
    for(size_t i=0;i<p.capacity;++i)p.slots.push_back({SlotLife(b[20+i*9]),get(b,21+i*9,8)});
    std::set<uint64_t> ids;if(!valid(p,ids,e))return false;out=std::move(p);e.clear();return true;
}
}
