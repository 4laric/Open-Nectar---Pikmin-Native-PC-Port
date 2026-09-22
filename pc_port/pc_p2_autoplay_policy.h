#pragma once

// TEST-ONLY headless autoplay bot policy (brief keys: bot-impl wf9, bot-v2 wf10).
//
// Engine-free state machine for the scripted player that drives the game
// through the NORMAL controller input path (synthesised pad state fed where
// the real pad is read: ControllerMgr::updateController honours
// pc_p2_input_script_override). The bot never teleports, never mode-forces,
// never mutates enemies/Pikmin: it only emits pad buttons + a desired world
// move direction, which the engine-linked driver
// (pc_port/pc_p2_autoplay.cpp) converts to a stick deflection through the
// live camera basis and publishes via pc_p2_input_script_set().
//
// Enabled only when PIKMIN_RANDOMIZER_AUTOPLAY is set (non-empty, != "0").
// The Brain also takes an explicit `enabled` sense: with enabled=false every
// update returns a neutral pad and stays IDLE, which is the
// inert-when-unset guarantee the native test pins (tools/p2_autoplay_test.cpp).
//
// bot-v2 (wf10) deltas, in brief priority order:
//   1. Onion receipt: Aftermath waits for the species' Onion receipt marker
//      (sensed as receiptSeen from the delivery ledger) or receiptTimeout;
//      RESULT carried=1 means a receipt line was seen, plus received=<0/1>.
//   2. Far targets: the driver plans over the routeMgr waypoint graph and
//      follows waypoint by waypoint (Brain honours waypointLeg + replan on
//      every STUCK window, not just the first).
//   3. Flyers: Sarai is only thrown at when low or holding a Pikmin (whistle
//      frees grabs); Kurage gets a longer attack window and rotating throws.
//   4. Full squad: the withdraw menu repeats until field>=wantSquad or the
//      Onion is empty (bounded by maxWithdrawCycles).
//   5. Kill claims: generic death latch (targetDead / health<=0 / !alive /
//      corpse) scores kills for every species, not just per-module markers.

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace p2autoplay {

// Pad button bits. Must match include/Controller.h KeyboardButtons exactly;
// pc_port/pc_p2_autoplay.cpp static_asserts the equality so a drift breaks
// the build instead of silently mis-driving input.
enum PadButton {
    PadCStickLeft = 1 << 0,
    PadCStickRight = 1 << 1,
    PadCStickUp = 1 << 2,
    PadCStickDown = 1 << 3,
    PadDPadLeft = 1 << 8,
    PadDPadRight = 1 << 9,
    PadDPadUp = 1 << 10,
    PadDPadDown = 1 << 11,
    PadA = 1 << 12,
    PadB = 1 << 13,
    PadX = 1 << 14,
    PadY = 1 << 15,
    PadZ = 1 << 16,
    PadL = 1 << 17,
    PadR = 1 << 18,
    PadMainUp = 1 << 19,
    PadMainRight = 1 << 20,
    PadMainDown = 1 << 21,
    PadMainLeft = 1 << 22,
    PadStart = 1 << 24,
};

// Env gating. Unset/empty/"0" means production: the driver returns before
// touching any input state.
inline bool isEnabled()
{
    const char* v = std::getenv("PIKMIN_RANDOMIZER_AUTOPLAY");
    return v && v[0] && std::strcmp(v, "0") != 0;
}

// Optional target filter: PIKMIN_RANDOMIZER_AUTOPLAY_TARGET=<generator key or
// species name>. Empty means "nearest live P2-bound teki".
inline std::string targetFilter()
{
    const char* v = std::getenv("PIKMIN_RANDOMIZER_AUTOPLAY_TARGET");
    return v ? std::string(v) : std::string();
}

enum class State {
    Idle = 0, // no input; waiting for enable + live captain
    WithdrawSeek, // walk to the stocked Onion
    WithdrawMenu, // Onion container UI: take Pikmin out, confirm
    Select, // (re)pick the current target
    Approach, // steer the stick toward the target (waypoint replan on stuck)
    Attack, // aim + throw (A); whistle (B) regroup when scattered
    Aftermath, // whistle back, let the corpse be carried
    Done, // no more targets: neutral input
};

inline const char* stateName(State s)
{
    switch (s) {
    case State::Idle: return "idle";
    case State::WithdrawSeek: return "withdraw_seek";
    case State::WithdrawMenu: return "withdraw_menu";
    case State::Select: return "select";
    case State::Approach: return "approach";
    case State::Attack: return "attack";
    case State::Aftermath: return "aftermath";
    case State::Done: return "done";
    }
    return "unknown";
}

// P2 source ids the bot knows by species name, for the TARGET filter and for
// per-species behaviour (Kogane never dies; flyers stay airborne).
// Values are the ENEMY_P2 source ids from the seed p2_layout bindings.
inline unsigned sourceForSpeciesName(const char* name)
{
    if (!name) return 0;
    struct Row {
        const char* name;
        unsigned source;
    };
    static const Row rows[] = {
        {"sokkuri", 79}, {"kogane", 9}, {"sarai", 23}, {"kurage", 57},
        {"miulin", 54}, {"bluekochappy", 44}, {"fireotakara", 59},
        {"waterotakara", 60}, {"gasotakara", 61}, {"elecotakara", 62},
    };
    // Case-insensitive compare, dash/space tolerant.
    char norm[64];
    size_t n = 0;
    for (const char* p = name; *p && n + 1 < sizeof(norm); ++p) {
        if (*p == ' ' || *p == '_' || *p == '-') continue;
        norm[n++] = char(*p >= 'A' && *p <= 'Z' ? *p + ('a' - 'A') : *p);
    }
    norm[n] = 0;
    for (const Row& row : rows) {
        if (std::strcmp(norm, row.name) == 0) return row.source;
    }
    return 0;
}

inline bool isKoganeLike(unsigned source) { return source == 9 || source == 10 || source == 11; }
inline bool isFlyer(unsigned source) { return source == 23 || source == 57; }

// Numeric filter matches a generator key; otherwise a species name match.
inline bool matchTarget(unsigned token, unsigned source, const char* species, const std::string& filter)
{
    if (filter.empty()) return true;
    char* end = nullptr;
    const unsigned long asNum = std::strtoul(filter.c_str(), &end, 10);
    if (end && *end == 0 && filter[0] >= '0' && filter[0] <= '9') {
        if (unsigned(asNum) == token) return true;
        // A bare source id also matches (e.g. TARGET=79 for any Sokkuri).
        if (unsigned(asNum) == source) return true;
        return false;
    }
    if (species && sourceForSpeciesName(filter.c_str()) == source) return true;
    if (species) {
        // Direct name compare as a fallback.
        char a[64], b[64];
        size_t i = 0;
        for (const char* p = species; *p && i + 1 < sizeof(a); ++p) {
            if (*p == ' ' || *p == '_' || *p == '-') continue;
            a[i++] = char(*p >= 'A' && *p <= 'Z' ? *p + ('a' - 'A') : *p);
        }
        a[i] = 0;
        size_t j = 0;
        for (const char* p = filter.c_str(); *p && j + 1 < sizeof(b); ++p) {
            if (*p == ' ' || *p == '_' || *p == '-') continue;
            b[j++] = char(*p >= 'A' && *p <= 'Z' ? *p + ('a' - 'A') : *p);
        }
        b[j] = 0;
        if (std::strcmp(a, b) == 0) return true;
    }
    return false;
}

struct Config {
    float withdrawTimeout = 150.0f; // walk to Onion + work the container UI
    float menuOpenTimeout = 12.0f; // wait for the container UI after pressing A
    float menuHoldDuration = 6.0f; // hold stick-down to accumulate withdraw delta
    float menuConfirmDuration = 2.0f; // pulse A to confirm after the hold
    float approachTimeout = 150.0f; // steer to one target
    float attackTimeout = 240.0f; // throw at one target
    float aftermathTimeout = 60.0f; // whistle back + let the corpse be carried
    float receiptTimeout = 180.0f; // after a kill, wait for the Onion receipt marker or this timeout
    float koganeConfirm = 20.0f; // after Kogane damage, watch escapes then move on
    float kurageAttackMultiplier = 2.0f; // Kurage has high HP: longer attack window
    float saraiLowHeight = 120.0f; // Sarai thrown at only when within this height above ground (or grabbing)
    float throwRange = 260.0f; // XZ distance at which throws start
    float arriveRadius = 90.0f; // XZ distance considered "at" the Onion
    float throwHold = 0.12f; // A held per throw pulse
    float throwGap = 0.55f; // gap between throw pulses
    float whistleHold = 1.6f; // B held to regroup / call back
    float stuckWindow = 4.0f; // no-progress window before STUCK + replan
    float stuckMinProgress = 30.0f; // XZ units that count as progress
    int wantSquad = 15; // withdrawn Pikmin before leaving the Onion
    int maxWithdrawCycles = 5; // repeat the withdraw menu until field>=wantSquad or Onion empty
};

// Plain-data senses gathered by the engine-linked driver each tick.
struct Senses {
    bool enabled = false; // PIKMIN_RANDOMIZER_AUTOPLAY gate
    bool naviAlive = false; // controlled captain exists and is alive
    float dt = 0.016f; // logical tick length (seconds)
    // Geometry (world XZ). The Brain steers in world space; the driver
    // converts the move vector through the live camera into stick bytes.
    float naviX = 0.0f;
    float naviZ = 0.0f;
    bool hasOnion = false;
    float onionX = 0.0f;
    float onionZ = 0.0f;
    float wpX = 0.0f; // detour waypoint when waypointLeg is set
    float wpZ = 0.0f;
    // Onion / squad facts for the withdraw phase.
    int fieldPikmin = 0; // live field Pikmin
    int onionStored = 0; // Pikmin stored in the nearest stocked Onion
    float onionDist = 1.0e30f; // XZ distance to that Onion
    bool containerOpen = false; // Onion container UI is up
    // Current-target facts. targetToken==0 means "no target".
    unsigned targetToken = 0;
    unsigned targetSource = 0;
    float tgtX = 0.0f;
    float tgtZ = 0.0f;
    float targetDist = 1.0e30f; // XZ distance navi -> target
    bool targetAlive = false;
    float targetHealthFrac = 1.0f; // 1 == untouched
    bool targetDamagedLatch = false; // family-observed combat (e.g. Kogane flip)
    bool targetRevealed = true; // Sokkuri disguise dropped
    bool targetDead = false; // generic death: health<=0 / !alive / dead-state / corpse formed
    bool receiptSeen = false; // Onion receipt marker for this token observed (delivery ledger)
    float targetHeight = 0.0f; // Y above ground (flyers: Sarai/Kurage)
    bool targetGrabbing = false; // flyer holds a Pikmin (Sarai grab): whistle to free
    bool targetLow = false; // flyer low enough to hit (driver compares height to saraiLowHeight)
    bool transportSeen = false; // any live Pikmin in TransportMode
    bool scattered = false; // squad scattered: whistle regroup
    bool waypointLeg = false; // steer the detour waypoint, not the target
};

// Pad output for one tick. moveX/moveZ is the desired world-space XZ move
// direction (driver converts through the live camera into stick bytes +
// MSTICK bits); (0,0) means "no movement". menuHold asks for the Onion
// container withdraw input (stick-down + MSTICK_DOWN) while the UI is open.
struct Command {
    unsigned buttons = 0;
    float moveX = 0.0f;
    float moveZ = 0.0f;
    bool menuHold = false;
};

struct Result {
    unsigned token = 0;
    bool damaged = false;
    bool killed = false;
    bool carried = false;
    bool received = false; // Onion receipt marker observed (carried=1 implies received=1)
    float seconds = 0.0f;
    bool koganeLike = false;
};

class Brain {
public:
    explicit Brain(const Config& config = Config()) : cfg(config) { reset(); }

    void reset()
    {
        state = State::Idle;
        stateTime = 0.0f;
        engageTime = 0.0f;
        pressPhase = 0.0f;
        pressOn = false;
        whistleTime = 0.0f;
        whistling = false;
        menuTaps = 0;
        menuHoldTime = 0.0f;
        menuConfirmed = false;
        stuckWindowStart = 0.0f;
        stuckWindowDist = 1.0e30f;
        wantReplan = false;
        progressBest = 1.0e30f;
        initialHealthFrac = 1.0f;
        sawDamage = false;
        sawKill = false;
        sawCarry = false;
        sawReceipt = false;
        withdrawCycles = 0;
        throwSpin = 0.0f;
        result = Result{};
        markers.clear();
        lastCommand = Command{};
        announced = false;
    }

    State current() const { return state; }
    Command command() const { return lastCommand; }
    bool replanWanted() const { return wantReplan; }
    void clearReplan() { wantReplan = false; }
    std::vector<std::string> takeMarkers()
    {
        std::vector<std::string> out;
        out.swap(markers);
        return out;
    }

    void update(float dt, const Senses& in)
    {
        lastCommand = Command{};
        if (dt <= 0.0f || dt > 0.5f) dt = 0.016f;
        lastDt = dt;
        if (!in.enabled) {
            // Inert-when-unset: no input, no markers, forced IDLE.
            if (state != State::Idle) reset();
            return;
        }
        if (!in.naviAlive) {
            holdIdle();
            return;
        }
        stateTime += dt;
        switch (state) {
        case State::Idle: tickIdle(in); break;
        case State::WithdrawSeek: tickWithdrawSeek(dt, in); break;
        case State::WithdrawMenu: tickWithdrawMenu(dt, in); break;
        case State::Select: tickSelect(in); break;
        case State::Approach: tickApproach(dt, in); break;
        case State::Attack: tickAttack(dt, in); break;
        case State::Aftermath: tickAftermath(dt, in); break;
        case State::Done: break;
        }
    }

private:
    void emitState(const Senses& in)
    {
        char buf[256];
        std::snprintf(buf, sizeof(buf), "AUTOPLAY_STATE state=%s token=%u field=%d bot-driven",
                      stateName(state), in.targetToken, in.fieldPikmin);
        markers.emplace_back(buf);
    }
    void enter(State next, const Senses& in)
    {
        state = next;
        stateTime = 0.0f;
        pressPhase = 0.0f;
        pressOn = false;
        whistleTime = 0.0f;
        whistling = false;
        menuTaps = 0;
        menuHoldTime = 0.0f;
        menuConfirmed = false;
        stuckWindowStart = 0.0f;
        stuckWindowDist = 1.0e30f;
        wantReplan = false;
        progressBest = 1.0e30f;
        emitState(in);
    }
    void holdIdle() { lastCommand = Command{}; }

    void tickIdle(const Senses& in)
    {
        // First live tick with the gate set: announce and go withdraw.
        if (!announced) {
            announced = true;
            markers.emplace_back("AUTOPLAY_STATE state=idle gate=open bot-driven");
        }
        enter(State::WithdrawSeek, in);
    }

    void steer(float fromX, float fromZ, float toX, float toZ)
    {
        const float dx = toX - fromX, dz = toZ - fromZ;
        const float len = std::sqrt(dx * dx + dz * dz);
        if (len > 1.0f) {
            lastCommand.moveX = dx / len;
            lastCommand.moveZ = dz / len;
        }
    }

    void tickWithdrawSeek(float dt, const Senses& in)
    {
        if (in.fieldPikmin >= cfg.wantSquad) {
            enter(State::Select, in);
            return;
        }
        if (in.onionStored <= 0 && in.fieldPikmin > 0) {
            // Nothing stocked to take; fight with the squad on the field.
            enter(State::Select, in);
            return;
        }
        if (in.containerOpen) {
            // UI already open (A landed while closing distance): work it.
            enter(State::WithdrawMenu, in);
            return;
        }
        if (in.hasOnion) {
            // Keep closing until the real container trigger (navi size +
            // coll radius, ~50) fires: arriveRadius (90) only starts the A
            // taps, it must not stop the steering or the captain stalls at
            // 90 and never opens the UI (wf9-2 withdraw_timeout).
            if (in.waypointLeg) steer(in.naviX, in.naviZ, in.wpX, in.wpZ);
            else steer(in.naviX, in.naviZ, in.onionX, in.onionZ);
            if (in.onionDist <= cfg.arriveRadius) {
                // At the Onion: tap A to open the container UI.
                pulseA(in, 0.12f, 0.6f);
            }
            // Progress / stuck tracking so a wall between spawn and the
            // Onion logs AUTOPLAY_STUCK and asks the driver to replan via
            // the map waypoint graph (same contract as approach).
            if (stuckWindowDist >= 1.0e29f) {
                stuckWindowDist = in.onionDist;
                stuckWindowStart = 0.0f;
                progressBest = in.onionDist;
            }
            if (in.onionDist < progressBest) progressBest = in.onionDist;
            stuckWindowStart += dt;
            if (stuckWindowStart >= cfg.stuckWindow) {
                if (stuckWindowDist - progressBest < cfg.stuckMinProgress) {
                    char buf[256];
                    std::snprintf(buf, sizeof(buf),
                                  "AUTOPLAY_STUCK state=withdraw_seek onion_dist=%.0f bot-driven",
                                  in.onionDist);
                    markers.emplace_back(buf);
                    wantReplan = true;
                }
                stuckWindowDist = progressBest;
                stuckWindowStart = 0.0f;
            }
        }
        if (stateTime >= cfg.withdrawTimeout) {
            giveUp(in, "withdraw_timeout");
            enter(State::Select, in);
        }
    }

    void tickWithdrawMenu(float dt, const Senses& in)
    {
        if (!in.containerOpen) {
            // UI closed after our A confirm: one withdraw cycle landed.
            if (menuConfirmed) {
                if (in.fieldPikmin >= cfg.wantSquad || in.onionStored <= 0
                    || withdrawCycles + 1 >= cfg.maxWithdrawCycles) {
                    enter(State::Select, in);
                    return;
                }
                // Squad still short and the Onion still stocks: loop back for
                // another cycle (bot-v2 gap 4: repeat until 15 or empty).
                ++withdrawCycles;
                char buf[256];
                std::snprintf(buf, sizeof(buf),
                              "AUTOPLAY_WITHDRAW cycle=%d field=%d stored=%d bot-driven",
                              withdrawCycles, in.fieldPikmin, in.onionStored);
                markers.emplace_back(buf);
                enter(State::WithdrawSeek, in);
                return;
            }
            // UI did not open (or closed early): tap A to (re)open, then wait.
            pulseA(in, 0.12f, 0.8f);
            if (stateTime >= cfg.menuOpenTimeout) {
                giveUp(in, "withdraw_menu_timeout");
                enter(State::Select, in);
            }
            return;
        }
        if (in.fieldPikmin >= cfg.wantSquad || in.onionStored <= 0) {
            // Already have a squad (e.g. re-entered): confirm and leave.
            pulseA(in, 0.12f, 0.4f);
            menuHoldTime += dt;
            if (menuHoldTime > 1.2f) menuConfirmed = true;
            if (menuConfirmed && stateTime > 4.0f) enter(State::Select, in);
            return;
        }
        // Need withdraw: the field count only rises AFTER the A confirm
        // (delta is UI-local until End), so a field-gated hold deadlocks
        // (wf9-3 withdraw_hold_timeout with field=0). Hold stick-down for
        // menuHoldDuration to accumulate delta, then pulse A to confirm.
        if (menuHoldTime < cfg.menuHoldDuration) {
            lastCommand.menuHold = true;
            menuHoldTime += dt;
            return;
        }
        pulseA(in, 0.12f, 0.4f);
        menuHoldTime += dt;
        if (menuHoldTime > cfg.menuHoldDuration + cfg.menuConfirmDuration) menuConfirmed = true;
        if (menuConfirmed) {
            // Keep pulsing until the UI closes (exitPikis runs on End).
            if (stateTime >= cfg.withdrawTimeout) {
                giveUp(in, "withdraw_hold_timeout");
                enter(State::Select, in);
            }
            return;
        }
        if (stateTime >= cfg.withdrawTimeout) {
            giveUp(in, "withdraw_hold_timeout");
            enter(State::Select, in);
        }
    }

    void tickSelect(const Senses& in)
    {
        if (in.targetToken == 0 || !in.targetAlive) {
            enter(State::Done, in);
            return;
        }
        engageTime = 0.0f;
        initialHealthFrac = in.targetHealthFrac;
        sawDamage = in.targetDamagedLatch;
        sawKill = in.targetDead;
        if (in.targetDead) sawDamage = true; // death implies damage
        sawCarry = false;
        sawReceipt = in.receiptSeen;
        result = Result{};
        result.token = in.targetToken;
        result.koganeLike = isKoganeLike(in.targetSource);
        enter(State::Approach, in);
    }

    void tickApproach(float dt, const Senses& in)
    {
        engageTime += dt;
        if (!in.targetAlive || in.targetDead) {
            // Died before we arrived (or despawned): score what we saw.
            // Generic death latch first so Otakara-style kills (no per-module
            // marker) still claim (bot-v2 gap 5).
            observeDeath(in);
            if (sawDamage || sawKill) {
                enter(State::Aftermath, in); // a corpse may still be carried
            } else {
                finishTarget(in, /*claimedKill*/ false);
            }
            return;
        }
        observeDamage(in);
        observeDeath(in);
        observeReceipt(in);
        const float closeEnough = isFlyer(in.targetSource) ? cfg.throwRange : cfg.throwRange * 0.75f;
        const float need = in.targetRevealed ? closeEnough : 120.0f; // walk onto disguised Sokkuri
        if (in.targetDist <= need) {
            enter(State::Attack, in);
            return;
        }
        // Progress / stuck tracking on straight-line distance.
        if (stuckWindowDist >= 1.0e29f) {
            stuckWindowDist = in.targetDist;
            stuckWindowStart = 0.0f;
            progressBest = in.targetDist;
        }
        if (in.targetDist < progressBest) progressBest = in.targetDist;
        stuckWindowStart += dt;
        if (stuckWindowStart >= cfg.stuckWindow) {
            if (stuckWindowDist - progressBest < cfg.stuckMinProgress) {
                char buf[256];
                std::snprintf(buf, sizeof(buf),
                              "AUTOPLAY_STUCK state=approach token=%u dist=%.0f bot-driven",
                              in.targetToken, in.targetDist);
                markers.emplace_back(buf);
                wantReplan = true;
            }
            stuckWindowDist = progressBest;
            stuckWindowStart = 0.0f;
        }
        // Driver fills moveX/moveZ toward the target or the detour waypoint.
        if (in.waypointLeg) steer(in.naviX, in.naviZ, in.wpX, in.wpZ);
        else steer(in.naviX, in.naviZ, in.tgtX, in.tgtZ);
        if (stateTime >= cfg.approachTimeout) {
            giveUp(in, "approach_timeout");
            finishTarget(in, /*killed*/ false);
        }
    }

    void tickAttack(float dt, const Senses& in)
    {
        engageTime += dt;
        if (!in.targetAlive || in.targetDead) {
            observeDeath(in);
            enter(State::Aftermath, in); // whistle back, watch the corpse
            return;
        }
        observeDamage(in);
        observeDeath(in);
        observeReceipt(in);
        // Kogane never dies: damage observed -> confirm, then move on.
        if (isKoganeLike(in.targetSource) && sawDamage && stateTime >= cfg.koganeConfirm) {
            finishTarget(in, /*killed*/ false);
            return;
        }
        const bool sarai = in.targetSource == 23;
        const bool kurage = in.targetSource == 57;
        // Sarai grab: whistle to free the grabbed Pikmin (priority over throws).
        const bool grabWhistle = sarai && in.targetGrabbing;
        if ((in.scattered || grabWhistle) && !whistling) {
            whistling = true;
            whistleTime = 0.0f;
        }
        if (whistling) {
            whistleTime += dt;
            lastCommand.buttons = PadB; // hold whistle to regroup / free grabs
            if (whistleTime >= cfg.whistleHold || (!in.scattered && !grabWhistle)) whistling = false;
            return;
        }
        if (sarai && !in.targetLow && !in.targetGrabbing) {
            // Flyer high and holding nothing: stay near it with the squad
            // following (steer under it), save Pikmin until it swoops low.
            steer(in.naviX, in.naviZ, in.tgtX, in.tgtZ);
            if (stateTime >= cfg.attackTimeout) {
                giveUp(in, "attack_timeout");
                finishTarget(in, /*killed*/ false);
            }
            return;
        }
        // Kurage: body is on the ground (visual float only) so throw at the
        // body position; high HP means a longer window, and throws rotate to
        // spread Pikmin around the bell.
        float aimX = in.tgtX, aimZ = in.tgtZ;
        float gap = cfg.throwGap;
        if (kurage) {
            throwSpin += dt * 1.5f;
            const float dx = in.tgtX - in.naviX, dz = in.tgtZ - in.naviZ;
            const float len = std::sqrt(dx * dx + dz * dz);
            if (len > 1.0f) {
                const float wob = std::sin(throwSpin) * 40.0f;
                aimX += (-dz / len) * wob;
                aimZ += (dx / len) * wob;
            }
            gap = cfg.throwGap * (1.0f + 0.4f * std::sin(throwSpin * 0.7f));
        }
        // Keep the stick toward the target so the cursor aims at it, and
        // pulse A to throw. Flyers are thrown at from range as the game allows.
        steer(in.naviX, in.naviZ, aimX, aimZ);
        pulseA(in, cfg.throwHold, gap);
        const float limit = kurage ? cfg.attackTimeout * cfg.kurageAttackMultiplier : cfg.attackTimeout;
        if (stateTime >= limit) {
            giveUp(in, "attack_timeout");
            finishTarget(in, /*killed*/ false);
        }
    }

    void tickAftermath(float dt, const Senses& in)
    {
        engageTime += dt;
        if (in.transportSeen) sawCarry = true;
        observeDeath(in);
        observeReceipt(in);
        if ((!in.targetAlive || in.targetDead) && !sawDamage && !sawKill && !sawCarry && !sawReceipt) {
            // Target gone with no combat observed: nothing to wait for.
            finishTarget(in, /*claimedKill*/ false);
            return;
        }
        // Idle near the corpse/Onion and whistle stragglers while the carry
        // resolves (bot-v2 gap 1): steer to the last-known corpse when far,
        // whistle scattered Pikmin back.
        if ((!whistling && stateTime < cfg.whistleHold) || (in.scattered && !whistling)) {
            whistling = true;
            whistleTime = 0.0f;
        }
        if (whistling) {
            whistleTime += dt;
            lastCommand.buttons = PadB;
            if (whistleTime >= cfg.whistleHold) whistling = false;
        } else if (in.waypointLeg) {
            steer(in.naviX, in.naviZ, in.wpX, in.wpZ);
        } else if (in.targetToken != 0) {
            const float dx = in.tgtX - in.naviX, dz = in.tgtZ - in.naviZ;
            if (dx * dx + dz * dz > 150.0f * 150.0f) steer(in.naviX, in.naviZ, in.tgtX, in.tgtZ);
        }
        if (sawReceipt && stateTime > cfg.whistleHold) {
            // Onion receipt landed: score it promptly.
            finishTarget(in, /*killed*/ true);
            return;
        }
        const float wait = (sawKill || sawDamage) ? cfg.receiptTimeout : cfg.aftermathTimeout;
        if (stateTime >= wait) {
            if (!sawReceipt) {
                char buf[256];
                std::snprintf(buf, sizeof(buf),
                              "AUTOPLAY_GIVEUP reason=receipt_timeout token=%u state=aftermath bot-driven",
                              in.targetToken);
                markers.emplace_back(buf);
            }
            finishTarget(in, /*killed*/ true);
        }
    }

    void observeDamage(const Senses& in)
    {
        if (in.targetHealthFrac < initialHealthFrac - 0.001f) sawDamage = true;
        if (in.targetDamagedLatch) sawDamage = true;
    }

    void observeDeath(const Senses& in)
    {
        // Generic death for every species (bot-v2 gap 5): engine death
        // signals, not per-module markers. Death implies damage.
        if (in.targetDead) {
            sawKill = true;
            sawDamage = true;
        }
        if (!in.targetAlive && (sawDamage || in.targetHealthFrac <= 0.001f)) {
            sawKill = true;
            if (in.targetHealthFrac <= 0.001f) sawDamage = true;
        }
    }

    void observeReceipt(const Senses& in)
    {
        if (in.receiptSeen) sawReceipt = true;
    }

    void giveUp(const Senses& in, const char* reason)
    {
        char buf[256];
        std::snprintf(buf, sizeof(buf), "AUTOPLAY_GIVEUP reason=%s token=%u state=%s bot-driven",
                      reason, in.targetToken, stateName(state));
        markers.emplace_back(buf);
    }

    void finishTarget(const Senses& in, bool claimedKill)
    {
        result.damaged = sawDamage || sawKill;
        result.killed = (claimedKill || sawKill) && result.damaged && !result.koganeLike;
        if (result.koganeLike) {
            // Kogane never dies; damage is the outcome.
            result.killed = false;
            result.carried = false;
            result.received = false;
        } else {
            // carried=1 means an Onion receipt line was seen (bot-v2 gap 1).
            result.received = sawReceipt;
            result.carried = sawReceipt;
        }
        result.seconds = engageTime;
        char buf[256];
        std::snprintf(buf, sizeof(buf),
                      "AUTOPLAY_RESULT target=%u damaged=%d killed=%d carried=%d received=%d seconds=%.0f bot-driven",
                      result.token, int(result.damaged), int(result.killed),
                      int(result.carried), int(result.received), result.seconds);
        markers.emplace_back(buf);
        // The driver advances to the next target (or Done when none remain).
        enter(State::Select, in);
    }

    // A-press pulse train: hold A for holdSecs, release for gapSecs.
    void pulseA(const Senses& in, float holdSecs, float gapSecs)
    {
        (void)in;
        pressPhase += lastDt;
        if (!pressOn && pressPhase >= gapSecs) {
            pressOn = true;
            pressPhase = 0.0f;
        } else if (pressOn && pressPhase >= holdSecs) {
            pressOn = false;
            pressPhase = 0.0f;
        }
        if (pressOn) lastCommand.buttons |= PadA;
    }

public:
    // Set by update() before the tick handlers run (pulseA needs dt).
    float lastDt = 0.016f;

private:
    Config cfg;
    State state = State::Idle;
    float stateTime = 0.0f;
    float engageTime = 0.0f;
    float pressPhase = 0.0f;
    bool pressOn = false;
    float whistleTime = 0.0f;
    bool whistling = false;
    int menuTaps = 0;
    float menuHoldTime = 0.0f;
    bool menuConfirmed = false;
    float stuckWindowStart = 0.0f;
    float stuckWindowDist = 1.0e30f;
    bool wantReplan = false;
    float progressBest = 1.0e30f;
    float initialHealthFrac = 1.0f;
    bool sawDamage = false;
    bool sawKill = false; // generic death latched (any species)
    bool sawCarry = false;
    bool sawReceipt = false; // Onion receipt latched (authoritative for carried)
    int withdrawCycles = 0; // withdraw-menu repeat count this run
    float throwSpin = 0.0f; // Kurage throw rotation phase
    bool announced = false;
    Result result;
    Command lastCommand;
    std::vector<std::string> markers;
};

} // namespace p2autoplay
