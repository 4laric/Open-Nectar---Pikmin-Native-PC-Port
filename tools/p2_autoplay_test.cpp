// TEST-ONLY autoplay bot policy test (bot-impl wf9, bot-v2 wf10, bot-v3 wf10). Engine-free.
//
// Pins the engine-free Brain in pc_port/pc_p2_autoplay_policy.h:
//   * inert-when-unset: with the PIKMIN_RANDOMIZER_AUTOPLAY gate closed the
//     Brain stays IDLE, emits a neutral pad and records no markers, no
//     matter how adversarial the senses are;
//   * the withdraw -> select -> approach -> attack -> aftermath -> result
//     flow, timeouts with AUTOPLAY_GIVEUP, stuck detection with replan,
//     Kogane damage-and-move-on, kill+carry scoring, and target matching;
//   * bot-v2: Onion receipt wait (carried=1 means a receipt line,
//     received=<0/1>), generic death latch for every species, withdraw-menu
//     repeat until 15-or-empty, Sarai low-or-grabbing throws + whistle,
//     Kurage extended attack with rotating throws, replan on every STUCK.
//   * bot-v3: STUCK lines carry navi=(x,z) (+replan=N in approach);
//     target_unreachable GIVEUP after maxApproachReplans consecutive STUCK
//     windows (count resets on real progress); Done idles near the Onion.
//
// Exit 0 only if every check passes; any failure prints FAIL and exits 1.
#include "pc_p2_autoplay_policy.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

int failures = 0;

#define CHECK(cond, name) do { \
    if (cond) { std::printf("PASS %s\n", name); } \
    else { std::printf("FAIL %s\n", name); ++failures; } \
} while (0)

void setEnv(const char* name, const char* value)
{
#ifdef _WIN32
    if (!value) {
        _putenv_s(name, "");
    } else {
        _putenv_s(name, value);
    }
#else
    if (!value) {
        unsetenv(name);
    } else {
        setenv(name, value, 1);
    }
#endif
}

p2autoplay::Senses liveSenses()
{
    p2autoplay::Senses s;
    s.enabled = true;
    s.naviAlive = true;
    s.dt = 0.05f;
    return s;
}

bool hasMarker(const std::vector<std::string>& markers, const char* substr)
{
    for (const std::string& m : markers) {
        if (m.find(substr) != std::string::npos) return true;
    }
    return false;
}

void testGate()
{
    setEnv("PIKMIN_RANDOMIZER_AUTOPLAY", nullptr);
    CHECK(!p2autoplay::isEnabled(), "gate/unset_is_disabled");
    setEnv("PIKMIN_RANDOMIZER_AUTOPLAY", "");
    CHECK(!p2autoplay::isEnabled(), "gate/empty_is_disabled");
    setEnv("PIKMIN_RANDOMIZER_AUTOPLAY", "0");
    CHECK(!p2autoplay::isEnabled(), "gate/zero_is_disabled");
    setEnv("PIKMIN_RANDOMIZER_AUTOPLAY", "1");
    CHECK(p2autoplay::isEnabled(), "gate/one_is_enabled");
    setEnv("PIKMIN_RANDOMIZER_AUTOPLAY_TARGET", "Sokkuri");
    CHECK(p2autoplay::targetFilter() == "Sokkuri", "gate/target_filter_read");
    setEnv("PIKMIN_RANDOMIZER_AUTOPLAY_TARGET", nullptr);
    CHECK(p2autoplay::targetFilter().empty(), "gate/target_filter_empty");
    setEnv("PIKMIN_RANDOMIZER_AUTOPLAY", nullptr);
}

void testInertWhenUnset()
{
    // Adversarial senses with the gate closed: must stay neutral and silent.
    p2autoplay::Brain brain;
    p2autoplay::Senses s = liveSenses();
    s.enabled = false;
    s.fieldPikmin = 20;
    s.hasOnion = true;
    s.onionDist = 10.0f;
    s.targetToken = 1234;
    s.targetAlive = true;
    s.targetDist = 50.0f;
    s.targetHealthFrac = 0.2f;
    s.transportSeen = true;
    s.scattered = true;
    s.targetDead = true;
    s.receiptSeen = true;
    s.targetGrabbing = true;
    s.targetLow = true;
    for (int i = 0; i < 600; ++i) brain.update(0.05f, s);
    CHECK(brain.current() == p2autoplay::State::Idle, "inert/stays_idle");
    const p2autoplay::Command cmd = brain.command();
    CHECK(cmd.buttons == 0 && cmd.moveX == 0.0f && cmd.moveZ == 0.0f && !cmd.menuHold,
          "inert/pad_neutral");
    CHECK(brain.takeMarkers().empty(), "inert/no_markers");
    CHECK(!brain.replanWanted(), "inert/no_replan");

    // Opening the gate lets the same senses drive the machine.
    s.enabled = true;
    brain.update(0.05f, s);
    CHECK(brain.current() == p2autoplay::State::WithdrawSeek, "inert/gate_opens_to_withdraw");
    CHECK(hasMarker(brain.takeMarkers(), "AUTOPLAY_STATE"), "inert/state_marker_when_open");
}

void testWithdrawFlow()
{
    p2autoplay::Config cfg;
    cfg.wantSquad = 15;
    p2autoplay::Brain brain(cfg);
    p2autoplay::Senses s = liveSenses();
    brain.update(0.05f, s); // idle -> withdraw_seek
    CHECK(brain.current() == p2autoplay::State::WithdrawSeek, "withdraw/enters_seek");

    // Far from the Onion: steers toward it in world space.
    s.hasOnion = true;
    s.naviX = 0.0f;
    s.naviZ = 0.0f;
    s.onionX = 400.0f;
    s.onionZ = 0.0f;
    s.onionDist = 400.0f;
    s.onionStored = 20;
    brain.update(0.05f, s);
    const p2autoplay::Command cmd = brain.command();
    CHECK(cmd.moveX > 0.9f && std::fabs(cmd.moveZ) < 0.01f, "withdraw/steers_to_onion");

    // At the Onion with the UI open: holds the withdraw input.
    s.onionDist = 10.0f;
    s.containerOpen = true;
    brain.update(0.05f, s);
    CHECK(brain.current() == p2autoplay::State::WithdrawMenu, "withdraw/enters_menu");
    brain.update(0.05f, s);
    CHECK(brain.command().menuHold, "withdraw/menu_hold");

    // Squad filled: confirms and leaves for target select.
    s.fieldPikmin = 20;
    for (int i = 0; i < 30 && brain.current() == p2autoplay::State::WithdrawMenu; ++i) {
        brain.update(0.05f, s);
    }
    s.containerOpen = false; // UI closed after the A confirm
    for (int i = 0; i < 10 && brain.current() == p2autoplay::State::WithdrawMenu; ++i) {
        brain.update(0.05f, s);
    }
    CHECK(brain.current() == p2autoplay::State::Select, "withdraw/leaves_when_filled");
}

void testWithdrawKeepsClosing()
{
    // Regression for wf9-2 withdraw_timeout: arriving within arriveRadius
    // (90) must not stop the steering -- the real container trigger is
    // ~50, so stopping at 90 stalls forever. Close distance still steers
    // while tapping A, honours waypoint detours, and logs STUCK + replan.
    p2autoplay::Config cfg;
    cfg.wantSquad = 15;
    cfg.stuckWindow = 0.2f;
    cfg.stuckMinProgress = 30.0f;
    p2autoplay::Brain brain(cfg);
    p2autoplay::Senses s = liveSenses();
    brain.update(0.05f, s); // idle -> withdraw_seek
    s.hasOnion = true;
    s.naviX = 0.0f;
    s.naviZ = 0.0f;
    s.onionX = 50.0f;
    s.onionZ = 0.0f;
    s.onionDist = 50.0f;
    s.onionStored = 20;
    s.containerOpen = false;
    brain.update(0.05f, s);
    const p2autoplay::Command close = brain.command();
    CHECK(close.moveX > 0.9f, "withdraw-close/keeps_steering_inside_arrive");
    int aOn = 0;
    for (int i = 0; i < 40; ++i) {
        brain.update(0.05f, s);
        if (brain.command().buttons & unsigned(p2autoplay::PadA)) ++aOn;
    }
    CHECK(aOn > 0, "withdraw-close/pulses_A_while_closing");
    CHECK(brain.current() == p2autoplay::State::WithdrawSeek, "withdraw-close/stays_until_open");

    // Waypoint detour overrides the straight line to the Onion.
    s.waypointLeg = true;
    s.wpX = 0.0f;
    s.wpZ = 400.0f;
    brain.update(0.05f, s);
    const p2autoplay::Command det = brain.command();
    CHECK(det.moveZ > 0.9f && std::fabs(det.moveX) < 0.2f, "withdraw-close/waypoint_detour");
    s.waypointLeg = false;

    // No progress: STUCK + replan wanted (driver routes via waypoints).
    std::vector<std::string> markers;
    for (int i = 0; i < 30; ++i) {
        brain.update(0.05f, s);
        const std::vector<std::string> got = brain.takeMarkers();
        markers.insert(markers.end(), got.begin(), got.end());
    }
    CHECK(hasMarker(markers, "AUTOPLAY_STUCK state=withdraw_seek"), "withdraw-close/stuck_marker");
    CHECK(brain.replanWanted(), "withdraw-close/replan_wanted");
}

void testWithdrawMenuHoldThenConfirm()
{
    // Regression for wf9-3 withdraw_hold_timeout: field stays 0 while the
    // container UI is open (delta is UI-local until A confirms), so a
    // field-gated hold deadlocks. The menu must hold stick-down for
    // menuHoldDuration, then pulse A to confirm even with field=0.
    p2autoplay::Config cfg;
    cfg.wantSquad = 15;
    cfg.menuHoldDuration = 1.0f;
    cfg.menuConfirmDuration = 1.0f;
    p2autoplay::Brain brain(cfg);
    p2autoplay::Senses s = liveSenses();
    brain.update(0.05f, s); // idle -> withdraw_seek
    s.hasOnion = true;
    s.onionDist = 10.0f;
    s.onionStored = 20;
    s.fieldPikmin = 0;
    s.containerOpen = true;
    brain.update(0.05f, s);
    CHECK(brain.current() == p2autoplay::State::WithdrawMenu, "withdraw-menu/enters");
    // Hold phase: menuHold, no A yet.
    brain.update(0.05f, s);
    CHECK(brain.command().menuHold, "withdraw-menu/holds_first");
    // After the hold duration: A pulses to confirm despite field=0.
    for (int i = 0; i < 30; ++i) brain.update(0.05f, s);
    int aOn = 0;
    for (int i = 0; i < 40; ++i) {
        brain.update(0.05f, s);
        if (brain.command().buttons & unsigned(p2autoplay::PadA)) ++aOn;
    }
    CHECK(aOn > 0, "withdraw-menu/confirms_after_hold");
    // UI closes after the confirm with field still 0 and the Onion stocked:
    // bot-v2 repeats the withdraw menu (another cycle) instead of leaving
    // empty-handed for target select.
    s.containerOpen = false;
    brain.update(0.05f, s);
    CHECK(brain.current() == p2autoplay::State::WithdrawSeek, "withdraw-menu/loops_for_another_cycle");
    CHECK(hasMarker(brain.takeMarkers(), "AUTOPLAY_WITHDRAW cycle=1"), "withdraw-menu/cycle_logged");
}

void testCombatFlow()
{
    p2autoplay::Config cfg;
    cfg.throwHold = 0.1f;
    cfg.throwGap = 0.2f;
    p2autoplay::Brain brain(cfg);
    p2autoplay::Senses s = liveSenses();
    s.fieldPikmin = 20;
    brain.update(0.05f, s); // idle -> withdraw_seek
    brain.update(0.05f, s); // withdraw_seek -> select (squad ready)
    CHECK(brain.current() == p2autoplay::State::Select, "combat/reaches_select");

    // Acquire a Sokkuri target.
    s.targetToken = 5465461;
    s.targetSource = 79;
    s.targetAlive = true;
    s.tgtX = 1000.0f;
    s.tgtZ = 0.0f;
    s.targetDist = 1000.0f;
    s.targetHealthFrac = 1.0f;
    brain.update(0.05f, s);
    CHECK(brain.current() == p2autoplay::State::Approach, "combat/approach");
    // Still disguised: keeps closing past throw range.
    s.targetRevealed = false;
    s.targetDist = 200.0f;
    brain.update(0.05f, s);
    CHECK(brain.current() == p2autoplay::State::Approach, "combat/closes_on_disguise");
    s.targetRevealed = true;
    s.targetDist = 100.0f;
    brain.update(0.05f, s);
    CHECK(brain.current() == p2autoplay::State::Attack, "combat/attacks_in_range");

    // Throw pulses: A toggles over time while closing the aim.
    int aOn = 0;
    for (int i = 0; i < 60; ++i) {
        brain.update(0.05f, s);
        if (brain.command().buttons & unsigned(p2autoplay::PadA)) ++aOn;
    }
    CHECK(aOn > 0 && aOn < 60, "combat/throw_pulses");
    // Aims at the target: move vector points at it.
    const p2autoplay::Command aim = brain.command();
    CHECK(aim.moveX != 0.0f || aim.moveZ != 0.0f, "combat/aims_while_throwing");

    // Scattered squad: whistle regroup takes over throwing.
    s.scattered = true;
    brain.update(0.05f, s);
    CHECK(brain.command().buttons & unsigned(p2autoplay::PadB), "combat/whistle_regroup");
    s.scattered = false;

    // Combat damage observed, then the kill: aftermath watches the corpse.
    s.targetHealthFrac = 0.6f;
    brain.update(0.05f, s);
    s.targetAlive = false;
    brain.update(0.05f, s);
    CHECK(brain.current() == p2autoplay::State::Aftermath, "combat/aftermath_after_kill");
    s.transportSeen = true;
    s.receiptSeen = true; // Onion receipt lands while the corpse is carried
    std::vector<std::string> markers;
    for (int i = 0; i < 200 && brain.current() == p2autoplay::State::Aftermath; ++i) {
        brain.update(0.05f, s);
        const std::vector<std::string> got = brain.takeMarkers();
        markers.insert(markers.end(), got.begin(), got.end());
    }
    CHECK(hasMarker(markers, "AUTOPLAY_RESULT target=5465461 damaged=1 killed=1 carried=1"),
          "combat/result_kill_carry");
    CHECK(hasMarker(markers, "received=1"), "combat/result_received");
    CHECK(hasMarker(markers, "bot-driven"), "combat/result_labelled_bot_driven");
}

void testKoganeMovesOn()
{
    p2autoplay::Config cfg;
    cfg.koganeConfirm = 1.0f;
    p2autoplay::Brain brain(cfg);
    p2autoplay::Senses s = liveSenses();
    s.fieldPikmin = 20;
    brain.update(0.05f, s);
    brain.update(0.05f, s); // -> select
    s.targetToken = 983680291;
    s.targetSource = 9;
    s.targetAlive = true;
    s.targetDist = 100.0f;
    s.targetHealthFrac = 1.0f; // Kogane never loses health
    brain.update(0.05f, s); // -> approach
    brain.update(0.05f, s); // -> attack
    CHECK(brain.current() == p2autoplay::State::Attack, "kogane/attacks");
    // A flip (family-observed combat) then the confirm window: moves on with
    // damage counted and no kill/carry claim.
    s.targetDamagedLatch = true;
    std::vector<std::string> markers;
    for (int i = 0; i < 200 && brain.current() != p2autoplay::State::Select; ++i) {
        brain.update(0.05f, s);
        const std::vector<std::string> got = brain.takeMarkers();
        markers.insert(markers.end(), got.begin(), got.end());
    }
    CHECK(hasMarker(markers, "AUTOPLAY_RESULT target=983680291 damaged=1 killed=0 carried=0"),
          "kogane/result_damage_no_kill");
}

void testTimeoutsAndStuck()
{
    p2autoplay::Config cfg;
    cfg.approachTimeout = 1.0f;
    cfg.stuckWindow = 0.2f;
    cfg.stuckMinProgress = 30.0f;
    p2autoplay::Brain brain(cfg);
    p2autoplay::Senses s = liveSenses();
    s.fieldPikmin = 20;
    brain.update(0.05f, s);
    brain.update(0.05f, s); // -> select
    s.targetToken = 111;
    s.targetSource = 23; // Sarai flyer
    s.targetAlive = true;
    s.targetDist = 2000.0f;
    s.tgtX = 2000.0f;
    s.tgtZ = 0.0f;
    brain.update(0.05f, s); // -> approach
    std::vector<std::string> markers;
    for (int i = 0; i < 100; ++i) {
        brain.update(0.05f, s); // no progress: stuck then timeout
        const std::vector<std::string> got = brain.takeMarkers();
        markers.insert(markers.end(), got.begin(), got.end());
    }
    CHECK(hasMarker(markers, "AUTOPLAY_STUCK"), "stuck/marker_logged");
    CHECK(hasMarker(markers, "AUTOPLAY_GIVEUP reason=approach_timeout"), "timeout/giveup_logged");
    CHECK(hasMarker(markers, "AUTOPLAY_RESULT target=111 damaged=0 killed=0 carried=0"),
          "timeout/result_no_claims");
}

void testTargetMatching()
{
    CHECK(p2autoplay::matchTarget(5465461, 79, "Sokkuri", ""), "match/empty_filter");
    CHECK(p2autoplay::matchTarget(5465461, 79, "Sokkuri", "5465461"), "match/generator_key");
    CHECK(p2autoplay::matchTarget(5465461, 79, "Sokkuri", "79"), "match/source_id");
    CHECK(p2autoplay::matchTarget(5465461, 79, "Sokkuri", "Sokkuri"), "match/species_name");
    CHECK(p2autoplay::matchTarget(5465461, 79, "Sokkuri", "sokkuri"), "match/name_case");
    CHECK(!p2autoplay::matchTarget(5465461, 79, "Sokkuri", "Sarai"), "match/wrong_species");
    CHECK(p2autoplay::matchTarget(2175753366u, 23, "Sarai", "23"), "match/sarai_source");
    CHECK(p2autoplay::matchTarget(983680291, 9, "Kogane", "Kogane"), "match/kogane_name");
    CHECK(p2autoplay::matchTarget(1787125272, 57, "Kurage", "1787125272"), "match/kurage_key");
    CHECK(p2autoplay::sourceForSpeciesName("Sokkuri") == 79, "match/sokkuri_id");
    CHECK(p2autoplay::sourceForSpeciesName("Kogane") == 9, "match/kogane_id");
    CHECK(p2autoplay::sourceForSpeciesName("Sarai") == 23, "match/sarai_id");
    CHECK(p2autoplay::sourceForSpeciesName("Kurage") == 57, "match/kurage_id");
}

void testReceiptWait()
{
    // bot-v2 gap 1: Aftermath waits for the Onion receipt marker or a
    // timeout. carried=1 must mean a receipt line; received=<0/1> is scored.
    p2autoplay::Config cfg;
    cfg.receiptTimeout = 1.0f;
    cfg.aftermathTimeout = 60.0f;
    cfg.whistleHold = 0.2f;
    p2autoplay::Brain brain(cfg);
    p2autoplay::Senses s = liveSenses();
    s.fieldPikmin = 20;
    brain.update(0.05f, s);
    brain.update(0.05f, s); // -> select
    s.targetToken = 777001;
    s.targetSource = 79;
    s.targetAlive = true;
    s.targetDist = 100.0f;
    brain.update(0.05f, s); // -> approach
    brain.update(0.05f, s); // -> attack
    // Kill without a receipt yet: stays in Aftermath, no RESULT.
    s.targetHealthFrac = 0.5f;
    brain.update(0.05f, s);
    s.targetAlive = false;
    s.transportSeen = true;
    s.receiptSeen = false;
    brain.update(0.05f, s);
    CHECK(brain.current() == p2autoplay::State::Aftermath, "receipt/waits_after_kill");
    for (int i = 0; i < 6; ++i) brain.update(0.05f, s);
    CHECK(brain.current() == p2autoplay::State::Aftermath, "receipt/keeps_waiting_without_receipt");
    CHECK(!hasMarker(brain.takeMarkers(), "AUTOPLAY_RESULT"), "receipt/no_early_result");
    // Timeout with no receipt: kill claimed, carry/receipt refused.
    std::vector<std::string> markers;
    for (int i = 0; i < 60 && brain.current() == p2autoplay::State::Aftermath; ++i) {
        brain.update(0.05f, s);
        const std::vector<std::string> got = brain.takeMarkers();
        markers.insert(markers.end(), got.begin(), got.end());
    }
    CHECK(hasMarker(markers, "AUTOPLAY_GIVEUP reason=receipt_timeout"), "receipt/timeout_logged");
    CHECK(hasMarker(markers, "AUTOPLAY_RESULT target=777001 damaged=1 killed=1 carried=0"),
          "receipt/timeout_no_carry_claim");
    CHECK(hasMarker(markers, "received=0"), "receipt/timeout_received_zero");

    // Second kill where the receipt lands mid-wait: prompt carried=1.
    p2autoplay::Brain brain2(cfg);
    brain2.update(0.05f, s); // idle -> withdraw_seek (field=20 still set)
    p2autoplay::Senses s2 = s;
    s2.targetAlive = true;
    s2.targetHealthFrac = 1.0f;
    s2.transportSeen = false;
    s2.receiptSeen = false;
    brain2.update(0.05f, s2); // -> select
    brain2.update(0.05f, s2); // -> approach
    brain2.update(0.05f, s2); // -> attack
    s2.targetHealthFrac = 0.4f;
    brain2.update(0.05f, s2);
    s2.targetAlive = false;
    s2.transportSeen = true;
    brain2.update(0.05f, s2);
    CHECK(brain2.current() == p2autoplay::State::Aftermath, "receipt/waits_second_kill");
    for (int i = 0; i < 4; ++i) brain2.update(0.05f, s2); // whistle window
    s2.receiptSeen = true;
    markers.clear();
    for (int i = 0; i < 40 && brain2.current() == p2autoplay::State::Aftermath; ++i) {
        brain2.update(0.05f, s2);
        const std::vector<std::string> got = brain2.takeMarkers();
        markers.insert(markers.end(), got.begin(), got.end());
    }
    CHECK(hasMarker(markers, "AUTOPLAY_RESULT target=777001 damaged=1 killed=1 carried=1"),
          "receipt/receipt_scores_carry");
    CHECK(hasMarker(markers, "received=1"), "receipt/receipt_scores_received");
}

void testGenericDeath()
{
    // bot-v2 gap 5: Otakara-style kill with no health-frac drop and no
    // per-module marker still claims damaged+killed via the generic latch.
    p2autoplay::Config cfg;
    cfg.receiptTimeout = 60.0f;
    cfg.whistleHold = 0.2f;
    p2autoplay::Brain brain(cfg);
    p2autoplay::Senses s = liveSenses();
    s.fieldPikmin = 20;
    brain.update(0.05f, s);
    brain.update(0.05f, s); // -> select
    s.targetToken = 3921089765u;
    s.targetSource = 59; // FireOtakara
    s.targetAlive = true;
    s.targetDist = 100.0f;
    s.targetHealthFrac = 1.0f; // no P2_OTAKARA_DAMAGE line, no frac drop
    brain.update(0.05f, s); // -> approach
    brain.update(0.05f, s); // -> attack
    CHECK(brain.current() == p2autoplay::State::Attack, "generic-death/attacks");
    // Host death seam (mDeadState): generic latch, health frac untouched.
    s.targetDead = true;
    s.targetAlive = false;
    brain.update(0.05f, s);
    CHECK(brain.current() == p2autoplay::State::Aftermath, "generic-death/aftermath");
    s.transportSeen = true;
    s.receiptSeen = true;
    std::vector<std::string> markers;
    for (int i = 0; i < 60 && brain.current() == p2autoplay::State::Aftermath; ++i) {
        brain.update(0.05f, s);
        const std::vector<std::string> got = brain.takeMarkers();
        markers.insert(markers.end(), got.begin(), got.end());
    }
    CHECK(hasMarker(markers, "AUTOPLAY_RESULT target=3921089765 damaged=1 killed=1 carried=1"),
          "generic-death/result_claims_kill");
}

void testWithdrawRepeat()
{
    // bot-v2 gap 4: a 5-Pikmin first cycle loops back for another cycle
    // while the Onion still stocks; a full squad leaves for select.
    p2autoplay::Config cfg;
    cfg.wantSquad = 15;
    cfg.menuHoldDuration = 0.5f;
    cfg.menuConfirmDuration = 0.5f;
    cfg.maxWithdrawCycles = 5;
    p2autoplay::Brain brain(cfg);
    p2autoplay::Senses s = liveSenses();
    brain.update(0.05f, s); // idle -> withdraw_seek
    s.hasOnion = true;
    s.onionDist = 10.0f;
    s.onionStored = 20;
    s.fieldPikmin = 0;
    s.containerOpen = true;
    brain.update(0.05f, s);
    CHECK(brain.current() == p2autoplay::State::WithdrawMenu, "withdraw-repeat/enters");
    for (int i = 0; i < 40 && brain.current() == p2autoplay::State::WithdrawMenu; ++i) {
        brain.update(0.05f, s);
        if (brain.takeMarkers().empty()) continue;
    }
    // First cycle lands only 5 Pikmin: loops back to seek, logs the cycle.
    s.containerOpen = false;
    s.fieldPikmin = 5;
    s.onionStored = 15;
    brain.update(0.05f, s);
    CHECK(brain.current() == p2autoplay::State::WithdrawSeek, "withdraw-repeat/loops_back_when_short");
    CHECK(hasMarker(brain.takeMarkers(), "AUTOPLAY_WITHDRAW cycle=1"), "withdraw-repeat/cycle_logged");
    // Second cycle: back at the Onion, UI reopens, fills to 15, leaves.
    s.onionDist = 10.0f;
    brain.update(0.05f, s);
    s.containerOpen = true;
    brain.update(0.05f, s);
    CHECK(brain.current() == p2autoplay::State::WithdrawMenu, "withdraw-repeat/reenters");
    for (int i = 0; i < 40; ++i) brain.update(0.05f, s);
    s.containerOpen = false;
    s.fieldPikmin = 15;
    s.onionStored = 5;
    for (int i = 0; i < 10 && brain.current() == p2autoplay::State::WithdrawMenu; ++i) {
        brain.update(0.05f, s);
    }
    CHECK(brain.current() == p2autoplay::State::Select, "withdraw-repeat/leaves_when_full");
    // Empty Onion with a short squad still leaves (nothing left to take).
    p2autoplay::Brain brain2(cfg);
    brain2.update(0.05f, s);
    p2autoplay::Senses s2 = liveSenses();
    s2.hasOnion = true;
    s2.onionDist = 10.0f;
    s2.onionStored = 0;
    s2.fieldPikmin = 5;
    brain2.update(0.05f, s2);
    CHECK(brain2.current() == p2autoplay::State::Select, "withdraw-repeat/leaves_when_empty");
}

void testSaraiFlyer()
{
    // bot-v2 gap 3 (Sarai 23): high + holding nothing -> follow, no throws;
    // grabbing -> whistle; low -> throws.
    p2autoplay::Config cfg;
    cfg.throwHold = 0.1f;
    cfg.throwGap = 0.2f;
    p2autoplay::Brain brain(cfg);
    p2autoplay::Senses s = liveSenses();
    s.fieldPikmin = 20;
    brain.update(0.05f, s);
    brain.update(0.05f, s); // -> select
    s.targetToken = 2175753366u;
    s.targetSource = 23;
    s.targetAlive = true;
    s.naviX = 0.0f;
    s.naviZ = 0.0f;
    s.tgtX = 200.0f;
    s.tgtZ = 0.0f;
    s.targetDist = 200.0f;
    s.targetLow = false;
    s.targetGrabbing = false;
    brain.update(0.05f, s); // -> approach
    brain.update(0.05f, s); // -> attack (flyer range)
    CHECK(brain.current() == p2autoplay::State::Attack, "sarai/attacks_in_range");
    int aOn = 0;
    for (int i = 0; i < 30; ++i) {
        brain.update(0.05f, s);
        if (brain.command().buttons & unsigned(p2autoplay::PadA)) ++aOn;
    }
    CHECK(aOn == 0, "sarai/holds_throws_while_high");
    CHECK(brain.command().moveX > 0.5f, "sarai/stays_near_while_high");
    // Grab: whistle takes over.
    s.targetGrabbing = true;
    brain.update(0.05f, s);
    CHECK(brain.command().buttons & unsigned(p2autoplay::PadB), "sarai/whistle_frees_grab");
    s.targetGrabbing = false;
    s.targetLow = true; // swooped low: throws resume
    aOn = 0;
    for (int i = 0; i < 40; ++i) {
        brain.update(0.05f, s);
        if (brain.command().buttons & unsigned(p2autoplay::PadA)) ++aOn;
    }
    CHECK(aOn > 0, "sarai/throws_when_low");
}

void testKurageLongAttack()
{
    // bot-v2 gap 3 (Kurage 57): high HP -> attack window is multiplied, and
    // throws rotate (aim varies) instead of a fixed pulse.
    p2autoplay::Config cfg;
    cfg.attackTimeout = 1.0f;
    cfg.kurageAttackMultiplier = 3.0f;
    cfg.throwHold = 0.1f;
    cfg.throwGap = 0.2f;
    p2autoplay::Brain brain(cfg);
    p2autoplay::Senses s = liveSenses();
    s.fieldPikmin = 20;
    brain.update(0.05f, s);
    brain.update(0.05f, s); // -> select
    s.targetToken = 1787125272u;
    s.targetSource = 57;
    s.targetAlive = true;
    s.targetDist = 100.0f;
    s.naviX = 0.0f;
    s.naviZ = 0.0f;
    s.tgtX = 100.0f;
    s.tgtZ = 0.0f;
    brain.update(0.05f, s); // -> approach
    brain.update(0.05f, s); // -> attack
    for (int i = 0; i < 30; ++i) brain.update(0.05f, s); // 1.5s > base 1.0s
    CHECK(brain.current() == p2autoplay::State::Attack, "kurage/outlasts_base_timeout");
    int aOn = 0;
    float firstZ = 0.0f, lastZ = 0.0f;
    for (int i = 0; i < 20; ++i) { // total 2.5s < 3.0s extended window
        brain.update(0.05f, s);
        const p2autoplay::Command cmd = brain.command();
        if (cmd.buttons & unsigned(p2autoplay::PadA)) ++aOn;
        if (i == 0) firstZ = cmd.moveZ;
        lastZ = cmd.moveZ;
    }
    CHECK(aOn > 0, "kurage/keeps_throwing");
    CHECK(firstZ != lastZ, "kurage/rotates_throws");
    // Control: a non-Kurage target times out at the base window (the Brain
    // re-engages the same live target afterwards, so pin the timeout markers
    // rather than the transient Select state).
    p2autoplay::Brain plain(cfg);
    plain.update(0.05f, s);
    plain.update(0.05f, s);
    p2autoplay::Senses s2 = s;
    s2.targetToken = 999001;
    s2.targetSource = 44;
    plain.update(0.05f, s2);
    plain.update(0.05f, s2);
    std::vector<std::string> plainMarkers;
    for (int i = 0; i < 40; ++i) {
        plain.update(0.05f, s2);
        const std::vector<std::string> got = plain.takeMarkers();
        plainMarkers.insert(plainMarkers.end(), got.begin(), got.end());
    }
    CHECK(hasMarker(plainMarkers, "AUTOPLAY_GIVEUP reason=attack_timeout"),
          "kurage/control_times_out_at_base");
    CHECK(hasMarker(plainMarkers, "AUTOPLAY_RESULT target=999001 damaged=0 killed=0 carried=0"),
          "kurage/control_no_claims");
}

void testKoganePathNeedsEngagement()
{
    // wf10-v2-1 Otakara lesson: a mid-fight source switch onto Kogane must
    // not score the stale (non-Kogane) engagement down the damage-and-move-on
    // path. The Kogane path needs BOTH the engagement-time flag and live senses.
    p2autoplay::Config cfg;
    cfg.koganeConfirm = 0.5f;
    p2autoplay::Brain brain(cfg);
    p2autoplay::Senses s = liveSenses();
    s.fieldPikmin = 20;
    brain.update(0.05f, s);
    brain.update(0.05f, s); // -> select
    s.targetToken = 3921089765u;
    s.targetSource = 59; // engaged as FireOtakara (not Kogane-like)
    s.targetAlive = true;
    s.targetDist = 100.0f;
    s.targetHealthFrac = 0.8f; // damage observed
    brain.update(0.05f, s); // -> approach
    brain.update(0.05f, s); // -> attack
    CHECK(brain.current() == p2autoplay::State::Attack, "kogane-guard/attacks");
    // Mid-fight source switch to Kogane with damage: must NOT move on.
    s.targetSource = 9;
    s.targetDamagedLatch = true;
    for (int i = 0; i < 30; ++i) brain.update(0.05f, s);
    CHECK(brain.current() == p2autoplay::State::Attack, "kogane-guard/no_move_on_for_stale_token");
    CHECK(!hasMarker(brain.takeMarkers(), "AUTOPLAY_RESULT"), "kogane-guard/no_stale_result");
}

void testReplanRepeats()
{    // bot-v2 gap 2: every STUCK window replans (far targets get repeated
    // graph replans, not just one detour).
    p2autoplay::Config cfg;
    cfg.approachTimeout = 30.0f;
    cfg.stuckWindow = 0.2f;
    cfg.stuckMinProgress = 30.0f;
    p2autoplay::Brain brain(cfg);
    p2autoplay::Senses s = liveSenses();
    s.fieldPikmin = 20;
    brain.update(0.05f, s);
    brain.update(0.05f, s); // -> select
    s.targetToken = 3886812794u;
    s.targetSource = 9;
    s.targetAlive = true;
    s.targetDist = 1290.0f;
    s.tgtX = 1290.0f;
    s.tgtZ = 0.0f;
    brain.update(0.05f, s); // -> approach
    int stuck = 0, replans = 0;
    for (int i = 0; i < 12; ++i) {
        brain.update(0.05f, s);
        for (const std::string& m : brain.takeMarkers()) {
            if (m.find("AUTOPLAY_STUCK") != std::string::npos) ++stuck;
        }
        if (brain.replanWanted()) {
            ++replans;
            brain.clearReplan(); // driver replans
        }
    }
    CHECK(stuck >= 2, "replan/repeats_on_stuck");
    CHECK(replans >= 2, "replan/replan_every_window");
}

void testStuckCarriesNaviPos()
{
    // bot-v3: STUCK lines prove whether the captain moves under stick input.
    p2autoplay::Config cfg;
    cfg.approachTimeout = 30.0f;
    cfg.stuckWindow = 0.2f;
    cfg.stuckMinProgress = 30.0f;
    p2autoplay::Brain brain(cfg);
    p2autoplay::Senses s = liveSenses();
    s.fieldPikmin = 20;
    brain.update(0.05f, s);
    brain.update(0.05f, s); // -> select
    s.targetToken = 424242u;
    s.targetSource = 44;
    s.targetAlive = true;
    s.targetDist = 1000.0f;
    s.naviX = -200.0f;
    s.naviZ = 70.0f;
    s.tgtX = 800.0f;
    s.tgtZ = 0.0f;
    brain.update(0.05f, s); // -> approach
    std::vector<std::string> markers;
    for (int i = 0; i < 12; ++i) {
        brain.update(0.05f, s);
        const std::vector<std::string> got = brain.takeMarkers();
        markers.insert(markers.end(), got.begin(), got.end());
        if (brain.replanWanted()) brain.clearReplan();
    }
    CHECK(hasMarker(markers, "AUTOPLAY_STUCK state=approach"), "stuckpos/approach_marker");
    CHECK(hasMarker(markers, "navi=(-200,70)"), "stuckpos/approach_navi_pos");
    CHECK(hasMarker(markers, "replan="), "stuckpos/approach_replan_count");
}

void testUnreachableGiveup()
{
    // bot-v3: N consecutive no-progress STUCK windows in one Approach stint
    // is GIVEUP reason=target_unreachable (then RESULT, no kill claims).
    // Progress resets the count.
    p2autoplay::Config cfg;
    cfg.approachTimeout = 60.0f;
    cfg.stuckWindow = 0.2f;
    cfg.stuckMinProgress = 30.0f;
    cfg.maxApproachReplans = 3;
    p2autoplay::Brain brain(cfg);
    p2autoplay::Senses s = liveSenses();
    s.fieldPikmin = 20;
    brain.update(0.05f, s);
    brain.update(0.05f, s); // -> select
    s.targetToken = 515001u;
    s.targetSource = 44;
    s.targetAlive = true;
    s.targetDist = 1000.0f;
    s.tgtX = 1000.0f;
    s.tgtZ = 0.0f;
    brain.update(0.05f, s); // -> approach
    std::vector<std::string> markers;
    for (int i = 0; i < 40; ++i) {
        brain.update(0.05f, s);
        const std::vector<std::string> got = brain.takeMarkers();
        markers.insert(markers.end(), got.begin(), got.end());
        if (brain.replanWanted()) brain.clearReplan();
    }
    CHECK(hasMarker(markers, "AUTOPLAY_GIVEUP reason=target_unreachable"),
          "unreachable/giveup_logged");
    CHECK(hasMarker(markers, "AUTOPLAY_RESULT target=515001 damaged=0 killed=0 carried=0"),
          "unreachable/result_no_claims");

    // Progress resets the out-of-reach count: two STUCK, then a big close,
    // then two more STUCK must NOT give up (count restarted).
    p2autoplay::Brain brain2(cfg);
    brain2.update(0.05f, s);
    brain2.update(0.05f, s);
    p2autoplay::Senses s2 = s;
    s2.targetDist = 1000.0f;
    brain2.update(0.05f, s2); // -> approach
    for (int i = 0; i < 8; ++i) { // ~2 STUCK windows, no progress
        brain2.update(0.05f, s2);
        if (brain2.replanWanted()) brain2.clearReplan();
    }
    brain2.takeMarkers();
    s2.targetDist = 500.0f; // real progress: well past stuckMinProgress
    for (int i = 0; i < 4; ++i) { // one window carrying the progress: resets the count
        brain2.update(0.05f, s2);
        if (brain2.replanWanted()) brain2.clearReplan();
    }
    brain2.takeMarkers();
    std::vector<std::string> m2;
    for (int i = 0; i < 8; ++i) { // two more STUCK after progress: count restarts (2 < 3)
        brain2.update(0.05f, s2);
        const std::vector<std::string> got = brain2.takeMarkers();
        m2.insert(m2.end(), got.begin(), got.end());
        if (brain2.replanWanted()) brain2.clearReplan();
    }
    CHECK(!hasMarker(m2, "target_unreachable"), "unreachable/progress_resets_count");
}

void testContainerGuards()
{
    // bot-v3 root-cause fix: leaving the withdraw menu (or engaging) while
    // the Onion container UI is open strands the navi in NAVISTATE_Container
    // (stick drives the UI, velocity stays 0: bc2 38x identical detour).
    // Select/Approach/Attack/Aftermath/Done with containerOpen bounce back
    // to WithdrawMenu; the menu never leaves while open.
    p2autoplay::Config cfg;
    p2autoplay::Brain brain(cfg);
    p2autoplay::Senses s = liveSenses();
    s.fieldPikmin = 20;
    brain.update(0.05f, s); // idle -> withdraw_seek
    brain.update(0.05f, s); // -> select
    CHECK(brain.current() == p2autoplay::State::Select, "containerguard/reaches_select");
    // Select with the UI open: back to the menu, never to approach.
    s.containerOpen = true;
    s.targetToken = 616001u;
    s.targetSource = 44;
    s.targetAlive = true;
    s.targetDist = 500.0f;
    brain.update(0.05f, s);
    CHECK(brain.current() == p2autoplay::State::WithdrawMenu, "containerguard/select_bounces_to_menu");
    // Full squad but the UI still open: the menu must NOT leave dirty (old
    // code entered Select after 4s); it keeps confirming until close.
    for (int i = 0; i < 100; ++i) brain.update(0.05f, s);
    CHECK(brain.current() == p2autoplay::State::WithdrawMenu, "containerguard/menu_waits_for_close");
    // Close the UI with a full squad: menu confirms, then leaves for select.
    s.containerOpen = false;
    s.onionStored = 0;
    for (int i = 0; i < 20 && brain.current() == p2autoplay::State::WithdrawMenu; ++i) {
        brain.update(0.05f, s);
    }
    CHECK(brain.current() == p2autoplay::State::Select, "containerguard/menu_leaves_when_closed");
    // Approach with the UI open: back to the menu (no steering freeze).
    brain.update(0.05f, s); // -> approach (closed, target live)
    CHECK(brain.current() == p2autoplay::State::Approach, "containerguard/enters_approach");
    s.containerOpen = true;
    brain.update(0.05f, s);
    CHECK(brain.current() == p2autoplay::State::WithdrawMenu, "containerguard/approach_bounces_to_menu");
    // Done with the UI open: back to the menu as well.
    p2autoplay::Brain brain2(cfg);
    brain2.update(0.05f, s);
    p2autoplay::Senses s2 = liveSenses();
    s2.fieldPikmin = 20;
    s2.containerOpen = false;
    brain2.update(0.05f, s2);
    s2.targetToken = 0;
    s2.targetAlive = false;
    brain2.update(0.05f, s2);
    CHECK(brain2.current() == p2autoplay::State::Done, "containerguard/enters_done");
    s2.containerOpen = true;
    brain2.update(0.05f, s2);
    CHECK(brain2.current() == p2autoplay::State::WithdrawMenu, "containerguard/done_bounces_to_menu");
}

void testDoneIdlesNearOnion()
{
    // bot-v3: Done (no targets left) steers back toward the Onion instead of
    // standing still; close to the Onion it goes neutral.
    p2autoplay::Config cfg;
    p2autoplay::Brain brain(cfg);
    p2autoplay::Senses s = liveSenses();
    s.fieldPikmin = 20;
    brain.update(0.05f, s); // idle -> withdraw_seek
    brain.update(0.05f, s); // withdraw_seek -> select (squad ready)
    CHECK(brain.current() == p2autoplay::State::Select, "done-idle/reaches_select");
    s.targetToken = 0; // no targets left
    s.targetAlive = false;
    brain.update(0.05f, s);
    CHECK(brain.current() == p2autoplay::State::Done, "done-idle/enters_done");
    s.hasOnion = true;
    s.naviX = 0.0f;
    s.naviZ = 0.0f;
    s.onionX = 400.0f;
    s.onionZ = 0.0f;
    s.onionDist = 400.0f;
    brain.update(0.05f, s);
    const p2autoplay::Command far = brain.command();
    CHECK(far.moveX > 0.9f && std::fabs(far.moveZ) < 0.01f, "done-idle/steers_to_onion");
    s.onionDist = 10.0f;
    s.onionX = 10.0f;
    brain.update(0.05f, s);
    const p2autoplay::Command near = brain.command();
    CHECK(near.moveX == 0.0f && near.moveZ == 0.0f, "done-idle/neutral_when_close");
}

} // namespace

int main()
{
    testGate();
    testInertWhenUnset();
    testWithdrawFlow();
    testWithdrawKeepsClosing();
    testWithdrawMenuHoldThenConfirm();
    testCombatFlow();
    testKoganeMovesOn();
    testTimeoutsAndStuck();
    testTargetMatching();
    testKoganePathNeedsEngagement();
    testReceiptWait();
    testGenericDeath();
    testWithdrawRepeat();
    testSaraiFlyer();
    testKurageLongAttack();
    testReplanRepeats();
    testStuckCarriesNaviPos();
    testUnreachableGiveup();
    testContainerGuards();
    testDoneIdlesNearOnion();
    if (failures == 0) {
        std::printf("PASS p2_autoplay\n");
        return 0;
    }
    std::printf("FAIL p2_autoplay failures=%d\n", failures);
    return 1;
}
