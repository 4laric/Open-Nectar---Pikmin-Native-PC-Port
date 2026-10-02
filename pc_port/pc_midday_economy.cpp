#include "pc_midday_economy.h"
#include <algorithm>
#include <cstring>
namespace pc_midday {
namespace {
constexpr int32_t ShipCapacity=100000;
bool valid(const EconomyFields& v,std::string& e){
    // Statistics may temporarily be negative in native update bookkeeping.
    // Preserve exact signed values; do not synthesize totals from live actors.
    for(const auto& row:v.onionStock)for(int32_t n:row)if(n<0){e="negative onion stock";return false;}
    int64_t total=0;for(const auto& row:v.shipStock)for(int32_t n:row){
        if(n<0||n>ShipCapacity){e="invalid ship stock";return false;}total+=n;
    }
    if(total>ShipCapacity){e="ship stock exceeds native capacity";return false;}
    return true;
}
void put(Bytes& b,int32_t n){uint32_t u=0;std::memcpy(&u,&n,4);for(unsigned i=0;i<4;++i)b.push_back(uint8_t(u>>(8*i)));}
int32_t get(const Bytes& b,size_t& at){uint32_t u=0;for(unsigned i=0;i<4;++i)u|=uint32_t(b[at++])<<(8*i);int32_t n;std::memcpy(&n,&u,4);return n;}
}
bool encodeEconomy(const EconomyFields& v,Bytes& out,std::string& e){
    if(!valid(v,e))return false;
    Bytes b={'P','C','E','C','O','N','0','1'};put(b,1);
    for(const auto& row:v.statistics)for(int32_t n:row)put(b,n);
    for(const auto& row:v.onionStock)for(int32_t n:row)put(b,n);
    for(const auto& row:v.shipStock)for(int32_t n:row)put(b,n);
    for(int32_t n:{v.killTekis,v.getPellets,v.minPikis,v.maxPikis})put(b,n);
    b.push_back(v.orimaDead?1:0);out.swap(b);e.clear();return true;
}
bool decodeEconomy(const Bytes& b,EconomyFields& out,std::string& e){
    const Bytes magic={'P','C','E','C','O','N','0','1'};
    if(b.size()!=221||!std::equal(magic.begin(),magic.end(),b.begin())){e="economy framing mismatch";return false;}
    size_t at=8;if(get(b,at)!=1){e="economy version mismatch";return false;}
    EconomyFields v;
    for(auto& row:v.statistics)for(int32_t& n:row)n=get(b,at);
    for(auto& row:v.onionStock)for(int32_t& n:row)n=get(b,at);
    for(auto& row:v.shipStock)for(int32_t& n:row)n=get(b,at);
    for(int32_t* n:{&v.killTekis,&v.getPellets,&v.minPikis,&v.maxPikis})*n=get(b,at);
    if(b[at]>1){e="noncanonical economy boolean";return false;}v.orimaDead=b[at]!=0;
    if(!valid(v,e))return false;
    out=v;e.clear();return true;
}
bool prepareEconomyComponent(EconomyFields& out,const Bytes& b,const RestoreGate& g,std::string& e){
    if(!g.freshProcess||!g.paused||!g.zeroInput||!g.birthEffectsSuppressed||!g.rewardsSuppressed||!g.rngDrawsSuppressed||!g.audioVoicesSuppressed){e="economy preparation requires full fresh constructor fence";return false;}
    return decodeEconomy(b,out,e);
}
}
