#pragma once
#include "pc_p2_cave_campaign_party.h"
#include <algorithm>
// A boundary transports the captured living party, including dismissed bodies.
// Cross-map distances to the departing captain are not destination positions.
inline bool p2CavePlaceLandingParty(P2CaveCampaignParty& party,
    const std::array<P2CavePartyPoint,2>& destinations){
    if(!party.valid()||!party.present)return false;
    unsigned slots=0;
    for(const auto& captain:party.captains){
        slots|=1u<<captain.slot;if(!destinations[captain.slot].valid())return false;
    }
    auto next=party;
    for(unsigned owner=0;owner<2;++owner){
        std::vector<std::size_t> indices;
        for(std::size_t i=0;i<next.bodies.size();++i){
            const auto& b=next.bodies[i];
            if(unsigned(b.owner>=0?b.owner:next.active)==owner)indices.push_back(i);
        }
        if(indices.empty())continue;
        if(!(slots&(1u<<owner)))return false;
        std::sort(indices.begin(),indices.end(),[&](std::size_t a,std::size_t b){return next.bodies[a].key<next.bodies[b].key;});
        const unsigned columns=unsigned(std::ceil(std::sqrt(float(indices.size()))));
        const unsigned rows=(unsigned(indices.size())+columns-1)/columns;
        for(unsigned rank=0;rank<indices.size();++rank){
            auto position=destinations[owner];
            position.x+=(float(rank%columns)-float(columns-1)*.5f)*9.f;
            position.z+=(float(rank/columns)-float(rows-1)*.5f)*9.f;
            if(!position.valid())return false;
            next.bodies[indices[rank]].position=position;
        }
    }
    for(auto& captain:next.captains)captain.position=destinations[captain.slot];
    if(!next.valid())return false;
    party=std::move(next);return true;
}
