#pragma once
#include "pc_p2_cave_anchor.h"
#include "pc_p2_cave_transfer.h"
#include <iomanip>

// Opt-in surface route presentation plus a live-party checkpoint. Distinct from
// cave floor entry: a surface does not instantiate a generated floor or receipts.
struct P2CaveSurfaceRoute {
    int wireVersion=1; // Custom surface1 stays base-only; surface2 adds real P/W.
    P2CaveAnchor entrance;
    P2CaveEntry party;
};

inline bool p2_cave_surface_route_read(std::istream& in, P2CaveSurfaceRoute& out) {
    std::string header, extra;
    P2CaveSurfaceRoute value;
    int count=0;
    if (!(in>>header>>value.party.token>>value.entrance.x>>value.entrance.y
          >>value.entrance.z>>value.entrance.radius>>value.party.health>>count)
        || (header!="P2_CAVE_ROUTE_SURFACE_1" && header!="P2_CAVE_ROUTE_SURFACE_2") || !p2_cave_token_valid(value.party.token)
        || !std::isfinite(value.party.health) || value.party.health<=0 || value.party.health>1
        || count<1 || count>P2CaveMaxSurvivors) return false;
    const auto& a=value.entrance;
    if (!std::isfinite(a.x) || !std::isfinite(a.y) || !std::isfinite(a.z)
        || !std::isfinite(a.radius) || a.radius<20 || a.radius>150
        || std::fabs(a.x)>100000 || std::fabs(a.y)>100000 || std::fabs(a.z)>100000) return false;
    value.wireVersion=header=="P2_CAVE_ROUTE_SURFACE_2"?2:1;
    const int maxSpecies=value.wireVersion==2?P2SpeciesWhite:P2SpeciesYellow;
    for(int i=0;i<count;++i) {
        P2CaveSurvivor p;
        if (!(in>>p.species>>p.maturity) || p.species<0 || p.species>maxSpecies
            || p.maturity<0 || p.maturity>2) return false;
        value.party.squad.push_back(p);
    }
    if ((in>>extra) || !in.eof()) return false;
    value.entrance.enabled=true;value.entrance.kind="hole";
    value.party.schema=2;value.party.floor=1;
    out=value;return true;
}

inline bool p2_cave_surface_route_eligible(const P2CaveAnchor& entrance,
        float x,float y,float z,bool walking,bool paused,bool ui,bool movie,
        bool dayEnd,bool online,float health) {
    return entrance.contains(x,y,z) && walking && !paused && !ui && !movie
        && !dayEnd && !online && std::isfinite(health) && health>1;
}

// Ordinary White acquisition can upgrade a bounded forest entry from wire1.
// Incoming wire1 still rejects White; the existing live writer promotes to2.
// Other profiles and Bulbmin retain their prior admission behavior.
inline bool p2_cave_bounded_live_species_supported(int entrySchema,int species,
        bool boundedForest,bool whiteAssets) {
    return p2_schema_supports(entrySchema,species)
        || (boundedForest && whiteAssets && entrySchema==P2SpeciesSchemaPurple
            && species==P2SpeciesWhite);
}

inline std::string p2_cave_surface_route_transfer(const P2CaveEntry& party,
        float x,float y,float z,int wireVersion=1) {
    if(!p2_cave_token_valid(party.token) || party.squad.empty()
        || party.squad.size()>P2CaveMaxSurvivors || !std::isfinite(party.health)
        || party.health<=0 || party.health>1 || !std::isfinite(x)
        || !std::isfinite(y) || !std::isfinite(z) || (wireVersion!=1 && wireVersion!=2)) return {};
    std::ostringstream out;
    out<<std::setprecision(9)<<"P2_CAVE_ROUTE_TRANSFER_"<<wireVersion<<'\n'<<party.token<<'\n'
       <<x<<' '<<y<<' '<<z<<' '<<party.health<<' '<<party.squad.size()<<'\n';
    for(const auto& p:party.squad) {
        if(p.species<0 || p.species>(wireVersion==2?P2SpeciesWhite:P2SpeciesYellow) || p.maturity<0 || p.maturity>2)return {};
        out<<p.species<<' '<<p.maturity<<'\n';
    }
    return out.str();
}
