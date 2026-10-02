#include "pc_midday_capture.h"
#include <iostream>
#include <stdexcept>
#include <limits>
using namespace pc_midday;
int checks=0;
void check(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}
int main(){try{
    int captain=0,piki=0,cargo=0,other=0;BirthLedger ids;std::string e;uint64_t first=0,reused=0;
    check(ids.birth(&piki,Family::Pikmin,first,e),"first observed birth");
    check(!ids.birth(&piki,Family::Pikmin,reused,e),"duplicate birth refused");
    check(ids.retire(&piki,e),"retire tracked lifetime");check(!ids.retire(&piki,e),"double retire refused");
    check(ids.birth(&piki,Family::Pikmin,reused,e)&&reused!=first&&ids.tombstones().count(first),"reused manager address has new stable incarnation");
    uint64_t id;check(ids.birth(&captain,Family::Captain,id,e),"captain birth");check(ids.birth(&cargo,Family::Cargo,id,e),"cargo birth");
    Census c;c.agreedReadOnlyFence=true;c.frameBefore=c.frameAfter=99;
    for(Family f:{Family::Captain,Family::Pikmin,Family::Enemy,Family::Boss,Family::Cargo,Family::WorldItem,Family::Structure,Family::Projectile})c.enumeratedFamilies.insert(f);
    Observation n;n.address=&captain;n.family=Family::Captain;n.captainSlot=0;n.health=n.maxHealth=100;Observation p;p.address=&piki;p.family=Family::Pikmin;p.species=TypedSpecies::Purple;p.maturity=2;p.references={{&captain,Role::CaptainOwner},{&cargo,Role::CarryTarget}};Observation g;g.address=&cargo;g.family=Family::Cargo;c.actors={n,p,g};
    Binding b;b.seed[0]=1;b.session[0]=2;b.content[0]=3;b.schema[0]=4;
    std::map<Family,ActorAdapter> adapters;for(Family f:{Family::Captain,Family::Pikmin,Family::Cargo})adapters[f]=[](const Observation&){return AdapterOutput{Completeness::Complete,{1},"pure contract fixture only"};};
    std::vector<Globals> globals;for(Global f:{Global::SceneClock,Global::BirthLedger,Global::StockEconomy,Global::GameplayRng,Global::LogicalAudio,Global::APLedger,Global::CaveGraph,Global::Jobs})globals.push_back({f,f==Global::BirthLedger?captureBirthLedger(ids):AdapterOutput{Completeness::Complete,{2},"pure contract fixture only"}});
    Snapshot s;Coverage cov;check(capture(c,ids,adapters,globals,b,1,0,s,cov,e),"pure typed capture contract positive");
    check(s.actors[1].id==reused&&s.actors[1].references[0]==ids.lookup(&captain)->id,"logical IDs/reference roles bound without pointer bytes");
    Bytes encoded;check(encode(s,cov,encoded,e),"candidate contract passes codec graph checks");
    auto bad=c;bad.frameAfter++;s.generation=777;check(!capture(bad,ids,adapters,globals,b,2,0,s,cov,e)&&s.generation==777,"moving tick refuses without output mutation");
    bad=c;bad.enumeratedFamilies.erase(Family::Projectile);check(!capture(bad,ids,adapters,globals,b,2,0,s,cov,e),"missing dynamic-family census refuses");
    bad=c;bad.actors.pop_back();check(!capture(bad,ids,adapters,globals,b,2,0,s,cov,e),"tracked cargo cannot silently disappear");
    bad=c;bad.actors[1].address=&other;check(!capture(bad,ids,adapters,globals,b,2,0,s,cov,e),"untracked manager birth refuses");
    bad=c;bad.actors[1].references[0].target=&cargo;check(!capture(bad,ids,adapters,globals,b,2,0,s,cov,e),"cargo cannot satisfy captain owner role");
    bad=c;bad.actors[1].references[1].target=&other;check(!capture(bad,ids,adapters,globals,b,2,0,s,cov,e),"untracked reference refuses");
    bad=c;bad.actors[1].species=TypedSpecies::Unknown;check(!capture(bad,ids,adapters,globals,b,2,0,s,cov,e),"unidentified species refuses");
    bad=c;bad.actors[0].health=std::numeric_limits<float>::quiet_NaN();check(!capture(bad,ids,adapters,globals,b,2,0,s,cov,e),"nonfinite native scalar refuses");
    bad=c;bad.actors[0].health=0;check(!capture(bad,ids,adapters,globals,b,2,0,s,cov,e),"dead or uninitialized captain refuses capture");
    bad=c;bad.expectedCaptains=2;check(!capture(bad,ids,adapters,globals,b,2,0,s,cov,e),"both requested captains must be captured");
    auto missing=globals;missing.pop_back();check(!capture(c,ids,adapters,missing,b,2,0,s,cov,e),"jobs global coverage cannot be omitted");
    missing=globals;missing[3].output.status=Completeness::Unsupported;check(!capture(c,ids,adapters,missing,b,2,0,s,cov,e),"offline libc RNG refuses unsupported capture");
    missing=globals;missing[1].output.state={1};check(!capture(c,ids,adapters,missing,b,2,0,s,cov,e),"identity section must bind exact counter/tombstones");
    auto unsupported=adapters;unsupported[Family::Pikmin]=[](const Observation&){return AdapterOutput{Completeness::Unsupported,{},"Purple flight timers unavailable"};};check(!capture(c,ids,unsupported,globals,b,2,0,s,cov,e),"unsupported P2 typed FSM refuses whole capture");
    unsupported=adapters;unsupported[Family::Pikmin]=[](const Observation&)->AdapterOutput{throw std::runtime_error("adapter fault");};check(!capture(c,ids,unsupported,globals,b,2,0,s,cov,e),"adapter exception visibly refuses capture");
    bad=c;bad.actors[1].references[0].role=Role(999);check(!capture(bad,ids,adapters,globals,b,2,0,s,cov,e),"unknown typed role refused");
    bad=c;bad.actors[1].references[0]={nullptr,Role(999)};s.generation=777;auto priorCoverage=cov;
    check(!capture(bad,ids,adapters,globals,b,2,0,s,cov,e)&&s.generation==777&&cov.observedActorIds==priorCoverage.observedActorIds,"null target cannot bypass unknown role or mutate output");
    for(Role role:{Role::AnyActor,Role::CaptainOwner,Role::CarryTarget,Role::WorkTarget,Role::Predator}){
        bad=c;bad.actors[1].references[0]={nullptr,role};
        check(capture(bad,ids,adapters,globals,b,2,0,s,cov,e),"valid nullable role preserves optional reference");
    }
    BirthLedger restored;check(restored.restoreCounter(ids.nextId(),ids.tombstones(),e),"logical allocator restore");
    check(restored.rebind(&other,{reused,Family::Pikmin},e),"restored ID binds fresh address");
    check(!restored.rebind(&piki,{reused,Family::Pikmin},e),"duplicate restored logical ID refuses");
    check(!restored.rebind(&piki,{first,Family::Pikmin},e),"retired ID cannot be rebound");
    check(!restored.birth(&piki,Family(999),id,e),"unknown actor family refuses birth registration");
    check(ids.birth(&other,Family::Captain,id,e),"second captain birth");auto two=c;two.expectedCaptains=2;auto n2=n;n2.address=&other;n2.captainSlot=1;two.actors.push_back(n2);auto g2=globals;g2[1].output=captureBirthLedger(ids);
    check(capture(two,ids,adapters,g2,b,2,0,s,cov,e)&&s.actors.size()==4,"two captain logical census positive contract");
    two.actors.back().captainSlot=0;check(!capture(two,ids,adapters,g2,b,3,0,s,cov,e),"duplicate captain owner slot refuses");
    std::cout<<"PASS "<<checks<<" pure capture-contract controls; no native world capture/restore\n";return 0;
}catch(const std::exception& x){std::cerr<<"FAIL "<<checks<<": "<<x.what()<<"\n";return 1;}}
