// Isolated fixtures for pc_p2_chappy_mouth.h (Chappy-family mouth-slot eating, #884).
// Build: g++ -std=gnu++17 -Wall -Wextra -Werror -Ipc_port tools/p2_chappy_mouth_test.cpp -o p2_chappy_mouth_test.exe
//
// Exercises the real runtime decision code (p2chappymouth::eat, slotWorld,
// eligible, hostPartIndex, profileForSource) against the source
// EnemyFunc::eatPikmin rules (enemyAction.cpp:1107-1142, 2113-2122). T14 keeps
// a verbatim engine-free transcription of the pre-#884 selection
// (pc_p2_chappy.cpp nearestEdiblePiki + doEat) to show the same scenes tell
// the two behaviours apart.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include "pc_p2_chappy_mouth.h"
#include "pc_p2_chappy_policy.h"

using namespace p2chappymouth;

static int gChecks = 0;
static void require(bool ok, const char* what)
{
    ++gChecks;
    if (!ok) {
        std::printf("FAIL p2_chappy_mouth_test: %s\n", what);
        std::fflush(stdout);
        std::_Exit(1);
    }
}

namespace {

constexpr float PI_F = 3.14159265f;
constexpr unsigned kAllSources[] = {2, 33, 35, 43, 53, 67, 76, 44};

constexpr const Profile* findProfile(unsigned source)
{
    for (const Profile& p : kProfiles) {
        if (p.source == source) return &p;
    }
    return nullptr;
}

// Source key-event frames must match the FSM constants that fire the bite.
static_assert(findProfile(2)->firstFrame == p2chappy::AttackBiteFrame, "Chappy eat frame");
static_assert(findProfile(33)->firstFrame == p2chappy::AttackBiteFrame, "FireChappy eat frame");
static_assert(findProfile(43)->firstFrame == p2chappy::AttackBiteFrame, "YellowChappy eat frame");
static_assert(findProfile(35)->firstFrame == p2chappy::AttackBiteFrame, "KumaChappy eat frame");
static_assert(findProfile(67)->firstFrame == p2chappy::AttackBiteFrame, "LeafChappy eat frame");
static_assert(findProfile(76)->firstFrame == p2chappy::DwarfAttackEatFrame, "KumaKochappy eat frame");
static_assert(findProfile(44)->firstFrame == p2chappy::DwarfAttackEatFrame, "BlueKochappy eat frame");
static_assert(findProfile(53)->firstFrame == 40 && findProfile(53)->lastFrame == 94, "King window 40..94");
static_assert(findProfile(42) == nullptr, "BlueChappy has no eat path");

Prey pikmin(const Vec3& pos)
{
    Prey p{};
    p.pos = pos;
    p.alive = true;
    p.visible = true;
    return p;
}

struct Capture {
    int prey;
    int slot;
};

// Runs one eat pass; `accept` scripts the receiver result per call.
struct Scene {
    std::vector<Prey> prey;
    bool occupied[MaxSlots] = {};
    std::vector<Capture> calls;
    std::vector<Capture> captured;
    std::vector<bool> accept; // per call; default true

    int run(const Profile& p, int frame, const Vec3& actor, float heading)
    {
        return eat(p, frame, actor, heading, prey.data(), (int)prey.size(), occupied, [&](int n, int slot) {
            const bool ok = calls.size() < accept.size() ? accept[calls.size()] : true;
            calls.push_back(Capture{n, slot});
            if (ok) captured.push_back(Capture{n, slot});
            return ok;
        });
    }
    bool stimulated(int n) const
    {
        for (const Capture& c : calls) {
            if (c.prey == n) return true;
        }
        return false;
    }
};

Vec3 add(const Vec3& a, const Vec3& b)
{
    return Vec3{a.x + b.x, a.y + b.y, a.z + b.z};
}

// ---- T14 legacy oracle: verbatim pre-#884 selection (engine-free) ---------
// pc_p2_chappy.cpp:193-211 nearestEdiblePiki: nearest alive Pikmin not stuck to
// a mouth or to anything, within `radius` (attackHitRange 80) of the feet, 3D.
int legacyNearestEdible(const std::vector<Prey>& prey, const Vec3& center, float radius)
{
    int best = -1;
    float bestSq = radius * radius;
    for (int n = 0; n < (int)prey.size(); ++n) {
        const Prey& p = prey[n];
        if (!p.alive) continue;
        if (p.stuckToAnyMouth || p.stuckToAny) continue;
        const float dx = p.pos.x - center.x, dy = p.pos.y - center.y, dz = p.pos.z - center.z;
        const float d = dx * dx + dy * dy + dz * dz;
        if (d < bestSq) {
            bestSq = d;
            best = n;
        }
    }
    return best;
}

struct LegacyResult {
    int prey = -1;
    bool nullPartKill = false; // InteractSwallow(actor, nullptr) -> immediate kill
    int hostSlot = -1;
};

// pc_p2_chappy.cpp:437-445 doEat: one prey; host getFreeSlot() or the null part.
LegacyResult legacyDoEat(const std::vector<Prey>& prey, const Vec3& center, const bool* hostOccupied,
                         int hostCount)
{
    LegacyResult r;
    r.prey = legacyNearestEdible(prey, center, 80.0f);
    if (r.prey < 0) return r;
    for (int i = 0; i < hostCount; ++i) {
        if (!hostOccupied[i]) {
            r.hostSlot = i;
            return r;
        }
    }
    r.nullPartKill = true;
    return r;
}

} // namespace

int main()
{
    const Profile& chappy = *profileForSource(2);
    const Profile& king = *profileForSource(53);
    const Profile& kumako = *profileForSource(76);
    const Vec3 origin{0.0f, 0.0f, 0.0f};

    // --- T1: rear Pikmin nearest and first in manager order; front at kamu3 ---
    {
        Scene sc;
        sc.prey.push_back(pikmin(Vec3{0.0f, 0.0f, -20.0f}));        // B: rear, 20 away
        sc.prey.push_back(pikmin(slotWorld(chappy, 10, 2, origin, 0.0f))); // A: at kamu3
        const int n = sc.run(chappy, chappy.firstFrame, origin, 0.0f);
        require(!sc.stimulated(0), "T1 rear nearest Pikmin is not captured");
        require(n == 1 && sc.captured.size() == 1, "T1 exactly one capture");
        require(sc.captured[0].prey == 1, "T1 front Pikmin captured");
        require(sc.captured[0].slot == 0, "T1 front Pikmin takes first empty in-reach slot (kamu1)");
        require(sc.occupied[0] && !sc.occupied[1] && !sc.occupied[2], "T1 only slot 0 occupied");
    }

    // --- T2: heading/position invariance ---
    {
        const float headings[] = {PI_F * 0.5f, PI_F, -PI_F * 0.5f, 0.7f};
        const Vec3 actors[] = {origin, Vec3{120.0f, 7.5f, -340.0f}};
        for (const Vec3& actor : actors) {
            for (float h : headings) {
                Scene sc;
                sc.prey.push_back(pikmin(localToWorld(actor, h, Vec3{0.0f, 0.0f, -20.0f})));
                sc.prey.push_back(pikmin(slotWorld(chappy, 10, 2, actor, h)));
                sc.run(chappy, chappy.firstFrame, actor, h);
                require(!sc.stimulated(0), "T2 rotated rear Pikmin never stimulated");
                require(sc.captured.size() == 1 && sc.captured[0].prey == 1 && sc.captured[0].slot == 0,
                        "T2 rotated front Pikmin captured into slot 0");
            }
        }
        // toLocal is the inverse of localToWorld.
        const Vec3 w = localToWorld(Vec3{5.0f, 1.0f, 9.0f}, 1.1f, Vec3{3.0f, 4.0f, 70.0f});
        const Vec3 l = toLocal(Vec3{5.0f, 1.0f, 9.0f}, 1.1f, w);
        require(std::fabs(l.x - 3.0f) < 1e-3f && std::fabs(l.y - 4.0f) < 1e-3f && std::fabs(l.z - 70.0f) < 1e-3f,
                "T2 toLocal inverts localToWorld");
        // local +z maps to (sin h, 0, cos h).
        const Vec3 f = localToWorld(origin, 0.5f, Vec3{0.0f, 0.0f, 1.0f});
        require(std::fabs(f.x - std::sin(0.5f)) < 1e-6f && std::fabs(f.z - std::cos(0.5f)) < 1e-6f,
                "T2 local +z is the facing (sin h, cos h)");
    }

    // --- T3: rear-only scenes never capture, any profile, any King frame ---
    {
        const Vec3 rearLocal[] = {{0.0f, 0.0f, 0.0f}, {30.0f, 0.0f, -20.0f}, {-30.0f, 5.0f, -40.0f},
                                  {50.0f, -3.0f, -60.0f}, {-10.0f, 0.0f, -79.0f}};
        const float headings[] = {0.0f, 1.3f, PI_F, -2.2f};
        for (unsigned src : kAllSources) {
            const Profile& p = *profileForSource(src);
            for (int frame = p.firstFrame; frame <= p.lastFrame; ++frame) {
                for (float h : headings) {
                    Scene sc;
                    for (const Vec3& l : rearLocal) sc.prey.push_back(pikmin(localToWorld(origin, h, l)));
                    const int n = sc.run(p, frame, origin, h);
                    require(n == 0 && sc.calls.empty(), "T3 rear-only Pikmin never captured");
                }
            }
        }
    }

    // --- T4: side boundary (outward -x from kamu2) ---
    {
        const Vec3 k2 = slotWorld(chappy, 10, 1, origin, 0.0f);
        Scene in;
        in.prey.push_back(pikmin(add(k2, Vec3{-34.9f, 0.0f, 0.0f})));
        in.run(chappy, 10, origin, 0.0f);
        require(in.captured.size() == 1 && in.captured[0].slot == 1, "T4 side 34.9 captured into slot 1");
        Scene at;
        const Vec3 atPos = add(k2, Vec3{-35.0f, 0.0f, 0.0f});
        at.prey.push_back(pikmin(atPos));
        at.run(chappy, 10, origin, 0.0f);
        require(distance(k2, atPos) >= 35.0f && at.calls.empty(), "T4 side 35.0 not captured");
        Scene out;
        out.prey.push_back(pikmin(add(k2, Vec3{-35.1f, 0.0f, 0.0f})));
        out.run(chappy, 10, origin, 0.0f);
        require(out.calls.empty(), "T4 side 35.1 not captured");
        // Exact boundary on exactly representable geometry: distance == radius
        // is refused (source `dist < slot->mRadius`), just inside is captured.
        static const float kExact[1][3] = {{0.0f, 16.0f, 64.0f}};
        const Profile exact{999, 1, 16.0f, 1.0f, 10, 10, kExact};
        Scene edge;
        edge.prey.push_back(pikmin(Vec3{16.0f, 16.0f, 64.0f}));
        edge.prey.push_back(pikmin(Vec3{0.0f, 0.0f, 64.0f}));
        edge.prey.push_back(pikmin(Vec3{0.0f, 16.0f, 48.0f}));
        require(eat(exact, 10, origin, 0.0f, edge.prey.data(), 3, edge.occupied, [](int, int) { return true; }) == 0,
                "T4 distance == radius is not captured (strict <)");
        Scene justIn;
        justIn.prey.push_back(pikmin(Vec3{15.5f, 16.0f, 64.0f}));
        require(eat(exact, 10, origin, 0.0f, justIn.prey.data(), 1, justIn.occupied, [](int, int) { return true; }) == 1,
                "T4 just inside radius captured");
    }

    // --- T5: height boundary (below kamu3) ---
    {
        const Vec3 k3 = slotWorld(chappy, 10, 2, origin, 0.0f);
        Scene in;
        in.prey.push_back(pikmin(add(k3, Vec3{0.0f, -34.9f, 0.0f})));
        in.run(chappy, 10, origin, 0.0f);
        require(in.captured.size() == 1 && in.captured[0].slot == 2, "T5 height 34.9 captured into slot 2");
        Scene out;
        out.prey.push_back(pikmin(add(k3, Vec3{0.0f, -35.1f, 0.0f})));
        out.run(chappy, 10, origin, 0.0f);
        require(out.calls.empty(), "T5 height 35.1 not captured");
    }

    // --- T6: capacity per species ---
    {
        struct Expect {
            unsigned source;
            int slots;
            int frame;
        };
        const Expect rows[] = {{2, 5, 10}, {33, 5, 10}, {35, 5, 10}, {43, 5, 10}, {67, 3, 10},
                               {76, 1, 8},  {44, 1, 8},  {53, 9, 45}};
        for (const Expect& e : rows) {
            const Profile& p = *profileForSource(e.source);
            require(p.slots == e.slots, "T6 source slot count");
            Scene sc;
            for (int i = 0; i < p.slots; ++i) sc.prey.push_back(pikmin(slotWorld(p, e.frame, i, origin, 0.0f)));
            while (sc.prey.size() < 12) sc.prey.push_back(pikmin(slotWorld(p, e.frame, 0, origin, 0.0f)));
            const int n = sc.run(p, e.frame, origin, 0.0f);
            require(n == e.slots, "T6 capacity equals the source slot count");
            int occ = 0;
            for (int i = 0; i < MaxSlots; ++i) occ += sc.occupied[i] ? 1 : 0;
            require(occ == e.slots, "T6 never more than slots occupied");
            bool distinct = true;
            for (size_t a = 0; a < sc.captured.size(); ++a)
                for (size_t b = a + 1; b < sc.captured.size(); ++b)
                    if (sc.captured[a].slot == sc.captured[b].slot) distinct = false;
            require(distinct, "T6 one Pikmin per slot");
            // A second bite with every slot full captures nothing (no overflow).
            Scene again;
            again.prey = sc.prey;
            for (int i = 0; i < p.slots; ++i) again.occupied[i] = true;
            require(again.run(p, e.frame, origin, 0.0f) == 0 && again.calls.empty(), "T6 full mouth never overflows");
        }
    }

    // --- T7: occupied slots ---
    {
        const Vec3 k1 = slotWorld(chappy, 10, 0, origin, 0.0f);
        const Vec3 k2 = slotWorld(chappy, 10, 1, origin, 0.0f);
        Scene a;
        for (int i = 0; i < 5; ++i) a.occupied[i] = true;
        a.prey.push_back(pikmin(k1));
        require(a.run(chappy, 10, origin, 0.0f) == 0 && a.calls.empty(), "T7a all occupied captures nothing");
        Scene b;
        for (int i = 1; i < 5; ++i) b.occupied[i] = true;
        b.prey.push_back(pikmin(k1));
        require(b.run(chappy, 10, origin, 0.0f) == 1 && b.captured[0].slot == 0, "T7b free slot 0 captures");
        Scene c;
        for (int i = 1; i < 5; ++i) c.occupied[i] = true;
        c.prey.push_back(pikmin(add(k2, Vec3{-34.9f, 0.0f, 0.0f})));
        require(c.run(chappy, 10, origin, 0.0f) == 0 && c.calls.empty(),
                "T7c free slot out of reach: no capture, no overflow");
        // Occupied slot skipped, later in-reach slot used for the same prey.
        Scene d;
        d.occupied[0] = true;
        d.prey.push_back(pikmin(slotWorld(chappy, 10, 2, origin, 0.0f)));
        require(d.run(chappy, 10, origin, 0.0f) == 1 && d.captured[0].slot != 0, "T7d occupied slot skipped");
    }

    // --- T8: failed stimulate leaves the slot free; the prey is not retried ---
    {
        Scene sc;
        const Vec3 k1 = slotWorld(chappy, 10, 0, origin, 0.0f);
        sc.prey.push_back(pikmin(k1));
        sc.prey.push_back(pikmin(k1));
        sc.accept = {false, true};
        const int n = sc.run(chappy, 10, origin, 0.0f);
        require(n == 1, "T8 one capture after a refused stimulate");
        require(sc.calls.size() == 2 && sc.calls[0].prey == 0 && sc.calls[1].prey == 1, "T8 failed prey not retried");
        require(sc.calls[0].slot == 0 && sc.calls[1].slot == 0, "T8 refused slot stays free for the next prey");
        require(sc.occupied[0], "T8 slot occupied only on success");
    }

    // --- T9: attached / ineligible prey ---
    {
        const Vec3 k3 = slotWorld(chappy, 10, 2, origin, 0.0f);
        Prey self = pikmin(k3);
        self.stuckToSelf = true;
        self.stuckToAny = true;
        Prey mouth = pikmin(k3);
        mouth.stuckToAnyMouth = true;
        mouth.stuckToAny = true;
        Prey dead = pikmin(k3);
        dead.alive = false;
        Prey hidden = pikmin(k3);
        hidden.visible = false;
        Prey buried = pikmin(k3);
        buried.buried = true;
        const Prey refused[] = {self, mouth, dead, hidden, buried};
        for (const Prey& p : refused) {
            require(!eligible(p), "T9 ineligible prey");
            Scene sc;
            sc.prey.push_back(p);
            require(sc.run(chappy, 10, origin, 0.0f) == 0 && sc.calls.empty(), "T9 ineligible prey never stimulated");
        }
        Prey other = pikmin(k3);
        other.stuckToAny = true; // stuck to another creature: source condition accepts
        require(eligible(other), "T9 Pikmin stuck to another creature is eligible");
        Scene sc;
        sc.prey.push_back(other);
        require(sc.run(chappy, 10, origin, 0.0f) == 1, "T9 Pikmin stuck elsewhere captured");
    }

    // --- T10: host mouth mapping ---
    {
        require(hostPartIndex(0, 0) == -1 && hostPartIndex(4, 0) == -1, "T10 no host mouth -> -1");
        require(hostPartIndex(5, 3) == 2, "T10 5 % 3 maps to 2");
        require(hostPartIndex(0, 1) == 0 && hostPartIndex(8, 5) == 3 && hostPartIndex(2, 5) == 2,
                "T10 shared-part mapping");
        Scene sc;
        sc.prey.push_back(pikmin(slotWorld(chappy, 10, 0, origin, 0.0f)));
        int refusedNoHost = 0, nullRequests = 0;
        const int hostCount = 0;
        const int n = eat(chappy, 10, origin, 0.0f, sc.prey.data(), (int)sc.prey.size(), sc.occupied,
                          [&](int, int slot) {
                              const int idx = hostPartIndex(slot, hostCount);
                              if (idx < 0) {
                                  ++refusedNoHost;
                                  return false;
                              }
                              ++nullRequests; // unreachable: a part index always exists here
                              return true;
                          });
        require(n == 0 && refusedNoHost == 1 && nullRequests == 0, "T10 no host mouth refuses the capture");
        require(!sc.occupied[0], "T10 refused capture leaves the slot free");
    }

    // --- T11: King side sweep needs the whole 40..94 window ---
    {
        const Vec3 side = slotWorld(king, 75, 0, origin, 0.0f);
        require(side.x < -60.0f, "T11 frame-75 slot 0 is on the side");
        Scene only40;
        only40.prey.push_back(pikmin(side));
        require(only40.run(king, 40, origin, 0.0f) == 0, "T11 frame 40 alone does not reach the side");
        Scene sweep;
        sweep.prey.push_back(pikmin(side));
        int captureFrame = -1;
        for (int f = king.firstFrame; f <= king.lastFrame; ++f) {
            if (sweep.run(king, f, origin, 0.0f) > 0 && captureFrame < 0) captureFrame = f;
            // Captured prey is stuck to the mouth afterwards (not eligible again).
            if (!sweep.captured.empty()) sweep.prey[0].stuckToAnyMouth = true;
        }
        require(captureFrame > 40 && captureFrame <= 75, "T11 side prey captured during the sweep");
        require(sweep.captured.size() == 1, "T11 side prey captured once");
        Scene rear;
        rear.prey.push_back(pikmin(Vec3{0.0f, 0.0f, -30.0f}));
        for (int f = king.firstFrame; f <= king.lastFrame; ++f) rear.run(king, f, origin, 0.0f);
        require(rear.calls.empty(), "T11 rear prey never captured across the window");
        require(eat(king, 39, origin, 0.0f, rear.prey.data(), 1, rear.occupied, [](int, int) { return true; }) == 0 &&
                    eat(king, 95, origin, 0.0f, rear.prey.data(), 1, rear.occupied, [](int, int) { return true; }) == 0,
                "T11 frames outside the window never eat");
    }

    // --- T12: manager order decides a contested single slot ---
    {
        const Vec3 k = slotWorld(kumako, 8, 0, origin, 0.0f);
        const Vec3 p = add(k, Vec3{2.0f, 0.0f, 0.0f});
        const Vec3 q = add(k, Vec3{-1.0f, 0.0f, 0.0f}); // nearer, but listed second
        Scene first;
        first.prey.push_back(pikmin(p));
        first.prey.push_back(pikmin(q));
        first.run(kumako, 8, origin, 0.0f);
        require(first.captured.size() == 1 && first.captured[0].prey == 0, "T12 manager-order first wins");
        Scene swapped;
        swapped.prey.push_back(pikmin(q));
        swapped.prey.push_back(pikmin(p));
        swapped.run(kumako, 8, origin, 0.0f);
        require(swapped.captured.size() == 1 && swapped.captured[0].prey == 0, "T12 reordered first wins");
    }

    // --- T13: table integrity ---
    {
        for (unsigned src : kAllSources) require(profileForSource(src) != nullptr, "T13 admitted source has a profile");
        require(profileForSource(42) == nullptr && profileForSource(0) == nullptr, "T13 no profile for others");
        struct Row {
            unsigned source;
            int slots;
            float radius;
            int first, last;
        };
        const Row rows[] = {{2, 5, 35.0f, 10, 10}, {33, 5, 35.0f, 10, 10}, {35, 5, 35.0f, 10, 10},
                            {43, 5, 35.0f, 10, 10}, {53, 9, 25.0f, 40, 94}, {67, 3, 30.0f, 10, 10},
                            {76, 1, 15.0f, 8, 8},   {44, 1, 15.0f, 8, 8}};
        for (const Row& r : rows) {
            const Profile& p = *profileForSource(r.source);
            require(p.slots == r.slots && p.radius == r.radius && p.scale == 1.0f, "T13 slots/radius/scale");
            require(p.firstFrame == r.first && p.lastFrame == r.last, "T13 frame window");
            require(p.slots <= MaxSlots, "T13 slots fit MaxSlots");
            for (int f = p.firstFrame; f <= p.lastFrame; ++f) {
                for (int i = 0; i < p.slots; ++i) {
                    const Vec3 l = slotLocal(p, f, i);
                    require(std::isfinite(l.x) && std::isfinite(l.y) && std::isfinite(l.z), "T13 finite slot");
                    require(l.z - effectiveRadius(p) > 0.0f, "T13 every slot sphere is in front of the feet");
                }
            }
        }
        require(profileForSource(2)->table == profileForSource(43)->table &&
                    profileForSource(2)->table == profileForSource(35)->table,
                "T13 Chappy/Yellow/Kuma share one table");
        require(profileForSource(76)->table == profileForSource(44)->table, "T13 KumaKo/BlueKochappy share one table");
    }

    // --- T14: legacy oracle separates old and new behaviour on the same scenes ---
    {
        std::vector<Prey> scene;
        scene.push_back(pikmin(Vec3{0.0f, 0.0f, -20.0f}));
        scene.push_back(pikmin(slotWorld(chappy, 10, 2, origin, 0.0f)));
        bool hostFree[5] = {};
        const LegacyResult old = legacyDoEat(scene, origin, hostFree, 5);
        require(old.prey == 0, "T14 legacy oracle picks the rear Pikmin");
        Scene now;
        now.prey = scene;
        now.run(chappy, 10, origin, 0.0f);
        require(!now.stimulated(0) && now.captured.size() == 1 && now.captured[0].prey == 1,
                "T14 eat() refuses the rear Pikmin the oracle takes");

        std::vector<Prey> full;
        full.push_back(pikmin(slotWorld(chappy, 10, 0, origin, 0.0f)));
        bool hostFull[5] = {true, true, true, true, true};
        const LegacyResult overflow = legacyDoEat(full, origin, hostFull, 5);
        require(overflow.prey == 0 && overflow.nullPartKill, "T14 legacy oracle overflow-kills through the null part");
        Scene noOverflow;
        noOverflow.prey = full;
        for (int i = 0; i < 5; ++i) noOverflow.occupied[i] = true;
        require(noOverflow.run(chappy, 10, origin, 0.0f) == 0 && noOverflow.calls.empty(),
                "T14 eat() never overflows a full mouth");
        // Diagnostic helpers used by the runtime log fields.
        const int legacyIdx = nearestIndex(scene.data(), (int)scene.size(), origin, 80.0f,
                                           [](const Prey& p) { return legacyEdible(p); });
        require(legacyIdx == 0 && toLocal(origin, 0.0f, scene[legacyIdx].pos).z <= 0.0f,
                "T14 legacy_would_eat_behind diagnostic flags the rear pick");
    }

    std::printf("PASS p2_chappy_mouth_test checks=%d\n", gChecks);
    return 0;
}
