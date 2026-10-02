#include "pc_midday_capture.h"
#include <iostream>
#include <stdexcept>
using namespace pc_midday;
int n=0;void check(bool ok,const char* why){++n;if(!ok)throw std::runtime_error(why);}
int main(){try{
    int old1=0,old2=0,old3=0,new1=0,new3=0;uint64_t first=0,retired=0,third=0;std::string e;BirthLedger original;
    check(original.birth(&old1,Family::Captain,first,e)&&original.birth(&old2,Family::Pikmin,retired,e)&&original.retire(&old2,e)&&original.birth(&old3,Family::Pikmin,third,e),"actual birth/retire ledger setup");
    Snapshot saved;saved.actors={{first,{uint32_t(Family::Captain),1},{1},{}},{third,{uint32_t(Family::Pikmin),1},{2},{first}}};saved.sections={{{uint32_t(Global::BirthLedger),1},captureBirthLedger(original).state}};
    std::map<uint64_t,const void*> map{{first,&new1},{third,&new3}};BirthLedger restored;
    check(restoreBirthLedger(saved,map,restored,e),"saved IDs rebind to fresh-process addresses");
    check(!restored.lookup(&old1)&&restored.lookup(&new1)->id==first&&restored.lookup(&new3)->id==third&&restored.nextId()==original.nextId()&&restored.tombstones()==original.tombstones(),"all live/counter/tombstone identity preserved without old pointers");
    check(!restoreBirthLedger(saved,map,restored,e)&&restored.lookup(&new1)->id==first,"already-live destination cannot be overwritten");
    uint64_t reborn=0;check(restored.retire(&new3,e)&&restored.birth(&new3,Family::Enemy,reborn,e)&&reborn==4&&restored.lookup(&new3)->family==Family::Enemy&&restored.tombstones().count(third),"reused address receives new monotonic lifetime");
    for(unsigned f=0;f<12;++f){auto bad=saved;auto addresses=map;
        if(f==0)bad.sections.clear();
        if(f==1)bad.sections.push_back(bad.sections[0]);
        if(f==2)bad.sections[0].adapter.version=2;
        if(f==3)bad.sections[0].state.pop_back();
        if(f==4)bad.sections[0].state[0]=0;
        if(f==5)bad.sections[0].state[12]=0;
        if(f==6)bad.actors[1].id=retired;
        if(f==7)bad.actors[1].id=first;
        if(f==8)addresses.erase(third);
        if(f==9)addresses[third]=nullptr;
        if(f==10)addresses[third]=&new1;
        if(f==11)bad.actors[1].adapter.family=999;
        BirthLedger destination;check(!restoreBirthLedger(bad,addresses,destination,e)&&destination.nextId()==1&&destination.liveCount()==0&&destination.tombstones().empty(),"malformed identity plan refuses without staging mutation");
    }
    auto bad=saved;bad.sections[0].state.insert(bad.sections[0].state.end(),8,0);bad.sections[0].state[8]=2;BirthLedger untouched;
    check(!restoreBirthLedger(bad,map,untouched,e)&&untouched.nextId()==1,"duplicate/noncanonical tombstone sequence refuses");
    std::cout<<"PASS "<<n<<" actual BirthLedger restore controls; no world state restored\n";return 0;
}catch(const std::exception& x){std::cerr<<"FAIL "<<n<<": "<<x.what()<<"\n";return 1;}}
