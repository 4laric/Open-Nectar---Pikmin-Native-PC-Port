#include "pc_midday_restore.h"
#include <set>
namespace pc_midday {
namespace {
bool safe(const RestoreGate& g){return g.freshProcess&&g.paused&&g.zeroInput&&g.birthEffectsSuppressed&&g.rewardsSuppressed&&g.rngDrawsSuppressed&&g.audioVoicesSuppressed;}
struct StageGuard {
    RestoreBackend& backend;bool active=true;
    ~StageGuard(){if(active)backend.abort();}
};
}
bool restorePaused(const Snapshot& saved,const Binding& expected,const Coverage& coverage,
    const std::map<std::pair<uint32_t,uint32_t>,ActorStateValidator>& actors,
    const std::map<std::pair<uint32_t,uint32_t>,GlobalStateValidator>& globals,
    RestoreBackend& backend,std::string& e){
    try{
        if(saved.binding.seed!=expected.seed||saved.binding.session!=expected.session||saved.binding.content!=expected.content||saved.binding.schema!=expected.schema){e="restore binding mismatch";return false;}
        if(!validate(saved,coverage,e))return false;
        // These callbacks validate complete typed schemas, including reference
        // roles/FSM/action fields, rather than accepting arbitrary opaque bytes.
        for(const auto& actor:saved.actors){
            auto adapter=actors.find({actor.adapter.family,actor.adapter.version});
            if(adapter==actors.end()){e="missing typed restore actor adapter";return false;}
            if(!adapter->second(actor,e))return false;
        }
        for(const auto& section:saved.sections){
            auto adapter=globals.find({section.adapter.family,section.adapter.version});
            if(adapter==globals.end()){e="missing typed restore global adapter";return false;}
            if(!adapter->second(section,e))return false;
        }
        if(!safe(backend.gate())){e="restore requires fresh paused zero-input world with constructor effects suppressed";return false;}
        // Begin may partially allocate a disposable scene even when it fails.
        StageGuard staging{backend};
        if(!backend.begin(saved,e))return false;
        std::map<uint64_t,StagedHandle> handles;std::set<StagedHandle> unique;
        for(const auto& actor:saved.actors){
            StagedHandle handle=0;
            if(!backend.allocate(actor,handle,e))return false;
            if(!handle||!unique.insert(handle).second){e="invalid/duplicate staged actor handle";return false;}
            handles.emplace(actor.id,handle);
        }
        // Every saved actor now exists. Zero denotes a schema-approved optional
        // reference; typed validators decide which roles may be absent.
        for(const auto& actor:saved.actors){
            std::vector<StagedHandle> references;references.reserve(actor.references.size());
            for(uint64_t id:actor.references)references.push_back(id?handles.at(id):0);
            if(!backend.bind(actor,handles.at(actor.id),references,e))return false;
        }
        for(const auto& section:saved.sections)if(!backend.applyGlobal(section,e))return false;
        if(!backend.verify(saved,handles,e))return false;
        if(!safe(backend.gate())){e="paused restore fence changed before publication";return false;}
        if(!backend.publishPaused(saved,e))return false;
        staging.active=false;e.clear();return true;
    }catch(const std::exception& failure){e=std::string("paused restore refused: ")+failure.what();return false;}
    catch(...){e="paused restore refused: unknown adapter/backend exception";return false;}
}
}
