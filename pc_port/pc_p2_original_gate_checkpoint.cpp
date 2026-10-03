#include "pc_p2_original_gate_checkpoint.h"
#include "netplay/pc_netplay_sha256.h"
#include <algorithm>
#include <cstring>
#include <map>
#include <set>

namespace p2original {
namespace {
constexpr std::size_t header = 72, record = 156, checksum = 32;
bool fail(std::string& error, const char* message) { error=message; return false; }
bool digest(const std::string& value) {
    return value.size()==64 && value.find_first_not_of("0123456789abcdef")==std::string::npos;
}
bool sources(const std::vector<GateRecord>& rows,
             std::map<unsigned,const GateRecord*>& result, std::string& error) {
    if(rows.empty()||rows.size()>4096) return fail(error,"gate checkpoint source census invalid");
    for(const auto& row:rows)
        if(!validateGate(row,error)||!result.emplace(row.uid,&row).second)
            return fail(error,"gate checkpoint source authority invalid/duplicate");
    return true;
}
void word(std::vector<std::uint8_t>& bytes, std::uint64_t value, unsigned width) {
    for(unsigned i=0;i<width;++i) bytes.push_back(static_cast<std::uint8_t>(value>>(i*8)));
}
std::uint64_t read(const std::vector<std::uint8_t>& bytes, std::size_t& at, unsigned width) {
    std::uint64_t value=0;
    for(unsigned i=0;i<width;++i) value|=std::uint64_t(bytes[at++])<<(i*8);
    return value;
}
bool validate(const std::string& campaign,
              const std::map<unsigned,const GateRecord*>& authority,
              const std::vector<GateCheckpointEntry>& entries, std::string& error) {
    if(!digest(campaign)||entries.size()!=authority.size())
        return fail(error,"gate checkpoint campaign/census mismatch");
    std::set<unsigned> seen, orders;
    for(const auto& entry:entries) {
        const auto found=authority.find(entry.identity.generator);
        // A typed gate generator owns exactly one body. Its source ordinal is
        // zero even when retirement/recreation changes physical birth order.
        if(found==authority.end()||!seen.insert(entry.identity.generator).second
            ||entry.identity.catalog!=campaign||entry.identity.ordinal!=0
            ||!entry.identity.activation||entry.order>=entries.size()
            ||!orders.insert(entry.order).second)
            return fail(error,"gate checkpoint incarnation/order invalid");
        if(!gateStateValid(*found->second,entry.state,error)) return false;
    }
    return true;
}
}
bool gateCheckpointExport(const std::string& campaign,
    const std::vector<GateRecord>& rows, const std::vector<GateCheckpointEntry>& entries,
    std::vector<std::uint8_t>& out, std::string& error) {
    std::map<unsigned,const GateRecord*> authority;
    if(!sources(rows,authority,error)||!validate(campaign,authority,entries,error)) return false;
    auto ordered=entries;
    std::sort(ordered.begin(),ordered.end(),[](const auto& a,const auto& b){return a.order<b.order;});
    std::vector<std::uint8_t> bytes{'G','C','P','1'};
    bytes.insert(bytes.end(),campaign.begin(),campaign.end()); word(bytes,ordered.size(),4);
    for(const auto& entry:ordered) {
        word(bytes,entry.identity.generator,4); word(bytes,entry.identity.ordinal,4);
        word(bytes,entry.identity.epoch,8); word(bytes,entry.identity.activation,8);
        word(bytes,entry.order,4);
        std::vector<std::uint8_t> state;
        if(!gateExport(*authority.at(entry.identity.generator),entry.state,state,error)||state.size()!=128)
            return fail(error,"gate checkpoint GS02 export failed");
        bytes.insert(bytes.end(),state.begin(),state.end());
    }
    std::uint8_t hash[checksum];pc_netplay_sha::sha256(bytes.data(),bytes.size(),hash);
    bytes.insert(bytes.end(),hash,hash+checksum);
    out.swap(bytes);error.clear();return true;
}
bool gateCheckpointImport(const std::string& campaign,
    const std::vector<GateRecord>& rows, const std::vector<std::uint8_t>& bytes,
    std::vector<GateCheckpointEntry>& out, std::string& error) {
    std::map<unsigned,const GateRecord*> authority;
    if(!digest(campaign)||!sources(rows,authority,error)) return fail(error,"gate checkpoint selection invalid");
    if(bytes.size()!=header+record*authority.size()+checksum
        ||std::memcmp(bytes.data(),"GCP1",4)||std::memcmp(bytes.data()+4,campaign.data(),64))
        return fail(error,"gate checkpoint envelope/source census mismatch");
    std::uint8_t hash[checksum];pc_netplay_sha::sha256(bytes.data(),bytes.size()-checksum,hash);
    if(std::memcmp(hash,bytes.data()+bytes.size()-checksum,checksum))
        return fail(error,"gate checkpoint checksum mismatch");
    std::size_t at=68;
    if(read(bytes,at,4)!=authority.size()) return fail(error,"gate checkpoint count mismatch");
    std::vector<GateCheckpointEntry> entries;
    for(std::size_t i=0;i<authority.size();++i) {
        GateCheckpointEntry entry;entry.identity.catalog=campaign;
        entry.identity.generator=static_cast<unsigned>(read(bytes,at,4));
        entry.identity.ordinal=static_cast<unsigned>(read(bytes,at,4));
        entry.identity.epoch=read(bytes,at,8);entry.identity.activation=read(bytes,at,8);
        entry.order=static_cast<unsigned>(read(bytes,at,4));
        const auto found=authority.find(entry.identity.generator);
        if(found==authority.end()) return fail(error,"gate checkpoint unknown source UID");
        std::vector<std::uint8_t> state(bytes.begin()+at,bytes.begin()+at+128);at+=128;
        if(!gateImport(*found->second,state,entry.state,error)) return false;
        entries.push_back(entry);
    }
    if(!validate(campaign,authority,entries,error)) return false;
    out.swap(entries);error.clear();return true;
}
}
