#pragma once
#include "pc_p2_cave_campaign_party.h"
#include <array>
#include <cstring>

// Read-only admission against a completed, authenticated selected checkpoint.
// Source authority stays with OriginalPikiOrigin; origins alone are tombstones.
inline bool p2CaveSurvivorPermit(const P2CaveCampaignParty& party,bool scoped,
    std::uint64_t proofGeneration,std::uint64_t activeGeneration,
    const std::array<std::uint8_t,32>& digest,const std::string& sourceKey,
    std::uint32_t recordUid,std::uint32_t attempt,std::uint64_t activation,
    const std::string& catalogFingerprint,std::uint64_t* generation,std::uint8_t sha[32]){
    if(!scoped||!proofGeneration||proofGeneration!=activeGeneration
        ||!party.present||!party.resumeLiving||!party.valid()||sourceKey.empty())return false;
    bool nonzero=false;for(auto byte:digest)nonzero|=byte!=0;if(!nonzero)return false;
    const P2CavePartyBody* survivor=nullptr;
    for(const auto& body:party.bodies)if(body.sourceKey==sourceKey&&body.sourceRecord==recordUid
        &&body.sourceAttempt==attempt&&body.sourceActivation==activation&&body.catalogFingerprint==catalogFingerprint){
        if(survivor)return false;
        survivor=&body;
    }
    if(!survivor)return false;
    if(generation)*generation=proofGeneration;
    if(sha)std::memcpy(sha,digest.data(),digest.size());
    return true;
}
