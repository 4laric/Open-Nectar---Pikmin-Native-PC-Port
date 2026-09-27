#pragma once

// Kabuto 75 (Armored Cannon Beetle Larva) travelling Stone fleet (#884).
//
// Engine-free emission rule + projectile fleet for the campaign Kabuto FSM
// (pc_p2_kabuto_fsm.cpp). The per-stone lifecycle policy is the existing
// P2CannonStone (pc_p2_cannon_stone.h); this header only adds what the
// campaign needs on top of it:
//   * KEYEVENT_2 emission timing for the attack clip,
//   * the source birth point (mouth XZ, body Y + 25),
//   * a fixed pool of stones owned independently of their shooters (a Stone
//     outlives the Kabuto that fired it), with 30 Hz ticking, host trace
//     adaptation, sphere contacts against a host target snapshot, a per-stone
//     strike ledger, dead-hold, release and flight metrics.
// Host dependencies (map trace, target enumeration, receivers, logs, draw) are
// injected; nothing here touches the P1 engine.
//
// Source (projectPiki/pikmin2, read-only checkout native/pikmin2-research):
//   KabutoState.cpp:347-372   StateAttack::exec: health gate first, then
//                             KEYEVENT_2 -> createStoneAttack.
//   Kabuto.cpp:268-290        createStoneAttack: Rock manager births
//                             EnemyID_Stone at (mouth.tx, 25 + mPosition.y,
//                             mouth.tz) facing mFaceDir; mIsHoming only for
//                             Rkabuto (95), so Kabuto 75's Stone never homes.
//   Rock.cpp:204-238/244-249  collisionCallback / wallCallback.
//   Rock.cpp:298-304          ignoreAtari: source enemy ignored for 1 s.
//   RockState.cpp:229-241     Move: timer, Dead on health <= 0 or timer > 15.
//   enemyBase.cpp:1878-1893   ground simulation keeps current Y velocity and
//                             applies gravity.
//   enemyBase.cpp:2080-2089   map sphere radius = fp01 (Stone disc 25).
// Retail data (experimental/pikmin2_cannon_projectile_assets.py):
//   EXPECTED_EVENTS Kabuto 'attack' [[50, 2]] (KEYEVENT_2 at frame 50),
//   DISC_PARMS 'Stone' general fp00 99999, fp01 25, fp06 250, fp08 0.03,
//   fp12 150, fp24 10, fp28 3.0; proper fp01 100.
//   docs/PIKMIN2_CANNON_PROJECTILE_ASSETS.md:164 Rock/Stone enemycoll root r40.

#include "pc_p2_cannon_stone.h"

#include <cmath>
#include <cstdint>
#include <vector>

namespace p2kabutostone {

constexpr int kAttackKey2Frame = 50;       // Kabuto attack clip KEYEVENT_2 (retail enemyanimmgr)
constexpr float kAnimFps = 30.0f;          // enemyAnimatorBase.cpp:4,11
constexpr float kBirthYOffset = 25.0f;     // Kabuto.cpp:276 (over the Kabuto's own Y)
constexpr float kMapRadius = 25.0f;        // Stone fp01, map sphere (enemyBase.cpp:2080-2089)
constexpr float kContactRadiusFull = 40.0f; // Rock/Stone enemycoll root r40, scaled (Rock.cpp:346)
// Host approximation of the "mouth" joint forward offset: the source samples
// the joint world matrix at attack frame 50, which is not extracted locally.
// 55 = Kabuto root collision sphere radius. Logged as mouth_source=host_approx.
constexpr float kMouthForwardHostApprox = 55.0f;
// Host stand-in for the Rock dead.bca length (unverified); matches the
// arena host's hold (pc_p2_projectiles.cpp kDeadHoldSeconds).
constexpr float kDeadHoldSeconds = 0.5f;
// Host capacity; RockMgr sizes its array per stage (RockMgr.cpp:101-115),
// retail per-stage count unverified. Exhaustion is tolerated (Kabuto.cpp:283).
constexpr int kFleetCapacity = 16;

inline P2CannonStoneConfig stoneConfig()
{
    P2CannonStoneConfig c;
    c.variant = P2CannonStoneVariant::Stone;
    c.moveSpeed = 250.0f;        // fp06
    c.searchRumbleSpeed = 100.0f; // proper fp01 (homing only; unused for 75)
    c.turnSpeed = 0.03f;         // fp08
    c.maxTurnAngle = 3.0f;       // fp28
    c.attackDamage = 10.0f;      // fp24 (InteractPress)
    c.sightRadius = 150.0f;      // fp12 (homing only; unused for 75)
    c.collisionRadius = kMapRadius;
    c.health = 99999.0f;         // fp00
    return c;
}

// KEYEVENT_2 time in attack-state seconds (frame 50 at 30 fps = 1.6667 s).
inline float key2Seconds() { return static_cast<float>(kAttackKey2Frame) / kAnimFps; }

// Float tolerance for the accumulated stateTime (50 x float(1/30) sums to just
// under 5/3); 1e-4 s is 0.003 animation frames.
constexpr float kKey2Epsilon = 1.0e-4f;

// True exactly on the host frame whose accumulated attack stateTime first
// reaches the KEYEVENT_2 time.
inline bool key2Crossed(float prev, float now)
{
    const float k = key2Seconds() - kKey2Epsilon;
    return prev < k && now >= k;
}

// The staged attack clip must contain frame 50 or the event can never play.
inline bool clipHasKey2(int durationFrames) { return durationFrames > kAttackKey2Frame; }

// StateAttack::exec order (KabutoState.cpp:350-358): a Kabuto with health <= 0
// transits to Dead before the event is looked at, so it never fires.
inline bool attackMayFire(float health, bool fireDone, float prev, float now)
{
    return health > 0.0f && !fireDone && key2Crossed(prev, now);
}

// createStoneAttack birth point (Kabuto.cpp:274-277): mouth XZ, body Y + 25.
inline P2CannonStoneVec3 birthPosition(const P2CannonStoneVec3& kabutoPos, float heading,
                                       float mouthForward)
{
    return { kabutoPos.x + std::sin(heading) * mouthForward, kabutoPos.y + kBirthYOffset,
             kabutoPos.z + std::cos(heading) * mouthForward };
}

// Host target snapshot entry (one per candidate creature per source tick).
struct Target {
    std::uint64_t token = 0;
    P2CannonStoneVec3 centre;
    float radius = 0.0f;
    P2CannonStoneContactKind kind = P2CannonStoneContactKind::NaviPiki;
    bool onFloor = false;
    bool alive = false;
};

enum class DeadReason { Wall, Contact, Timeout, Invalid };

inline const char* deadReasonName(DeadReason r)
{
    switch (r) {
    case DeadReason::Wall: return "wall";
    case DeadReason::Contact: return "contact";
    case DeadReason::Timeout: return "timeout";
    default: return "invalid";
    }
}

struct Strike {
    int slot = -1;
    std::uint32_t stone = 0;
    std::uint64_t owner = 0; // live shooter token, 0 once forgotten
    std::uint64_t target = 0;
    P2CannonStoneContactKind targetKind = P2CannonStoneContactKind::NaviPiki;
    P2CannonStoneStrikeKind kind = P2CannonStoneStrikeKind::None;
    float damage = 0.0f;
    float flight = 0.0f;
    float travel = 0.0f;
};

struct DeadEvent {
    int slot = -1;
    std::uint32_t stone = 0;
    std::uint64_t owner = 0;
    DeadReason reason = DeadReason::Invalid;
    float flight = 0.0f;
    float travel = 0.0f;
    float maxLateral = 0.0f;
    float closestNaviPiki = 0.0f;
    int hits = 0;
    P2CannonStoneVec3 pos;
};

struct Released {
    int slot = -1;
    std::uint32_t stone = 0;
};

// Host map trace in the P2 base-point convention: `base` is mPosition, the
// velocity already carries the gravity-updated Y. Returns false when no trace
// was performed.
typedef bool (*TraceFn)(void* ctx, const P2CannonStoneVec3& base,
                        const P2CannonStoneVec3& velocity, float dt, float radius,
                        P2CannonStoneTraceResult& out);

class Fleet {
public:
    static constexpr int capacity() { return kFleetCapacity; }

    // Scene teardown / re-entry: drop every stone. The id counter is NOT reset
    // so stone ids stay unique for the whole process.
    void reset()
    {
        for (Slot& s : mSlots) {
            s.clear();
        }
    }

    // createStoneAttack. Kabuto 75: homing is always false. Returns the slot,
    // or -1 on exhaustion/invalid input with no state change.
    int fire(std::uint64_t owner, const P2CannonStoneVec3& birth, float faceDir,
             std::uint32_t& id)
    {
        for (int i = 0; i < kFleetCapacity; ++i) {
            Slot& s = mSlots[i];
            if (s.used) {
                continue;
            }
            const std::uint32_t next = mNextId + 1u;
            s.stone.reset(stoneConfig());
            if (!s.stone.birth(birth, faceDir, /*homing*/ false, owner, selfToken(next))) {
                s.stone.reset(stoneConfig());
                return -1;
            }
            s.clearMetrics();
            s.used = true;
            s.id = next;
            s.owner = owner;
            s.birth = birth;
            s.dirX = std::sin(s.stone.faceDir());
            s.dirZ = std::cos(s.stone.faceDir());
            mNextId = next;
            id = next;
            return i;
        }
        return -1;
    }

    // Shooter destroyed: its stones keep flying; later strikes carry owner 0.
    int forgetOwner(std::uint64_t owner)
    {
        if (!owner) {
            return 0;
        }
        int n = 0;
        for (Slot& s : mSlots) {
            if (s.used && s.owner == owner) {
                s.owner = 0;
                ++n;
            }
        }
        return n;
    }

    // One 30 Hz source tick over every used slot.
    void tick(float gravity, TraceFn trace, void* traceCtx, const Target* targets, int targetCount,
              Strike* strikes, int strikeCap, int& strikeCount, DeadEvent* deads, int deadCap,
              int& deadCount, Released* released, int releasedCap, int& releasedCount)
    {
        strikeCount = deadCount = releasedCount = 0;
        const float dt = P2CannonStone::kSourceDelta;
        for (int i = 0; i < kFleetCapacity; ++i) {
            Slot& s = mSlots[i];
            if (!s.used) {
                continue;
            }
            const P2CannonStonePhase phase = s.stone.phase();
            if (phase == P2CannonStonePhase::Killed || phase == P2CannonStonePhase::Inactive) {
                if (releasedCount < releasedCap) {
                    released[releasedCount++] = Released{ i, s.id };
                }
                s.clear();
                continue;
            }
            if (phase == P2CannonStonePhase::Dead) {
                // Dead disables atari (RockState.cpp:275-281): no contacts.
                s.deadHold += dt;
                if (s.deadHold >= kDeadHoldSeconds) {
                    s.stone.finishDeath();
                }
                continue;
            }

            // ROCK_Move.
            TraceAdapter adapter{ trace, traceCtx, gravity, &s.vy, false };
            s.stone.update(dt, P2CannonStoneTarget{}, &TraceAdapter::call, &adapter);
            updateMetrics(s);
            if (!s.stone.isAlive()) {
                DeadEvent e;
                e.slot = i;
                e.stone = s.id;
                e.owner = s.owner;
                e.reason = adapter.wall                      ? DeadReason::Wall
                    : s.stone.hasHealthZeroed()                ? DeadReason::Contact
                    : s.stone.timer() > P2CannonStone::kMoveTimeoutSeconds ? DeadReason::Timeout
                                                                       : DeadReason::Invalid;
                e.flight = s.stone.timer();
                e.travel = s.travel;
                e.maxLateral = s.maxLateral;
                e.closestNaviPiki = s.closest;
                e.hits = s.hits;
                e.pos = s.stone.position();
                if (deadCount < deadCap) {
                    deads[deadCount++] = e;
                }
                continue;
            }

            // Contacts (collisionCallback, Rock.cpp:204-238) while still Move.
            const float scale = s.stone.scale();
            const float r = kContactRadiusFull * scale;
            const P2CannonStoneVec3 p = s.stone.position();
            const P2CannonStoneVec3 c{ p.x, p.y + r, p.z };
            for (int t = 0; t < targetCount; ++t) {
                const Target& tg = targets[t];
                if (!tg.alive) {
                    continue;
                }
                const float dx = tg.centre.x - c.x, dy = tg.centre.y - c.y, dz = tg.centre.z - c.z;
                const float d = std::sqrt(dx * dx + dy * dy + dz * dz);
                const float gap = d - r - tg.radius;
                if (tg.kind == P2CannonStoneContactKind::NaviPiki && gap < s.closest) {
                    s.closest = gap;
                }
                if (gap > 0.0f || s.struck(tg.token)) {
                    continue;
                }
                const P2CannonStoneContactResult res =
                    s.stone.contact(tg.kind, tg.onFloor, false, tg.token);
                if (res.ignored) {
                    // Source grace (Rock.cpp:298-304): not a strike, not
                    // recorded, so the contact is eligible once grace ends.
                    ++mGraceIgnored;
                    continue;
                }
                if (!res.strikeEmitted) {
                    continue; // e.g. airborne Navi/Piki: no press, stone rolls on
                }
                s.ledger.push_back(tg.token);
                ++s.hits;
                if (strikeCount < strikeCap) {
                    Strike k;
                    k.slot = i;
                    k.stone = s.id;
                    k.owner = s.owner;
                    k.target = tg.token;
                    k.targetKind = tg.kind;
                    k.kind = res.strike.kind;
                    k.damage = res.strike.damage;
                    k.flight = s.stone.timer();
                    k.travel = s.travel;
                    strikes[strikeCount++] = k;
                }
            }
        }
    }

    int active() const
    {
        int n = 0;
        for (const Slot& s : mSlots) {
            n += s.used ? 1 : 0;
        }
        return n;
    }
    bool used(int slot) const { return valid(slot) && mSlots[slot].used; }
    const P2CannonStone& stone(int slot) const { return mSlots[valid(slot) ? slot : 0].stone; }
    std::uint32_t id(int slot) const { return valid(slot) ? mSlots[slot].id : 0u; }
    std::uint64_t owner(int slot) const { return valid(slot) ? mSlots[slot].owner : 0u; }
    float travel(int slot) const { return valid(slot) ? mSlots[slot].travel : 0.0f; }
    float maxLateral(int slot) const { return valid(slot) ? mSlots[slot].maxLateral : 0.0f; }
    int hits(int slot) const { return valid(slot) ? mSlots[slot].hits : 0; }
    // Contacts suppressed by the source-enemy grace (diagnostic counter).
    std::uint64_t graceIgnored() const { return mGraceIgnored; }
    int ownedBy(std::uint64_t owner) const
    {
        int n = 0;
        for (const Slot& s : mSlots) {
            n += (s.used && owner && s.owner == owner) ? 1 : 0;
        }
        return n;
    }

private:
    struct Slot {
        bool used = false;
        P2CannonStone stone;
        std::uint32_t id = 0;
        std::uint64_t owner = 0;
        P2CannonStoneVec3 birth;
        float dirX = 0.0f, dirZ = 1.0f;
        float vy = 0.0f;
        float deadHold = 0.0f;
        float travel = 0.0f;
        float maxLateral = 0.0f;
        float closest = 1.0e30f;
        int hits = 0;
        std::vector<std::uint64_t> ledger;

        void clearMetrics()
        {
            vy = 0.0f;
            deadHold = 0.0f;
            travel = 0.0f;
            maxLateral = 0.0f;
            closest = 1.0e30f;
            hits = 0;
            ledger.clear();
        }
        void clear()
        {
            used = false;
            stone.reset(stoneConfig());
            id = 0;
            owner = 0;
            clearMetrics();
        }
        bool struck(std::uint64_t token) const
        {
            for (std::uint64_t t : ledger) {
                if (t == token) {
                    return true;
                }
            }
            return false;
        }
    };

    // Adapts the policy's (position, targetVelocity) trace call to the host:
    // horizontal velocity follows the target, Y keeps the current velocity and
    // takes gravity (enemyBase.cpp:1878-1893).
    struct TraceAdapter {
        TraceFn host;
        void* ctx;
        float gravity;
        float* vy;
        bool wall;
        static bool call(void* context, const P2CannonStoneVec3& base,
                         const P2CannonStoneVec3& targetVelocity, float dt, float radius,
                         P2CannonStoneTraceResult& out)
        {
            TraceAdapter& a = *static_cast<TraceAdapter*>(context);
            if (!a.host) {
                return false;
            }
            const P2CannonStoneVec3 vel{ targetVelocity.x, *a.vy - a.gravity * dt, targetVelocity.z };
            if (!a.host(a.ctx, base, vel, dt, radius, out)) {
                return false;
            }
            *a.vy = out.velocity.y;
            a.wall = a.wall || out.wall;
            return true;
        }
    };

    static std::uint64_t selfToken(std::uint32_t id)
    {
        // Distinct from any creature address token (top bit set).
        return (std::uint64_t(1) << 63) | id;
    }
    static bool valid(int slot) { return slot >= 0 && slot < kFleetCapacity; }

    static void updateMetrics(Slot& s)
    {
        const P2CannonStoneVec3& p = s.stone.position();
        const float dx = p.x - s.birth.x, dz = p.z - s.birth.z;
        s.travel = std::sqrt(dx * dx + dz * dz);
        const float lateral = std::fabs(dx * s.dirZ - dz * s.dirX);
        if (lateral > s.maxLateral) {
            s.maxLateral = lateral;
        }
    }

    Slot mSlots[kFleetCapacity];
    std::uint32_t mNextId = 0;
    std::uint64_t mGraceIgnored = 0;
};

} // namespace p2kabutostone
