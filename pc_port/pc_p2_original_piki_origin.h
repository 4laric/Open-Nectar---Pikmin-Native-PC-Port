#pragma once
#include <cstdint>
#include <string>
#include <vector>
class Piki;
struct OriginalPikiOrigin {
 std::string sourceKey;
 std::uint32_t recordUid=0,attempt=0;
 std::uint64_t activation=0;
 std::string catalogFingerprint;
};
struct OriginalPikiSource {std::string sourceKey;std::uint32_t uid=0,count=0;std::uint8_t species=0;};
// Full immutable Piki catalog, including currently inactive calendar members.
// Install only before births/after old scene associations have been forgotten.
bool pc_p2_original_piki_origin_install(const std::string& fingerprint,const std::vector<OriginalPikiSource>&,std::string& error);
// Called ONLY after a successful original source birth, never ordinary P1 birth.
bool pc_p2_original_piki_origin_associate_birth(Piki*,const OriginalPikiOrigin&);
bool pc_p2_original_piki_origin_query(const Piki*,OriginalPikiOrigin& out);
bool pc_p2_original_piki_origin_restore_saved(Piki*,const OriginalPikiOrigin&);
void pc_p2_original_piki_origin_forget(Piki*);
// Cave owns the authenticated selected-checkpoint/transaction proof. This is
// false outside committed teardown or scoped physical party restoration. An
// older selected SAVE is permitted: current-scene death is not global permadeath.
bool pc_p2_cave_campaign_survivor_permit(const std::string& sourceKey,std::uint32_t recordUid,std::uint32_t attempt,std::uint64_t activation,const std::string& catalogFingerprint,std::uint64_t* generation,std::uint8_t sha[32]);

// Successful canonical association notifies the party consumer transactionally.
bool pc_p2_cave_campaign_party_associate_birth(Piki*,const char*,std::uint32_t,std::uint32_t,std::uint64_t,const char*);

// Original-source logical body flags are independent of P1 FreeMode/AP access.
// Source setZikatu(true) sets both bits; recruitment clears only wild.
struct OriginalPikiBodyState {
 std::uint8_t species=0;
 bool wild=false,wasWild=false;
};
struct OriginalPikiBody {
 OriginalPikiOrigin origin;
 OriginalPikiBodyState state;
};
bool pc_p2_original_piki_body_associate_birth(Piki*,const OriginalPikiBody&);
// Unlabelled P1 / legacy origin-only associations return false unchanged.
bool pc_p2_original_piki_body_query(const Piki*,OriginalPikiBody& out);
bool pc_p2_original_piki_body_restore_saved(Piki*,const OriginalPikiBody&);
// Call only after actual accepted RGB source recruitment, never on an attempt.
bool pc_p2_original_piki_body_recruited(Piki*);
// Cave-owned authenticated selected living body, not a tuple-only ticket. All
// outputs unchanged on refusal; checkpoint proof matches the selected body.
bool pc_p2_cave_campaign_survivor_body(const std::string& sourceKey,
 std::uint32_t recordUid,std::uint32_t attempt,std::uint64_t activation,
 const std::string& catalogFingerprint,OriginalPikiBodyState& state,
 std::uint64_t* generation,std::uint8_t sha[32]);
