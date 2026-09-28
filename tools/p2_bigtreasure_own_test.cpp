// Engine-free regression for the Titan Dweevil campaign core
// (pc_p2_bigtreasure_own, #246). Every scenario names the defect it guards
// (the mutant it kills) against the preview-lane modules it replaces.
#include "pc_p2_bigtreasure_own.h"

#include <cmath>
#include <cstdio>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using namespace p2btown;

namespace {
int gFail = 0;
#define CHECK(cond, name)                                                         \
    do {                                                                          \
        if (!(cond)) {                                                            \
            ++gFail;                                                              \
            std::printf("FAIL %s (%s:%d)\n", name, __FILE__, __LINE__);           \
        } else {                                                                  \
            std::printf("ok   %s\n", name);                                       \
        }                                                                         \
    } while (0)

// Retail durations (bigtreasure.json source_frames) and enemyanimmgr.txt key
// events for the 29 unique clips (research import, GPVE01 rev 0).
struct ClipDef { const char* name; int frames; std::vector<std::pair<int, int>> events; };
std::vector<ClipDef> retailClips() {
    const std::vector<std::pair<int, int>> atk119 = {{0, 0}, {1, 2}, {119, 1}};
    return {
        {"appear", 210, {{26, 2}, {30, 3}, {50, 4}, {60, 5}, {82, 6}, {90, 7}, {100, 8}, {115, 9}, {191, 10}}},
        {"appear2", 18, {}},
        {"wait1", 90, {{0, 0}, {89, 1}}},
        {"preattackf", 89, {{20, 2}, {68, 0}, {82, 1}, {85, 3}}},
        {"attackf", 120, atk119},
        {"attackendf", 61, {}},
        {"preattackfr", 4, {}},
        {"attackfr", 120, atk119},
        {"attackendfr", 61, {}},
        {"preattackfl", 4, {}},
        {"attackfl", 120, atk119},
        {"attackendfl", 61, {}},
        {"preattackfb", 4, {}},
        {"attackfb", 120, atk119},
        {"attackendfb", 61, {}},
        {"preattackw", 89, {{20, 2}, {68, 0}, {82, 1}}},
        {"attackw", 30, {{0, 0}, {1, 2}, {29, 1}}},
        {"attackendw", 61, {}},
        {"preattackg", 89, {{20, 2}, {68, 0}, {82, 1}}},
        {"attackg", 80, {{0, 0}, {1, 2}, {79, 1}}},
        {"attackendg", 61, {}},
        {"preattacke", 89, {{20, 2}, {68, 0}, {82, 1}}},
        {"attacke", 40, {{0, 0}, {1, 2}, {39, 1}}},
        {"attackende", 61, {}},
        {"dropitem", 90, {{60, 2}}},
        {"wait2", 30, {{0, 0}, {29, 1}}},
        {"flick", 90, {{35, 2}}},
        {"dead", 332, {{60, 2}, {100, 3}, {125, 4}, {150, 5}, {175, 6}, {200, 7}, {290, 8}, {295, 9}, {300, 10}, {305, 11}, {320, 100}}},
        {"move1", 90, {}},
    };
}
p2retail::Table retailTable() {
    p2retail::Table t;
    t.registrySha = std::string(64, 'a');
    for (const auto& c : retailClips()) {
        p2retail::Motion m;
        m.name = std::string(c.name) + ".bca";
        m.sha = std::string(64, 'b');
        m.duration = c.frames;
        m.attribute = 0;
        for (const auto& e : c.events) m.events.push_back({e.first, e.second});
        t.motions.push_back(m);
    }
    return t;
}

const char* kParm =
    "# Creature::Property\n{\n\t{s000} 4 0.500000 \t# friction\n\t{s003} 4 0.100000 \t# accel\n\t{_eof} \n}\n"
    "# EnemyParmsBase\n{\n\t{fp00} 4 5000.000000 \t# life\n\t{fp06} 4 100.000000 \n\t{fp08} 4 0.200000 \n"
    "\t{fp28} 4 60.000000 \n\t{fp09} 4 250.000000 \n\t{fp10} 4 75.000000 \n\t{fp11} 4 100.000000 \n"
    "\t{fp12} 4 300.000000 \n\t{fp13} 4 180.000000 \n\t{fp14} 4 300.000000 \n\t{fp16} 4 1.000000 \n"
    "\t{fp17} 4 100.000000 \n\t{fp18} 4 0.000000 \n\t{fp20} 4 75.000000 \n\t{fp21} 4 25.000000 \n"
    "\t{fp24} 4 10.000000 \n\t{ip01} 4 6 \n\t{ip02} 4 5 \n\t{ip03} 4 12 \n\t{ip04} 4 10 \n"
    "\t{ip05} 4 17 \n\t{ip06} 4 20 \n\t{ip07} 4 22 \n\t{_eof} \n}\n"
    "# EnemyParmsBase\n{\n\t{fp01} 4 3.000000 \n\t{fp02} 4 -0.200000 \n\t{fp03} 4 0.500000 \n"
    "\t{fp04} 4 -2.000000 \n\t{fp05} 4 10.000000 \n\t{fp06} 4 120.000000 \n\t{fp10} 4 2.500000 \n"
    "\t{fp11} 4 2.800000 \n\t{fp31} 4 2.500000 \n\t{fp12} 4 2.500000 \n\t{fp13} 4 2.500000 \n"
    "\t{fp20} 4 5.000000 \n\t{fp21} 4 5.000000 \n\t{fp22} 4 5.000000 \n\t{fp23} 4 5.000000 \n"
    "\t{fe00} 4 0.750000 \n\t{_eof} \n}\n";

Params retailParams() {
    std::istringstream in(kParm);
    Params p;
    std::string error;
    parseEnemyParm(in, p, error);
    return p;
}

Animator retailAnimator() {
    Animator a;
    std::string error;
    a.load(retailTable(), error);
    return a;
}

struct Sim {
    Fsm fsm;
    std::vector<Candidate> cand;
    Vec3 pos{0, 0, 0};
    float health = 5000.0f;
    std::vector<State> states;
    std::vector<TickOutput> log;
    explicit Sim(std::uint32_t seed = 7u) {
        fsm.init(retailParams(), defaultBank(), retailAnimator(), pos, 0.0f, seed);
        states.push_back(fsm.state());
    }
    TickOutput step(const std::vector<Hit>& hits = {}) {
        TickInput in;
        in.position = pos;
        in.health = health;
        in.candidates = cand.data();
        in.count = cand.size();
        in.hits = hits.data();
        in.hitCount = hits.size();
        TickOutput o = fsm.tick(in);
        health -= o.bodyDamage;
        pos.x += o.velocity.x * kSourceDelta;
        pos.z += o.velocity.z * kSourceDelta;
        for (const auto& t : o.entered) states.push_back(t.to);
        return o;
    }
    // Runs until `state` is entered (or the budget ends). Returns ticks used, -1 on timeout.
    int runUntil(State state, int budget, const std::vector<Hit>& hits = {}) {
        for (int i = 0; i < budget; ++i) {
            TickOutput o = step(hits);
            log.push_back(o);
            for (const auto& t : o.entered)
                if (t.to == state) return i + 1;
        }
        return -1;
    }
    Vec3 worldAround(float angleFromFace, float r) const {
        const float a = fsm.gait().active() ? fsm.gait().faceDir() + angleFromFace : angleFromFace;
        return {pos.x + r * std::sin(a), pos.y, pos.z + r * std::cos(a)};
    }
};

void testParser() {
    std::istringstream in(kParm);
    Params p;
    std::string error;
    const bool ok = parseEnemyParm(in, p, error);
    CHECK(ok, "parm/parses_retail_layout");
    CHECK(p.health == 5000.0f && p.privateRadius == 100.0f && p.maxTurnAngle == 60.0f, "parm/general_block");
    // Block separation: proper fp21 (fire attack time 5) must not read the
    // general fp21 (attackable angle 25); proper fp11 (2.8) not general fp11 (100).
    CHECK(p.fireAttackMax == 5.0f && p.fireWait1 == 2.8f && p.fireWait2 == 2.5f, "parm/proper_block_by_content");
    CHECK(p.baseFactor == 3.0f && p.raiseDecelFactor == -0.2f && p.legSwing == 120.0f, "parm/ik_parms");
    CHECK(p.shakeOffBlowA == 6 && p.shakeOffSticking3 == 20 && p.shakeOffBlowD == 22, "parm/shake_off");
    CHECK(p.retail, "parm/retail_flag");
    std::istringstream bad("{\n\t{fp06} 4 100 \n\t{_eof}\n}\n");
    Params q;
    CHECK(!parseEnemyParm(bad, q, error), "parm/fails_without_general_block");
}

void testGait() {
    GaitParams gp; // retail proper parms are the defaults
    LegLayout legs;
    Gait g;
    g.init(gp, legs, {0, 0, 0}, 0.0f);
    CHECK(!g.active(), "gait/inactive_before_programed_ik");
    g.startProgramedIK({0, 0, 0}, 0.0f);
    CHECK(!g.isFinishIKMotion(), "gait/programed_ik_not_finished");
    g.startIKMotion();
    std::vector<int> starts;
    int prev[4] = {0, 0, 0, 0};
    int ticks = 0;
    for (; ticks < 2000; ++ticks) {
        g.update(g.centre(), g.faceDir(), {0, 0, 1000}, kSourceDelta);
        for (int l = 0; l < 4; ++l) {
            if (g.legState(l) == 1 && prev[l] != 1) starts.push_back(l);
            prev[l] = g.legState(l);
        }
        if (starts.size() >= 8) break;
    }
    CHECK(starts.size() >= 8 && starts[0] == 0 && starts[1] == 1 && starts[2] == 2 && starts[3] == 3 && starts[4] == 0,
          "gait/legs_step_in_source_order");
    CHECK(!g.isFinishIKMotion(), "gait/not_finished_while_in_motion"); // kills the always-true IK shortcut
    // One cycle moves the centre ~moveSpeed toward the target (never more).
    const float z1 = g.centre().z;
    CHECK(z1 > 60.0f && z1 < 2.0f * gp.moveSpeed + 1.0f, "gait/centre_advances_by_move_speed");
    // A leg step lasts the source moveRatio integration (~1 s at retail parms).
    CHECK(ticks > 4 * 20 && ticks < 4 * 90, "gait/step_duration_from_ik_parms");
    // finishIKMotion completes the running cycle, then reports finished.
    g.finishIKMotion();
    int finishedAt = -1;
    for (int i = 0; i < 1000; ++i) {
        g.update(g.centre(), g.faceDir(), {0, 0, 1000}, kSourceDelta);
        if (g.isFinishIKMotion()) { finishedAt = i; break; }
    }
    CHECK(finishedAt > 0, "gait/finish_completes_cycle");
    const float zDone = g.centre().z;
    for (int i = 0; i < 200; ++i) g.update(g.centre(), g.faceDir(), {0, 0, 1000}, kSourceDelta);
    CHECK(std::fabs(g.centre().z - zDone) < 1e-3f, "gait/no_new_cycle_after_finish");

    // Target behind: outside the 30 deg view the body turns in place by at
    // most maxTurnAngle (60 deg) per cycle.
    Gait t;
    t.init(gp, legs, {0, 0, 0}, 0.0f);
    t.startProgramedIK({0, 0, 0}, 0.0f);
    t.startIKMotion();
    int cycles = 0, last = 0;
    for (int i = 0; i < 2000 && cycles < 1; ++i) {
        t.update(t.centre(), t.faceDir(), {0, 0, -1000}, kSourceDelta);
        const int s = t.legState(3);
        if (s == 3 && last != 3) ++cycles;
        last = s;
    }
    const float turned = std::fabs(std::remainder(t.faceDir(), kTau));
    CHECK(turned > 0.8f && turned < 1.3f, "gait/turns_in_place_by_max_turn");
    CHECK(std::fabs(t.centre().x) < 5.0f && std::fabs(t.centre().z) < 5.0f, "gait/turn_keeps_centre");
}

void testAnimator() {
    Animator a = retailAnimator();
    CHECK(a.loaded(), "anim/loads_29_clips_30_slots");
    a.start(AnimWait2);
    int ends = 0, loops = 0;
    for (int i = 0; i < 200; ++i) {
        a.animate();
        if (a.is(KeyEnd)) ++ends;
        if (a.is(KeyLoopEnd)) ++loops;
    }
    CHECK(ends == 0 && loops >= 5, "anim/wait2_loops_without_end");
    a.finish();
    int endAt = -1;
    for (int i = 0; i < 60; ++i) {
        a.animate();
        if (a.is(KeyEnd)) { endAt = i; break; }
    }
    CHECK(endAt >= 0, "anim/finish_motion_reaches_end");
    // Wait2_2 (Walk) is the same clip without the loop rows.
    a.start(AnimWait2_2);
    int end2 = -1;
    for (int i = 0; i < 40; ++i) {
        a.animate();
        if (a.is(KeyEnd)) { end2 = i; break; }
    }
    CHECK(end2 >= 28 && end2 <= 31, "anim/wait2_2_plays_to_end");
    // Latch: an event is visible for exactly one update.
    a.start(AnimFlick);
    int seen = 0;
    for (int i = 0; i < 90; ++i) { a.animate(); if (a.is(Key2)) ++seen; }
    CHECK(seen == 1, "anim/one_frame_latch");
}

void wakeAndLand(Sim& s) {
    s.cand.push_back({1, {0, 0, 60}, true, true, false});
    const int land = s.runUntil(State::Land, 10);
    CHECK(land == 2, "fsm/stay_to_land_without_demo");
}

void testWakeLandWalk() {
    Sim s;
    CHECK(s.fsm.state() == State::Stay, "fsm/starts_in_stay");
    for (int i = 0; i < 30; ++i) s.step();
    CHECK(s.fsm.state() == State::Stay, "fsm/stay_waits_for_target");
    wakeAndLand(s);
    const int walk = s.runUntil(State::ItemWalk, 400);
    CHECK(walk > 200 && walk < 230, "fsm/land_plays_appear_to_end_then_itemwalk");
    // randWeightFloat(10) seeding of the walk timer (preview FSM seeded 0).
    std::set<int> timers;
    for (std::uint32_t seed = 1; seed < 12; ++seed) {
        Sim t(seed);
        t.cand.push_back({1, {0, 0, 60}, true, true, false});
        t.runUntil(State::ItemWalk, 400);
        timers.insert(int(t.fsm.stateTimer()));
    }
    CHECK(timers.size() >= 3, "fsm/walk_timer_seeded_randweight10");
    // The Titan walks: host position follows the IK centre.
    const Vec3 before = s.pos;
    s.cand[0].pos = {0, 0, 200};
    for (int i = 0; i < 150; ++i) s.step();
    const float moved = std::sqrt((s.pos.x - before.x) * (s.pos.x - before.x) + (s.pos.z - before.z) * (s.pos.z - before.z));
    CHECK(moved > 20.0f, "fsm/itemwalk_moves_through_gait");
}

void testDamageRouting() {
    Sim s;
    wakeAndLand(s);
    // Land quarters Pikmin damage (damageCallBack BIGTREASURE_Land).
    const float elec0 = s.fsm.ownership().weaponHealth(P2BTWEAPON_Elec);
    Hit front{{0, 0, 120}, 100.0f, true};
    s.step({front});
    CHECK(std::fabs(s.fsm.ownership().weaponHealth(P2BTWEAPON_Elec) - (elec0 - 25.0f)) < 0.01f, "dmg/land_quarter");
    s.runUntil(State::ItemWalk, 400);
    // Sides map to the otakara joints: front elec, left fire, back gas, right water.
    const float face = s.fsm.gait().faceDir();
    auto around = [&](float a) { return Vec3{s.pos.x + 120.0f * std::sin(face + a), 0.0f, s.pos.z + 120.0f * std::cos(face + a)}; };
    const float w0[4] = {s.fsm.ownership().weaponHealth(0), s.fsm.ownership().weaponHealth(1),
                         s.fsm.ownership().weaponHealth(2), s.fsm.ownership().weaponHealth(3)};
    TickOutput o = s.step({{around(0.0f), 10.0f, true}, {around(kPi / 2), 20.0f, true},
                           {around(kPi), 30.0f, true}, {around(-kPi / 2), 40.0f, true}});
    CHECK(std::fabs(w0[0] - s.fsm.ownership().weaponHealth(0) - 10.0f) < 0.01f
              && std::fabs(w0[1] - s.fsm.ownership().weaponHealth(1) - 20.0f) < 0.01f
              && std::fabs(w0[2] - s.fsm.ownership().weaponHealth(2) - 30.0f) < 0.01f
              && std::fabs(w0[3] - s.fsm.ownership().weaponHealth(3) - 40.0f) < 0.01f,
          "dmg/sides_map_to_weapons");
    CHECK(o.bodyDamage == 0.0f && s.health == 5000.0f, "dmg/no_body_damage_while_armed");
    // Knock off the elec weapon; the drop pops (0,100,0) and flicks its stickers.
    s.cand.push_back({2, around(0.0f), true, false, true, true});
    s.step({{around(0.0f), 7000.0f, true}});
    TickOutput d;
    bool dropped = false;
    for (int i = 0; i < 3 && !dropped; ++i) {
        d = s.step();
        for (const auto& drop : s.log.empty() ? d.drops : d.drops) (void)drop;
        if (!d.drops.empty()) dropped = true;
    }
    // (the drop fires on the damage tick's updateTreasure)
    CHECK(!s.fsm.ownership().isWeaponAttached(P2BTWEAPON_Elec), "dmg/weapon_knocked_off_at_zero");
    // Hits on the dropped side now land on the (unexposed) body: ignored.
    TickOutput ign = s.step({{around(0.0f), 500.0f, true}});
    CHECK(ign.ignoredHits == 1 && ign.bodyDamage == 0.0f, "dmg/dropped_side_is_body_ignored_while_armed");
}

void testDropEventAndPartFlick() {
    Sim s;
    wakeAndLand(s);
    s.runUntil(State::ItemWalk, 400);
    const float face = s.fsm.gait().faceDir();
    const Vec3 front{s.pos.x + 120.0f * std::sin(face), 0.0f, s.pos.z + 120.0f * std::cos(face)};
    const Vec3 back{s.pos.x - 120.0f * std::sin(face), 0.0f, s.pos.z - 120.0f * std::cos(face)};
    s.cand.push_back({2, front, true, false, true, true});
    s.cand.push_back({3, back, true, false, true, true});
    TickOutput o = s.step({{front, 6000.0f, true}});
    CHECK(o.drops.size() == 1 && o.drops[0].weapon == P2BTWEAPON_Elec && o.drops[0].velocity.y == 100.0f,
          "drop/elec_pops_up_100");
    CHECK(o.partFlick.size() == 1 && o.partFlick[0] == 2, "drop/flicks_only_that_parts_stickers");
    CHECK(o.drops.size() == 1 && o.drops[0].position.y > 100.0f, "drop/at_the_otakara_joint");
}

// Knock off every weapon but `keep`.
void stripTo(Sim& s, int keep) {
    const float face = s.fsm.gait().faceDir();
    const float ang[4] = {0.0f, kPi / 2, kPi, -kPi / 2};
    for (int w = 0; w < 4; ++w) {
        if (w == keep) continue;
        const Vec3 p{s.pos.x + 120.0f * std::sin(face + ang[w]), 0.0f, s.pos.z + 120.0f * std::cos(face + ang[w])};
        s.step({{p, 6000.0f, true}});
    }
}

void testAttackLoopFireDirection() {
    Sim s(11u);
    wakeAndLand(s);
    s.runUntil(State::ItemWalk, 400);
    stripTo(s, P2BTWEAPON_Fire);
    CHECK(s.fsm.ownership().weaponCount() == 1 && s.fsm.ownership().isWeaponAttached(P2BTWEAPON_Fire),
          "atk/only_fire_left");
    // The captain stands to the Titan's right: getFireAttackAnimIndex picks FR.
    const float face = s.fsm.gait().faceDir();
    s.cand[0].pos = {s.pos.x - 150.0f * std::cos(face), 0.0f, s.pos.z + 150.0f * std::sin(face)};
    // A Pikmin at the right (in the flame path) and one at the left.
    int started = -1, variant = -1, rightHits = 0, leftHits = 0, pre = 0;
    bool emitted = false;
    for (int i = 0; i < 2400; ++i) {
        const float f = s.fsm.gait().faceDir();
        const Vec3 right{s.pos.x - 80.0f * std::cos(f), 0.0f, s.pos.z + 80.0f * std::sin(f)};
        const Vec3 left{s.pos.x + 80.0f * std::cos(f), 0.0f, s.pos.z - 80.0f * std::sin(f)};
        if (s.cand.size() < 3) {
            s.cand.push_back({10, right, true, false, true, false});
            s.cand.push_back({11, left, true, false, true, false});
        }
        s.cand[1].pos = right;
        s.cand[2].pos = left;
        s.cand[0].pos = {s.pos.x - 150.0f * std::cos(f), 0.0f, s.pos.z + 150.0f * std::sin(f)}; // captain at right
        TickOutput o = s.step();
        for (const auto& t : o.entered) if (t.to == State::PreAttack) ++pre;
        if (o.attackStarted >= 0 && started < 0) { started = o.attackStarted; variant = o.fireVariant; }
        if (o.attackNodes > 0) emitted = true;
        for (const auto& h : o.elementHits) {
            if (h.id == 10) ++rightHits;
            if (h.id == 11) ++leftHits;
        }
        if (started >= 0 && s.fsm.state() == State::PutItem) break;
    }
    CHECK(pre >= 1, "atk/attack_limit_reaches_preattack");
    CHECK(started == P2BTWEAPON_Fire, "atk/picked_the_live_weapon");
    CHECK(variant == 1, "atk/fire_target_angle_right_variant");
    CHECK(emitted, "atk/element_emits_nodes");
    CHECK(rightHits >= 1 && leftHits == 0, "atk/fire_follows_facing_and_variant");
}

// Staged bank: the fire direction comes from the animated otakara_fire_eff
// column 0, not from a yaw guess. The staged FR attack pose points the
// nozzle BACKWARD, so only the Pikmin behind is burned.
void testStagedFireJoint() {
    Bank bank = defaultBank();
    bank.staged = true;
    for (int anim : {AnimAttackF, AnimAttackFR, AnimAttackFL, AnimAttackFB}) {
        bank.clip[anim].frames = 120;
        bank.clip[anim].poses.assign(1, PoseJoints{});
        Mat34 m;
        // column 0 = (0, -0.25, -1) (backward, slightly down), at 60 u height.
        m.m[0][0] = 0.0f; m.m[1][0] = -0.25f; m.m[2][0] = -1.0f;
        m.m[0][3] = 0.0f; m.m[1][3] = 60.0f; m.m[2][3] = 0.0f;
        bank.clip[anim].poses[0].joint[JointFireEff] = m;
        bank.clip[anim].poses[0].have[JointFireEff] = true;
    }
    Fsm fsm;
    fsm.init(retailParams(), bank, retailAnimator(), {0, 0, 0}, 0.0f, 11u);
    Sim s(11u);
    s.fsm = fsm;
    wakeAndLand(s);
    s.runUntil(State::ItemWalk, 400);
    stripTo(s, P2BTWEAPON_Fire);
    int backHits = 0, frontHits = 0, started = -1;
    for (int i = 0; i < 2400; ++i) {
        const float f = s.fsm.gait().faceDir();
        const Vec3 back{s.pos.x - 120.0f * std::sin(f), 0.0f, s.pos.z - 120.0f * std::cos(f)};
        const Vec3 front{s.pos.x + 120.0f * std::sin(f), 0.0f, s.pos.z + 120.0f * std::cos(f)};
        if (s.cand.size() < 3) {
            s.cand.push_back({30, back, true, false, true, false});
            s.cand.push_back({31, front, true, false, true, false});
        }
        s.cand[1].pos = back;
        s.cand[2].pos = front;
        TickOutput o = s.step();
        if (o.attackStarted >= 0) started = o.attackStarted;
        for (const auto& h : o.elementHits) {
            if (h.id == 30) ++backHits;
            if (h.id == 31) ++frontHits;
        }
        if (started >= 0 && s.fsm.state() == State::PutItem) break;
    }
    CHECK(started == P2BTWEAPON_Fire, "atk/staged_fire_started");
    CHECK(backHits >= 1 && frontHits == 0, "atk/fire_direction_from_eff_joint_column0");
}

// All weapons gone: ItemWalk swaps to the dropitem clip, DropItem END goes
// to Walk (no flick, health > 0), and the body is now the only damage sink.
void testUnarmedWalk() {
    Sim s(3u);
    wakeAndLand(s);
    s.runUntil(State::ItemWalk, 400);
    stripTo(s, -1);
    bool dropItem = false;
    for (State st : s.states) if (st == State::DropItem) dropItem = true;
    const int walk = s.runUntil(State::Walk, 900);
    for (State st : s.states) if (st == State::DropItem) dropItem = true;
    CHECK(walk >= 0, "unarmed/reaches_walk");
    CHECK(dropItem || s.fsm.animator().anim() == AnimWait2_2, "unarmed/dropitem_before_walk");
    CHECK(s.fsm.animator().anim() == AnimWait2_2, "unarmed/walk_plays_wait2_2");
    // Walk moves through the gait and its timer is randWeightFloat(10).
    const Vec3 before = s.pos;
    s.cand[0].pos = {s.pos.x + 200.0f, 0.0f, s.pos.z};
    for (int i = 0; i < 200; ++i) s.step();
    const float moved = std::fabs(s.pos.x - before.x) + std::fabs(s.pos.z - before.z);
    CHECK(moved > 10.0f, "unarmed/walk_moves");
}

void testDeath() {
    Sim s;
    wakeAndLand(s);
    s.runUntil(State::ItemWalk, 400);
    stripTo(s, -1);
    CHECK(s.fsm.ownership().weaponCount() == 0, "death/all_weapons_off");
    // Body exposed: hits now damage health (x1 outside Land).
    int body = 0;
    for (int i = 0; i < 400 && s.health > 0.0f; ++i) {
        TickOutput o = s.step({{{s.pos.x + 10, 0, s.pos.z}, 100.0f, true}});
        if (o.bodyHits) ++body;
    }
    CHECK(body >= 50 && s.health <= 0.0f, "death/body_damage_after_zero_weapons");
    // Body damage lands before the FSM exec, so Dead is entered on the same tick.
    const int dead = s.fsm.state() == State::Dead ? 0 : s.runUntil(State::Dead, 600);
    CHECK(dead >= 0 && s.fsm.state() == State::Dead, "death/reaches_dead");
    // deathProcedure setAlive(false): hits after Dead do nothing.
    {
        const float before = s.health;
        TickOutput o = s.step({{{s.pos.x + 10, 0, s.pos.z}, 100.0f, true}});
        CHECK(o.bodyDamage == 0.0f && o.deadHits == 1 && s.health == before, "death/no_damage_after_dead");
    }
    bool louie = false, kill = false;
    int louieTick = -1, killTick = -1;
    for (int i = 0; i < 400 && !kill; ++i) {
        TickOutput o = s.step();
        if (o.louieReleased) { louie = true; louieTick = i; }
        if (o.killRequest) { kill = true; killTick = i; }
    }
    CHECK(louie && louieTick > 310 && louieTick < 330, "death/keyevent100_releases_louie");
    CHECK(kill && killTick > louieTick, "death/keyevent_end_kills");
}

void testFlickFromStuckPikmin() {
    Sim s;
    wakeAndLand(s);
    // Stuck Pikmin + repeated hits raise the flick timer; ItemWalk leaves
    // for PreAttack (captured) through isStartFlick.
    s.runUntil(State::ItemWalk, 400);
    const float face = s.fsm.gait().faceDir();
    const Vec3 front{s.pos.x + 120.0f * std::sin(face), 0.0f, s.pos.z + 120.0f * std::cos(face)};
    for (int k = 0; k < 3; ++k) s.cand.push_back({std::uint64_t(20 + k), front, true, false, true, true});
    std::vector<Hit> hits(10, Hit{front, 1.0f, true});
    s.step(hits);
    CHECK(s.fsm.flickTimer() >= 10.0f, "flick/timer_counts_weapon_hits");
    int pre = s.runUntil(State::PreAttack, 400);
    CHECK(pre >= 0, "flick/itemwalk_to_preattack_on_flick");
    bool flicked = false;
    for (int i = 0; i < 120 && !flicked; ++i) {
        TickOutput o = s.step();
        if (!o.flick.empty() && o.flickReason && std::string(o.flickReason) == "preattack") flicked = true;
    }
    CHECK(flicked && s.fsm.flickTimer() == 0.0f, "flick/preattack_key2_flicks_and_resets");
}

void testTimeMax() {
    // A water attack loops (0..29) until the fp23 attack time max finishes it.
    Sim s(5u);
    wakeAndLand(s);
    s.runUntil(State::ItemWalk, 400);
    stripTo(s, P2BTWEAPON_Water);
    int attackTicks = -1, begin = -1;
    for (int i = 0; i < 3000; ++i) {
        TickOutput o = s.step();
        for (const auto& t : o.entered) {
            if (t.to == State::Attack) begin = i;
            if (t.from == State::Attack && begin >= 0) { attackTicks = i - begin; break; }
        }
        if (attackTicks >= 0) break;
    }
    // fp23 = 5 s (150 ticks) then the loop tail to END.
    CHECK(attackTicks >= 150 && attackTicks <= 185, "time/attack_time_max_fp23");
}
} // namespace

int main() {
    testParser();
    testGait();
    testAnimator();
    testWakeLandWalk();
    testDamageRouting();
    testDropEventAndPartFlick();
    testAttackLoopFireDirection();
    testStagedFireJoint();
    testUnarmedWalk();
    testDeath();
    testFlickFromStuckPikmin();
    testTimeMax();
    std::printf("%s (%d failures)\n", gFail ? "FAILED" : "PASSED", gFail);
    return gFail ? 1 : 0;
}
