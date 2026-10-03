#include "pc_p2_tamago_egg_policy.h"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <limits>
#include <map>
#include <vector>
struct IO {
    int limit=10,occupied=0,hostFree=80,fail=-1,draws=0;
    bool manager=true;
    std::vector<std::string> events;
    std::array<p2original::Position,10> pos{},vel{};
    std::array<float,10> face{};
    std::array<bool,10> allocated{},balls{};
    bool available(){return manager;}
    int freeSlots(){return std::min(limit-occupied,hostFree);}
    float draw(){events.push_back("rng");++draws;return .25f;}
    void event(const char* name,unsigned n){events.push_back(std::string(name)+std::to_string(n));}
    bool birth(unsigned n,const p2original::Position& p,float f){event("birth",n);if(int(n)==fail)return false;++occupied;--hostFree;allocated[n]=true;pos[n]=p;face[n]=f;return true;}
    void init(unsigned n){event("init",n);auto s=p2tamago::initialize(pos[n],face[n],[&](){return draw();});assert(s.appearWait==37);assert(s.appearFrame==3.75f);assert(s.activeMax==153.f);assert(std::fabs(s.moveFactor-.475f)<.00001f);}
    void ball(unsigned n){event("ball",n);balls[n]=true;}
    void velocity(unsigned n,const p2original::Position& v){event("velocity",n);vel[n]=v;}
    p2original::Position leaderVelocity(){return vel[0];}
    void leader(unsigned n,unsigned leader){event("leader",n);if(n!=leader&&balls[leader])ball(n);}
    void place(unsigned n,const p2original::Position& p,float f){event("place",n);pos[n]=p;face[n]=f;}
    void retire(unsigned n){assert(allocated[n]);allocated[n]=false;--occupied;++hostFree;}
};
p2tamago::EggGroupRequest request(){p2tamago::EggGroupRequest r;r.identity.parent={std::string(64,'a'),0x52000017,27,3,8};r.position={100,15,200};r.velocity={0,200,0};r.facing=1.25f;return r;}
int main(){
    auto r=request();assert(p2tamago::valid(r)); // parent ordinal is independent of ten child members
    IO gate;gate.occupied=1;assert(!p2tamago::createEggGroup(r,gate));assert(gate.events.empty());
    IO host;host.hostFree=9;assert(!p2tamago::createEggGroup(r,host));assert(host.events.empty());
    IO missing;missing.manager=false;assert(!p2tamago::createEggGroup(r,missing));assert(missing.events.empty());
    IO failed;failed.fail=0;assert(!p2tamago::createEggGroup(r,failed));assert(failed.events==std::vector<std::string>{"birth0"});assert(!failed.draws);
    IO full;assert(p2tamago::createEggGroup(r,full)==10);assert(full.draws==71&&full.occupied==10);
    assert(full.events[0]=="birth0"&&full.events[1]=="init0");for(int i=2;i<9;++i)assert(full.events[i]=="rng");
    assert(full.events[9]=="ball0"&&full.events[10]=="velocity0"&&full.events[11]=="leader0"&&full.events[12]=="rng"&&full.events[13]=="place0"&&full.events[14]=="birth1");
    assert(full.pos[0].x==100&&full.pos[0].y==15&&full.pos[0].z==200&&full.face[0]==0);
    for(unsigned n=1;n<10;++n){float a=6.28318531f*float(n-1)/10.f;assert(full.pos[n].x==100-10*std::sin(a));assert(full.pos[n].z==200-10*std::cos(a));assert(full.face[n]==-a&&full.vel[n].y==200&&full.balls[n]);}
    auto count=full.events.size();assert(!p2tamago::createEggGroup(r,full));assert(full.events.size()==count); // dead/corpse still allocated
    full.retire(0);assert(!p2tamago::createEggGroup(r,full));for(unsigned n=1;n<10;++n)full.retire(n);assert(p2tamago::createEggGroup(r,full)==10);
    IO partial;partial.fail=5;assert(p2tamago::createEggGroup(r,partial)==9);assert(partial.draws==64&&!partial.allocated[5]&&partial.allocated[9]);
    auto it=std::find(partial.events.begin(),partial.events.end(),"birth5");assert(*(it+1)=="birth6");
    auto invalid=r;invalid.identity.slot=1;IO clean;assert(!p2tamago::createEggGroup(invalid,clean)&&clean.events.empty());
    invalid=r;invalid.position.x=std::numeric_limits<float>::infinity();assert(!p2tamago::valid(invalid));invalid=r;invalid.identity.parent.activation=0;assert(!p2tamago::valid(invalid));
    p2tamago::MemberIdentity member{r.identity,9};assert(member.source==68&&member.group.slot==0&&member.member==9);
    std::map<p2tamago::GroupIdentity,int> ledger;
    assert(p2tamago::claimGroup(ledger,r.identity,0));assert(!p2tamago::claimGroup(ledger,r.identity,10));assert(ledger.at(r.identity)==0);
    auto next=r.identity;++next.parent.activation;assert(p2tamago::claimGroup(ledger,next,10));
    auto carried=r.identity;carried.parent.generator=16;assert(p2tamago::claimGroup(ledger,carried,10)); // caller authenticates captured Egg provenance
    assert(p2tamago::orphanLeaderRole(0));for(unsigned n=1;n<10;++n)assert(!p2tamago::orphanLeaderRole(n));
    p2tamago::MemberFrontier frontier;frontier.born=true;frontier.terminal=p2tamago::Terminal::Death;p2tamago::retireMember(frontier);assert(frontier.retired&&frontier.terminal==p2tamago::Terminal::Death);
    int rewardDraws=0,rewardCalls=0;
    for(auto outcome:{P2TamagoHoneyBirth::Born,P2TamagoHoneyBirth::PoolEmpty,P2TamagoHoneyBirth::ResourceError}){
        p2tamago::MemberFrontier m;
        assert(p2tamago::attemptHoney(m,1.f,[&]{++rewardDraws;return .5f;},[&]{++rewardCalls;return outcome;}));
        assert(!p2tamago::attemptHoney(m,1.f,[&]{++rewardDraws;return .5f;},[&]{++rewardCalls;return outcome;}));
        assert(m.honeyAttempted&&m.honeyBorn==(outcome==P2TamagoHoneyBirth::Born)&&m.honeyFault==(outcome==P2TamagoHoneyBirth::ResourceError));
        assert(p2tamago::retireHoney(m,true)==m.honeyBorn);assert(!p2tamago::retireHoney(m,true));assert(m.honeyConsumed==m.honeyBorn);
    }
    assert(rewardDraws==3&&rewardCalls==3);
    p2tamago::MemberFrontier endpoint;
    assert(p2tamago::attemptHoney(endpoint,1.f,[]{return 1.f;},[]{return P2TamagoHoneyBirth::Born;}));assert(endpoint.honeyBorn); // retail !(rand > rate), inclusive
    p2tamago::MemberFrontier absorbing;absorbing.honeyBorn=true;
    assert(p2tamago::consumeHoney(absorbing));assert(!absorbing.honeyRetired&&absorbing.honeyConsumed);assert(!p2tamago::consumeHoney(absorbing));
    assert(p2tamago::retireHoney(absorbing,false));assert(absorbing.honeyRetired&&absorbing.honeyConsumed);assert(!p2tamago::retireHoney(absorbing,false));
    std::string error;P2TamagoHoneyProvider provider;assert(!p2tamago::preflightHoney(provider,error));
    struct Context {bool ready=false;int calls=0;} context;
    provider.context=&context;
    provider.preflight=[](void* p,std::string& e){auto& c=*static_cast<Context*>(p);++c.calls;e=c.ready?"":"resources missing";return c.ready;};
    provider.birth=[](void*,const p2tamago::MemberIdentity&,unsigned,const p2original::Position&,const p2original::Position&) noexcept{return P2TamagoHoneyBirth::PoolEmpty;};
    assert(!p2tamago::preflightHoney(provider,error)&&context.calls==1);context.ready=true;assert(p2tamago::preflightHoney(provider,error)&&context.calls==2);
    std::puts("p2_tamago_egg_policy_test PASS");
}
