#pragma once
// Watery Blowhog breath stream: visual-only layout (owner playtest 2026-09-30,
// "no visible stream of water coming out of its nose"). Engine-free so the
// emission layout is unit-tested (tools/p2_tank_stream_test.cpp).
//
// What P2 draws (native/pikmin2-research): Wtank owns efx::TWtankEffect
// (Wtank.cpp:44-112): TTankWat, four synced JPA emitters PID_TankWat_1..4 that
// chase the hoppe joint matrix, created at the breath key event and faded when
// the breath ends, with TParticleCallBack_TankFire limiting particle travel to
// mMaxDistance (= the live breath range, Wtank.cpp:131-132) and TTankWatHit
// splashes where particles land; TTankWatYodare drools at the mouth after the
// attack (startYodare, TankState.cpp:911). P1 has none of those assets, so the
// port layers P1 water particles along the same emitter ray and range.
//
// This header only places points; it never reads or writes simulation state.
namespace p2tankstream {
constexpr int   POINTS_PER_TICK = 6;    // droplets along the live range each tick
constexpr int   MAX_POINTS      = POINTS_PER_TICK + 2; // + muzzle burst + tip splash
constexpr short DROP_LIFETIME   = 7;    // generator frames (30/s): overlap into a body
constexpr short MUZZLE_LIFETIME = 8;
constexpr float MIN_RANGE       = 12.0f; // below this the stream has not left the nose
constexpr int   MAX_LIVE_GENERATORS = 220; // skip a tick rather than flood the manager

enum class Kind { Muzzle, Drop, Tip };
struct Point { Kind kind; float x, y, z; float scale; };

// Emitter origin (ox,oy,oz), unit XZ direction (dx,dz) and the live range from
// p2tankbreath::advance. Drops sit at even fractions of the range and sag with
// distance so the jet arcs a little instead of being a ruler line. The tick
// index phases the drops half a slot so successive ticks interleave.
inline int layout(float ox, float oy, float oz, float dx, float dz, float range, unsigned tick, Point* out) {
    if (!(range >= MIN_RANGE)) return 0;
    int n = 0;
    out[n++] = {Kind::Muzzle, ox, oy, oz, 1.0f};
    const float phase = (tick & 1u) ? 0.5f : 0.0f;
    for (int i = 0; i < POINTS_PER_TICK; ++i) {
        const float t = (float(i) + 0.25f + phase) / float(POINTS_PER_TICK);
        const float d = range * t;
        const float sag = 0.0012f * d * d; // gentle arc, about 4 units at 60
        out[n++] = {Kind::Drop, ox + dx * d, oy - sag, oz + dz * d, 0.9f + 0.4f * t};
    }
    // Tip splash where the range ends (TTankWatHit analogue). It lands at the
    // same sag so it stays on the jet.
    out[n++] = {Kind::Tip, ox + dx * range, oy - 0.0012f * range * range, oz + dz * range, 1.2f};
    return n;
}
} // namespace p2tankstream
