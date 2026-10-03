#include "pc_p2_retail_treasure_resource_bank.h"
#include <cassert>
#include <iostream>
struct Owner{std::string hash;unsigned parseAttempts=0;bool complete=false;};
int main(){
    int system=0,otherSystem=0;std::string error;bool fresh=false;
    p2retailcargo::RetainedResourceBank<Owner> bank;
    assert(!bank.acquire("map01","hash",fresh,error));
    assert(!bank.pin(&system,"bank",202,error));assert(bank.pin(&system,"bank",2,error));
    auto* partial=bank.acquire("map01","hash",fresh,error);assert(partial&&fresh);++partial->parseAttempts;
    // An interrupted load retains its owner. Repeated attempts cannot allocate
    // another graph; actual loader refuses the incomplete retained entry.
    for(unsigned visit=0;visit<100;++visit){
        auto* same=bank.acquire("map01","hash",fresh,error);
        assert(same==partial&&!fresh&&!same->complete&&same->parseAttempts==1);
    }
    assert(!bank.acquire("map01","changed",fresh,error));
    auto* complete=bank.acquire("map02","hash2",fresh,error);assert(complete&&fresh);complete->complete=true;
    for(unsigned visit=0;visit<100;++visit)assert(bank.acquire("map02","hash2",fresh,error)==complete&&!fresh);
    assert(!bank.acquire("third","hash3",fresh,error)&&bank.entries().size()==2);
    assert(!bank.pin(&otherSystem,"bank",2,error));assert(!bank.pin(&system,"foreign",2,error));
    assert(!bank.pin(&system,"bank",1,error));assert(bank.pin(&system,"bank",2,error));
    std::cout<<"PASS bounded partial ownership, repeated reuse, hash/System/bank refusal; no native model or gameplay authority\n";
}
