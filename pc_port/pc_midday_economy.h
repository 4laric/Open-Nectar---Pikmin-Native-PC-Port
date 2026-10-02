#pragma once
#include "pc_midday_codec.h"
#include "pc_midday_restore.h"
namespace pc_midday {
// Real native layouts: three P1 color slots; Purple/White ship stock is separate.
// Component only. PlayerState/progression, actor inventories and jobs are required
// separately; this record must never alone mark StockEconomy complete.
struct EconomyFields {
    int32_t statistics[11][3]{}; // declared GameStat order, Blue/Red/Yellow
    int32_t onionStock[3][3]{}; // PikiInfMgr color x Leaf/Bud/Flower
    int32_t shipStock[2][3]{}; // Purple/White x Leaf/Bud/Flower
    int32_t killTekis=0,getPellets=0,minPikis=0,maxPikis=0;
    bool orimaDead=false;
};
bool encodeEconomy(const EconomyFields&,Bytes&,std::string&);
bool decodeEconomy(const Bytes&,EconomyFields&,std::string&);
bool captureEconomyComponent(Bytes&,std::string&);
// Decode into backend-owned staging only. Publication with other scene globals
// is a separate final transaction, not an actor restore intermediate phase.
bool prepareEconomyComponent(EconomyFields&,const Bytes&,const RestoreGate&,std::string&);
}
