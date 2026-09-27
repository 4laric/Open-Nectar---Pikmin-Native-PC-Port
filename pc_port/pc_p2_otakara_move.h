#pragma once
// Engine-free OtakaraBase move decision (#884) shared by pc_p2_otakara.cpp and
// tools/p2_otakara_move_test.cpp. Encodes the P2 source decision logic
// (pikmin2-research @ 632af93787b9c95b63f0c13be32b161375ce3a96):
//   * OtakaraBase::Obj::isMovePositionSet  (OtakaraBase.cpp:361-389)
//       treasure target -> destination = treasure position (pursuit);
//       creature target -> destination = getTargetPosition(target) (escape).
//   * OtakaraBase::Obj::getTargetPosition  (OtakaraBase.cpp:424-444)
//       one moveSpeed step AWAY from the creature, projected onto the home
//       territory circle when it would leave it.
//   * EnemyFunc::getNearestPikminOrNavi    (enemyAction.cpp:746-762, :17-61, :368-400)
//       nearest navi then a strictly-closer searchable piki; view angle fp13=180
//       (full circle); ConditionNotStickClientAndItem (ConditionNotStick.h:28-46)
//       and Piki::isSearchable (Piki.h:243-251) filter the piki.
//   * StateWait/Move/Turn/Flick::exec      (OtakaraBaseState.cpp:89-136,165-199,225-269,297-334)
//       THIRD_PI facing gate, Flick then Dead override, commit at KEYEVENT_END.
//   * EnemyBase::turnToTarget              (EnemyBase.h:463-471), walkToTarget
//       (enemyAction.cpp:2102-2107).
// BombOtakara (93) chases its target (StateBombMove, OtakaraBaseState.cpp:812-848),
// so it uses Mode::Pursue and is never inverted.
#include <cmath>

namespace p2otakaramove {

struct Vec2 {
    float x, z;
};

constexpr float kPi = 3.14159265f;
constexpr float kTwoPi = 6.28318531f;
constexpr float kFacingGate = 1.04719755f; // THIRD_PI, OtakaraBaseState.cpp:125/172/232/305
constexpr float kTurnFactor = 0.25f;       // fp08 turn speed (retail)
constexpr float kMaxTurnDeg = 4.0f;        // fp28 max turn angle per source tick (retail)
constexpr float kViewAngleDeg = 180.0f;    // fp13 view angle (retail): full circle
constexpr float kSourceHz = 30.0f;         // assumed source tick rate (unverified, design 2.6)

enum class Mode { Escape, Pursue };
enum class TargetKind { None, Creature, Treasure };

struct Candidate {
    Vec2 pos;
    bool isNavi;
    bool alive;
    bool stuckToSelf;  // ConditionNotStickClientAndItem: mSticker == this dweevil
    bool stuckToMouth; // Piki::isSearchable: !isStickToMouth()
};

inline float distSqXZ(Vec2 a, Vec2 b) {
    const float dx = a.x - b.x, dz = a.z - b.z;
    return dx * dx + dz * dz;
}
inline float distXZ(Vec2 a, Vec2 b) { return std::sqrt(distSqXZ(a, b)); }

inline float wrapPi(float a) {
    while (a > kPi) a -= kTwoPi;
    while (a < -kPi) a += kTwoPi;
    return a;
}

// angXZ(dest, self) (trig.h:79-83): atan2(dx, dz).
inline float headingTo(Vec2 self, Vec2 dest) { return std::atan2(dest.x - self.x, dest.z - self.z); }

// getNearestPikminOrNavi: nearest navi within sight (strict <), then a piki must be
// strictly closer than the navi (shared targetDist). The view angle is a full circle,
// so no angle test is needed. Returns the candidate index or -1.
inline int selectThreat(const Candidate* c, int n, Vec2 self, float sight) {
    const float sightSq = sight * sight;
    float best = sightSq;
    int pick = -1;
    for (int i = 0; i < n; ++i) { // getNearestNavi (enemyAction.cpp:17-61)
        if (!c[i].isNavi || !c[i].alive) continue;
        const float d = distSqXZ(c[i].pos, self);
        if (d < best) { best = d; pick = i; }
    }
    for (int i = 0; i < n; ++i) { // getNearestPikmin (enemyAction.cpp:368-400)
        if (c[i].isNavi || !c[i].alive || c[i].stuckToMouth || c[i].stuckToSelf) continue;
        const float d = distSqXZ(c[i].pos, self);
        if (d < best) { best = d; pick = i; }
    }
    return pick;
}

// OtakaraBase::Obj::getTargetPosition (OtakaraBase.cpp:424-444). A zero separation
// normalises to zero (Vector3.h:636-648), so the destination is self.
inline Vec2 escapePosition(Vec2 self, Vec2 threat, Vec2 home, float moveSpeed, float territory,
                           bool* clamped = nullptr) {
    Vec2 sep{self.x - threat.x, self.z - threat.z};
    const float len = std::sqrt(sep.x * sep.x + sep.z * sep.z);
    if (len > 0.0f) {
        sep.x /= len;
        sep.z /= len;
    } else {
        sep = Vec2{0.0f, 0.0f};
    }
    sep.x = sep.x * moveSpeed + self.x;
    sep.z = sep.z * moveSpeed + self.z;
    bool clip = false;
    if (distSqXZ(sep, home) > territory * territory) {
        // Vector3f::getFlatDirectionFromTo(home, sep) (Vector3.h:230-235)
        Vec2 dir{sep.x - home.x, sep.z - home.z};
        const float dl = std::sqrt(dir.x * dir.x + dir.z * dir.z);
        if (dl > 0.0f) {
            dir.x /= dl;
            dir.z /= dl;
        }
        sep = Vec2{home.x + dir.x * territory, home.z + dir.z * territory};
        clip = true;
    }
    if (clamped) *clamped = clip;
    return sep;
}

// isMovePositionSet destination. Treasure -> treasure position (OtakaraBase.cpp:370-372,
// never produced at runtime: no treasure is staged); Creature+Escape -> getTargetPosition;
// Creature+Pursue -> target position (StateBombMove walkToTarget, OtakaraBaseState.cpp:823-831).
inline Vec2 movePosition(Mode mode, TargetKind kind, Vec2 self, Vec2 target, Vec2 home, float moveSpeed,
                         float territory, bool* clamped = nullptr) {
    if (clamped) *clamped = false;
    switch (kind) {
    case TargetKind::Treasure:
        return target;
    case TargetKind::Creature:
        if (mode == Mode::Pursue) return target;
        return escapePosition(self, target, home, moveSpeed, territory, clamped);
    default:
        return self;
    }
}

// |angDist(angXZ(dest, self), faceDir)| <= THIRD_PI (OtakaraBaseState.cpp:170-172, 230-232).
inline bool facingWithinGate(float heading, Vec2 self, Vec2 dest) {
    return std::fabs(wrapPi(headingTo(self, dest) - heading)) <= kFacingGate;
}

// EnemyBase::turnToTarget (EnemyBase.h:463-471) per dt: clamp(angDist*turnSpeed,
// TORADIANS(maxTurnAngle)) per source tick, converted to dt at kSourceHz.
inline float turnStep(float heading, Vec2 self, Vec2 dest, float dt) {
    const float ticks = kSourceHz * dt;
    const float angDist = wrapPi(headingTo(self, dest) - heading);
    const float factor = 1.0f - std::pow(1.0f - kTurnFactor, ticks);
    const float maxTurn = kMaxTurnDeg * (kPi / 180.0f) * ticks;
    float step = angDist * factor;
    if (step > maxTurn) step = maxTurn;
    if (step < -maxTurn) step = -maxTurn;
    return wrapPi(heading + step);
}

enum class St { Dead, Flick, Wait, Move, Turn };

struct In {
    St cur;
    bool hasTarget;
    bool facing;
    bool flick;
    bool dead;
};

// The state requested this frame (mNextState). Returning `cur` means no request.
// Source priority: target decision, then Flick, then Dead override
// (OtakaraBaseState.cpp:165-199 / 225-269 / 297-334). A no-target Wait stays Wait:
// the source Wait has no idle transition.
inline St decide(const In& in) {
    St next = in.cur;
    switch (in.cur) {
    case St::Wait:
        if (in.hasTarget) next = in.facing ? St::Move : St::Turn;
        break;
    case St::Move:
        next = in.hasTarget ? (in.facing ? St::Move : St::Turn) : St::Wait;
        break;
    case St::Turn:
        next = in.hasTarget ? (in.facing ? St::Move : St::Turn) : St::Wait;
        break;
    default:
        break;
    }
    if (in.flick) next = St::Flick;
    if (in.dead) next = St::Dead;
    return next;
}

// StateFlick KEYEVENT_END (OtakaraBaseState.cpp:115-133).
inline St afterFlick(bool dead, bool hasTarget, bool facing) {
    if (dead) return St::Dead;
    if (hasTarget) return facing ? St::Move : St::Turn;
    return St::Wait;
}

// finishMotion -> KEYEVENT_END gate: a looped clip reaches its end when stateTime
// crosses a whole multiple of the clip duration.
inline bool clipEndCrossed(float prevStateTime, float stateTime, float clipDuration) {
    if (!(clipDuration > 0.0f)) return true;
    return std::floor(stateTime / clipDuration) > std::floor(prevStateTime / clipDuration);
}

} // namespace p2otakaramove
