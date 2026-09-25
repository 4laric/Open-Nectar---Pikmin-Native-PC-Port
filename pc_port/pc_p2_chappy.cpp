#include "pc_p2_chappy.h"
#include "pc_p2_chappy_policy.h"
#include "pc_p2_chappy_fsm.h"
#include "pc_p2_campaign_actor.h"
#include "pc_p2_enemy.h"
#include "pc_p2_kochappy.h"
#include "pc_p2_dwarf_orange.h"
#include "pc_p2_sheargrub.h"
#include "pc_p2_sokkuri.h"
#include "pc_p2_setup_failsafe.h"
#include "pc_p2_white.h"
#include "pc_randomizer.h"
#include "teki.h"
#include "Generator.h"
#include "Shape.h"
#include "Texture.h"
#include "Material.h"
#include "gameflow.h"
#include "Graphics.h"
#include "Camera.h"
#include "PaniAnimator.h"
#include "Interactions.h"
#include "Piki.h"
#include "PikiMgr.h"
#include "Navi.h"
#include "NaviMgr.h"
#include "Stickers.h"
#include "system.h"
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace {
struct ClipDef {
    int frames = 0;
    int poses = 0;
};
struct SpeciesBank {
    unsigned source = 0;
    std::string species;
    std::map<std::string, ClipDef> clips;
};
std::map<std::string, SpeciesBank> banks; // enum -> bank (from p2-chappy-bank.txt)
std::map<std::string, std::vector<Shape*>> shapes; // "Enum|clip" -> poses
std::map<PelletView*, const p2chappy::SpeciesParams*> actors; // view -> species
std::map<PelletView*, std::string> lastClip;
std::map<PelletView*, unsigned> corpseGenerators; // delivered-corpse lookup (preview path)
p2chappy::Health health;
bool bankLoaded = false;
std::set<PelletView*> drawnLive, drawnCorpse;
size_t bankBytes = 0;

// Own-identity runtime FSM (inst2-chappy, #871). The P2 FSM decides every
// tick: movement/targeting/attacks are driven here via inputDrive/mVelocity/
// setDirection plus source animation-key attacks (bite/eat/swallow, flick,
// fire aura, warcry, burrow, rebirth, parent-follow). The P1 host AI is
// suppressed (doAI early-return + host param blinding) so this is exclusive.
constexpr float PI_F = 3.14159265f;
constexpr float TURN_RATE = 2.5f; // port adaptation (host drive rate)
constexpr float TERRITORY = 300.0f; // port adaptation (source mTerritoryRadius)
constexpr float HOME_RADIUS = 50.0f; // port adaptation (source mHomeRadius)
constexpr int FLICK_STUCK_MIN = 3; // source shake-off graduation first tier
constexpr float LOST_REBIRTH_S = 10.0f; // Kuma proper fp12 respawn (adaptation)
constexpr float KING_BURROW_IDLE_S = 20.0f; // King ip01 incubation (adaptation)
constexpr float KING_HIDEWAIT_S = 200.0f / 30.0f; // King ip02 appearance (adaptation)
constexpr float TURN_DURATION_S = 25.0f / 30.0f; // waitact1 (adaptation)
constexpr float FLICK_DURATION_S = 80.0f / 30.0f; // flick (adaptation)
constexpr float WARCRY_DURATION_S = 60.0f / 30.0f; // cry (adaptation)

struct ChappyFsm {
    const p2chappy::SpeciesParams* spec = nullptr;
    p2chappyfsm::Family family = p2chappyfsm::FAMILY_ADULT;
    int state = 7;
    float stateTime = 0.0f;
    float heading = 0.0f;
    Vector3f home;
    Vector3f wander;
    bool wanderValid = false;
    unsigned rng = 1;
    bool deadLogged = false;
    bool escaped = false;
    bool attackFired = false;
    bool swallowFired = false;
    bool flickFired = false;
    bool auraLogged = false;
    bool birthLogged = false;
    bool healthAsserted = false;
    bool pressed = false; // kumako Press latch
    std::string clip = "wait1";
    float phase = 0.0f;
    float logTimer = 0.0f;
    float auraTimer = 0.0f;
    float lastHealth = 0.0f;
};
std::map<PelletView*, ChappyFsm> fsms;

bool claimedElsewhere(BTeki* actor)
{
    PelletView* view = static_cast<PelletView*>(actor);
    return pc_p2_enemy_name(view) || pc_p2_kochappy_name(view) || pc_p2_dwarf_orange_name(view)
           || pc_p2_sheargrub_name(view) || pc_p2_sokkuri_registered(actor);
}

float wrapPi(float a)
{
    while (a > PI_F) a -= 2.0f * PI_F;
    while (a < -PI_F) a += 2.0f * PI_F;
    return a;
}

float distXZ(const Vector3f& a, const Vector3f& b)
{
    const float dx = a.x - b.x, dz = a.z - b.z;
    return std::sqrt(dx * dx + dz * dz);
}

unsigned nextRand(ChappyFsm& s)
{
    s.rng = s.rng * 1664525u + 1013904223u;
    return s.rng >> 8;
}

float rand01(ChappyFsm& s) { return float(nextRand(s) & 0xffff) / 65535.0f; }

void stop(BTeki* a)
{
    a->inputDrive(Vector3f(0.0f, 0.0f, 0.0f));
    a->mVelocity.x = 0.0f;
    a->mVelocity.z = 0.0f;
}

bool turnTo(BTeki* a, ChappyFsm& s, const Vector3f& target, float dt, float tolerance)
{
    const Vector3f pos = a->getPosition();
    const float desired = std::atan2(target.x - pos.x, target.z - pos.z);
    const float maxTurn = TURN_RATE * dt;
    float diff = wrapPi(desired - s.heading);
    if (diff > maxTurn) diff = maxTurn;
    if (diff < -maxTurn) diff = -maxTurn;
    s.heading = wrapPi(s.heading + diff);
    a->setDirection(s.heading);
    return std::fabs(wrapPi(desired - s.heading)) <= tolerance;
}

void walkTo(BTeki* a, ChappyFsm& s, const Vector3f& target, float dt, float speed)
{
    const Vector3f pos = a->getPosition();
    const float desired = std::atan2(target.x - pos.x, target.z - pos.z);
    const float maxTurn = TURN_RATE * dt;
    float diff = wrapPi(desired - s.heading);
    if (diff > maxTurn) diff = maxTurn;
    if (diff < -maxTurn) diff = -maxTurn;
    s.heading = wrapPi(s.heading + diff);
    a->setDirection(s.heading);
    const Vector3f drive(std::sin(s.heading) * speed, 0.0f, std::cos(s.heading) * speed);
    a->inputDrive(drive);
    a->mVelocity.set(drive);
}

Creature* nearestTarget(const Vector3f& pos, float sight)
{
    Creature* best = nullptr;
    float bestSq = sight * sight;
    if (naviMgr) {
        Navi* n = naviMgr->getNavi();
        if (n && n->isAlive()) {
            const Vector3f p = n->getPosition();
            const float dx = p.x - pos.x, dz = p.z - pos.z;
            const float d = dx * dx + dz * dz;
            if (d < bestSq) { bestSq = d; best = n; }
        }
    }
    if (pikiMgr) {
        Iterator it(pikiMgr);
        CI_LOOP(it)
        {
            Piki* p = static_cast<Piki*>(*it);
            if (!p || !p->isAlive()) continue;
            const Vector3f q = p->getPosition();
            const float dx = q.x - pos.x, dz = q.z - pos.z;
            const float d = dx * dx + dz * dz;
            if (d < bestSq) { bestSq = d; best = p; }
        }
    }
    return best;
}

Piki* nearestEdiblePiki(const Vector3f& center, float radius)
{
    Piki* best = nullptr;
    float bestSq = radius * radius;
    if (pikiMgr) {
        Iterator it(pikiMgr);
        CI_LOOP(it)
        {
            Piki* p = static_cast<Piki*>(*it);
            if (!p || !p->isAlive()) continue;
            if (p->isStickToMouth() || p->isStickTo()) continue;
            const Vector3f q = p->getPosition();
            const float dx = q.x - center.x, dy = q.y - center.y, dz = q.z - center.z;
            const float d = dx * dx + dy * dy + dz * dz;
            if (d < bestSq) { bestSq = d; best = p; }
        }
    }
    return best;
}

int stuckPikminCount(Creature* c)
{
    int n = 0;
    for (Creature* s = c->mStickListHead; s; s = s->mNextSticker) {
        if (!s || !s->isPiki() || !s->isAlive()) continue;
        ++n;
    }
    return n;
}

bool attackable(const ChappyFsm& s, const Vector3f& pos, const Creature* target)
{
    if (!target) return false;
    const Vector3f tp = target->getPosition();
    if (distXZ(tp, pos) >= s.spec->attackRange) return false;
    const float ang = std::fabs(wrapPi(std::atan2(tp.x - pos.x, tp.z - pos.z) - s.heading));
    return ang <= s.spec->attackAngle * PI_F / 180.0f;
}

// KumaKo parent-follow: nearest live Kuma-family (35/67) FSM actor.
bool parentNear(const BTeki* self, const Vector3f& pos, Vector3f& parentPos)
{
    float bestSq = -1.0f;
    bool found = false;
    for (const auto& kv : fsms) {
        if (kv.second.family != p2chappyfsm::FAMILY_KUMA) continue;
        const BTeki* other = static_cast<const BTeki*>(static_cast<const void*>(kv.first));
        if (other == self) continue;
        const Vector3f q = kv.second.home;
        (void)q;
    }
    // Scan live actors for the nearest Kuma-family home within sight.
    for (const auto& kv : fsms) {
        if (kv.second.family != p2chappyfsm::FAMILY_KUMA) continue;
        if (kv.first == static_cast<PelletView*>(const_cast<BTeki*>(self))) continue;
        // Use the parent's current home as the follow anchor (stable).
        const float dx = kv.second.home.x - pos.x, dz = kv.second.home.z - pos.z;
        const float d = dx * dx + dz * dz;
        if (!found || d < bestSq) { bestSq = d; parentPos = kv.second.home; found = true; }
    }
    if (!found) return false;
    const float sight = 500.0f;
    return bestSq <= sight * sight;
}

int clipFrames(const std::string& species, const std::string& clip)
{
    auto b = banks.find(species);
    if (b == banks.end()) return 30;
    auto c = b->second.clips.find(clip);
    if (c == b->second.clips.end()) return 30;
    return c->second.frames > 0 ? c->second.frames : 30;
}

float clipDuration(const std::string& species, const std::string& clip)
{
    return float(clipFrames(species, clip)) / 30.0f;
}

// Resolve an FSM state's preferred bank clip, falling back through the
// family's actual staged stems so a missing stem never aborts the draw.
const char* clipForState(const SpeciesBank& bank, p2chappyfsm::Family family, int state)
{
    const char* prefs[6] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
    int n = 0;
    auto push = [&](const char* c) { if (n < 6) prefs[n++] = c; };
    if (family == p2chappyfsm::FAMILY_ADULT) {
        switch (state) {
        case p2chappy::ADULT_DEAD: push("dead"); push("dead1"); push("pdead1"); break;
        case p2chappy::ADULT_ATTACK: push("attack"); push("attack1"); push("attack2"); break;
        case p2chappy::ADULT_FLICK: push("flick"); break;
        case p2chappy::ADULT_WALK: case p2chappy::ADULT_GO_HOME:
            push("move1"); push("move"); push("walk"); push("run1"); break;
        case p2chappy::ADULT_TURN: case p2chappy::ADULT_TURN_TO_HOME:
            push("waitact1"); push("wait1"); break;
        case p2chappy::ADULT_SLEEP: push("wait2"); push("sleep"); push("wait1"); break;
        default: push("wait1"); push("wait2"); break;
        }
    } else if (family == p2chappyfsm::FAMILY_KUMA) {
        switch (state) {
        case 0: push("dead"); push("dead1"); break;
        case 3: push("attack"); push("attack1"); break;
        case 4: push("flick"); break;
        case 7: case 8: push("move1"); push("move"); break;
        case 5: case 6: push("waitact1"); push("wait1"); break;
        case 2: push("wait2"); push("wait1"); break;
        case 1: push("waitact2"); push("wait2"); push("wait1"); break;
        default: push("wait1"); break;
        }
    } else if (family == p2chappyfsm::FAMILY_KUMAKO) {
        switch (state) {
        case 0: push("dead"); push("dead1"); break;
        case 1: push("press"); push("waitact2"); push("wait1"); break;
        case 3: push("attack"); push("attack1"); break;
        case 4: push("flick"); break;
        case 5: case 6: push("move1"); push("move"); break;
        case 2: default: push("wait1"); push("wait2"); break;
        }
    } else { // KING
        switch (state) {
        case 2: push("dead"); break;
        case 1: push("attack"); break;
        case 3: push("flick"); break;
        case 0: push("move1"); push("move"); break;
        case 6: push("waitact1"); push("wait1"); break;
        case 4: push("waitact2"); push("cry"); push("wait1"); break;
        case 5: push("waitact1"); push("wait1"); break;
        case 8: case 9: push("wait2"); push("wait1"); break;
        case 10: push("waitact1"); push("wait1"); break;
        case 11: push("waitact2"); push("wait1"); break;
        case 7: case 12: push("attack"); push("waitact2"); break;
        default: push("wait1"); break;
        }
    }
    for (int i = 0; i < n; ++i) {
        if (prefs[i] && bank.clips.count(prefs[i])) return bank.clips.find(prefs[i])->first.c_str();
    }
    // Final fallback: any staged wait/dead stem, else the first clip.
    static const char* const fallbacks[] = {"wait1", "wait", "wait2", "dead", nullptr};
    for (const char* const* f = fallbacks; *f; ++f) {
        if (bank.clips.count(*f)) return bank.clips.find(*f)->first.c_str();
    }
    return bank.clips.empty() ? nullptr : bank.clips.begin()->first.c_str();
}

const char* fsmStateName(p2chappyfsm::Family family, int state)
{
    if (family == p2chappyfsm::FAMILY_ADULT) {
        switch (state) {
        case p2chappy::ADULT_TURN: return "turn";
        case p2chappy::ADULT_DEAD: return "dead";
        case p2chappy::ADULT_FLICK: return "flick";
        case p2chappy::ADULT_WALK: return "walk";
        case p2chappy::ADULT_ATTACK: return "attack";
        case p2chappy::ADULT_TURN_TO_HOME: return "turntohome";
        case p2chappy::ADULT_GO_HOME: return "gohome";
        case p2chappy::ADULT_SLEEP: return "sleep";
        default: return "null";
        }
    }
    if (family == p2chappyfsm::FAMILY_KUMA) {
        switch (state) {
        case 0: return "dead"; case 1: return "rebirth"; case 2: return "lost";
        case 3: return "attack"; case 4: return "flick"; case 5: return "turn";
        case 6: return "turnpath"; case 7: return "walk"; case 8: return "walkpath";
        default: return "null";
        }
    }
    if (family == p2chappyfsm::FAMILY_KUMAKO) {
        switch (state) {
        case 0: return "dead"; case 1: return "press"; case 2: return "wait";
        case 3: return "attack"; case 4: return "flick"; case 5: return "walk";
        case 6: return "walkpath"; default: return "null";
        }
    }
    switch (state) {
    case 0: return "walk"; case 1: return "attack"; case 2: return "dead";
    case 3: return "flick"; case 4: return "warcry"; case 5: return "damage";
    case 6: return "turn"; case 7: return "eat"; case 8: return "hide";
    case 9: return "hidewait"; case 10: return "appear"; case 11: return "caution";
    case 12: return "swallow"; default: return "null";
    }
}

void transition(BTeki* actor, ChappyFsm& s, int next, unsigned gen)
{
    s.state = next;
    s.stateTime = 0.0f;
    s.attackFired = false;
    s.swallowFired = false;
    s.flickFired = false;
    auto b = banks.find(s.spec->enumName);
    if (b != banks.end()) {
        if (const char* c = clipForState(b->second, s.family, next)) s.clip = c;
    }
    std::printf("P2_CHAPPY_STATE generator=%u source_id=%u state=%s clip=%s\n", gen,
                s.spec->source, fsmStateName(s.family, next), s.clip.c_str());
    std::fflush(stdout);
}

void setPhase(ChappyFsm& s)
{
    const float dur = clipDuration(s.spec->enumName, s.clip);
    float ph = dur > 0.0f ? s.stateTime / dur : 1.0f;
    if (ph > 1.0f) ph = 1.0f;
    s.phase = ph;
}

int initialState(p2chappyfsm::Family family)
{
    if (family == p2chappyfsm::FAMILY_KUMA) return 6; // TurnPath
    if (family == p2chappyfsm::FAMILY_KUMAKO) return 2; // Wait
    if (family == p2chappyfsm::FAMILY_KING) return 0; // Walk
    return p2chappy::ADULT_SLEEP;
}

void initFsm(PelletView* view, BTeki* actor, const p2chappy::SpeciesParams* spec, unsigned token)
{
    ChappyFsm& s = fsms[view];
    s.spec = spec;
    s.family = p2chappyfsm::familyForSource(spec->source);
    s.state = initialState(s.family);
    s.stateTime = 0.0f;
    s.home = actor->getPosition();
    s.heading = actor->getDirection();
    s.wander = s.home;
    s.wanderValid = true;
    s.rng = (token * 2654435761u) | 1u;
    s.lastHealth = actor->mHealth;
    s.deadLogged = false;
    s.escaped = false;
    s.healthAsserted = true;
    auto b = banks.find(spec->enumName);
    if (b != banks.end()) {
        if (const char* c = clipForState(b->second, s.family, s.state)) s.clip = c;
    }
    s.phase = 0.0f;
    s.logTimer = 0.0f;
    s.auraTimer = 0.0f;
}

// Source ChappyBase::StateAttack KEYEVENT_2: attackNavi + eatAttackPikmin;
// KEYEVENT_3: swallowPikmin with white poison. Ported onto the P1 host with
// the banked adult/dwarf keyframes (policy AttackBiteFrame etc.).
bool doEat(BTeki* actor, const ChappyFsm& s)
{
    const Vector3f center = actor->getPosition();
    Piki* prey = nearestEdiblePiki(center, s.spec->attackHitRange);
    if (!prey) return false;
    CollPart* slot = actor->mCollInfo ? actor->getFreeSlot() : nullptr;
    if (slot) return prey->stimulate(InteractSwallow(actor, slot, 0));
    return prey->stimulate(InteractSwallow(actor, nullptr, 0));
}

int doSwallow(BTeki* actor, const ChappyFsm& s, unsigned gen, int& whitePoisoned)
{
    whitePoisoned = 0;
    int count = 0;
    Stickers stickers(actor);
    Iterator it(&stickers);
    CI_LOOP(it)
    {
        Creature* stuck = *it;
        if (!stuck || !stuck->isPiki() || !stuck->isStickToMouth()) continue;
        Piki* piki = static_cast<Piki*>(stuck);
        const bool white = pc_p2_is_white(piki);
        if (piki->stimulate(InteractKill(actor, 0))) {
            ++count;
            if (white) {
                actor->mStoredDamage += s.spec->poisonDamage;
                whitePoisoned = 1;
            }
        }
    }
    if (count > 0 || whitePoisoned) {
        std::printf("P2_CHAPPY_SWALLOW generator=%u source_id=%u swallowed=%d white=%d\n", gen,
                    s.spec->source, count, whitePoisoned);
        std::fflush(stdout);
    }
    return count;
}

void doBite(BTeki* actor, ChappyFsm& s, unsigned gen, int frame)
{
    const Vector3f pos = actor->getPosition();
    int hitNavi = 0, hitPiki = 0;
    if (naviMgr) {
        Navi* n = naviMgr->getNavi();
        if (n && n->isAlive()) {
            const Vector3f np = n->getPosition();
            if (distXZ(np, pos) < s.spec->attackHitRange) {
                const float ang = std::fabs(wrapPi(std::atan2(np.x - pos.x, np.z - pos.z) - s.heading));
                if (ang <= s.spec->attackAngle * PI_F / 180.0f) {
                    if (n->stimulate(InteractAttack(actor, nullptr, s.spec->attackDamage, false))) hitNavi = 1;
                }
            }
        }
    }
    Creature* hit = nearestTarget(pos, s.spec->attackHitRange);
    if (hit && hit->isPiki()) {
        const Vector3f tp = hit->getPosition();
        const float ang = std::fabs(wrapPi(std::atan2(tp.x - pos.x, tp.z - pos.z) - s.heading));
        if (distXZ(tp, pos) < s.spec->attackHitRange
            && ang <= s.spec->attackAngle * PI_F / 180.0f) {
            if (hit->stimulate(InteractAttack(actor, nullptr, s.spec->attackDamage, false))) hitPiki = 1;
        }
    }
    const bool eaten = doEat(actor, s);
    std::printf("P2_CHAPPY_ATTACK generator=%u source_id=%u frame=%d navi=%d piki=%d eaten=%d\n",
                gen, s.spec->source, frame, hitNavi, hitPiki, eaten ? 1 : 0);
    std::fflush(stdout);
}

void doFlick(BTeki* actor, const ChappyFsm& s, unsigned gen, int frame)
{
    const Vector3f pos = actor->getPosition();
    int hit = 0;
    if (pikiMgr) {
        Iterator it(pikiMgr);
        CI_LOOP(it)
        {
            Piki* p = static_cast<Piki*>(*it);
            if (!p || !p->isAlive()) continue;
            if (distXZ(p->getPosition(), pos) < s.spec->attackHitRange) {
                if (p->stimulate(InteractFlick(actor, 300.0f, 0.0f, FLICK_BACKWARDS_ANGLE))) ++hit;
            }
        }
    }
    if (naviMgr) {
        Navi* n = naviMgr->getNavi();
        if (n && n->isAlive() && distXZ(n->getPosition(), pos) < s.spec->attackHitRange) {
            if (n->stimulate(InteractFlick(actor, 300.0f, 0.0f, FLICK_BACKWARDS_ANGLE))) ++hit;
        }
    }
    std::printf("P2_CHAPPY_FLICK generator=%u source_id=%u frame=%d hit=%d\n", gen, s.spec->source,
                frame, hit);
    std::fflush(stdout);
}

// FireChappy signature (FireChappy.h mOnFire/TYakiBody/Flick): burning touch.
// Port adaptation: periodic InteractFire to creatures inside the hit radius
// while alive; the P2 material animation has no host equivalent.
void doFireAura(BTeki* actor, ChappyFsm& s, unsigned gen)
{
    const Vector3f pos = actor->getPosition();
    int hit = 0;
    if (pikiMgr) {
        Iterator it(pikiMgr);
        CI_LOOP(it)
        {
            Piki* p = static_cast<Piki*>(*it);
            if (!p || !p->isAlive()) continue;
            if (distXZ(p->getPosition(), pos) < s.spec->attackHitRange) {
                if (p->stimulate(InteractFire(actor, s.spec->attackDamage))) ++hit;
            }
        }
    }
    if (naviMgr) {
        Navi* n = naviMgr->getNavi();
        if (n && n->isAlive() && distXZ(n->getPosition(), pos) < s.spec->attackHitRange) {
            if (n->stimulate(InteractFire(actor, s.spec->attackDamage))) ++hit;
        }
    }
    if (hit > 0 || !s.auraLogged) {
        s.auraLogged = true;
        std::printf("P2_CHAPPY_AURA generator=%u source_id=33 hit=%d\n", gen, hit);
        std::fflush(stdout);
    }
}

void setWanderTarget(ChappyFsm& s)
{
    const float radius = 0.5f * (TERRITORY - HOME_RADIUS) * rand01(s) + HOME_RADIUS * 0.5f;
    const float angle = 2.0f * PI_F * rand01(s);
    s.wander.x = s.home.x + radius * std::sin(angle);
    s.wander.y = s.home.y;
    s.wander.z = s.home.z + radius * std::cos(angle);
    s.wanderValid = true;
}

// Bind with an explicit seed source (generated-placement path).
bool bindActorAs(BTeki* actor, unsigned generator, unsigned sourceId)
{
    if (!actor || !generator) return false;
    const p2chappy::SpeciesParams* spec = p2chappy::speciesForSource(sourceId);
    if (!spec) return false;
    PelletView* view = static_cast<PelletView*>(actor);
    if (actors.count(view)) return true;
    if (actor->mTekiType != spec->host) return false;
    if (claimedElsewhere(actor)) return false;
    if (!health.bind(view, spec->health)) return false;
    actors[view] = spec;
    actor->mHealth = spec->health;
    initFsm(view, actor, spec, generator);
    pc_randomizer_p2_bind_source(view, sourceId, generator);
    std::printf("P2_CHAPPY_DELIVERY_BIND generator=%u source_id=%u key=chappy|%s\n", generator,
                sourceId, spec->enumName);
    const auto& pos = actor->getPosition();
    std::printf("P2_ENEMY_READY species=%s source_id=%u native_family=Chappy generator=%u "
                "x=%.7f y=%.7f z=%.7f health=%.1f max_health=%.1f behavior=native host=%d "
                "source_FSM=implemented\n",
                spec->enumName, sourceId, generator, pos.x, pos.y, pos.z, actor->mHealth,
                spec->health, spec->host);
    std::printf("P2_CHAPPY_BIND generator=%u source_id=%u species=%s visual_only=0\n", generator,
                sourceId, spec->enumName);
    std::fflush(stdout);
    return true;
}
} // namespace

void pc_p2_chappy_reset()
{
    banks.clear();
    shapes.clear();
    actors.clear();
    lastClip.clear();
    corpseGenerators.clear();
    fsms.clear();
    health.reset();
    bankLoaded = false;
    drawnLive.clear();
    drawnCorpse.clear();
    bankBytes = 0;
}

void pc_p2_chappy_forget(BTeki* actor)
{
    PelletView* view = static_cast<PelletView*>(actor);
    if (actors.erase(view)) {
        std::printf("P2_CHAPPY_FORGET registered=1\n");
        std::fflush(stdout);
    }
    lastClip.erase(view);
    corpseGenerators.erase(view);
    fsms.erase(view);
    drawnLive.erase(view);
    drawnCorpse.erase(view);
    health.forget(view);
}

float pc_p2_chappy_max_health(const BTeki* actor, float fallback)
{
    return health.life(actor, fallback);
}

float pc_p2_chappy_param_f(const BTeki* actor, int idx, float fallback)
{
    if (!bankLoaded || !actors.count(const_cast<BTeki*>(actor))) return fallback;
    if (idx == TPF_Life) return health.life(actor, fallback);
    // Host blinding (catfish pattern): the suppressed P1 strategy must not
    // see/decide on stale P1 radii even if a future path calls it.
    switch (idx) {
    case TPF_VisibleRange:
    case TPF_VisibleAngle:
    case TPF_AttackableRange:
    case TPF_AttackableAngle:
    case TPF_AttackRange:
    case TPF_AttackHitRange:
    case TPF_AttackPower:
    case TPF_DangerTerritoryRange:
    case TPF_SafetyTerritoryRange:
    case TPF_LifeRecoverRate:
        return 0.0f;
    default:
        return fallback;
    }
}

bool pc_p2_chappy_suppress_ai(const BTeki* actor)
{
    return bankLoaded && actors.count(const_cast<BTeki*>(actor)) != 0;
}

bool pc_p2_chappy_probe(const BTeki* actor, const char** state, const char** clip, float* phase)
{
    auto* view = static_cast<PelletView*>(const_cast<BTeki*>(actor));
    if (!actors.count(view)) return false;
    auto ft = fsms.find(view);
    if (ft == fsms.end()) return false;
    if (state) *state = fsmStateName(ft->second.family, ft->second.state);
    if (clip) *clip = ft->second.clip.c_str();
    if (phase) *phase = ft->second.phase;
    return true;
}

void pc_p2_chappy_press(BTeki* actor)
{
    if (!actor) return;
    auto* view = static_cast<PelletView*>(actor);
    auto ft = fsms.find(view);
    if (ft == fsms.end() || ft->second.family != p2chappyfsm::FAMILY_KUMAKO) return;
    ft->second.pressed = true;
    actor->mHealth = 0.0f;
    const unsigned gen = actor->mGenerator ? pc_p2_campaign_token(actor) : 0;
    transition(actor, ft->second, 1, gen);
}

const char* pc_p2_chappy_name(PelletView* actor)
{
    auto it = actors.find(actor);
    return it == actors.end() ? nullptr : it->second->english;
}

bool pc_p2_chappy_registered(const BTeki* actor)
{
    return actors.count(const_cast<BTeki*>(actor)) != 0;
}

unsigned long pc_p2_chappy_count()
{
    return (unsigned long)actors.size();
}

bool pc_p2_chappy_receipt(PelletView* view, unsigned& generator)
{
    auto it = corpseGenerators.find(view);
    if (it == corpseGenerators.end()) return false;
    generator = it->second;
    return true;
}

void pc_p2_chappy_setup()
{
    pc_p2_chappy_reset();
    std::ifstream bank("p2-chappy-bank.txt"), bindings("p2-chappy-actors.txt");
    if (!bank && !bindings) return;
    if (!bank || !bindings || !tekiMgr) std::abort();
    // Bank: P2_CHAPPY_BANK_1, species rows, clip rows.
    std::string token;
    if (!(bank >> token) || token != "P2_CHAPPY_BANK_1") std::abort();
    std::string word;
    while (bank >> word) {
        if (word == "species") {
            std::string species;
            unsigned long long id = 0;
            if (!(bank >> species >> id)) std::abort();
            const p2chappy::SpeciesParams* spec = p2chappy::speciesForEnum(species);
            if (!spec || spec->source != (unsigned)id) std::abort();
            SpeciesBank& entry = banks[species];
            entry.source = (unsigned)id;
            entry.species = species;
        } else if (word == "clip") {
            std::string species, name, events, posesWord, status;
            long long frames = 0;
            int poses = 0;
            if (!(bank >> species >> name >> frames >> events >> posesWord >> poses >> status)) std::abort();
            if (posesWord != "poses" || status != "converted") std::abort();
            if (!banks.count(species)) std::abort();
            if (frames < 1 || frames > 10000 || poses < 1 || poses > 64) std::abort();
            if (!banks[species].clips.emplace(name, ClipDef{(int)frames, poses}).second) std::abort();
        } else {
            std::abort();
        }
    }
    if (banks.empty()) std::abort();
    // Actors: P2_CHAPPY_ACTORS_1 <count> + <generator> <Species> rows.
    if (!(bindings >> word)) std::abort();
    int count = 0;
    if (word != "P2_CHAPPY_ACTORS_1" || !(bindings >> count) || count < 1 || count > 100) std::abort();
    std::map<unsigned, std::string> wanted;
    for (int i = 0; i < count; ++i) {
        unsigned long long generator = 0;
        std::string species;
        if (!(bindings >> generator >> species)) std::abort();
        if (!generator || generator > 0xffffffffULL) std::abort();
        if (!p2chappy::speciesForEnum(species) || !wanted.emplace((unsigned)generator, species).second) std::abort();
    }
    if (bindings >> word) std::abort();
    if (pc_randomizer_p2_bridge()) {
        // Campaign identity comes from the seed, not the filed placeholders.
        // This bridge arm runs in campaign sessions via pc_p2_preview_setup.
        wanted.clear();
        for (const auto& entry : banks) {
            for (unsigned id : pc_p2_campaign_ids(entry.second.source)) wanted[id] = entry.second.species;
        }
    }
    if (wanted.empty()) return;
    // Load the staged pose bank before touching actors (fail-closed).
    for (const auto& entry : banks) {
        for (const auto& clip : entry.second.clips) {
            for (int i = 0; i < clip.second.poses; ++i) {
                char rel[192];
                std::snprintf(rel, sizeof(rel),
                              "assets/dataDir/courses/pikmin2room/ch_%s_%s_%02d.mod",
                              entry.second.species.c_str(), clip.first.c_str(), i);
                std::ifstream probe(rel, std::ios::binary | std::ios::ate);
                if (!probe) std::abort();
                const auto size = probe.tellg();
                if (size <= 0 || size_t(size) > 512 * 1024 || bankBytes + size_t(size) > 48 * 1024 * 1024) std::abort();
                bankBytes += size_t(size);
            }
        }
    }
    for (const auto& entry : banks) {
        for (const auto& clip : entry.second.clips) {
            std::vector<Shape*>& out = shapes[entry.second.species + "|" + clip.first];
            for (int i = 0; i < clip.second.poses; ++i) {
                char load[192];
                std::snprintf(load, sizeof(load), "courses/pikmin2room/ch_%s_%s_%02d.mod",
                              entry.second.species.c_str(), clip.first.c_str(), i);
                Shape* shape = gameflow.loadShape(load, true);
                if (!shape) std::abort();
                out.push_back(shape);
            }
        }
    }
    bankLoaded = true;
    // Bind the actors.
    std::set<unsigned> found;
    Iterator it(tekiMgr);
    CI_LOOP(it)
    {
        Teki* actor = static_cast<Teki*>(*it);
        if (!actor || !actor->mGenerator) continue;
        const unsigned token = pc_randomizer_p2_bridge() ? pc_p2_campaign_token(actor)
                                                         : (unsigned)actor->mGenerator->_70;
        auto match = wanted.find(token);
        if (match == wanted.end()) continue;
        const p2chappy::SpeciesParams* spec = p2chappy::speciesForEnum(match->second);
        if (!spec) std::abort();
        if (actor->mTekiType != spec->host || claimedElsewhere(actor)) {
            if (pc_p2_setup_skip(pc_randomizer_p2_bridge(), spec->enumName, "actor_identity_mismatch")) return;
        }
        // Pack generators share one campaign token across members: bind
        // every member, count the token once (proxy Finding 5 pattern).
        found.insert(token);
        PelletView* view = static_cast<PelletView*>(actor);
        if (!health.bind(view, spec->health)) std::abort();
        actors[view] = spec;
        actor->mHealth = spec->health;
        initFsm(view, actor, spec, token);
        if (pc_randomizer_p2_bridge()) {
            pc_randomizer_p2_bind_source(view, spec->source, token);
            std::printf("P2_CHAPPY_DELIVERY_BIND generator=%u source_id=%u key=chappy|%s\n", token,
                        spec->source, spec->enumName);
        }
        const auto& pos = actor->getPosition();
        std::printf("P2_ENEMY_READY species=%s source_id=%u native_family=Chappy generator=%u "
                    "x=%.7f y=%.7f z=%.7f health=%.1f max_health=%.1f behavior=native host=%d "
                    "source_FSM=implemented\n",
                    spec->enumName, spec->source, token, pos.x, pos.y, pos.z, actor->mHealth,
                    spec->health, spec->host);
        std::printf("P2_CHAPPY_BIND generator=%u source_id=%u species=%s visual_only=0\n", token,
                    spec->source, spec->enumName);
        std::printf("P2_CHAPPY_STATE generator=%u source_id=%u state=%s clip=%s\n", token,
                    spec->source, fsmStateName(fsms[view].family, fsms[view].state),
                    fsms[view].clip.c_str());
    }
    if (found.size() != wanted.size()) {
        std::printf("P2_CHAPPY_MISSING found=%zu wanted=%zu\n", found.size(), wanted.size());
        if (pc_p2_setup_skip(true, "Chappy", "actor_roster_incomplete")) return;
    }
    std::printf("P2_CHAPPY_BANK species=%zu mod_bytes=%zu\n", banks.size(), bankBytes);
    std::fflush(stdout);
}

bool pc_p2_chappy_bind_dynamic(BTeki* actor, unsigned generatorId, unsigned sourceId)
{
    const p2chappy::SpeciesParams* spec = p2chappy::speciesForSource(sourceId);
    if (!spec || !actor || !generatorId) return false;
    PelletView* view = static_cast<PelletView*>(actor);
    if (actors.count(view)) return true;
    // Only staged species bind: the pose bank arrives with the stage setup,
    // so an unstaged family member refuses here and keeps its P1/proxy path.
    if (!bankLoaded || !banks.count(spec->enumName)) return false;
    if (actor->mTekiType != spec->host) return false;
    if (claimedElsewhere(actor)) return false;
    // The pose bank arrives with the stage setup; the identity binds now so
    // damage/death/delivery track from spawn even before the first draw.
    if (!health.bind(view, spec->health)) return false;
    actors[view] = spec;
    actor->mHealth = spec->health;
    initFsm(view, actor, spec, generatorId);
    pc_randomizer_p2_bind_source(view, sourceId, generatorId);
    std::printf("P2_CHAPPY_DELIVERY_BIND generator=%u source_id=%u key=chappy|%s\n", generatorId,
                sourceId, spec->enumName);
    const auto& pos = actor->getPosition();
    std::printf("P2_ENEMY_READY species=%s source_id=%u native_family=Chappy generator=%u "
                "x=%.7f y=%.7f z=%.7f health=%.1f max_health=%.1f behavior=native host=%d "
                "source_FSM=implemented\n",
                spec->enumName, sourceId, generatorId, pos.x, pos.y, pos.z, actor->mHealth,
                spec->health, spec->host);
    std::printf("P2_CHAPPY_BIND generator=%u source_id=%u species=%s visual_only=0\n", generatorId,
                sourceId, spec->enumName);
    std::printf("P2_CHAPPY_STATE generator=%u source_id=%u state=%s clip=%s\n", generatorId,
                sourceId, fsmStateName(fsms[view].family, fsms[view].state),
                fsms[view].clip.c_str());
    std::fflush(stdout);
    return true;
}

void pc_p2_chappy_update(BTeki* actor)
{
    if (!actor || !bankLoaded) return;
    PelletView* view = static_cast<PelletView*>(actor);
    auto it = actors.find(view);
    if (it == actors.end()) return;
    auto ft = fsms.find(view);
    if (ft == fsms.end()) return;
    ChappyFsm& s = ft->second;
    const float dt = gsys->getFrameTime();
    if (dt <= 0.0f || dt > 0.5f) return;
    const unsigned generator = actor->mGenerator ? pc_p2_campaign_token(actor) : 0;

    // The suppressed P1 strategy normally applies stored damage through its
    // damage reaction; apply it here so real Pikmin hits reach mHealth.
    if (actor->mStoredDamage > 0.0f) actor->makeDamaged();

    // Natural-combat observability: incremental still-positive decrease is
    // real attack damage. Death marker records prior health.
    const float previousHealth = s.lastHealth;
    if (actor->mHealth < s.lastHealth && actor->mHealth > 0.0f) {
        std::printf("P2_CHAPPY_DAMAGE generator=%u source_id=%u health=%.1f\n", generator,
                    s.spec->source, actor->mHealth);
        std::fflush(stdout);
    }
    s.lastHealth = actor->mHealth;
    (void)previousHealth;

    // Death takes precedence in every family (source checkDead). The host
    // dieSoon() only runs inside the suppressed doAI, so finalize with
    // pcEscapeNow() once the P2 dead clip completes (frog pattern). The
    // host death funnel may still birth the pellet; the corpse is the real
    // P2-drawn pellet via pc_p2_chappy_draw(corpse=true).
    const bool deadNow = (actor->mHealth <= 0.0f || actor->mDeadState != 0);
    const int deadState = (s.family == p2chappyfsm::FAMILY_ADULT) ? p2chappy::ADULT_DEAD
        : (s.family == p2chappyfsm::FAMILY_KING)                 ? 2
                                                                : 0;
    if (deadNow && s.state != deadState && !(s.family == p2chappyfsm::FAMILY_KUMAKO && s.state == 1)) {
        if (s.family == p2chappyfsm::FAMILY_KUMAKO && s.pressed) {
            transition(actor, s, 1, generator);
        } else {
            transition(actor, s, deadState, generator);
        }
    }
    if (s.state == deadState || (s.family == p2chappyfsm::FAMILY_KUMAKO && s.state == 1)) {
        stop(actor);
        if (!s.deadLogged) {
            s.deadLogged = true;
            std::printf("P2_CHAPPY_DEAD generator=%u source_id=%u\n", generator, s.spec->source);
            std::printf("P2_CHAPPY_CORPSE_READY generator=%u source_id=%u\n", generator, s.spec->source);
            if (generator) corpseGenerators[view] = generator;
            if (!health.markDead(view)) { /* already marked */ }
            std::fflush(stdout);
        }
        s.stateTime += dt;
        setPhase(s);
        const float deadDur = clipDuration(s.spec->enumName, s.clip);
        const float wait = (s.family == p2chappyfsm::FAMILY_KUMAKO && s.state == 1)
            ? clipDuration(s.spec->enumName, s.clip)
            : deadDur;
        if (s.family == p2chappyfsm::FAMILY_KUMAKO && s.state == 1) {
            if (s.stateTime >= wait) transition(actor, s, 0, generator);
        } else if (!s.escaped && s.stateTime >= wait) {
            s.escaped = true;
            actor->pcEscapeNow();
        }
        return;
    }

    // Fire signature runs while alive (all states).
    if (s.spec->source == 33) {
        s.auraTimer += dt;
        if (s.auraTimer >= 0.5f) {
            s.auraTimer = 0.0f;
            doFireAura(actor, s, generator);
        }
    }

    const Vector3f pos = actor->getPosition();
    Creature* target = nearestTarget(pos, s.spec->sight);
    if (target) actor->setCreaturePointer(0, target);
    else actor->clearCreaturePointer(0);
    const bool sees = target != nullptr;
    const bool inRange = attackable(s, pos, target);
    // Source EnemyFunc::isStartFlick keys on Pikmin stuck to the body, not
    // mere proximity: a nearby-swarm latch flicks every few seconds and the
    // bot can never accumulate attackers (round-2 FireChappy stalemate).
    const bool flickWanted = !inRange && stuckPikminCount(actor) >= FLICK_STUCK_MIN;
    const bool farHome = distXZ(pos, s.home) > TERRITORY;
    const bool nearHome = distXZ(pos, s.home) < HOME_RADIUS;

    s.stateTime += dt;

    if (s.family == p2chappyfsm::FAMILY_ADULT) {
        switch (s.state) {
        case p2chappy::ADULT_SLEEP: {
            stop(actor);
            if (flickWanted) { transition(actor, s, p2chappy::ADULT_FLICK, generator); break; }
            if (sees || actor->mHealth < s.spec->health) {
                transition(actor, s, p2chappy::ADULT_TURN, generator);
            }
            break;
        }
        case p2chappy::ADULT_TURN: {
            stop(actor);
            if (flickWanted) { transition(actor, s, p2chappy::ADULT_FLICK, generator); break; }
            if (!sees) { transition(actor, s, p2chappy::ADULT_TURN_TO_HOME, generator); break; }
            if (inRange) { transition(actor, s, p2chappy::ADULT_ATTACK, generator); break; }
            if (target && turnTo(actor, s, target->getPosition(), dt,
                                 s.spec->attackAngle * PI_F / 180.0f)) {
                transition(actor, s, p2chappy::ADULT_WALK, generator);
            } else if (s.stateTime >= TURN_DURATION_S) {
                transition(actor, s, p2chappy::ADULT_WALK, generator);
            }
            break;
        }
        case p2chappy::ADULT_WALK: {
            if (inRange) { transition(actor, s, p2chappy::ADULT_ATTACK, generator); break; }
            if (!sees) { transition(actor, s, p2chappy::ADULT_TURN_TO_HOME, generator); break; }
            if (flickWanted) { transition(actor, s, p2chappy::ADULT_FLICK, generator); break; }
            if (farHome) {
                stop(actor);
                transition(actor, s, p2chappy::ADULT_TURN_TO_HOME, generator);
                break;
            }
            if (target) {
                const Vector3f tp = target->getPosition();
                const float ang = std::fabs(wrapPi(std::atan2(tp.x - pos.x, tp.z - pos.z) - s.heading));
                if (ang <= 25.0f * PI_F / 180.0f) walkTo(actor, s, tp, dt, s.spec->moveSpeed);
                else {
                    stop(actor);
                    transition(actor, s, p2chappy::ADULT_TURN, generator);
                }
            } else {
                stop(actor);
                transition(actor, s, p2chappy::ADULT_TURN_TO_HOME, generator);
            }
            break;
        }
        case p2chappy::ADULT_ATTACK: {
            stop(actor);
            const float frames = s.stateTime * 30.0f;
            if (!s.attackFired && frames >= float(p2chappy::AttackBiteFrame)) {
                s.attackFired = true;
                doBite(actor, s, generator, p2chappy::AttackBiteFrame);
            }
            if (!s.swallowFired && frames >= float(p2chappy::AttackSwallowFrame)) {
                s.swallowFired = true;
                int white = 0;
                doSwallow(actor, s, generator, white);
            }
            if (frames >= float(p2chappy::AttackEndFrame)) {
                if (inRange) transition(actor, s, p2chappy::ADULT_ATTACK, generator);
                else if (sees) transition(actor, s, p2chappy::ADULT_TURN, generator);
                else transition(actor, s, p2chappy::ADULT_TURN_TO_HOME, generator);
            }
            break;
        }
        case p2chappy::ADULT_FLICK: {
            stop(actor);
            if (!s.flickFired && s.stateTime * 30.0f >= 31.0f) {
                s.flickFired = true;
                doFlick(actor, s, generator, 31);
            }
            if (s.stateTime >= FLICK_DURATION_S) {
                if (inRange) transition(actor, s, p2chappy::ADULT_ATTACK, generator);
                else if (sees) transition(actor, s, p2chappy::ADULT_WALK, generator);
                else transition(actor, s, p2chappy::ADULT_TURN_TO_HOME, generator);
            }
            break;
        }
        case p2chappy::ADULT_TURN_TO_HOME: {
            stop(actor);
            if (inRange) { transition(actor, s, p2chappy::ADULT_ATTACK, generator); break; }
            if (nearHome) { transition(actor, s, p2chappy::ADULT_SLEEP, generator); break; }
            if (turnTo(actor, s, s.home, dt, 0.1f) || s.stateTime >= TURN_DURATION_S) {
                transition(actor, s, p2chappy::ADULT_GO_HOME, generator);
            }
            break;
        }
        case p2chappy::ADULT_GO_HOME: {
            if (nearHome) {
                stop(actor);
                transition(actor, s, p2chappy::ADULT_SLEEP, generator);
                break;
            }
            if (inRange) { transition(actor, s, p2chappy::ADULT_ATTACK, generator); break; }
            if (sees) { transition(actor, s, p2chappy::ADULT_WALK, generator); break; }
            walkTo(actor, s, s.home, dt, s.spec->moveSpeed);
            break;
        }
        default:
            stop(actor);
            transition(actor, s, p2chappy::ADULT_SLEEP, generator);
            break;
        }
    } else if (s.family == p2chappyfsm::FAMILY_KUMA) {
        // Leaf (67) rides the Kuma FSM with its own smaller parms; its
        // birthChildren Bulbmin spawn has no host equivalent and is logged.
        if (s.spec->source == 67 && !s.birthLogged && sees) {
            s.birthLogged = true;
            std::printf("P2_CHAPPY_BIRTH generator=%u source_id=67 children=0 note=no_host_equivalent\n",
                        generator);
            std::fflush(stdout);
        }
        switch (s.state) {
        case 6: { // TurnPath
            stop(actor);
            if (inRange) { transition(actor, s, 3, generator); break; }
            if (flickWanted) { transition(actor, s, 4, generator); break; }
            if (sees) { transition(actor, s, 7, generator); break; }
            if (s.stateTime >= TURN_DURATION_S) transition(actor, s, 8, generator);
            break;
        }
        case 8: { // WalkPath (patrol wander)
            if (inRange) { transition(actor, s, 3, generator); break; }
            if (flickWanted) { transition(actor, s, 4, generator); break; }
            if (sees) { transition(actor, s, 7, generator); break; }
            if (!s.wanderValid || distXZ(s.wander, pos) < 20.0f) setWanderTarget(s);
            walkTo(actor, s, s.wander, dt, s.spec->moveSpeed);
            if (farHome) { transition(actor, s, 2, generator); break; }
            break;
        }
        case 7: { // Walk (chase)
            if (inRange) { transition(actor, s, 3, generator); break; }
            if (!sees) { transition(actor, s, 2, generator); break; }
            if (flickWanted) { transition(actor, s, 4, generator); break; }
            if (target) walkTo(actor, s, target->getPosition(), dt, s.spec->moveSpeed);
            if (farHome && !sees) { transition(actor, s, 2, generator); break; }
            break;
        }
        case 3: { // Attack
            stop(actor);
            const float frames = s.stateTime * 30.0f;
            if (!s.attackFired && frames >= float(p2chappy::AttackBiteFrame)) {
                s.attackFired = true;
                doBite(actor, s, generator, p2chappy::AttackBiteFrame);
            }
            if (!s.swallowFired && frames >= float(p2chappy::AttackSwallowFrame)) {
                s.swallowFired = true;
                int white = 0;
                doSwallow(actor, s, generator, white);
            }
            if (frames >= float(p2chappy::AttackEndFrame)) {
                if (inRange) transition(actor, s, 3, generator);
                else if (sees) transition(actor, s, 7, generator);
                else transition(actor, s, 2, generator);
            }
            break;
        }
        case 4: { // Flick
            stop(actor);
            if (!s.flickFired && s.stateTime * 30.0f >= 31.0f) {
                s.flickFired = true;
                doFlick(actor, s, generator, 31);
            }
            if (s.stateTime >= FLICK_DURATION_S) {
                transition(actor, s, sees ? 7 : 6, generator);
            }
            break;
        }
        case 2: { // Lost
            stop(actor);
            if (inRange) { transition(actor, s, 3, generator); break; }
            if (sees) { transition(actor, s, 7, generator); break; }
            // Rebirth timer (KumaChappy proper fp12 adaptation): the carcass
            // revive has no host equivalent while alive, so Lost graduates
            // back to patrol via Rebirth.
            if (s.stateTime >= LOST_REBIRTH_S) {
                transition(actor, s, 1, generator);
                std::printf("P2_CHAPPY_REBIRTH generator=%u source_id=%u\n", generator, s.spec->source);
                std::fflush(stdout);
            }
            break;
        }
        case 1: { // Rebirth
            stop(actor);
            if (s.stateTime >= clipDuration(s.spec->enumName, s.clip)) {
                transition(actor, s, 6, generator);
            }
            break;
        }
        case 5: { // Turn
            stop(actor);
            if (inRange) { transition(actor, s, 3, generator); break; }
            if (s.stateTime >= TURN_DURATION_S) transition(actor, s, 7, generator);
            break;
        }
        default:
            stop(actor);
            transition(actor, s, 6, generator);
            break;
        }
    } else if (s.family == p2chappyfsm::FAMILY_KUMAKO) {
        Vector3f parentPos = s.home;
        const bool hasParent = parentNear(actor, pos, parentPos);
        switch (s.state) {
        case 2: { // Wait
            stop(actor);
            if (inRange) { transition(actor, s, 3, generator); break; }
            if (flickWanted) { transition(actor, s, 4, generator); break; }
            if (hasParent || sees) transition(actor, s, 6, generator);
            break;
        }
        case 6: { // WalkPath (parent-follow / chase)
            if (inRange) { transition(actor, s, 3, generator); break; }
            if (flickWanted) { transition(actor, s, 4, generator); break; }
            if (target && sees) walkTo(actor, s, target->getPosition(), dt, s.spec->moveSpeed);
            else if (hasParent) walkTo(actor, s, parentPos, dt, s.spec->moveSpeed);
            else transition(actor, s, 2, generator);
            break;
        }
        case 5: { // Walk
            if (inRange) { transition(actor, s, 3, generator); break; }
            if (!sees && !hasParent) { transition(actor, s, 2, generator); break; }
            if (target && sees) walkTo(actor, s, target->getPosition(), dt, s.spec->moveSpeed);
            else if (hasParent) walkTo(actor, s, parentPos, dt, s.spec->moveSpeed);
            break;
        }
        case 3: { // Attack
            stop(actor);
            const float frames = s.stateTime * 30.0f;
            if (!s.attackFired && frames >= float(p2chappy::DwarfAttackEatFrame)) {
                s.attackFired = true;
                doBite(actor, s, generator, p2chappy::DwarfAttackEatFrame);
            }
            if (!s.swallowFired && frames >= float(p2chappy::DwarfAttackSwallowFrame)) {
                s.swallowFired = true;
                int white = 0;
                doSwallow(actor, s, generator, white);
            }
            if (frames >= float(p2chappy::DwarfAttackSwallowFrame) + 2.0f) {
                if (inRange) transition(actor, s, 3, generator);
                else transition(actor, s, 5, generator);
            }
            break;
        }
        case 4: { // Flick
            stop(actor);
            if (!s.flickFired && s.stateTime * 30.0f >= 31.0f) {
                s.flickFired = true;
                doFlick(actor, s, generator, 31);
            }
            if (s.stateTime >= FLICK_DURATION_S) transition(actor, s, 5, generator);
            break;
        }
        default:
            stop(actor);
            transition(actor, s, 2, generator);
            break;
        }
    } else { // KING
        switch (s.state) {
        case 0: { // Walk
            if (inRange) { transition(actor, s, 1, generator); break; }
            if (flickWanted) { transition(actor, s, 3, generator); break; }
            if (sees) { transition(actor, s, 4, generator); break; }
            if (s.stateTime >= KING_BURROW_IDLE_S && !sees) {
                transition(actor, s, 8, generator);
                break;
            }
            if (target && sees) walkTo(actor, s, target->getPosition(), dt, s.spec->moveSpeed);
            else {
                if (!s.wanderValid || distXZ(s.wander, pos) < 20.0f) setWanderTarget(s);
                walkTo(actor, s, s.wander, dt, s.spec->moveSpeed);
            }
            break;
        }
        case 4: { // WarCry (source roar fp03/fp04 adaptation: log + hold)
            stop(actor);
            if (s.stateTime >= WARCRY_DURATION_S) {
                std::printf("P2_CHAPPY_WARCRY generator=%u source_id=53\n", generator);
                std::fflush(stdout);
                transition(actor, s, 0, generator);
            }
            break;
        }
        case 6: { // Turn
            stop(actor);
            if (inRange) { transition(actor, s, 1, generator); break; }
            if (s.stateTime >= TURN_DURATION_S) transition(actor, s, 0, generator);
            break;
        }
        case 1: { // Attack
            stop(actor);
            const float frames = s.stateTime * 30.0f;
            if (!s.attackFired && frames >= float(p2chappy::AttackBiteFrame)) {
                s.attackFired = true;
                doBite(actor, s, generator, p2chappy::AttackBiteFrame);
            }
            if (!s.swallowFired && frames >= float(p2chappy::AttackSwallowFrame)) {
                s.swallowFired = true;
                int white = 0;
                doSwallow(actor, s, generator, white);
            }
            if (frames >= float(p2chappy::AttackEndFrame)) {
                if (inRange) transition(actor, s, 1, generator);
                else transition(actor, s, 0, generator);
            }
            break;
        }
        case 3: { // Flick
            stop(actor);
            if (!s.flickFired && s.stateTime * 30.0f >= 31.0f) {
                s.flickFired = true;
                doFlick(actor, s, generator, 31);
            }
            if (s.stateTime >= FLICK_DURATION_S) transition(actor, s, 0, generator);
            break;
        }
        case 5: { // Damage (bomb stun; bombs have no host equivalent)
            stop(actor);
            if (s.stateTime >= clipDuration(s.spec->enumName, s.clip)) transition(actor, s, 0, generator);
            break;
        }
        case 8: { // Hide
            stop(actor);
            if (s.stateTime >= 1.0f) transition(actor, s, 9, generator);
            break;
        }
        case 9: { // HideWait (burrowed; ip02 adaptation)
            stop(actor);
            if (s.stateTime >= KING_HIDEWAIT_S) transition(actor, s, 10, generator);
            break;
        }
        case 10: { // Appear
            stop(actor);
            if (s.stateTime >= clipDuration(s.spec->enumName, s.clip)) {
                std::printf("P2_CHAPPY_APPEAR generator=%u source_id=53\n", generator);
                std::fflush(stdout);
                transition(actor, s, 11, generator);
            }
            break;
        }
        case 11: { // Caution
            stop(actor);
            if (s.stateTime >= clipDuration(s.spec->enumName, s.clip)) transition(actor, s, 0, generator);
            break;
        }
        case 7: { // Eat -> Swallow (bomb path has no host equivalent)
            stop(actor);
            transition(actor, s, 12, generator);
            break;
        }
        case 12: { // Swallow
            stop(actor);
            if (s.stateTime >= clipDuration(s.spec->enumName, s.clip)) transition(actor, s, 0, generator);
            break;
        }
        default:
            stop(actor);
            transition(actor, s, 0, generator);
            break;
        }
    }

    setPhase(s);
    s.logTimer += dt;
    if (s.logTimer >= 1.0f) {
        s.logTimer = 0.0f;
        const Vector3f now = actor->getPosition();
        std::printf("P2_CHAPPY_FSM_POS generator=%u source_id=%u state=%s x=%.2f y=%.2f z=%.2f health=%.1f\n",
                    generator, s.spec->source, fsmStateName(s.family, s.state), now.x, now.y, now.z,
                    actor->mHealth);
        std::fflush(stdout);
    }
}

bool pc_p2_chappy_draw(BTeki* actor, Graphics& gfx, const Matrix4f& matrix, bool corpse)
{
    if (!actor || !bankLoaded) return false;
    PelletView* view = static_cast<PelletView*>(actor);
    auto it = actors.find(view);
    if (it == actors.end()) return false;
    const p2chappy::SpeciesParams* spec = it->second;
    auto bank = banks.find(spec->enumName);
    if (bank == banks.end() || bank->second.clips.empty()) return false;
    // The P2 FSM decides the clip/phase every tick; the host animator motion
    // is never consulted (it reflects the suppressed P1 AI).
    std::string clip = "wait1";
    float phase = 0.0f;
    bool dead = corpse;
    auto ft = fsms.find(view);
    if (ft != fsms.end()) {
        clip = ft->second.clip;
        phase = ft->second.phase;
        if (actor->mHealth <= 0.0f || actor->mDeadState != 0) dead = true;
    } else {
        dead = corpse || actor->mHealth <= 0.0f || actor->mDeadState != 0;
    }
    if (dead) {
        const char* prefs[] = {"dead", "dead1", "pdead1", nullptr};
        for (const char** p = prefs; *p; ++p) {
            if (bank->second.clips.count(*p)) { clip = *p; break; }
        }
        phase = 1.0f;
    } else if (!bank->second.clips.count(clip)) {
        if (const char* c = clipForState(bank->second,
                                         ft != fsms.end() ? ft->second.family
                                                          : p2chappyfsm::familyForSource(spec->source),
                                         ft != fsms.end() ? ft->second.state : 7)) {
            clip = c;
        } else {
            return false;
        }
    }
    auto shapesIt = shapes.find(std::string(spec->enumName) + "|" + clip);
    if (shapesIt == shapes.end() || shapesIt->second.empty()) return false;
    size_t index = 0;
    if (!dead && shapesIt->second.size() > 1) {
        float ph = phase;
        if (!(ph >= 0.0f && ph <= 1.0f)) ph = 0.0f;
        index = size_t(ph * float(shapesIt->second.size() - 1) + 0.5f);
        if (index >= shapesIt->second.size()) index = shapesIt->second.size() - 1;
    } else {
        index = shapesIt->second.size() - 1;
    }
    Shape* shape = shapesIt->second[index];
    // Per-actor first-draw markers (frog pattern): every bound actor logs
    // its own live draw and its own corpse draw with its campaign token, so
    // a bystander drawn first can never consume another actor's evidence.
    if (!corpse) {
        if (drawnLive.insert(view).second) {
            const unsigned generator = actor->mGenerator ? pc_p2_campaign_token(actor) : 0;
            std::printf("P2_CHAPPY_DRAW corpse=0 species=%s generator=%u clip=%s\n", spec->enumName,
                        generator, clip.c_str());
            std::fflush(stdout);
        }
    } else if (drawnCorpse.insert(view).second) {
        const unsigned generator = actor->mGenerator ? pc_p2_campaign_token(actor) : 0;
        unsigned logged = generator;
        if (!logged) {
            auto cg = corpseGenerators.find(view);
            if (cg != corpseGenerators.end()) logged = cg->second;
        }
        std::printf("P2_CHAPPY_DRAW corpse=1 species=%s generator=%u clip=%s\n", spec->enumName,
                    logged, clip.c_str());
        std::fflush(stdout);
    }
    {
        auto known = lastClip.find(view);
        if (known == lastClip.end() || known->second != clip) {
            lastClip[view] = clip;
            const unsigned generator = actor->mGenerator ? pc_p2_campaign_token(actor) : 0;
            std::printf("P2_CHAPPY_STATE generator=%u source_id=%u clip=%s\n", generator,
                        spec->source, clip.c_str());
            std::fflush(stdout);
        }
    }
    shape->updateAnim(gfx, matrix, nullptr, actor);
    shape->drawshape(gfx, *gfx.mCamera, nullptr);
    return true;
}
