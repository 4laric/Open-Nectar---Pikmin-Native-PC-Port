// TEST-ONLY headless autoplay bot driver (brief key: bot-impl, wf9).
//
// Drives the game through the NORMAL controller input path: each logical
// tick ControllerMgr::update() calls pc_p2_autoplay_tick(), which senses
// live game state (read-only), steps the engine-free Brain
// (pc_p2_autoplay_policy.h), converts its world-space move vector through
// the live camera basis into a stick deflection, and publishes pad buttons +
// stick via pc_p2_input_script_set() -- the same hook the real pad read
// feeds (ControllerMgr::updateController). The bot never teleports,
// never mode-forces, never mutates enemies/Pikmin: only pad state.
//
// Enabled only when PIKMIN_RANDOMIZER_AUTOPLAY is set (non-empty, != "0").
// Otherwise tick() returns before touching anything: production input is
// untouched (native test: tools/p2_autoplay_test.cpp).

#include "pc_p2_autoplay_policy.h"

#include "pc_p2_input_script.h"
#include "pc_p2_campaign_actor.h"
#include "pc_p2_sokkuri.h"
#include "pc_p2_kogane.h"
#include "pc_randomizer.h"
#include "Controller.h"

#include "teki.h"
#include "Piki.h"
#include "PikiMgr.h"
#include "Navi.h"
#include "NaviMgr.h"
#include "NaviState.h"
#include "Generator.h"
#include "GoalItem.h"
#include "ItemMgr.h"
#include "Route.h"
#include "Camera.h"
#include "gameflow.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <set>
#include <string>

// Pad bits emitted here must equal the engine's KeyboardButtons.
static_assert(unsigned(p2autoplay::PadA) == unsigned(KBBTN_A), "autoplay pad bits drifted");
static_assert(unsigned(p2autoplay::PadB) == unsigned(KBBTN_B), "autoplay pad bits drifted");
static_assert(unsigned(p2autoplay::PadMainUp) == unsigned(KBBTN_MSTICK_UP), "autoplay pad bits drifted");
static_assert(unsigned(p2autoplay::PadMainDown) == unsigned(KBBTN_MSTICK_DOWN), "autoplay pad bits drifted");
static_assert(unsigned(p2autoplay::PadMainLeft) == unsigned(KBBTN_MSTICK_LEFT), "autoplay pad bits drifted");
static_assert(unsigned(p2autoplay::PadMainRight) == unsigned(KBBTN_MSTICK_RIGHT), "autoplay pad bits drifted");
static_assert(unsigned(p2autoplay::PadStart) == unsigned(KBBTN_START), "autoplay pad bits drifted");

namespace {

float distXZ(float ax, float az, float bx, float bz)
{
    const float dx = ax - bx, dz = az - bz;
    return std::sqrt(dx * dx + dz * dz);
}

const char* sourceDisplayName(unsigned source)
{
    switch (source) {
    case 79: return "Sokkuri";
    case 9: return "Kogane";
    case 23: return "Sarai";
    case 57: return "Kurage";
    case 54: return "Miulin";
    case 44: return "BlueKochappy";
    case 59: return "FireOtakara";
    case 60: return "WaterOtakara";
    case 61: return "GasOtakara";
    case 62: return "ElecOtakara";
    default: return "unknown";
    }
}

struct Engagement {
    unsigned token = 0;
    unsigned source = 0;
    float initialHealth = 0.0f;
    int initialNectar = 0;
    float lastX = 0.0f;
    float lastZ = 0.0f;
    bool lastAlive = false;
    bool carryLatch = false;
};

p2autoplay::Brain sBrain;
Engagement sEngage;
std::set<unsigned> sCompleted;
bool sHaveDetour = false;
float sDetourX = 0.0f, sDetourZ = 0.0f;
float sDetourTime = 0.0f;
long long sTicks = 0;
std::chrono::steady_clock::time_point sFpsStart = std::chrono::steady_clock::now();
bool sFpsLogged = false;

void planDetour(float naviX, float naviZ, float tgtX, float tgtZ)
{
    sHaveDetour = false;
    sDetourTime = 0.0f;
    // Prefer the map's real waypoint graph: nearest waypoint to the target
    // via a synced path from the nearest waypoint to us.
    if (routeMgr) {
        Vector3f from(naviX, 0.0f, naviZ), to(tgtX, 0.0f, tgtZ);
        WayPoint* selfWp = routeMgr->findNearestWayPoint('test', from, true);
        WayPoint* tgtWp = routeMgr->findNearestWayPoint('test', to, true);
        PathFinder* finder = routeMgr->getPathFinder('test');
        if (selfWp && tgtWp && finder && selfWp != tgtWp) {
            WayPoint* path[16] = {};
            const int n = finder->findSync(path, 16, selfWp->mIndex, tgtWp->mIndex, false);
            for (int i = 0; i < n; ++i) {
                if (!path[i]) continue;
                if (distXZ(naviX, naviZ, path[i]->mPosition.x, path[i]->mPosition.z) > 100.0f) {
                    sDetourX = path[i]->mPosition.x;
                    sDetourZ = path[i]->mPosition.z;
                    sHaveDetour = true;
                    break;
                }
            }
            if (!sHaveDetour && n > 0 && path[0]) {
                // Degenerate path: still step onto the graph toward the target.
                sDetourX = path[0]->mPosition.x;
                sDetourZ = path[0]->mPosition.z;
                sHaveDetour = true;
            }
        } else if (tgtWp) {
            sDetourX = tgtWp->mPosition.x;
            sDetourZ = tgtWp->mPosition.z;
            sHaveDetour = true;
        }
    }
    if (!sHaveDetour) {
        // Graph unavailable: perpendicular sidestep around the straight line.
        const float dx = tgtX - naviX, dz = tgtZ - naviZ;
        const float len = std::sqrt(dx * dx + dz * dz);
        if (len > 1.0f) {
            const float side = (sTicks % 2 == 0) ? 1.0f : -1.0f;
            sDetourX = naviX + dx * 0.35f - dz / len * 220.0f * side;
            sDetourZ = naviZ + dz * 0.35f + dx / len * 220.0f * side;
            sHaveDetour = true;
        }
    }
    if (sHaveDetour) {
        std::printf("AUTOPLAY_REPLAN token=%u detour=(%.0f,%.0f) bot-driven\n",
                    sEngage.token, sDetourX, sDetourZ);
        std::fflush(stdout);
    }
}

} // namespace

void pc_p2_autoplay_tick(void)
{
    if (!p2autoplay::isEnabled()) {
        return; // inert when unset: production input path untouched
    }
    if (!naviMgr || !pikiMgr || !tekiMgr || !itemMgr) {
        return; // boot/menus: no game state yet, emit nothing
    }
    Navi* navi = naviMgr->getNavi();
    if (!navi || !navi->isAlive()) {
        sBrain.update(0.016f, p2autoplay::Senses{});
        pc_p2_input_script_set(1, 0, 0, 0);
        return;
    }

    const float dt = gsys ? gsys->getFrameTime() : 0.016f;
    const float naviX = navi->getPosition().x;
    const float naviZ = navi->getPosition().z;

    // --- Pikmin census (read-only) ---
    int alive = 0, nearCount = 0, farCount = 0, transport = 0;
    {
        Iterator it(pikiMgr);
        CI_LOOP(it)
        {
            Piki* p = static_cast<Piki*>(*it);
            if (!p || !p->isAlive()) continue;
            ++alive;
            const float d = distXZ(naviX, naviZ, p->getPosition().x, p->getPosition().z);
            if (d < 350.0f) ++nearCount;
            if (d > 550.0f) ++farCount;
            if (p->mMode == PikiMode::TransportMode) ++transport;
        }
    }

    // --- Nearest stocked Onion (read-only) ---
    bool hasOnion = false;
    float onionX = 0.0f, onionZ = 0.0f, onionDist = 1.0e30f;
    int onionStored = 0;
    for (int color = 0; color < 3; ++color) {
        GoalItem* onion = itemMgr->getContainer(color);
        if (!onion) continue;
        const float d = distXZ(naviX, naviZ, onion->getPosition().x, onion->getPosition().z);
        const int stored = onion->getTotalStorePikis();
        if (!hasOnion || (stored > 0 && onionStored <= 0) || (stored > 0 && d < onionDist)
            || (stored == 0 && onionStored == 0 && d < onionDist)) {
            hasOnion = true;
            onionX = onion->getPosition().x;
            onionZ = onion->getPosition().z;
            onionDist = d;
            onionStored = stored;
        }
    }

    // --- P2-bound target scan (read-only) ---
    const std::string filter = p2autoplay::targetFilter();
    struct Candidate {
        unsigned token;
        unsigned source;
        float x, z, dist;
        BTeki* actor;
        float health;
    };
    std::vector<Candidate> candidates;
    {
        Iterator it(tekiMgr);
        CI_LOOP(it)
        {
            Teki* teki = static_cast<Teki*>(*it);
            if (!teki) continue;
            BTeki* actor = static_cast<BTeki*>(teki);
            if (!actor->mGenerator) continue;
            if (!actor->isAlive() || actor->mHealth <= 0.0f) continue;
            const unsigned token = pc_p2_campaign_token(actor);
            if (!token) continue;
            unsigned source = pc_randomizer_p2_source_for(actor);
            if (!source) source = pc_randomizer_p2_source_for_id(token);
            if (!source) continue; // not P2-bound
            if (sCompleted.count(token)) continue;
            const char* name = sourceDisplayName(source);
            if (!p2autoplay::matchTarget(token, source, name, filter)) continue;
            Candidate c;
            c.token = token;
            c.source = source;
            c.x = actor->getPosition().x;
            c.z = actor->getPosition().z;
            c.dist = distXZ(naviX, naviZ, c.x, c.z);
            c.actor = actor;
            c.health = actor->mHealth;
            candidates.push_back(c);
        }
    }
    // Keep the sticky engagement when it is still live; else nearest.
    const Candidate* pick = nullptr;
    if (sEngage.token) {
        for (const Candidate& c : candidates) {
            if (c.token == sEngage.token) {
                pick = &c;
                break;
            }
        }
    }
    if (!pick && !candidates.empty()) {
        pick = &*std::min_element(candidates.begin(), candidates.end(),
                                  [](const Candidate& a, const Candidate& b) { return a.dist < b.dist; });
    }
    if (pick && pick->token != sEngage.token) {
        sEngage = Engagement{};
        sEngage.token = pick->token;
        sEngage.source = pick->source;
        sEngage.initialHealth = pick->health > 0.0f ? pick->health : 1.0f;
        if (p2autoplay::isKoganeLike(pick->source)) sEngage.initialNectar = pc_p2_kogane_nectar_dropped(pick->token);
        sHaveDetour = false;
        sDetourTime = 0.0f;
    }

    // --- Senses ---
    p2autoplay::Senses senses;
    senses.enabled = true;
    senses.naviAlive = true;
    senses.dt = dt;
    senses.naviX = naviX;
    senses.naviZ = naviZ;
    senses.hasOnion = hasOnion;
    senses.onionX = onionX;
    senses.onionZ = onionZ;
    senses.fieldPikmin = alive;
    senses.onionStored = onionStored;
    senses.onionDist = onionDist;
    senses.containerOpen = navi->getCurrState() && navi->getCurrState()->getID() == NAVISTATE_Container;
    senses.scattered = (farCount >= 3) || (alive >= 10 && nearCount < 5);
    if (transport > 0) sEngage.carryLatch = true;
    senses.transportSeen = sEngage.carryLatch;
    if (pick) {
        senses.targetToken = pick->token;
        senses.targetSource = pick->source;
        senses.tgtX = pick->x;
        senses.tgtZ = pick->z;
        senses.targetDist = pick->dist;
        senses.targetAlive = true;
        senses.targetHealthFrac = pick->health / (sEngage.initialHealth > 0.0f ? sEngage.initialHealth : 1.0f);
        if (senses.targetHealthFrac < 0.0f) senses.targetHealthFrac = 0.0f;
        sEngage.lastX = pick->x;
        sEngage.lastZ = pick->z;
        sEngage.lastAlive = true;
        const bool kogane = p2autoplay::isKoganeLike(pick->source);
        if (!kogane && pick->health < sEngage.initialHealth - 0.5f) senses.targetDamagedLatch = true;
        if (kogane) {
            if (pc_p2_kogane_nectar_dropped(pick->token) > sEngage.initialNectar) senses.targetDamagedLatch = true;
            if (pc_p2_kogane_escaped(pick->actor)) senses.targetDamagedLatch = true;
        }
        if (pick->source == 79 && pc_p2_sokkuri_revealed(pick->actor)) senses.targetRevealed = true;
        else if (pick->source == 79) senses.targetRevealed = false;
    } else if (sEngage.token) {
        // Engagement target no longer live-listed: report last-known facts;
        // the Brain scores the outcome (kill vs giveup) from damage history.
        senses.targetToken = sEngage.token;
        senses.targetSource = sEngage.source;
        senses.tgtX = sEngage.lastX;
        senses.tgtZ = sEngage.lastZ;
        senses.targetDist = distXZ(naviX, naviZ, sEngage.lastX, sEngage.lastZ);
        senses.targetAlive = false;
        senses.targetHealthFrac = 0.0f;
    }

    // --- Stuck replan via the map's route/waypoint graph ---
    if (sBrain.replanWanted()) {
        if (sBrain.current() == p2autoplay::State::WithdrawSeek && hasOnion) {
            planDetour(naviX, naviZ, onionX, onionZ);
        } else if (pick) {
            planDetour(naviX, naviZ, pick->x, pick->z);
        }
        sBrain.clearReplan();
    }
    if (sHaveDetour) {
        sDetourTime += dt > 0.0f && dt <= 0.5f ? dt : 0.016f;
        senses.waypointLeg = true;
        senses.wpX = sDetourX;
        senses.wpZ = sDetourZ;
        if (distXZ(naviX, naviZ, sDetourX, sDetourZ) < 80.0f || sDetourTime > 25.0f) {
            sHaveDetour = false;
            senses.waypointLeg = false;
        }
    }

    sBrain.update(dt, senses);

    // Completed-target bookkeeping from emitted RESULT lines.
    for (const std::string& marker : sBrain.takeMarkers()) {
        std::printf("%s\n", marker.c_str());
        unsigned done = 0;
        if (std::sscanf(marker.c_str(), "AUTOPLAY_RESULT target=%u", &done) == 1 && done) {
            sCompleted.insert(done);
            if (done == sEngage.token) {
                sEngage = Engagement{};
                sHaveDetour = false;
            }
        }
    }
    std::fflush(stdout);

    // --- Pad synthesis through the live camera basis ---
    const p2autoplay::Command cmd = sBrain.command();
    unsigned buttons = cmd.buttons;
    int stickX = 0, stickY = 0;
    if (cmd.menuHold) {
        stickY = -127; // container withdraw direction
        buttons |= unsigned(p2autoplay::PadMainDown);
    } else if (cmd.moveX != 0.0f || cmd.moveZ != 0.0f) {
        float yaw = 0.0f;
        if (navi->mNaviCamera) yaw = std::atan2(navi->mNaviCamera->mViewXAxis.z, navi->mNaviCamera->mViewXAxis.x);
        const float c = std::cos(yaw), s = std::sin(yaw);
        // Inverse of makeVelocity's RotY(yaw): local = RotY(-yaw) * world.
        const float lx = c * cmd.moveX + s * cmd.moveZ;
        const float lz = -s * cmd.moveX + c * cmd.moveZ;
        float sx = lx > 1.0f ? 1.0f : (lx < -1.0f ? -1.0f : lx);
        float sy = lz > 1.0f ? -1.0f : (lz < -1.0f ? 1.0f : -lz);
        stickX = int(sx * 127.0f);
        stickY = int(sy * 127.0f);
        if (stickX > 32) buttons |= unsigned(p2autoplay::PadMainRight);
        else if (stickX < -32) buttons |= unsigned(p2autoplay::PadMainLeft);
        if (stickY > 32) buttons |= unsigned(p2autoplay::PadMainUp);
        else if (stickY < -32) buttons |= unsigned(p2autoplay::PadMainDown);
    }
    pc_p2_input_script_set(1, buttons, stickX, stickY);

    // --- Withdraw diagnostics (bot-driven): proves the captain closes to
    // the real container trigger instead of stalling at arriveRadius. ---
    if (sBrain.current() == p2autoplay::State::WithdrawSeek && (sTicks % 300 == 0)) {
        std::printf("AUTOPLAY_WITHDRAW navi=(%.0f,%.0f) onion=(%.0f,%.0f) dist=%.0f stored=%d field=%d open=%d bot-driven\n",
                    naviX, naviZ, onionX, onionZ, onionDist, onionStored, alive,
                    senses.containerOpen ? 1 : 0);
        std::fflush(stdout);
    }

    // --- FPS evidence ---
    if (++sTicks % 3600 == 0) {
        const auto now = std::chrono::steady_clock::now();
        const double secs = std::chrono::duration<double>(now - sFpsStart).count();
        if (secs > 0.0) {
            std::printf("AUTOPLAY_FPS ticks=%lld seconds=%.0f fps=%.1f bot-driven\n",
                        sTicks, secs, double(sTicks) / secs);
            std::fflush(stdout);
            sFpsLogged = true;
        }
    }
    (void)sFpsLogged;
}
