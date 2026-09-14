#pragma once
#include "pc_p2_bulbmin_policy.h"
#include "pc_p2_captain_policy.h"
#include <cstddef>
#include <cstdint>
#include <istream>
#include <string>
#include <vector>

// P2 Bulbmin engine bridge (#131), lane 11 of
// docs/PIKMIN2_IMPLEMENTATION_FANOUT.md.
//
// The header-only contract in pc_p2_bulbmin_policy.h owns the dependent ledger
// and recruitment/cave rules. This module is the boundary the live engine
// calls into: it parses the opt-in config, binds one Mother Bulbmin
// (LeafChappy) epoch, records the dependents the birth path hands it, converts
// a whistled dependent in place and (when a captain ownership table is bound)
// hands it to the captain/squad path from pc_p2_captain_policy.h.
//
// Opt-in only. With no `p2-bulbmin.txt` and no PIKMIN_P2_BULBMIN environment
// value the engine entrypoints are a no-op, so the module is inert by default.
//
// Source basis (native/pikmin2-research, US GPVE01 revision 0):
//   src/plugProjectNishimuraU/LeafChappy.cpp:131-152
//       birthChildren(): 10 pikiMgr->birth(), initArg.mLeader = mother,
//       positions at 2.5*i+17.5 units behind the mother
//   src/plugProjectKandoU/interactPiki.cpp:178-179
//       captain whistle resets IsWildBulbmin
//   src/plugProjectKandoU/pikiMgr.cpp:722-723,762
//       cave exit drops all Bulbmin, a floor descent keeps only isPikmin()
//   src/plugProjectKandoU/piki.cpp:155-156
//       changeShape(Bulbmin) + setFPFlag(FPFLAGS_IsWildBulbmin)

// Configuration body:
//   P2_BULBMIN_1 <mother_epoch> <dependents>
// `mother_epoch` is a nonzero decimal id identifying the LeafChappy instance;
// `dependents` is the source ten-dependent bound (1..P2BULBMIN_MAX_DEPENDENTS).
// Unknown headers, missing fields, non-numeric tokens, a zero epoch, an
// out-of-range dependent count or trailing data are all rejected.
struct P2BulbminConfig {
    std::uint64_t motherEpoch = 0;
    int maxDependents = P2BULBMIN_MAX_DEPENDENTS;
};

bool p2_bulbmin_read(std::istream& in, P2BulbminConfig& out);

// Engine-free bridge core. The unit test drives this directly; the engine
// translation unit owns one instance and maps live Piki pointers onto ids.
class P2BulbminBridge {
    P2BulbminFlock flock;
    P2BulbminLeader mother;
    P2CaptainOwnershipTable* captains = nullptr;
    P2BulbminConfig config;
    bool active = false;
    int dependents = 0;

public:
    void reset();
    // Returns false for an epoch-zero or over-cap config, leaving it inert.
    bool setup(const P2BulbminConfig& cfg, P2CaptainOwnershipTable* ownership = nullptr);
    void bindCaptains(P2CaptainOwnershipTable* ownership) { captains = ownership; }
    bool enabled() const { return active; }
    const P2BulbminConfig& settings() const { return config; }
    int dependentCount() const { return dependents; }

    // Birth entry for the configured mother epoch (source birthChildren one
    // iteration). Refused when inert, already at the configured cap, or the
    // dependent was already born.
    P2BulbminCommand birth(std::uint32_t id);
    // Whistle/recruit entry. Converts the body in place; when an ownership
    // table is bound and `captain` is valid the recruited Piki is claimed by
    // that captain. Pass P2CaptainInvalid to skip the handoff.
    P2BulbminCommand whistle(std::uint32_t id, int captain = P2CaptainInvalid);
    // Mother death releases only wild dependents.
    std::vector<std::uint32_t> leaderDied();
    // Drop one body (host Piki destroyed / slot reuse).
    bool forget(std::uint32_t id);
    // Cave-transition filter (pikiMgr save filter).
    P2BulbminTransitionOut transition(P2BulbminCaveTransition move);

    std::size_t size() const { return flock.size(); }
    std::size_t wildCount() const { return flock.wildCount(); }
    std::size_t recruitedCount() const { return flock.recruitedCount(); }
    int phaseOf(std::uint32_t id) const { return flock.phaseOf(id); }
};

inline bool p2_bulbmin_read(std::istream& in, P2BulbminConfig& out) {
    out = P2BulbminConfig{};
    std::string header, epoch, dependents, extra;
    if (!(in >> header >> epoch >> dependents)) return false;
    if (header != "P2_BULBMIN_1") return false;
    if (in >> extra) return false; // reject trailing data
    if (epoch.empty() || epoch.size() > 20
        || epoch.find_first_not_of("0123456789") != std::string::npos)
        return false;
    if (dependents.empty() || dependents.size() > 2
        || dependents.find_first_not_of("0123456789") != std::string::npos)
        return false;
    unsigned long long mother = 0;
    int cap = 0;
    try {
        mother = std::stoull(epoch);
        cap = std::stoi(dependents);
    } catch (...) {
        return false;
    }
    if (mother == 0 || cap < 1 || cap > P2BULBMIN_MAX_DEPENDENTS) return false;
    out.motherEpoch = static_cast<std::uint64_t>(mother);
    out.maxDependents = cap;
    return true;
}

inline void P2BulbminBridge::reset() {
    if (active) mother.cancel();
    flock = P2BulbminFlock{};
    mother = P2BulbminLeader{};
    captains = nullptr;
    config = P2BulbminConfig{};
    active = false;
    dependents = 0;
}

inline bool P2BulbminBridge::setup(const P2BulbminConfig& cfg,
                                   P2CaptainOwnershipTable* ownership) {
    reset();
    if (cfg.motherEpoch == 0 || cfg.maxDependents < 1
        || cfg.maxDependents > P2BULBMIN_MAX_DEPENDENTS)
        return false;
    if (!mother.bind(&flock, cfg.motherEpoch)) return false;
    config = cfg;
    captains = ownership;
    active = true;
    return true;
}

inline P2BulbminCommand P2BulbminBridge::birth(std::uint32_t id) {
    P2BulbminCommand out;
    if (!active || dependents >= config.maxDependents) return out;
    out = mother.birth(config.motherEpoch, id);
    if (out.accepted) ++dependents;
    return out;
}

inline P2BulbminCommand P2BulbminBridge::whistle(std::uint32_t id, int captain) {
    P2BulbminCommand out;
    if (!active) return out;
    out = mother.whistle(config.motherEpoch, id);
    if (out.accepted && captains && P2CaptainOwnershipTable::isCaptain(captain))
        captains->tryClaim(id, captain);
    return out;
}

inline std::vector<std::uint32_t> P2BulbminBridge::leaderDied() {
    if (!active) return {};
    dependents = 0;
    return mother.leaderDied(config.motherEpoch);
}

inline bool P2BulbminBridge::forget(std::uint32_t id) {
    if (!active) return false;
    if (!flock.remove(id, config.motherEpoch)) return false;
    if (dependents > 0) --dependents;
    return true;
}

inline P2BulbminTransitionOut P2BulbminBridge::transition(P2BulbminCaveTransition move) {
    if (!active) return P2BulbminTransitionOut{};
    return flock.applyTransition(move);
}

class Piki;
class Creature;

// Live engine bridge. No-op unless opted in.
void pc_p2_bulbmin_setup();
void pc_p2_bulbmin_reset();
bool pc_p2_bulbmin_active();

// Birth entry: records a live dependent Piki for the configured mother epoch
// and marks its species. Returns false when inert or refused by the ledger.
bool pc_p2_bulbmin_birth(Piki* bulbmin);
// Source LeafChappy::birthChildren: birth a body through pikiMgr, bind it to
// `leader` and the configured epoch, and place it behind the mother. Returns
// nullptr when there is no live manager/scene. Compile-backed only: no
// LeafChappy actor calls it yet (see the contract doc).
Piki* pc_p2_bulbmin_birth_dependent(Creature* leader, const struct Vector3f& motherPos,
                                    float faceDir, int index);
// Whistle/recruit entry: converts the body in place and reassigns captain
// ownership at the engine level. Returns false when inert or not a dependent.
bool pc_p2_bulbmin_whistle(Piki* bulbmin);
// Drop a destroyed dependent.
void pc_p2_bulbmin_forget(Piki* piki);
// Apply the cave save filter, returning the bodies that must not be saved.
std::vector<std::uint32_t> pc_p2_bulbmin_transition(P2BulbminCaveTransition move);
// Optional handoff target for whistle; another lane can bind its captain
// ownership table so recruited Bulbmin join the squad.
void pc_p2_bulbmin_bind_captain_table(P2CaptainOwnershipTable* ownership);
