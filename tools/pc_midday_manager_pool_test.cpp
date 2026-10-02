#include "pc_midday_manager_pool.h"
#include <iostream>
#include <stdexcept>
using namespace pc_midday;
int n=0;void check(bool yes,const char* why){++n;if(!yes)throw std::runtime_error(why);}
int main(){try{
    int a=0,b=0,c=0;BirthLedger ledger;uint64_t live=0,retained=0;std::string e;
    check(ledger.birth(&a,Family::Pikmin,live,e)&&ledger.birth(&c,Family::Pikmin,retained,e),"manager live and retained root identities");
    MonoPoolView v{3,2,{0,-1,-2},{&a,&b,&c}};MonoPoolPlan plan;
    check(planMonoPool(v,ledger,plan,e)&&plan.slots[0].actor==live&&plan.slots[1].life==SlotLife::Free&&plan.slots[2].life==SlotLife::Retained&&plan.slots[2].actor==retained,"retained minus2 included with original slot order");
    Bytes wire;check(encodeMonoPool(plan,wire,e),"typed pool plan encoding");MonoPoolPlan decoded;
    check(decodeMonoPool(wire,decoded,e)&&validateMonoPool(decoded,{live,retained},e),"exact pool and actor inventory closure");
    check(!validateMonoPool(decoded,{live},e),"missing retained actor refuses instead of dropping it");
    for(unsigned f=0;f<8;++f){auto bad=v;
        if(f==0)bad.count=1;
        if(f==1)bad.statuses[2]=-3;
        if(f==2)bad.objects[2]=nullptr;
        if(f==3)bad.objects[2]=&a;
        if(f==4)bad.statuses[0]=-1;
        if(f==5)bad.capacity=MaxActors+1;
        if(f==6)bad.objects.pop_back();
        if(f==7)bad.objects[1]=nullptr;
        MonoPoolPlan unchanged;unchanged.capacity=99;check(!planMonoPool(bad,ledger,unchanged,e)&&unchanged.capacity==99,"malformed native inventory does not mutate output");
    }
    for(unsigned f=0;f<6;++f){auto bad=wire;
        if(f==0)bad.pop_back();
        if(f==1)bad[8]=2;
        if(f==2)bad[20]=3;
        if(f==3)bad[16]=1;
        if(f==4)bad[30]=uint8_t(live);
        if(f==5)bad[39]=uint8_t(live);
        MonoPoolPlan unchanged;unchanged.capacity=99;check(!decodeMonoPool(bad,unchanged,e)&&unchanged.capacity==99,"malformed pool wire refuses before stage allocation");
    }
    std::cout<<"PASS "<<n<<" manager pool plan controls; native engine bridge syntax separately, no world restored\n";return 0;
}catch(const std::exception& x){std::cerr<<"FAIL "<<n<<": "<<x.what()<<"\n";return 1;}}
