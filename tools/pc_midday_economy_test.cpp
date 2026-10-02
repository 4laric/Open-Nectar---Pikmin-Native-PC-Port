#include "pc_midday_economy.h"
#include <cstdlib>
#include <iostream>
#include <limits>
using namespace pc_midday;
int checks=0;
void check(bool ok){++checks;if(!ok){std::cerr<<"economy check failed: "<<checks<<"\n";std::exit(1);}}
Bytes bytes(const EconomyFields& v){Bytes b;std::string e;check(encodeEconomy(v,b,e));return b;}
int main(){
    EconomyFields v;std::string e;
    for(unsigned i=0;i<11;++i)for(unsigned j=0;j<3;++j)v.statistics[i][j]=int32_t(i*3+j)-17;
    v.statistics[0][0]=std::numeric_limits<int32_t>::min();v.statistics[10][2]=std::numeric_limits<int32_t>::max();
    for(unsigned c=0;c<3;++c)for(unsigned h=0;h<3;++h)v.onionStock[c][h]=int32_t(c*10+h);
    v.onionStock[2][2]=std::numeric_limits<int32_t>::max();
    v.shipStock[0][0]=40000;v.shipStock[1][2]=60000;v.killTekis=-7;v.getPellets=9;
    v.minPikis=-1;v.maxPikis=200;v.orimaDead=true;
    const Bytes b=bytes(v);check(b.size()==221);EconomyFields out;check(decodeEconomy(b,out,e));check(bytes(out)==b);
    auto refuses=[&](Bytes bad){EconomyFields sentinel;sentinel.maxPikis=1234;check(!decodeEconomy(bad,sentinel,e)&&sentinel.maxPikis==1234);};
    Bytes bad=b;bad.pop_back();refuses(bad);bad=b;bad.push_back(0);refuses(bad);
    bad=b;bad[0]='X';refuses(bad);bad=b;bad[8]=2;refuses(bad);bad=b;bad.back()=2;refuses(bad);
    bad=b;for(unsigned i=0;i<4;++i)bad[12+33*4+i]=255;refuses(bad); // negative Onion stock
    bad=b;for(unsigned i=0;i<4;++i)bad[12+42*4+i]=255;refuses(bad); // negative ship stock
    EconomyFields invalid=v;invalid.shipStock[0][1]=1;Bytes unchanged={7};check(!encodeEconomy(invalid,unchanged,e)&&unchanged==Bytes{7});
    invalid=v;invalid.shipStock[0][0]=100001;check(!encodeEconomy(invalid,unchanged,e));
    RestoreGate gate{true,true,true,true,true,true,true};EconomyFields staged;check(prepareEconomyComponent(staged,b,gate,e)&&bytes(staged)==b);
    for(unsigned i=0;i<7;++i){RestoreGate g=gate;bool* flags[]={&g.freshProcess,&g.paused,&g.zeroInput,&g.birthEffectsSuppressed,&g.rewardsSuppressed,&g.rngDrawsSuppressed,&g.audioVoicesSuppressed};*flags[i]=false;
        EconomyFields sentinel;sentinel.maxPikis=4321;check(!prepareEconomyComponent(sentinel,b,g,e)&&sentinel.maxPikis==4321);}
    std::cout<<checks<<" economy controls PASS\n";
}
