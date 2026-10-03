#pragma once
// GPVE01 r0 firechappy/enemyparm.txt
// SHA256 e448fb31f55cbb3f3d59b6a7f81ccdc2e66f7a89b1fc6376b389e2fd5dacf30c.
// ChappyBase::flickStatePikmin + enemyAction::flickNearbyPikmin/Navi.
namespace p2fireflick {
constexpr float Chance = 1.0f;
constexpr float Knockback = 120.0f;
constexpr float Damage = 1.0f;
constexpr float Range = 40.0f;
inline bool nearby(float x, float y, float z) {
    return x*x + y*y + z*z < Range*Range;
}
}
