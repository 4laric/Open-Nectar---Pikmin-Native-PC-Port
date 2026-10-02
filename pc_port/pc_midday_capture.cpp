#include "pc_midday_capture.h"
#include <limits>
#include <cmath>

namespace pc_midday {
namespace {bool known(Family f){return uint32_t(f)>=uint32_t(Family::Captain)&&uint32_t(f)<=uint32_t(Family::Projectile);}}
AdapterOutput captureBirthLedger(const BirthLedger& ledger){
    AdapterOutput out;
    if(ledger.tombstones().size()>MaxActors){out.reason="birth tombstone count exceeds bound";return out;}
    auto put=[&](uint64_t n,unsigned width){for(unsigned i=0;i<width;++i)out.state.push_back(uint8_t(n>>(8*i)));};
    put(ledger.nextId(),8);put(ledger.tombstones().size(),4);for(auto id:ledger.tombstones())put(id,8);
    out.status=Completeness::Complete;return out;
}
bool restoreBirthLedger(const Snapshot& saved,const std::map<uint64_t,const void*>& addresses,
                        BirthLedger& out,std::string& e){
    try{
        if(out.liveCount()||out.nextId()!=1||!out.tombstones().empty()){e="ledger restore destination is not fresh staging";return false;}
        const Section* section=nullptr;
        for(const auto& candidate:saved.sections)if(candidate.adapter.family==uint32_t(Global::BirthLedger)){
            if(section||candidate.adapter.version!=1){e="duplicate/versioned identity section";return false;}section=&candidate;
        }
        if(!section){e="missing saved identity section";return false;}
        const auto& b=section->state;
        if(b.size()<12){e="truncated identity state";return false;}
        auto get=[&](size_t at,unsigned width){uint64_t value=0;for(unsigned i=0;i<width;++i)value|=uint64_t(b[at+i])<<(i*8);return value;};
        const uint64_t next=get(0,8),count=get(8,4);
        if(!next||count>MaxActors||b.size()!=12+size_t(count)*8){e="invalid identity counter/tombstone bounds";return false;}
        std::set<uint64_t> dead;uint64_t previous=0;
        for(size_t i=0;i<size_t(count);++i){uint64_t id=get(12+i*8,8);if(!id||id<=previous||id>=next){e="noncanonical identity tombstones";return false;}dead.insert(id);previous=id;}
        if(saved.actors.size()>MaxActors||addresses.size()!=saved.actors.size()){e="incomplete allocated identity census";return false;}
        std::set<uint64_t> live;std::set<const void*> unique;
        for(const auto& actor:saved.actors){
            Family family=Family(actor.adapter.family);auto local=addresses.find(actor.id);
            if(!known(family)||actor.adapter.version!=1||!actor.id||actor.id>=next||dead.count(actor.id)||!live.insert(actor.id).second||
               local==addresses.end()||!local->second||!unique.insert(local->second).second){e="invalid or incomplete restored actor identity";return false;}
        }
        BirthLedger staged;if(!staged.restoreCounter(next,dead,e))return false;
        for(const auto& actor:saved.actors)if(!staged.rebind(addresses.at(actor.id),{actor.id,Family(actor.adapter.family)},e))return false;
        out=std::move(staged);e.clear();return true;
    }catch(const std::exception& failure){e=std::string("identity staging failed: ")+failure.what();return false;}
}
bool BirthLedger::birth(const void* p,Family f,uint64_t& id,std::string& e){
    if(!p||!known(f)||live_.count(p)||next_==std::numeric_limits<uint64_t>::max()){e="duplicate/null/unknown birth or identity counter exhausted";return false;}
    id=next_++;live_.emplace(p,Lifetime{id,f});e.clear();return true;
}
bool BirthLedger::retire(const void* p,std::string& e){
    auto i=live_.find(p);if(i==live_.end()){e="untracked actor retirement";return false;}
    retired_.insert(i->second.id);live_.erase(i);e.clear();return true;
}
const Lifetime* BirthLedger::lookup(const void* p)const{auto i=live_.find(p);return i==live_.end()?nullptr:&i->second;}
bool BirthLedger::restoreCounter(uint64_t next,const std::set<uint64_t>& dead,std::string& e){
    if(!live_.empty()||!next||dead.count(0)||(!dead.empty()&&*dead.rbegin()>=next)){e="invalid identity ledger restore";return false;}
    next_=next;retired_=dead;e.clear();return true;
}
bool BirthLedger::rebind(const void* p,Lifetime life,std::string& e){
    if(!p||!known(life.family)||!life.id||life.id>=next_||retired_.count(life.id)||live_.count(p)){e="invalid restored actor identity";return false;}
    for(auto i:live_)if(i.second.id==life.id){e="duplicate restored actor ID";return false;}
    live_.emplace(p,life);e.clear();return true;
}
namespace {
bool roleAllows(Role r,Family f){
    switch(r){
    case Role::CaptainOwner:return f==Family::Captain;
    case Role::CarryTarget:return f==Family::Cargo;
    case Role::WorkTarget:return f==Family::Structure;
    case Role::Predator:return f==Family::Enemy||f==Family::Boss;
    case Role::AnyActor:return true;
    default:return false;
    }
}
bool finite(const Observation& a){
    if(!std::isfinite(a.health)||!std::isfinite(a.maxHealth)||a.maxHealth<0)return false;
    for(int i=0;i<3;++i)if(!std::isfinite(a.position[i])||!std::isfinite(a.velocity[i]))return false;
    return true;
}
}
bool capture(const Census& census,const BirthLedger& ids,const std::map<Family,ActorAdapter>& adapters,
             const std::vector<Globals>& globals,const Binding& binding,uint64_t generation,
             uint64_t dayEnd,Snapshot& out,Coverage& coverageOut,std::string& e){
    try {
    if(!census.agreedReadOnlyFence||census.frameBefore!=census.frameAfter){e="capture is not at an agreed stable world boundary";return false;}
    for(Family f:{Family::Captain,Family::Pikmin,Family::Enemy,Family::Boss,Family::Cargo,Family::WorldItem,Family::Structure,Family::Projectile})
        if(!census.enumeratedFamilies.count(f)){e="incomplete manager/dynamic-family census";return false;}
    if(census.actors.size()>MaxActors||census.actors.size()!=ids.liveCount()){e="actor census exceeds bound or omits tracked lifetime";return false;}
    std::set<const void*> addresses;std::set<int> captains;Snapshot s;s.binding=binding;s.generation=generation;s.dayEndGeneration=dayEnd;s.frame=census.frameBefore;Coverage coverage;
    for(const auto& observed:census.actors){
        if(!observed.address||!addresses.insert(observed.address).second||!finite(observed)){e="invalid/duplicate/nonfinite actor observation";return false;}
        auto life=ids.lookup(observed.address);if(!life||life->family!=observed.family){e="untracked birth or mismatched actor family";return false;}
        if(observed.family==Family::Captain){if(observed.health<=0||observed.maxHealth<=0||observed.captainSlot<0||observed.captainSlot>1||!captains.insert(observed.captainSlot).second){e="uninitialized/dead/invalid/duplicate captain";return false;}}
        if(observed.family==Family::Pikmin&&(observed.species==TypedSpecies::Unknown||observed.maturity<0||observed.maturity>2)){e="invalid typed Pikmin identity/maturity";return false;}
        auto adapter=adapters.find(observed.family);if(adapter==adapters.end()){e="missing typed actor adapter";return false;}
        auto data=adapter->second(observed);if(data.status!=Completeness::Complete||data.state.empty()){e="unsupported typed actor state: "+data.reason;return false;}
        Actor actor;actor.id=life->id;actor.adapter={uint32_t(observed.family),1};actor.state=std::move(data.state);
        for(const auto& reference:observed.references){
            switch(reference.role){
            case Role::AnyActor:case Role::CaptainOwner:case Role::CarryTarget:case Role::WorkTarget:case Role::Predator:break;
            default:e="unknown logical actor reference role";return false;
            }
            if(!reference.target){actor.references.push_back(0);continue;}
            auto target=ids.lookup(reference.target);if(!target||!roleAllows(reference.role,target->family)){e="dangling or mistyped logical actor reference";return false;}actor.references.push_back(target->id);
        }
        s.actors.push_back(std::move(actor));coverage.observedActorIds.push_back(life->id);
    }
    if(census.expectedCaptains<1||census.expectedCaptains>2||captains.size()!=census.expectedCaptains){e="captain census missing/inconsistent";return false;}
    std::set<Global> sections;
    for(const auto& global:globals){
        if(uint32_t(global.family)<uint32_t(Global::SceneClock)||uint32_t(global.family)>uint32_t(Global::Jobs)||!sections.insert(global.family).second||global.output.status!=Completeness::Complete||global.output.state.empty()){e="missing/unsupported/duplicate global typed-state coverage: "+global.output.reason;return false;}
        if(global.family==Global::BirthLedger&&global.output.state!=captureBirthLedger(ids).state){e="birth ledger section does not match captured identity counter/tombstones";return false;}
        Capability capability{uint32_t(global.family),1};s.sections.push_back({capability,global.output.state});coverage.supported.push_back(capability);
    }
    for(Global g:{Global::SceneClock,Global::BirthLedger,Global::StockEconomy,Global::GameplayRng,Global::LogicalAudio,Global::APLedger,Global::CaveGraph,Global::Jobs}){
        if(!sections.count(g)){e="required clock/identity/economy/RNG/audio/AP/cave/jobs coverage missing";return false;}
        coverage.requiredSections.push_back({uint32_t(g),1});
    }
    for(auto a:adapters)coverage.supported.push_back({uint32_t(a.first),1});
    if(!validate(s,coverage,e))return false;
    out=std::move(s);coverageOut=std::move(coverage);e.clear();return true;
    }catch(const std::exception& x){e=std::string("typed capture failed: ")+x.what();return false;}
}
}
