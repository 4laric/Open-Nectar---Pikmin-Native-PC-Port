#pragma once
#include "pc_p2_demon_attack_window.h"
#include <cmath>

// Bounded translation of SaraiState::StateCatchFly::exec. Physics and turning
// remain owned by the native actor; this only exposes the source decision order.
namespace p2demon {
struct CatchFlyInput {
    float x, y, z;
    float targetX, targetY, targetZ;
    float mapY, elapsedSeconds, grabFlightHeight, transitionHeight;
    float riseFactor, climbingFactor, grabSpeed;
    int stuckCount;
    int heightNext;
    bool targetAttached;
};
struct CatchFlyResult {
    float velocityX = 0, velocityY = 0, velocityZ = 0;
    bool finishMotion = false;
    P2DemonAttackNext next = P2DemonAttackNext::None;
};

inline CatchFlyResult catchFly(const CatchFlyInput& in)
{
    CatchFlyResult out;
    if (!std::isfinite(in.x) || !std::isfinite(in.y) || !std::isfinite(in.z) ||
        !std::isfinite(in.targetX) || !std::isfinite(in.targetY) || !std::isfinite(in.targetZ) ||
        !std::isfinite(in.mapY) || !std::isfinite(in.elapsedSeconds) || in.elapsedSeconds < 0 ||
        !std::isfinite(in.grabFlightHeight) || !std::isfinite(in.transitionHeight) ||
        !std::isfinite(in.riseFactor) || !std::isfinite(in.climbingFactor) || !std::isfinite(in.grabSpeed))
        return out;
    const float dx = in.targetX - in.x, dz = in.targetZ - in.z;
    const float distance2 = dx * dx + dz * dz;
    if (in.elapsedSeconds > 10.0f || distance2 < 625.0f) {
        out.finishMotion = true;
    } else if (in.grabSpeed > 0 && distance2 > 0) {
        const float scale = in.grabSpeed / std::sqrt(distance2);
        out.velocityX = dx * scale;
        out.velocityZ = dz * scale;
    }
    // Source checks attachment before height transition and timer increment.
    if (!in.targetAttached) { out.next = P2DemonAttackNext::Move; return out; }
    const int weight = in.stuckCount < 0 ? 0 : (in.stuckCount > 5 ? 5 : in.stuckCount);
    const float t = float(weight) / 5.0f;
    const float factor = (1.0f - t) * in.riseFactor + t * in.climbingFactor;
    out.velocityY = factor * ((in.mapY + in.grabFlightHeight) - in.y);
    const float height = in.y - in.mapY;
    if ((height > in.transitionHeight || in.elapsedSeconds > 3.0f) && in.heightNext >= 0)
        out.next = static_cast<P2DemonAttackNext>(in.heightNext);
    return out;
}
} // namespace p2demon
