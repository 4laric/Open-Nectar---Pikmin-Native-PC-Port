#include "pc_midday_restore.h"
#include <iostream>
#include <stdexcept>
using namespace pc_midday;
int checks=0;
void check(bool yes,const char* why){++checks;if(!yes)throw std::runtime_error(why);}
struct Mock : RestoreBackend {
    RestoreGate admission{true,true,true,true,true,true,true};
    std::string fault;bool staged=false;unsigned expected=0,allocations=0,subobjects=0,bindings=0,aborts=0,begins=0;
    uint64_t activeGeneration=99;std::vector<std::string> order;
    RestoreGate gate()const override{return admission;}
    bool begin(const Snapshot& s,std::string&)override{++begins;staged=true;expected=unsigned(s.actors.size());order.push_back("begin");return fault!="begin";}
    bool allocate(const Actor& a,StagedHandle& out,std::string&)override{
        ++allocations;order.push_back("allocate");out=fault=="duplicate"?1000:a.id+1000;return fault!="allocate"||allocations!=2;
    }
    bool allocateSubobjects(const Actor& a,StagedHandle self,std::string&)override{
        if(allocations!=expected||bindings||self!=a.id+1000)throw std::runtime_error("subobject phase ordering/identity failure");
        ++subobjects;order.push_back("subobjects");
        if(fault=="subobject-throw")throw std::runtime_error("staged pool allocation exception");
        return fault!="subobjects"||subobjects!=2;
    }
    bool bind(const Actor& a,StagedHandle self,const std::vector<StagedHandle>& refs,std::string&)override{
        if(allocations!=expected||subobjects!=expected||self!=a.id+1000)throw std::runtime_error("pass ordering/identity failure");
        for(size_t i=0;i<refs.size();++i)if(refs[i]!=(a.references[i]?a.references[i]+1000:0))throw std::runtime_error("reference identity failure");
        ++bindings;order.push_back("bind");if(fault=="throw")throw std::runtime_error("staged adapter exception");return fault!="bind";
    }
    bool applyGlobal(const Section&,std::string&)override{if(bindings!=expected)throw std::runtime_error("globals before references");order.push_back("global");return fault!="global";}
    bool verify(const Snapshot&,const std::map<uint64_t,StagedHandle>& handles,std::string&)override{
        if(handles.size()!=expected)throw std::runtime_error("incomplete staged census");
        order.push_back("verify");if(fault=="fence")admission.paused=false;return fault!="verify";
    }
    bool publishPaused(const Snapshot& s,std::string&)override{order.push_back("publish");if(fault=="publish")return false;activeGeneration=s.generation;staged=false;return true;}
    void abort()noexcept override{++aborts;staged=false;}
};
int main(){try{
    Snapshot s;s.binding.seed[0]=1;s.binding.session[0]=2;s.binding.content[0]=3;s.binding.schema[0]=4;s.generation=2;s.frame=100;
    s.actors={{2,{1,1},{2},{1,0}},{1,{1,1},{1},{2}}};s.sections={{{10,1},{3}}};Coverage coverage{{{1,1},{10,1}},{{10,1}},{1,2}};
    std::map<std::pair<uint32_t,uint32_t>,ActorStateValidator> actors{{{1,1},[](const Actor& a,std::string&){return a.state.size()==1&&a.state[0]==a.id;}}};
    std::map<std::pair<uint32_t,uint32_t>,GlobalStateValidator> globals{{{10,1},[](const Section& a,std::string&){return a.state==Bytes{3};}}};
    std::string e;Mock good;check(restorePaused(s,s.binding,coverage,actors,globals,good,e),"synthetic staged two-pass restore positive");
    check(good.order==std::vector<std::string>{"begin","allocate","allocate","subobjects","subobjects","bind","bind","global","verify","publish"},"all allocation precedes all reference binding/global verification/publication");
    check(good.activeGeneration==2&&!good.staged&&!good.aborts,"successful paused publication owns final staged world");
    for(const char* fault:{"begin","allocate","duplicate","subobjects","subobject-throw","bind","throw","global","verify","fence","publish"}){
        Mock failed;failed.fault=fault;check(!restorePaused(s,s.binding,coverage,actors,globals,failed,e),"staged fault refuses restore");
        if(failed.fault=="subobjects"||failed.fault=="subobject-throw")check(!failed.bindings,"subobject refusal precedes every reference/global mutation");
        check(failed.activeGeneration==99&&!failed.staged&&failed.aborts==1,"staged fault aborts and leaves active world unchanged");
    }
    for(unsigned flag=0;flag<7;++flag){Mock failed;bool* flags[]={&failed.admission.freshProcess,&failed.admission.paused,&failed.admission.zeroInput,&failed.admission.birthEffectsSuppressed,&failed.admission.rewardsSuppressed,&failed.admission.rngDrawsSuppressed,&failed.admission.audioVoicesSuppressed};*flags[flag]=false;
        check(!restorePaused(s,s.binding,coverage,actors,globals,failed,e)&&failed.begins==0,"missing constructor/paused admission refuses before allocation");}
    Mock failed;auto different=s.binding;different.seed[0]++;
    check(!restorePaused(s,different,coverage,actors,globals,failed,e)&&failed.begins==0,"foreign seed refuses before begin");
    auto bad=s;bad.actors[0].references[0]=999;check(!restorePaused(bad,s.binding,coverage,actors,globals,failed,e)&&failed.begins==0,"dangling logical reference refuses before begin");
    bad=s;bad.actors[0].state={99};check(!restorePaused(bad,s.binding,coverage,actors,globals,failed,e)&&failed.begins==0,"invalid typed actor payload refuses before begin");
    bad=s;bad.sections[0].state={99};check(!restorePaused(bad,s.binding,coverage,actors,globals,failed,e)&&failed.begins==0,"invalid typed global payload refuses before begin");
    auto missing=actors;missing.clear();check(!restorePaused(s,s.binding,coverage,missing,globals,failed,e)&&failed.begins==0,"unsupported actor adapter refuses before begin");
    auto missingGlobal=globals;missingGlobal.clear();check(!restorePaused(s,s.binding,coverage,actors,missingGlobal,failed,e)&&failed.begins==0,"unsupported global adapter refuses before begin");
    std::cout<<"PASS "<<checks<<" pure staged restore-contract controls; no live native world restored\n";return 0;
}catch(const std::exception& x){std::cerr<<"FAIL "<<checks<<": "<<x.what()<<"\n";return 1;}}
