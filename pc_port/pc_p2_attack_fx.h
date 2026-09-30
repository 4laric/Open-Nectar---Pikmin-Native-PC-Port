#pragma once
// Shared visual layouts and lifecycle for P2 attack streams (fire, water, gas,
// electric) built from P1 particle effects. Generalises the Watery Blowhog
// stream (pc_p2_tank_stream.h, native PR #51). Engine-free: this header only
// places points and counts lifecycle events; it never reads or writes
// simulation state. The engine side (EffectMgr::create / owner kill) lives in
// pc_p2_attack_fx.cpp behind p2attackfx::Emitter.
//
// Why P1 stand-ins: P2 draws these attacks with its own JPA emitters (Titan
// Dweevil: efx::TOootaFire / TOootaGas / TOootaWbShot+TOootaWbomb+TOootaWbHit /
// TOootaElecLeg+ElecAttack1/2+Phouden+Elec, BigTreasureAttack.cpp:256-363,
// 1080-1090, 1146-1150, 1213-1214, 1372, 1808, 2340-2373, 2716-2737; Tank:
// TTankFire / TTankWat, Wtank.cpp:44-112). P1 has none of those assets, so the
// port layers the closest P1 effects along the same emitter rays, node positions
// and ranges the gameplay code already computes:
//   fire   EFF_Tank_Fire (tankfire.pcr)       the P1 Tank flame (TAItank FireEffect)
//   water  EFF_Frog_Water1/2, EFF_P_Bubbles   the Wollywog/Wtank spray (PR #51)
//   gas    EFF_Kinoko_AttackCloud/Spores      the P1 Puffstool poison cloud
//   elec   EFF_Rocket_Biri (rkt_biri.pcr)     the P1 electric-spark trouble visual
// (ids are EffectMgr::effTypeTable values; pc_p2_attack_fx.cpp static_asserts
// them against the enum).
//
// Lifecycle contract (what makes "no lingering effects" testable):
//   begin(element)   on the first tick an attack is active
//   emit() ...       only while active (every emitted generator is owned by the
//                    actor's Emitter)
//   end(reason)      attack end, state change, death, forget, reset: the engine
//                    Emitter force-finishes every generator it owns, then the
//                    Session reports outstanding == 0.
namespace p2attackfx {

enum class Element { Fire = 0, Water = 1, Gas = 2, Elec = 3 };
enum class Kind { Muzzle, Body, Tip, Node, Arc };

struct Point {
    Kind kind;
    float x, y, z;
    float scale;
    float dx, dz;   // unit XZ emission direction of the stream the point belongs to
};

// EffectMgr::effTypeTable values (static_asserted in pc_p2_attack_fx.cpp).
constexpr int EFF_P_Bubbles = 15;
constexpr int EFF_Frog_Water1 = 83;
constexpr int EFF_Frog_Water2 = 84;
constexpr int EFF_Tank_Fire = 100;
constexpr int EFF_Kinoko_AttackCloud = 152;
constexpr int EFF_Kinoko_PostAttackCloud = 153;
constexpr int EFF_Kinoko_AttackSpores = 154;
constexpr int EFF_Spider_DeadBombSparks = 189;
constexpr int EFF_Rocket_Biri = 268;

struct Look {
    int effect;
    short life;   // generator frames (0 = keep the authored emission)
    bool burst;   // true: configureOneShotBurst(1, life); false: authored emission
};

inline Look look(Element e, Kind k) {
    switch (e) {
    case Element::Fire:
        if (k == Kind::Muzzle) return {EFF_Tank_Fire, 0, false};
        if (k == Kind::Body) return {EFF_Tank_Fire, 0, false};
        return {EFF_Tank_Fire, 0, false};
    case Element::Water:
        if (k == Kind::Muzzle) return {EFF_Frog_Water1, 8, true};
        if (k == Kind::Tip) return {EFF_P_Bubbles, 8, true};
        return {EFF_Frog_Water2, 7, true};
    case Element::Gas:
        if (k == Kind::Muzzle) return {EFF_Kinoko_AttackSpores, 8, true};
        if (k == Kind::Tip) return {EFF_Kinoko_PostAttackCloud, 10, true};
        return {EFF_Kinoko_AttackCloud, 9, true};
    case Element::Elec:
        if (k == Kind::Node) return {EFF_Spider_DeadBombSparks, 6, true};
        return {EFF_Rocket_Biri, 5, true};
    }
    return {EFF_P_Bubbles, 6, true};
}

inline const char* elementName(Element e) {
    switch (e) {
    case Element::Fire: return "fire";
    case Element::Water: return "water";
    case Element::Gas: return "gas";
    case Element::Elec: return "elec";
    }
    return "?";
}

constexpr float MIN_RANGE = 12.0f;
constexpr int MAX_STREAM_POINTS = 12;
constexpr int MAX_ARC_POINTS = 6;
constexpr int MAX_LIVE_GENERATORS = 220; // skip a tick rather than flood the manager

// Stream presets. `drops` interior points; `sag` world units per range^2 (the jet
// arcs a little); `emitKinds` decides whether a muzzle and a tip splash are added.
struct StreamPreset {
    int drops;
    float sag;
    float baseScale, scaleGain;
    bool muzzle, tip;
};
inline StreamPreset preset(Element e) {
    switch (e) {
    case Element::Water: return {6, 0.0012f, 0.9f, 0.4f, true, true};  // PR #51 constants
    case Element::Gas:   return {5, 0.0f, 1.0f, 1.6f, true, false};    // puffs widen with distance
    case Element::Fire:  return {0, 0.0f, 1.0f, 0.0f, true, false};    // one authored jet from the muzzle
    case Element::Elec:  return {0, 0.0f, 1.0f, 0.0f, false, false};
    }
    return {0, 0.0f, 1.0f, 0.0f, false, false};
}

// Emitter origin (ox,oy,oz), unit XZ direction (dx,dz) and the live range.
// Drops sit at even fractions of the range; the tick index phases them half a
// slot so successive ticks interleave. `scale` multiplies every point (Titan fire
// scale 1.0 / 1.25). Returns the point count (0 before the stream has left the
// emitter).
inline int layoutStream(Element e, float ox, float oy, float oz, float dx, float dz, float range, unsigned tick,
                        float scale, Point* out) {
    if (!(range >= MIN_RANGE)) return 0;
    const StreamPreset p = preset(e);
    int n = 0;
    if (p.muzzle) out[n++] = {Kind::Muzzle, ox, oy, oz, scale, dx, dz};
    const float phase = (tick & 1u) ? 0.5f : 0.0f;
    for (int i = 0; i < p.drops; ++i) {
        const float t = (float(i) + 0.25f + phase) / float(p.drops);
        const float d = range * t;
        out[n++] = {Kind::Body, ox + dx * d, oy - p.sag * d * d, oz + dz * d, (p.baseScale + p.scaleGain * t) * scale, dx, dz};
    }
    if (p.tip) out[n++] = {Kind::Tip, ox + dx * range, oy - p.sag * range * range, oz + dz * range, 1.2f * scale, dx, dz};
    return n;
}

// Electric arc between two node positions: points along the segment with a
// deterministic perpendicular zigzag so the arc crackles frame to frame.
inline unsigned hash(unsigned v) {
    v ^= v >> 16; v *= 0x7feb352dU; v ^= v >> 15; v *= 0x846ca68bU; v ^= v >> 16;
    return v;
}
inline float hashSigned(unsigned v) { return float(hash(v) & 0xffffu) / 32767.5f - 1.0f; }

inline int layoutArc(float ax, float ay, float az, float bx, float by, float bz, unsigned tick, unsigned salt,
                     float jitter, Point* out) {
    const float sx = bx - ax, sy = by - ay, sz = bz - az;
    const float len = sx * sx + sz * sz;
    if (!(len >= 1.0f)) return 0;
    const float inv = 1.0f / __builtin_sqrtf(len);
    const float px = -sz * inv, pz = sx * inv; // XZ perpendicular
    const int pieces = MAX_ARC_POINTS - 1;
    int n = 0;
    for (int i = 0; i <= pieces; ++i) {
        const float t = float(i) / float(pieces);
        const float edge = (i == 0 || i == pieces) ? 0.0f : 1.0f;
        const float j = jitter * edge * hashSigned(tick * 131u + salt * 17u + unsigned(i) * 7919u);
        const float jy = jitter * 0.5f * edge * hashSigned(tick * 197u + salt * 29u + unsigned(i) * 104729u);
        out[n++] = {i == 0 || i == pieces ? Kind::Node : Kind::Arc, ax + sx * t + px * j, ay + sy * t + jy,
                    az + sz * t + pz * j, 1.0f, px, pz};
    }
    return n;
}

// Lifecycle bookkeeping for one actor's running attack visual.
enum class EndReason { AttackEnd, StateChange, Death, Forget, Reset };
inline const char* endReasonName(EndReason r) {
    switch (r) {
    case EndReason::AttackEnd: return "attack_end";
    case EndReason::StateChange: return "state_change";
    case EndReason::Death: return "death";
    case EndReason::Forget: return "forget";
    case EndReason::Reset: return "reset";
    }
    return "?";
}

struct Session {
    bool active = false;
    Element element = Element::Fire;
    unsigned ticks = 0;
    unsigned points = 0;
    unsigned generators = 0;   // created since begin
    unsigned started = 0;      // lifetime counters (evidence)
    unsigned stopped = 0;

    bool begin(Element e) {
        if (active && element == e) return false;
        active = true;
        element = e;
        ticks = points = generators = 0;
        ++started;
        return true;
    }
    void note(unsigned pts, unsigned made) {
        ++ticks;
        points += pts;
        generators += made;
    }
    // Returns true when an active session was ended.
    bool end() {
        if (!active) return false;
        active = false;
        ++stopped;
        return true;
    }
    // Generators owned by a session that has ended must be zero; the engine
    // Emitter reports what it force-finished.
    unsigned outstanding() const { return started - stopped; }
};

} // namespace p2attackfx
