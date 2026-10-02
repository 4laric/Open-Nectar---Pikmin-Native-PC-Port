#include "pc_midday_codec.h"
#include "netplay/pc_netplay_sha256.h"
#include <algorithm>
#include <cstring>
#include <set>
#include <stdexcept>

namespace pc_midday {
namespace {
bool eq(Capability a, Capability b) { return a.family == b.family && a.version == b.version; }
bool supports(const Coverage& c, Capability a) {
    return a.family && a.version && std::any_of(c.supported.begin(), c.supported.end(), [a](Capability b){return eq(a,b);});
}
bool bound(const Binding& a, const Binding& b) {
    return a.seed==b.seed && a.session==b.session && a.content==b.content && a.schema==b.schema;
}
void put(Bytes& b, uint64_t n, unsigned width) { for(unsigned i=0;i<width;++i)b.push_back(uint8_t(n>>(i*8))); }
void blob(Bytes& b, const Bytes& v) { put(b,v.size(),4); b.insert(b.end(),v.begin(),v.end()); }
void hash(Bytes& b, const Digest& h) { b.insert(b.end(),h.begin(),h.end()); }
struct Reader {
    const Bytes& b; size_t p=0, end;
    void need(size_t n) { if(n>end-p)throw std::runtime_error("truncated checkpoint"); }
    uint64_t get(unsigned n) {need(n);uint64_t v=0;for(unsigned i=0;i<n;++i)v|=uint64_t(b[p++])<<(i*8);return v;}
    Digest hash() {need(32);Digest h;std::copy_n(b.begin()+p,32,h.begin());p+=32;return h;}
    Bytes blob() {size_t n=size_t(get(4));need(n);Bytes v(b.begin()+p,b.begin()+p+n);p+=n;return v;}
};
}
Digest digest(const Bytes& b) { Digest h;pc_netplay_sha::sha256(b.data(),b.size(),h.data());return h; }
bool validate(const Snapshot& s, const Coverage& c, std::string& e) {
    auto fail=[&](const char* m){e=m;return false;};
    for(const Digest* binding:{&s.binding.seed,&s.binding.session,&s.binding.content,&s.binding.schema})
        if(std::all_of(binding->begin(),binding->end(),[](uint8_t v){return v==0;}))
            return fail("anonymous seed/session/content/schema binding refused");
    if(!s.generation || s.dayEndGeneration>=s.generation)return fail("invalid generation/day-end precedence");
    if(s.actors.size()>MaxActors || s.sections.size()>1024)return fail("record count exceeds bound");
    std::set<uint64_t> ids, observed;
    size_t size=200;
    for(const auto& a:s.actors) {
        if(!a.id || !ids.insert(a.id).second)return fail("duplicate/zero logical actor ID");
        if(!supports(c,a.adapter))return fail("unsupported actor adapter");
        if(a.references.size()>MaxActors || a.state.size()>MaxBytes)return fail("actor payload exceeds bound");
        size+=56+a.state.size()+a.references.size()*8;
        if(size>MaxBytes)return fail("checkpoint exceeds byte bound");
    }
    for(auto id:c.observedActorIds)if(!id || !observed.insert(id).second)return fail("invalid capture inventory");
    if(ids!=observed)return fail("incomplete actor capability coverage");
    for(const auto& a:s.actors)for(auto id:a.references)if(id && !ids.count(id))return fail("dangling logical reference");
    std::set<std::pair<uint32_t,uint32_t>> sections;
    for(const auto& x:s.sections) {
        if(!supports(c,x.adapter) || !sections.emplace(x.adapter.family,x.adapter.version).second)return fail("unsupported/duplicate global section");
        if(x.state.size()>MaxBytes)return fail("section exceeds bound");
        size+=44+x.state.size();if(size>MaxBytes)return fail("checkpoint exceeds byte bound");
    }
    for(auto x:c.requiredSections)if(!sections.count({x.family,x.version}))return fail("missing required global section");
    e.clear();return true;
}
bool encode(const Snapshot& s, const Coverage& c, Bytes& out, std::string& e) {
    if(!validate(s,c,e))return false;
    Bytes b={'P','C','M','I','D','D','A','Y'};put(b,FormatVersion,4);
    hash(b,s.binding.seed);hash(b,s.binding.session);hash(b,s.binding.content);hash(b,s.binding.schema);
    put(b,s.generation,8);put(b,s.frame,8);put(b,s.dayEndGeneration,8);
    put(b,s.actors.size(),4);put(b,s.sections.size(),4);
    auto actors=s.actors;std::sort(actors.begin(),actors.end(),[](const Actor&a,const Actor&b){return a.id<b.id;});
    for(const auto& a:actors){put(b,a.id,8);put(b,a.adapter.family,4);put(b,a.adapter.version,4);blob(b,a.state);hash(b,digest(a.state));put(b,a.references.size(),4);for(auto id:a.references)put(b,id,8);}
    auto sections=s.sections;std::sort(sections.begin(),sections.end(),[](const Section&a,const Section&b){return std::make_pair(a.adapter.family,a.adapter.version)<std::make_pair(b.adapter.family,b.adapter.version);});
    for(const auto& x:sections){put(b,x.adapter.family,4);put(b,x.adapter.version,4);blob(b,x.state);hash(b,digest(x.state));}
    hash(b,digest(b));if(b.size()>MaxBytes){e="encoded checkpoint exceeds bound";return false;}out=std::move(b);return true;
}
bool decode(const Bytes& b, const Binding& binding, const Coverage& c, Snapshot& out, std::string& e) {
    try {
        if(b.size()<204 || b.size()>MaxBytes)throw std::runtime_error("invalid checkpoint size");
        Bytes body(b.begin(),b.end()-32);auto sum=digest(body);
        if(!std::equal(sum.begin(),sum.end(),b.end()-32))throw std::runtime_error("checkpoint checksum mismatch");
        Reader r{b,0,b.size()-32};r.need(8);
        if(std::memcmp(b.data(),"PCMIDDAY",8))throw std::runtime_error("invalid checkpoint magic");
        r.p=8;
        if(r.get(4)!=FormatVersion)throw std::runtime_error("incompatible checkpoint version");
        Snapshot s;s.binding={r.hash(),r.hash(),r.hash(),r.hash()};
        if(!bound(s.binding,binding))throw std::runtime_error("seed/session/content/schema mismatch");
        s.generation=r.get(8);s.frame=r.get(8);s.dayEndGeneration=r.get(8);
        auto n=r.get(4),m=r.get(4);if(n>MaxActors || m>1024)throw std::runtime_error("record count exceeds bound");
        for(uint64_t i=0;i<n;++i){Actor a;a.id=r.get(8);a.adapter={uint32_t(r.get(4)),uint32_t(r.get(4))};a.state=r.blob();if(r.hash()!=digest(a.state))throw std::runtime_error("actor checksum mismatch");auto k=r.get(4);if(k>MaxActors)throw std::runtime_error("reference count exceeds bound");r.need(size_t(k)*8);for(uint64_t j=0;j<k;++j)a.references.push_back(r.get(8));s.actors.push_back(std::move(a));}
        for(uint64_t i=0;i<m;++i){Section x;x.adapter={uint32_t(r.get(4)),uint32_t(r.get(4))};x.state=r.blob();if(r.hash()!=digest(x.state))throw std::runtime_error("section checksum mismatch");s.sections.push_back(std::move(x));}
        if(r.p!=r.end)throw std::runtime_error("trailing checkpoint bytes");
        // Restore has no live world census yet. Validate the decoded inventory
        // itself; capture-time observedActorIds are checked by encode instead.
        Coverage restore=c;restore.observedActorIds.clear();
        for(const auto& a:s.actors)restore.observedActorIds.push_back(a.id);
        if(!validate(s,restore,e))return false;
        out=std::move(s);e.clear();return true;
    }catch(const std::exception& x){e=x.what();return false;}
}
}
