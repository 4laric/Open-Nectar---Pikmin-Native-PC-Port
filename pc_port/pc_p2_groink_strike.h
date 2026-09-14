#pragma once

#include "pc_p2_groink_hit.h"
#include "pc_p2_projectile_receiver.h"

#include <cstddef>
#include <cstdint>

// Private Groink shell-strike -> health bridge (#205 receiver half). It is the
// only new host seam between the engine-free shell classifier and the lane-20
// proxy receiver: classify the supplied sweep, then map a Bomb command onto a
// proxy InteractAttack. Damage is never re-derived here; it is exactly the
// classifier command's value.
//
// Vendored dependency (copied byte-identical, not modified):
//   pc_port/pc_p2_projectile_receiver.{h,cpp}  from native lane
//   `opencode/p2-projectiles-integration` @ be8037af5e1d50dcd0d96333d48761cf52e27f84
//     pc_p2_projectile_receiver.h   sha256 d127e060b85fa53e5f8e72fbc6de91505ac3e25d06a90791d51149c8743b0cbc
//     pc_p2_projectile_receiver.cpp sha256 4dd711b3ef71c09a86372b273cff378d8efb38b8e9b0a925178f7cadef5de56b
//   Its required type headers are vendored the same way:
//     pc_p2_cannon_stone.h          sha256 abf3ea470f80a394ba6673a1e98223251416ed69ffce5b0c39251d0eb56e1508
//     pc_p2_rock_hazard.h           sha256 a99cc889068a6f5b662e9c8b6568622010803eb991b8336c1dae837760eef4c7
//
// The receiver owns no impulse field, so the classifier's knockback vector is
// carried through the result unchanged for the host to apply separately.

struct P2GroinkStrikeInput {
    P2GroinkHitInput hit;
    std::uint64_t targetToken = 0;
    std::uint64_t attributedToken = 0;
};

struct P2GroinkStrikeResult {
    bool valid = false;
    bool insideSweep = false;
    P2GroinkHitKind kind = P2GroinkHitKind::None;
    float damage = 0.0f;
    P2GroinkVec3 impulse;
    bool applied = false;
    bool died = false;
    float health = 0.0f;
};

// Bomb -> InteractAttack with the classifier's damage applied to targetToken;
// Wind -> no receiver health change, impulse carried through; None/invalid ->
// no-op. `applied` and `died` come from the receiver's single-strike report.
P2GroinkStrikeResult p2_groink_apply_strike(P2ProjectileReceiverRegistry& registry,
                                            const P2GroinkStrikeInput& input,
                                            const P2GroinkHitCandidate& candidate);

// Host-side dedup for hosts that process one shell across many moving steps.
// It records (shellSlot, targetToken) so one shell damages a given candidate at
// most once per flight, while a different shell can still hit the same
// candidate. p2_groink_apply_strike stays pure; this tracker is the only state.
//
// Hosts consult it only after the classifier reports a non-None kind, then call
// p2_groink_apply_strike when `firstHit` returns true. `clearSlot` forgets one
// shell once its pool slot recycles so a reused slot starts a fresh flight.
class P2GroinkStrikeTracker {
public:
    static constexpr std::size_t kMaxTracked = 64;

    // Records the pair and returns true the first time it is seen; a repeated
    // identical call returns false. Bounded: if full, the pair is reported as a
    // hit but not stored (see implementation note).
    bool firstHit(std::size_t shellSlot, std::uint64_t targetToken);
    // Forgets every pair for one shell slot. Call when that shell recycles.
    void clearSlot(std::size_t shellSlot);
    void reset();

private:
    struct Entry {
        std::size_t slot = 0;
        std::uint64_t token = 0;
    };
    Entry mEntries[kMaxTracked];
    std::size_t mCount = 0;
};

// Convenience wrapper for hosts that prefer a free function over the method.
bool p2_groink_strike_first_hit(P2GroinkStrikeTracker& tracker,
                                std::size_t shellSlot,
                                std::uint64_t targetToken);
