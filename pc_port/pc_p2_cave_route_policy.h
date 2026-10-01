#pragma once
#include "pc_p2_cave_anchor.h"
#include "pc_p2_cave_transfer.h"
#include <iomanip>

// Opt-in surface route presentation plus a live-party checkpoint. Distinct from
// cave floor entry: a surface does not instantiate a generated floor or receipts.
struct P2CaveSurfaceRoute {
    P2CaveAnchor entrance;
    P2CaveEntry party;
};

inline bool p2_cave_surface_route_read(std::istream& in, P2CaveSurfaceRoute& out) {
    std::string header, extra;
    P2CaveSurfaceRoute value;
    int count=0;
    if (!(in>>header>>value.party.token>>value.entrance.x>>value.entrance.y
          >>value.entrance.z>>value.entrance.radius>>value.party.health>>count)
        || header!="P2_CAVE_ROUTE_SURFACE_1" || !p2_cave_token_valid(value.party.token)
        || !std::isfinite(value.party.health) || value.party.health<=0 || value.party.health>1
        || count<1 || count>P2CaveMaxSurvivors) return false;
    const auto& a=value.entrance;
    if (!std::isfinite(a.x) || !std::isfinite(a.y) || !std::isfinite(a.z)
        || !std::isfinite(a.radius) || a.radius<20 || a.radius>150
        || std::fabs(a.x)>100000 || std::fabs(a.y)>100000 || std::fabs(a.z)>100000) return false;
    for(int i=0;i<count;++i) {
        P2CaveSurvivor p;
        if (!(in>>p.species>>p.maturity) || p.species<0 || p.species>2
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

inline std::string p2_cave_surface_route_transfer(const P2CaveEntry& party,
        float x,float y,float z) {
    if(!p2_cave_token_valid(party.token) || party.squad.empty()
        || party.squad.size()>P2CaveMaxSurvivors || !std::isfinite(party.health)
        || party.health<=0 || party.health>1 || !std::isfinite(x)
        || !std::isfinite(y) || !std::isfinite(z)) return {};
    std::ostringstream out;
    out<<std::setprecision(9)<<"P2_CAVE_ROUTE_TRANSFER_1\n"<<party.token<<'\n'
       <<x<<' '<<y<<' '<<z<<' '<<party.health<<' '<<party.squad.size()<<'\n';
    for(const auto& p:party.squad) {
        if(p.species<0 || p.species>2 || p.maturity<0 || p.maturity>2)return {};
        out<<p.species<<' '<<p.maturity<<'\n';
    }
    return out.str();
}
