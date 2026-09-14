#include "pc_p2_groink_strike.h"

P2GroinkStrikeResult p2_groink_apply_strike(P2ProjectileReceiverRegistry& registry,
                                            const P2GroinkStrikeInput& input,
                                            const P2GroinkHitCandidate& candidate)
{
    P2GroinkStrikeResult result;
    const P2GroinkHitCommand command = p2_groink_classify_hit(input.hit, candidate);
    result.valid = command.valid;
    result.insideSweep = command.insideSweep;
    result.kind = command.kind;
    result.damage = command.damage;
    result.impulse = command.impulse;
    if (!command.valid || command.kind == P2GroinkHitKind::None) {
        return result;
    }

    if (command.kind == P2GroinkHitKind::Bomb) {
        // The classifier's damage is authoritative; InteractAttack is the
        // source-compatible Bomb receiver entry point for this proxy.
        const P2ProjectileReceiverHit hit = registry.applyStrike(
            P2ProjectileReceiverStrikeKind::InteractAttack, command.damage,
            input.targetToken, input.attributedToken);
        result.applied = hit.applied;
        result.died = hit.died;
        result.health = hit.health;
    } else {
        // Wind: report the receiver state without mutating it. A None-kind
        // strike performs no damage and returns the current health.
        const P2ProjectileReceiverHit hit = registry.applyStrike(
            P2ProjectileReceiverStrikeKind::None, 0.0f, input.targetToken,
            input.attributedToken);
        result.health = hit.health;
    }
    return result;
}

bool P2GroinkStrikeTracker::firstHit(std::size_t shellSlot, std::uint64_t targetToken)
{
    for (std::size_t i = 0; i < mCount; ++i) {
        if (mEntries[i].slot == shellSlot && mEntries[i].token == targetToken) {
            return false;
        }
    }
    if (mCount < kMaxTracked) {
        mEntries[mCount].slot = shellSlot;
        mEntries[mCount].token = targetToken;
        ++mCount;
    }
    // Overflow is a best-effort host bound: report the strike rather than drop
    // an otherwise-valid one, at the cost of not remembering the pair.
    return true;
}

void P2GroinkStrikeTracker::clearSlot(std::size_t shellSlot)
{
    std::size_t kept = 0;
    for (std::size_t i = 0; i < mCount; ++i) {
        if (mEntries[i].slot != shellSlot) {
            mEntries[kept++] = mEntries[i];
        }
    }
    mCount = kept;
}

void P2GroinkStrikeTracker::reset()
{
    mCount = 0;
}

bool p2_groink_strike_first_hit(P2GroinkStrikeTracker& tracker,
                                std::size_t shellSlot,
                                std::uint64_t targetToken)
{
    return tracker.firstHit(shellSlot, targetToken);
}
