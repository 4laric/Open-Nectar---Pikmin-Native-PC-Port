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
    std::cout<<"PASS 7 actual RefCountable constructor/header staged reconciliation controls; no callbacks and no partial invalid writes\n";return 0;
}
