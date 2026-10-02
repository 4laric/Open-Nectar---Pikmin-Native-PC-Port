#include "pc_midday_economy.h"
#include "GameStat.h"
#include "BaseInf.h"
#include "pc_p2_ship_store.h"
namespace pc_midday {
bool captureEconomyComponent(Bytes& out,std::string& e){
    static_assert(PikiColorCount==3&&PikiHappaCount==3,"economy schema requires native three-color/maturity layout");
    static_assert(p2ship::Capacity==100000,"ship capacity schema changed");
    const GameStat::ColCounter* counters[]={&GameStat::deadPikis,&GameStat::fallPikis,
        &GameStat::formationPikis,&GameStat::freePikis,&GameStat::workPikis,&GameStat::mePikis,
        &GameStat::containerPikis,&GameStat::bornPikis,&GameStat::victimPikis,&GameStat::mapPikis,&GameStat::allPikis};
    EconomyFields v;
    for(unsigned i=0;i<11;++i)for(unsigned c=0;c<3;++c)v.statistics[i][c]=counters[i]->mCounts[c];
    for(unsigned c=0;c<3;++c)for(unsigned h=0;h<3;++h)v.onionStock[c][h]=pikiInfMgr.mPikiCounts[c][h];
    for(unsigned c=0;c<2;++c)for(unsigned h=0;h<3;++h)v.shipStock[c][h]=p2ship::stock.counts[c][h];
    v.killTekis=GameStat::killTekis.mCount;v.getPellets=GameStat::getPellets.mCount;
    v.minPikis=GameStat::minPikis;v.maxPikis=GameStat::maxPikis;v.orimaDead=GameStat::orimaDead;
    return encodeEconomy(v,out,e);
}
}
