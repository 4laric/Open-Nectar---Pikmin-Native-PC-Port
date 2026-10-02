#include "pc_midday_refcounts.h"
#include <iostream>
#include <stdexcept>
using namespace pc_midday;
int checks=0;
void check(bool yes,const char* why){++checks;if(!yes)throw std::runtime_error(why);}
int main(){try{
    std::map<uint64_t,int32_t> observed{{1,2},{2,1},{3,0}},out;
    std::vector<StrongReference> refs={{ReferenceOwner::Actor,1,100,2},{ReferenceOwner::Actor,2,100,1},{ReferenceOwner::Global,10,200,1},{ReferenceOwner::Actor,3,300,0}};
    std::string e;check(reconcileReferenceCounts(observed,{10},refs,out,e)&&out==observed,"actor and global roots reconcile including zero-count actor");
    for(int fault=0;fault<9;++fault){auto bad=refs;auto counts=observed;std::set<uint32_t> globals{10};
        switch(fault){
        case 0:bad.pop_back();bad.pop_back();break; // missing strong global root
        case 1:bad.push_back(bad.front());break;
        case 2:bad[0].target=999;break;
        case 3:bad[0].owner=999;break;
        case 4:bad[3].ownerKind=ReferenceOwner(999);break;
        case 5:bad[0].field=0;break;
        case 6:globals.clear();break;
        case 7:counts[1]=-1;break;
        case 8:counts[0]=0;break;
        }
        out={{777,777}};check(!reconcileReferenceCounts(counts,globals,bad,out,e)&&out==std::map<uint64_t,int32_t>{{777,777}},"invalid/incomplete strong graph refuses without output mutation");
    }
    refs.push_back({ReferenceOwner::Global,10,201,0});check(reconcileReferenceCounts(observed,{10},refs,out,e),"known nullable global field does not invent reference");
    check(!reconcileReferenceCounts(observed,{0,10},refs,out,e),"unknown zero global family refuses");
    std::cout<<"PASS "<<checks<<" strong-reference graph controls; no full world restored\n";return 0;
}catch(const std::exception& x){std::cerr<<"FAIL "<<checks<<": "<<x.what()<<"\n";return 1;}}
