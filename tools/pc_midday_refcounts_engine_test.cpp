#include "pc_midday_refcounts.h"
#include "RefCountable.h"
#include <iostream>
struct Counter : RefCountable {
    int callbacks=0;
    void addCntCallback()override{++callbacks;}
    void subCntCallback()override{++callbacks;}
};
int main(){
    Counter a,b;a.mCount=99;b.mCount=88;std::string e;
    std::map<uint64_t,RefCountable*> actors{{1,&a},{2,&b}};std::map<uint64_t,int32_t> counts{{1,2},{2,1}};
    if(pc_midday::applyReferenceCounts(actors,counts,{false,true},e)||a.mCount!=99||b.mCount!=88)return 1;
    if(pc_midday::applyReferenceCounts(actors,counts,{true,false},e)||a.mCount!=99||b.mCount!=88)return 2;
    auto bad=actors;bad[2]=&a;if(pc_midday::applyReferenceCounts(bad,counts,{true,true},e)||a.mCount!=99||b.mCount!=88)return 3;
    bad=actors;bad[2]=nullptr;if(pc_midday::applyReferenceCounts(bad,counts,{true,true},e)||a.mCount!=99||b.mCount!=88)return 4;
    auto invalid=counts;invalid[2]=-1;if(pc_midday::applyReferenceCounts(actors,invalid,{true,true},e)||a.mCount!=99||b.mCount!=88)return 5;
    invalid=counts;invalid.erase(2);if(pc_midday::applyReferenceCounts(actors,invalid,{true,true},e)||a.mCount!=99||b.mCount!=88)return 6;
    if(!pc_midday::applyReferenceCounts(actors,counts,{true,true},e)||a.mCount!=2||b.mCount!=1||a.callbacks||b.callbacks)return 7;
    std::map<uint64_t,const RefCountable*> live{{1,&a},{2,&b}};
    std::map<uint64_t,int32_t> observed{{99,99}};
    if(!pc_midday::observeReferenceCounts(live,{1,2},{true,30,30},observed,e)||observed!=counts||a.callbacks||b.callbacks)return 8;
    for(int fault=0;fault<8;++fault){auto bad=live;std::set<uint64_t> expected{1,2};pc_midday::ReferenceReadFence fence{true,30,30};
        switch(fault){
        case 0:fence.agreedReadOnlyFence=false;break;
        case 1:fence.tickAfter=31;break;
        case 2:expected.insert(3);break;
        case 3:expected={1,3};break;
        case 4:bad[2]=&a;break;
        case 5:bad[2]=nullptr;break;
        case 6:b.mCount=-1;break;
        case 7:b.mCount=10000000;break;
        }
        observed={{99,99}};if(pc_midday::observeReferenceCounts(bad,expected,fence,observed,e)||observed!=std::map<uint64_t,int32_t>{{99,99}}||a.callbacks||b.callbacks)return 9+fault;
        b.mCount=1;
    }
    observed={{99,99}};if(pc_midday::observeReferenceCounts({}, {}, {true,30,30},observed,e)||observed!=std::map<uint64_t,int32_t>{{99,99}})return 17;
    std::cout<<"PASS 17 actual RefCountable constructor/header capture and staged reconciliation controls; no callbacks and no partial invalid writes\n";return 0;
}
