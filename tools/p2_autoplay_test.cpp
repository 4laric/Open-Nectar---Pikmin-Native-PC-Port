// TEST-ONLY autoplay bot policy test (bot-impl wf9, bot-v2/v3/v4 wf10, bot-v5 wf10). Engine-free.
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
//   * bot-v5: aftermath never whistles (release B), walks onto the corpse
//     (contact ring) + throws to seed grabs, backs off, re-throws bounded
//     times; a grabbed-but-stalled lift re-throws to grow the crew; receipt
//     window extends only while carriers>0 AND the corpse moves now;
//     giveups name the broken link; received=1 only from the token's
//     own ledger receipt.
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
    // Timeout with no receipt: kill claimed, carry/receipt refused. The lift
    // was seen (transport) but the corpse never moved: bot-v5 names the stall
    // (carry_stalled) instead of the generic receipt_timeout.
    std::vector<std::string> markers;
    for (int i = 0; i < 60 && brain.current() == p2autoplay::State::Aftermath; ++i) {
        brain.update(0.05f, s);
        const std::vector<std::string> got = brain.takeMarkers();
        markers.insert(markers.end(), got.begin(), got.end());
    }
    CHECK(hasMarker(markers, "AUTOPLAY_GIVEUP reason=carry_stalled"), "receipt/stall_logged");
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

void testPowerGate()
{
    // bot-v4: PIKMIN_RANDOMIZER_AUTOPLAY_POWER is off by default and ONLY
    // meaningful when the autoplay gate is already on (inert in normal play).
    setEnv("PIKMIN_RANDOMIZER_AUTOPLAY", nullptr);
    setEnv("PIKMIN_RANDOMIZER_AUTOPLAY_POWER", nullptr);
    CHECK(!p2autoplay::isPowerEnabled(), "power/off_by_default");
    CHECK(p2autoplay::powerDamageMult() == 1.0f, "power/mult_one_when_off");
    // Inert in normal play: POWER set but the autoplay gate closed.
    setEnv("PIKMIN_RANDOMIZER_AUTOPLAY_POWER", "10");
    CHECK(!p2autoplay::isPowerEnabled(), "power/inert_when_gate_closed");
    CHECK(p2autoplay::powerDamageMult() == 1.0f, "power/mult_one_when_gate_closed");
    setEnv("PIKMIN_RANDOMIZER_AUTOPLAY", "0");
    CHECK(!p2autoplay::isPowerEnabled(), "power/inert_when_gate_zero");
    // Gate open, POWER unset: still off.
    setEnv("PIKMIN_RANDOMIZER_AUTOPLAY", "1");
    setEnv("PIKMIN_RANDOMIZER_AUTOPLAY_POWER", nullptr);
    CHECK(!p2autoplay::isPowerEnabled(), "power/off_when_unset");
    // Gate open + POWER on: numeric value configures the multiplier, any
    // other non-empty non-"0" value means on with the default x10.
    setEnv("PIKMIN_RANDOMIZER_AUTOPLAY_POWER", "1");
    CHECK(p2autoplay::isPowerEnabled(), "power/on_with_gate");
    CHECK(p2autoplay::powerDamageMult() == 1.0f, "power/numeric_one_is_x1");
    setEnv("PIKMIN_RANDOMIZER_AUTOPLAY_POWER", "on");
    CHECK(p2autoplay::powerDamageMult() == 10.0f, "power/default_x10");
    setEnv("PIKMIN_RANDOMIZER_AUTOPLAY_POWER", "7.5");
    CHECK(p2autoplay::isPowerEnabled(), "power/on_with_number");
    CHECK(std::fabs(p2autoplay::powerDamageMult() - 7.5f) < 0.001f, "power/numeric_configures_mult");
    setEnv("PIKMIN_RANDOMIZER_AUTOPLAY_POWER", "0");
    CHECK(!p2autoplay::isPowerEnabled(), "power/zero_is_off");
    // Effective squad: ~100 in power mode, cfg.wantSquad otherwise.
    p2autoplay::Config cfg;
    setEnv("PIKMIN_RANDOMIZER_AUTOPLAY_POWER", "10");
    CHECK(p2autoplay::effectiveWantSquad(cfg) == 100, "power/want_100");
    setEnv("PIKMIN_RANDOMIZER_AUTOPLAY_POWER", nullptr);
    CHECK(p2autoplay::effectiveWantSquad(cfg) == cfg.wantSquad, "power/want_normal_when_off");

    // bot-v4b Onion stock delta: pure function, inert when power is off.
    CHECK(p2autoplay::powerStockTarget() == 100, "power/stock_target_100");
    // Inert when either gate is unset: delta is 0 no matter the counts.
    setEnv("PIKMIN_RANDOMIZER_AUTOPLAY", nullptr);
    setEnv("PIKMIN_RANDOMIZER_AUTOPLAY_POWER", "10");
    CHECK(p2autoplay::powerStockDelta(p2autoplay::isPowerEnabled(), 20, 0, 20, 100) == 0,
          "power/stock_inert_when_gate_closed");
    setEnv("PIKMIN_RANDOMIZER_AUTOPLAY", "1");
    setEnv("PIKMIN_RANDOMIZER_AUTOPLAY_POWER", nullptr);
    CHECK(p2autoplay::powerStockDelta(p2autoplay::isPowerEnabled(), 20, 0, 20, 100) == 0,
          "power/stock_inert_when_power_unset");
    setEnv("PIKMIN_RANDOMIZER_AUTOPLAY_POWER", "0");
    CHECK(p2autoplay::powerStockDelta(p2autoplay::isPowerEnabled(), 20, 0, 20, 100) == 0,
          "power/stock_inert_when_power_zero");
    // Enabled: fresh boot (stored 20, field 0) tops up 80 to reach 100.
    setEnv("PIKMIN_RANDOMIZER_AUTOPLAY_POWER", "10");
    CHECK(p2autoplay::powerStockDelta(p2autoplay::isPowerEnabled(), 20, 0, 20, 100) == 80,
          "power/stock_80_from_boot");
    // Mid-run (stored 0, field 20 already withdrawn) still tops to 100 total.
    CHECK(p2autoplay::powerStockDelta(p2autoplay::isPowerEnabled(), 0, 20, 20, 100) == 80,
          "power/stock_80_mid_run");
    // Already full: no top-up. At the pool limit: no top-up.
    CHECK(p2autoplay::powerStockDelta(p2autoplay::isPowerEnabled(), 100, 0, 100, 100) == 0,
          "power/stock_none_when_full");
    CHECK(p2autoplay::powerStockDelta(p2autoplay::isPowerEnabled(), 20, 0, 100, 100) == 0,
          "power/stock_none_at_limit");
    // Capped by pool room, never overfills past the limit.
    CHECK(p2autoplay::powerStockDelta(p2autoplay::isPowerEnabled(), 20, 0, 90, 100) == 10,
          "power/stock_capped_by_limit");

    // RESULT tagging: power=1 on every result of a power run, absent otherwise.
    setEnv("PIKMIN_RANDOMIZER_AUTOPLAY_POWER", "10");
    {
        p2autoplay::Brain brain(cfg);
        p2autoplay::Senses s = liveSenses();
        s.fieldPikmin = 20;
        brain.update(0.05f, s);
        brain.update(0.05f, s); // -> select
        s.targetToken = 610001;
        s.targetSource = 79;
        s.targetAlive = true;
        s.targetDist = 100.0f;
        brain.update(0.05f, s); // -> approach
        brain.update(0.05f, s); // -> attack
        s.targetHealthFrac = 0.5f;
        brain.update(0.05f, s);
        s.targetAlive = false;
        s.transportSeen = true;
        brain.update(0.05f, s); // -> aftermath
        s.receiptSeen = true;
        std::vector<std::string> markers;
        for (int i = 0; i < 60 && brain.current() == p2autoplay::State::Aftermath; ++i) {
            brain.update(0.05f, s);
            const std::vector<std::string> got = brain.takeMarkers();
            markers.insert(markers.end(), got.begin(), got.end());
        }
        CHECK(hasMarker(markers, "power=1"), "power/result_tagged");
    }
    setEnv("PIKMIN_RANDOMIZER_AUTOPLAY_POWER", nullptr);
    {
        p2autoplay::Brain brain(cfg);
        p2autoplay::Senses s = liveSenses();
        s.fieldPikmin = 20;
        brain.update(0.05f, s);
        brain.update(0.05f, s); // -> select
        s.targetToken = 610002;
        s.targetSource = 79;
        s.targetAlive = true;
        s.targetDist = 100.0f;
        brain.update(0.05f, s); // -> approach
        brain.update(0.05f, s); // -> attack
        s.targetHealthFrac = 0.5f;
        brain.update(0.05f, s);
        s.targetAlive = false;
        s.transportSeen = true;
        brain.update(0.05f, s); // -> aftermath
        s.receiptSeen = true;
        std::vector<std::string> markers;
        for (int i = 0; i < 60 && brain.current() == p2autoplay::State::Aftermath; ++i) {
            brain.update(0.05f, s);
            const std::vector<std::string> got = brain.takeMarkers();
            markers.insert(markers.end(), got.begin(), got.end());
        }
        CHECK(hasMarker(markers, "AUTOPLAY_RESULT target=610002"), "power/control_result_logged");
        CHECK(!hasMarker(markers, "power=1"), "power/control_result_untagged");
    }
    setEnv("PIKMIN_RANDOMIZER_AUTOPLAY", nullptr);
    setEnv("PIKMIN_RANDOMIZER_AUTOPLAY_POWER", nullptr);
}

void testRegroupDistress()
{
    // bot-v4 regroup rule: distress (grabbed/thrown-off/burning) or a
    // scattered squad whistles first, then re-throws once the squad is back.
    // A plain attack-latch (targetGrabbing on a ground enemy) must NOT
    // whistle: those Pikmin are dealing damage, and recalling them stalls the
    // fight (v4dev-1 Chappy regression: permanent whistle, hp stuck at 0.96).
    p2autoplay::Config cfg;
    cfg.throwHold = 0.1f;
    cfg.throwGap = 0.2f;
    cfg.whistleHold = 0.2f;
    p2autoplay::Brain brain(cfg);
    p2autoplay::Senses s = liveSenses();
    s.fieldPikmin = 20;
    brain.update(0.05f, s);
    brain.update(0.05f, s); // -> select
    s.targetToken = 620001;
    s.targetSource = 44; // ground enemy
    s.targetAlive = true;
    s.targetDist = 100.0f;
    s.targetHealthFrac = 1.0f;
    brain.update(0.05f, s); // -> approach
    brain.update(0.05f, s); // -> attack
    CHECK(brain.current() == p2autoplay::State::Attack, "regroup/attacks");
    // Distress (thrown-off/burning): whistle takes over throwing.
    s.squadDistress = true;
    brain.update(0.05f, s);
    CHECK(brain.command().buttons & unsigned(p2autoplay::PadB), "regroup/distress_whistles");
    // Attack-latch on a ground enemy is not a grab: no whistle, throws continue.
    s.squadDistress = false;
    s.targetGrabbing = true;
    for (int i = 0; i < 10; ++i) brain.update(0.05f, s);
    CHECK(!(brain.command().buttons & unsigned(p2autoplay::PadB)), "regroup/latch_does_not_whistle");
    int aOn = 0;
    for (int i = 0; i < 40; ++i) {
        brain.update(0.05f, s);
        if (brain.command().buttons & unsigned(p2autoplay::PadA)) ++aOn;
    }
    CHECK(aOn > 0, "regroup/keeps_throwing_while_latched");
    // Sarai capture (flyer grab): whistle frees the grabbed Pikmin.
    p2autoplay::Brain brain2(cfg);
    brain2.update(0.05f, s);
    p2autoplay::Senses s2 = liveSenses();
    s2.fieldPikmin = 20;
    brain2.update(0.05f, s2);
    s2.targetToken = 620002;
    s2.targetSource = 23; // Sarai
    s2.targetAlive = true;
    s2.targetDist = 200.0f;
    s2.targetLow = true;
    s2.targetGrabbing = true;
    brain2.update(0.05f, s2); // -> approach
    brain2.update(0.05f, s2); // -> attack
    brain2.update(0.05f, s2); // whistle answers the grab
    CHECK(brain2.command().buttons & unsigned(p2autoplay::PadB), "regroup/sarai_grab_whistles");
    // Squad back: whistle releases and throws resume.
    s.squadDistress = false;
    s.targetGrabbing = false;
    for (int i = 0; i < 10; ++i) brain.update(0.05f, s);
    aOn = 0;
    for (int i = 0; i < 40; ++i) {
        brain.update(0.05f, s);
        if (brain.command().buttons & unsigned(p2autoplay::PadA)) ++aOn;
    }
    CHECK(aOn > 0, "regroup/rethrows_after_regroup");
}

void testResupply()
{
    // bot-v4 resupply rule: field below threshold + Onion stock => disengage
    // to WithdrawSeek with AUTOPLAY_RESUPPLY; no detour when the Onion is
    // empty or the squad is healthy.
    p2autoplay::Config cfg;
    cfg.resupplyThreshold = 5;
    p2autoplay::Brain brain(cfg);
    p2autoplay::Senses s = liveSenses();
    s.fieldPikmin = 20;
    brain.update(0.05f, s);
    brain.update(0.05f, s); // -> select
    s.targetToken = 630001;
    s.targetSource = 44;
    s.targetAlive = true;
    s.targetDist = 100.0f;
    s.targetHealthFrac = 1.0f;
    s.hasOnion = true;
    s.onionStored = 10;
    s.onionDist = 500.0f;
    s.onionX = -500.0f;
    s.onionZ = 0.0f;
    brain.update(0.05f, s); // -> approach
    brain.update(0.05f, s); // -> attack
    CHECK(brain.current() == p2autoplay::State::Attack, "resupply/attacks");
    s.fieldPikmin = 2; // squad eaten, Onion still stocks
    brain.update(0.05f, s);
    CHECK(brain.current() == p2autoplay::State::WithdrawSeek, "resupply/disengages_to_withdraw");
    CHECK(hasMarker(brain.takeMarkers(), "AUTOPLAY_RESUPPLY"), "resupply/marker_logged");
    // Back at the Onion with a fresh squad: the machine can re-engage.
    s.fieldPikmin = 15;
    s.onionDist = 10.0f;
    brain.update(0.05f, s);
    CHECK(brain.current() == p2autoplay::State::Select
              || brain.current() == p2autoplay::State::WithdrawSeek,
          "resupply/withdraws_then_selects");
    // Control: empty Onion never disengages (nothing to withdraw).
    p2autoplay::Brain brain2(cfg);
    brain2.update(0.05f, s);
    p2autoplay::Senses s2 = liveSenses();
    s2.fieldPikmin = 20;
    brain2.update(0.05f, s2);
    s2.targetToken = 630002;
    s2.targetSource = 44;
    s2.targetAlive = true;
    s2.targetDist = 100.0f;
    s2.hasOnion = true;
    s2.onionStored = 0;
    brain2.update(0.05f, s2);
    brain2.update(0.05f, s2);
    s2.fieldPikmin = 2;
    brain2.update(0.05f, s2);
    CHECK(brain2.current() == p2autoplay::State::Attack, "resupply/no_detour_when_empty");
    CHECK(!hasMarker(brain2.takeMarkers(), "AUTOPLAY_RESUPPLY"), "resupply/no_marker_when_empty");
}

void testAftermathEscortExtension()
{
    // bot-v4: a carry en route doubles the receipt window instead of timing
    // out while the corpse is still being carried (bc3 receipt_timeout).
    // bot-v5: the extension needs carriers > 0 AND the corpse moving (live);
    // a latched-but-stalled lift times out bounded with carry_stalled.
    p2autoplay::Config cfg;
    cfg.receiptTimeout = 1.0f;
    cfg.aftermathTimeout = 60.0f;
    cfg.whistleHold = 0.2f;
    p2autoplay::Brain brain(cfg);
    p2autoplay::Senses s = liveSenses();
    s.fieldPikmin = 20;
    brain.update(0.05f, s);
    brain.update(0.05f, s); // -> select
    s.targetToken = 640001;
    s.targetSource = 79;
    s.targetAlive = true;
    s.targetDist = 100.0f;
    brain.update(0.05f, s); // -> approach
    brain.update(0.05f, s); // -> attack
    s.targetHealthFrac = 0.5f;
    brain.update(0.05f, s);
    s.targetAlive = false;
    s.transportSeen = true; // corpse en route, no receipt yet
    s.corpseMoving = true; // ... and the corpse is actually moving
    s.receiptSeen = false;
    brain.update(0.05f, s);
    CHECK(brain.current() == p2autoplay::State::Aftermath, "escort/waits_after_kill");
    for (int i = 0; i < 30; ++i) brain.update(0.05f, s); // 1.5s > base 1.0s window
    CHECK(brain.current() == p2autoplay::State::Aftermath, "escort/outlasts_base_window_while_carrying");
    CHECK(!hasMarker(brain.takeMarkers(), "AUTOPLAY_RESULT"), "escort/no_early_result_while_carrying");

    // Control: carriers but no motion -> no extension, bounded stall giveup.
    p2autoplay::Brain stalled(cfg);
    stalled.update(0.05f, s);
    p2autoplay::Senses s2 = liveSenses();
    s2.fieldPikmin = 20;
    stalled.update(0.05f, s2); // -> select
    s2.targetToken = 640002;
    s2.targetSource = 79;
    s2.targetAlive = true;
    s2.targetDist = 100.0f;
    stalled.update(0.05f, s2); // -> approach
    stalled.update(0.05f, s2); // -> attack
    s2.targetHealthFrac = 0.5f;
    stalled.update(0.05f, s2);
    s2.targetAlive = false;
    s2.transportSeen = true; // lift seen...
    s2.corpseMoving = false; // ... but the corpse never moves
    s2.receiptSeen = false;
    stalled.update(0.05f, s2);
    std::vector<std::string> stalledMarkers;
    for (int i = 0; i < 60 && stalled.current() == p2autoplay::State::Aftermath; ++i) {
        stalled.update(0.05f, s2);
        const std::vector<std::string> got = stalled.takeMarkers();
        stalledMarkers.insert(stalledMarkers.end(), got.begin(), got.end());
    }
    CHECK(hasMarker(stalledMarkers, "AUTOPLAY_GIVEUP reason=carry_stalled"),
          "escort/stall_no_extension");
    CHECK(hasMarker(stalledMarkers, "AUTOPLAY_RESULT target=640002 damaged=1 killed=1 carried=0"),
          "escort/stall_no_carry_claim");
}

void testAftermathNoWhistle()
{
    // bot-v5 (v4b diagnosis): aftermath HOLDS whistle (B) while standing
    // 36-65 u from the corpse, so Pikmin gather at the navi and no carry
    // ever initiates. Aftermath must never whistle, even scattered / in
    // distress / grabbing: it releases B and delivers with stick + throws.
    p2autoplay::Config cfg;
    cfg.throwHold = 0.1f;
    cfg.throwGap = 0.2f;
    p2autoplay::Brain brain(cfg);
    p2autoplay::Senses s = liveSenses();
    s.fieldPikmin = 20;
    brain.update(0.05f, s);
    brain.update(0.05f, s); // -> select
    s.targetToken = 650001;
    s.targetSource = 2;
    s.targetAlive = true;
    s.targetDist = 100.0f;
    s.naviX = 0.0f;
    s.naviZ = 0.0f;
    s.tgtX = 100.0f;
    s.tgtZ = 0.0f;
    brain.update(0.05f, s); // -> approach
    brain.update(0.05f, s); // -> attack
    s.targetHealthFrac = 0.5f;
    brain.update(0.05f, s);
    s.targetAlive = false;
    s.targetDist = 50.0f; // corpse 50 u out
    s.tgtX = 50.0f;
    s.scattered = true; // worst case: scattered + distress + grab
    s.squadDistress = true;
    s.targetGrabbing = true;
    brain.update(0.05f, s);
    CHECK(brain.current() == p2autoplay::State::Aftermath, "nowhistle/aftermath");
    bool whistled = false, steered = false;
    for (int i = 0; i < 60; ++i) {
        brain.update(0.05f, s);
        const p2autoplay::Command cmd = brain.command();
        if (cmd.buttons & unsigned(p2autoplay::PadB)) whistled = true;
        if (cmd.moveX > 0.5f) steered = true; // walks ONTO the corpse, not idle
        if (brain.current() != p2autoplay::State::Aftermath) break;
    }
    CHECK(!whistled, "nowhistle/releases_B_despite_scatter");
    CHECK(steered, "nowhistle/closes_onto_corpse");
}

void testAftermathSeedBackoffRethrow()
{
    // bot-v5 delivery loop: seed (onto the corpse + throws), bounded wait,
    // back off out of contact, re-approach + re-throw bounded times, then a
    // named giveup (carry_no_grab) - all pad-only.
    p2autoplay::Config cfg;
    cfg.throwHold = 0.1f;
    cfg.throwGap = 0.2f;
    cfg.receiptTimeout = 60.0f;
    cfg.aftermathTimeout = 60.0f;
    cfg.carryGrabWait = 0.5f;
    cfg.aftermathSettleWait = 0.5f;
    cfg.aftermathRethrowMax = 2;
    cfg.aftermathApproachRadius = 60.0f;
    cfg.aftermathBackoffDist = 200.0f;
    p2autoplay::Brain brain(cfg);
    p2autoplay::Senses s = liveSenses();
    s.fieldPikmin = 20;
    brain.update(0.05f, s);
    brain.update(0.05f, s); // -> select
    s.targetToken = 650002;
    s.targetSource = 27;
    s.targetAlive = true;
    s.targetDist = 300.0f;
    s.naviX = 0.0f;
    s.naviZ = 0.0f;
    s.tgtX = 300.0f;
    s.tgtZ = 0.0f;
    brain.update(0.05f, s); // -> approach
    brain.update(0.05f, s); // -> attack
    s.targetHealthFrac = 0.5f;
    brain.update(0.05f, s);
    s.targetAlive = false; // kill: corpse 300 u out, nobody grabs it
    s.transportSeen = false;
    s.carryCount = 0;
    s.pelletCarriers = 0;
    s.corpseMoving = false;
    s.receiptSeen = false;
    brain.update(0.05f, s);
    CHECK(brain.current() == p2autoplay::State::Aftermath, "seed/aftermath");
    // Far: steers onto the corpse and throws once in range.
    brain.update(0.05f, s);
    CHECK(brain.command().moveX > 0.5f, "seed/steers_onto_corpse");
    s.targetDist = 50.0f; // closed into the contact ring
    s.tgtX = 50.0f;
    int aOn = 0;
    for (int i = 0; i < 12; ++i) {
        brain.update(0.05f, s);
        if (brain.command().buttons & unsigned(p2autoplay::PadA)) ++aOn;
    }
    CHECK(aOn > 0, "seed/throws_at_corpse");
    // Bounded wait on the corpse with no grab: backs off (steers AWAY).
    bool backed = false;
    for (int i = 0; i < 20 && !backed; ++i) {
        brain.update(0.05f, s);
        if (brain.command().moveX < -0.5f) backed = true;
    }
    CHECK(backed, "seed/backs_off_when_no_grab");
    // Settle out of contact (or settle wait): re-approach + re-throw logged.
    std::vector<std::string> markers;
    s.targetDist = 200.0f; // reached backoff distance
    for (int i = 0; i < 5; ++i) {
        brain.update(0.05f, s);
        const std::vector<std::string> got = brain.takeMarkers();
        markers.insert(markers.end(), got.begin(), got.end());
    }
    CHECK(hasMarker(markers, "AUTOPLAY_RETHROW token=650002 attempt=1"), "seed/rethrow_logged");
    // Window expiry far from the corpse with no grab: named giveup, kill
    // kept, no carry claim. (Seed never reaches the contact ring at
    // tdist=200, so the bounded window, not a phase cap, ends it.)
    for (int i = 0; i < 1400 && brain.current() == p2autoplay::State::Aftermath; ++i) {
        brain.update(0.05f, s);
        const std::vector<std::string> got = brain.takeMarkers();
        markers.insert(markers.end(), got.begin(), got.end());
    }
    CHECK(hasMarker(markers, "AUTOPLAY_GIVEUP reason=carry_no_grab"), "seed/giveup_names_no_grab");
    CHECK(hasMarker(markers, "AUTOPLAY_RESULT target=650002 damaged=1 killed=1 carried=0"),
          "seed/result_no_carry_claim");
    int rethrows = 0;
    for (const std::string& m : markers) {
        if (m.find("AUTOPLAY_RETHROW") != std::string::npos) ++rethrows;
    }
    CHECK(rethrows <= 2, "seed/rethrows_bounded");
}

void testAftermathEscortNoThrows()
{
    // bot-v5 escort: while the carry is active the bot follows the corpse
    // (steers when far) and stops throwing so the crew keeps hauling.
    p2autoplay::Config cfg;
    cfg.receiptTimeout = 60.0f;
    cfg.throwHold = 0.1f;
    cfg.throwGap = 0.2f;
    p2autoplay::Brain brain(cfg);
    p2autoplay::Senses s = liveSenses();
    s.fieldPikmin = 20;
    brain.update(0.05f, s);
    brain.update(0.05f, s); // -> select
    s.targetToken = 650003;
    s.targetSource = 79;
    s.targetAlive = true;
    s.targetDist = 100.0f;
    s.naviX = 0.0f;
    s.naviZ = 0.0f;
    s.tgtX = 500.0f;
    s.tgtZ = 0.0f;
    brain.update(0.05f, s); // -> approach
    brain.update(0.05f, s); // -> attack
    s.targetHealthFrac = 0.5f;
    brain.update(0.05f, s);
    s.targetAlive = false;
    s.transportSeen = true;
    s.carryCount = 6;
    s.corpseMoving = true;
    s.receiptSeen = false;
    s.targetDist = 500.0f;
    brain.update(0.05f, s);
    CHECK(brain.current() == p2autoplay::State::Aftermath, "escort2/aftermath");
    int aOn = 0;
    bool followed = false;
    for (int i = 0; i < 20; ++i) {
        brain.update(0.05f, s);
        const p2autoplay::Command cmd = brain.command();
        if (cmd.buttons & unsigned(p2autoplay::PadA)) ++aOn;
        if (cmd.buttons & unsigned(p2autoplay::PadB)) aOn += 1000; // whistle forbidden too
        if (cmd.moveX > 0.5f) followed = true;
    }
    CHECK(aOn == 0, "escort2/no_throws_no_whistle_while_hauling");
    CHECK(followed, "escort2/follows_corpse");
    CHECK(brain.current() == p2autoplay::State::Aftermath, "escort2/keeps_escorting");
}

void testAftermathGiveupReasons()
{    // bot-v5: the giveup names the broken link (despawned / no grab /
    // stalled / out of reach / slow receipt).
    p2autoplay::Config cfg;
    cfg.receiptTimeout = 0.5f;
    cfg.aftermathTimeout = 60.0f;
    cfg.carryGrabWait = 100.0f; // stay seeding: reach the window, not a phase cap
    cfg.aftermathRethrowMax = 100;
    cfg.corpseOutOfReach = 800.0f;
    const auto runKill = [&](unsigned token, p2autoplay::Senses s, float corpseDist) {
        p2autoplay::Brain* brain = new p2autoplay::Brain(cfg);
        brain->update(0.05f, s);
        brain->update(0.05f, s); // -> select
        s.targetToken = token;
        s.targetSource = 2;
        s.targetAlive = true;
        s.targetDist = 100.0f;
        brain->update(0.05f, s); // -> approach
        brain->update(0.05f, s); // -> attack
        s.targetHealthFrac = 0.5f;
        brain->update(0.05f, s);
        s.targetAlive = false;
        s.targetDist = corpseDist; // aftermath sees the corpse at this distance
        s.receiptSeen = false;
        brain->update(0.05f, s); // -> aftermath
        std::vector<std::string> markers;
        for (int i = 0; i < 120 && brain->current() == p2autoplay::State::Aftermath; ++i) {
            brain->update(0.05f, s);
            const std::vector<std::string> got = brain->takeMarkers();
            markers.insert(markers.end(), got.begin(), got.end());
        }
        delete brain;
        return markers;
    };
    // Corpse gone entirely (no body, no pellet, no carry): despawned.
    p2autoplay::Senses d = liveSenses();
    d.fieldPikmin = 20;
    d.pelletExists = false;
    d.transportSeen = false;
    d.carryCount = 0;
    d.targetDist = 100.0f;
    CHECK(hasMarker(runKill(651001, d, 100.0f), "reason=corpse_despawned"), "reasons/despawned");
    // Present corpse, nobody grabs, far away: out of reach.
    p2autoplay::Senses f = liveSenses();
    f.fieldPikmin = 20;
    f.pelletExists = true;
    f.transportSeen = false;
    f.carryCount = 0;
    f.targetDist = 900.0f;
    CHECK(hasMarker(runKill(651002, f, 900.0f), "reason=corpse_out_of_reach"), "reasons/out_of_reach");
    // Present corpse, moving haul, receipt just slow: receipt_timeout kept.
    p2autoplay::Senses r = liveSenses();
    r.fieldPikmin = 20;
    r.pelletExists = true;
    r.transportSeen = true;
    r.carryCount = 4;
    r.corpseMoving = true;
    r.targetDist = 100.0f;
    p2autoplay::Config cfg2 = cfg;
    cfg2.receiptTimeout = 0.5f; // base 0.5, extended 1.0: loop 120 ticks (6 s) covers it
    p2autoplay::Brain brainR(cfg2);
    brainR.update(0.05f, r);
    brainR.update(0.05f, r);
    p2autoplay::Senses r2 = r;
    r2.targetToken = 651003;
    r2.targetSource = 2;
    r2.targetAlive = true;
    brainR.update(0.05f, r2);
    brainR.update(0.05f, r2);
    r2.targetHealthFrac = 0.5f;
    brainR.update(0.05f, r2);
    r2.targetAlive = false;
    brainR.update(0.05f, r2);
    std::vector<std::string> mr;
    for (int i = 0; i < 120 && brainR.current() == p2autoplay::State::Aftermath; ++i) {
        brainR.update(0.05f, r2);
        const std::vector<std::string> got = brainR.takeMarkers();
        mr.insert(mr.end(), got.begin(), got.end());
    }
    CHECK(hasMarker(mr, "reason=receipt_timeout"), "reasons/slow_receipt");
}

void testReceiptPerTokenOnly()
{
    // bot-v5: RESULT received=1 comes ONLY from this token's own ledger
    // receipt (receiptSeen). A kill + visible carry with NO receipt scores
    // carried=0 received=0: bystander CHECK Bestiary:Deliver lines (which the
    // harness used to count) must never flip the bot's verdict.
    p2autoplay::Config cfg;
    cfg.receiptTimeout = 0.5f;
    cfg.whistleHold = 0.2f;
    p2autoplay::Brain brain(cfg);
    p2autoplay::Senses s = liveSenses();
    s.fieldPikmin = 20;
    brain.update(0.05f, s);
    brain.update(0.05f, s); // -> select
    s.targetToken = 652001;
    s.targetSource = 30; // Queen: v4b saw a bystander Spotty-Bulborb deliver CHECK here
    s.targetAlive = true;
    s.targetDist = 100.0f;
    brain.update(0.05f, s); // -> approach
    brain.update(0.05f, s); // -> attack
    s.targetHealthFrac = 0.4f;
    brain.update(0.05f, s);
    s.targetAlive = false;
    s.transportSeen = true; // crew on it...
    s.receiptSeen = false; // ... but no ledger line for THIS token
    brain.update(0.05f, s);
    std::vector<std::string> markers;
    for (int i = 0; i < 120 && brain.current() == p2autoplay::State::Aftermath; ++i) {
        brain.update(0.05f, s);
        const std::vector<std::string> got = brain.takeMarkers();
        markers.insert(markers.end(), got.begin(), got.end());
    }
    CHECK(hasMarker(markers, "AUTOPLAY_RESULT target=652001 damaged=1 killed=1 carried=0"),
          "pertoken/carry_refused_without_ledger");
    CHECK(hasMarker(markers, "received=0"), "pertoken/received_refused_without_ledger");
    CHECK(!hasMarker(markers, "received=1"), "pertoken/no_bystander_receipt");
}

void testAftermathStallRethrow()
{
    // bot-v5 (v5dev-1 Chappy/Kurage lesson): a grabbed-but-stalled lift (min
    // carriers not met - the crew holds a corpse it cannot lift) re-throws to
    // grow the crew instead of escorting it forever. Recent motion gates the
    // window: moved-then-stopped names carry_stalled, not receipt_timeout.
    p2autoplay::Config cfg;
    cfg.throwHold = 0.1f;
    cfg.throwGap = 0.2f;
    cfg.receiptTimeout = 60.0f;
    cfg.aftermathTimeout = 60.0f;
    cfg.carryStallWait = 0.5f;
    cfg.aftermathRethrowMax = 2;
    cfg.carryStallBurst = 0.5f;
    p2autoplay::Brain brain(cfg);
    p2autoplay::Senses s = liveSenses();
    s.fieldPikmin = 20;
    brain.update(0.05f, s);
    brain.update(0.05f, s); // -> select
    s.targetToken = 653001;
    s.targetSource = 2; // Chappy-weight corpse, light crew
    s.targetAlive = true;
    s.targetDist = 100.0f;
    s.naviX = 0.0f;
    s.naviZ = 0.0f;
    s.tgtX = 100.0f;
    s.tgtZ = 0.0f;
    brain.update(0.05f, s); // -> approach
    brain.update(0.05f, s); // -> attack
    s.targetHealthFrac = 0.5f;
    brain.update(0.05f, s);
    s.targetAlive = false; // kill: 2 carriers grab but cannot lift
    s.transportSeen = true;
    s.carryCount = 2;
    s.corpseMoving = true; // hauling at first...
    s.corpseMoved = true;
    s.receiptSeen = false;
    brain.update(0.05f, s);
    CHECK(brain.current() == p2autoplay::State::Aftermath, "stall/aftermath");
    // Escort while the corpse moves (advance the fix each tick so the stall
    // watch sees motion, like the driver's live pellet tracking).
    for (int i = 0; i < 10; ++i) {
        s.tgtX += 10.0f;
        brain.update(0.05f, s);
    }
    CHECK(brain.current() == p2autoplay::State::Aftermath, "stall/escorts_while_moving");
    // ... then the lift stalls (moved-ever, not moving now): freeze the fix.
    s.corpseMoving = false;
    std::vector<std::string> markers;
    for (int i = 0; i < 20; ++i) {
        brain.update(0.05f, s);
        const std::vector<std::string> got = brain.takeMarkers();
        markers.insert(markers.end(), got.begin(), got.end());
    }
    CHECK(hasMarker(markers, "AUTOPLAY_RETHROW token=653001 attempt=1"), "stall/rethrow_logged");
    CHECK(hasMarker(markers, "reason=stalled"), "stall/reason_names_stall");
    // Re-seeding throws again (pad-only) instead of idling the escort: the
    // burst refires every stall wait, so a loop spanning cycles sees throws.
    int aOn = 0;
    for (int i = 0; i < 40; ++i) {
        brain.update(0.05f, s);
        if (brain.command().buttons & unsigned(p2autoplay::PadA)) ++aOn;
        const std::vector<std::string> got = brain.takeMarkers();
        markers.insert(markers.end(), got.begin(), got.end());
    }
    CHECK(aOn > 0, "stall/rethrows_to_grow_crew");
    // Still stalled past the bounded budget: carry_stalled, kill kept.
    for (int i = 0; i < 200 && brain.current() == p2autoplay::State::Aftermath; ++i) {
        brain.update(0.05f, s);
        const std::vector<std::string> got = brain.takeMarkers();
        markers.insert(markers.end(), got.begin(), got.end());
    }
    CHECK(hasMarker(markers, "AUTOPLAY_GIVEUP reason=carry_stalled"), "stall/giveup_names_stall");
    CHECK(hasMarker(markers, "AUTOPLAY_RESULT target=653001 damaged=1 killed=1 carried=0"),
          "stall/result_no_carry_claim");
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
    testPowerGate();
    testRegroupDistress();
    testResupply();
    testAftermathEscortExtension();
    testAftermathNoWhistle();
    testAftermathSeedBackoffRethrow();
    testAftermathEscortNoThrows();
    testAftermathGiveupReasons();
    testReceiptPerTokenOnly();
    testAftermathStallRethrow();
    if (failures == 0) {
        std::printf("PASS p2_autoplay\n");
        return 0;
    }
    std::printf("FAIL p2_autoplay failures=%d\n", failures);
    return 1;
}
