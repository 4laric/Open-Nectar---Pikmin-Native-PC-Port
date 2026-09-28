#include "pc_p2_bigtreasure_teki.h"
#include "pc_p2_bigtreasure_own.h"
#include "pc_p2_bigtreasure_map_trace.h"
#include "pc_p2_campaign_actor.h"
#include "pc_p2_setup_failsafe.h"
#include "pc_p2_navi_select.h"
#include "pc_p2_species.h"
#include "pc_p2_animation.h"
#include "pc_bbft.h"
#include "Generator.h"
#include "GlobalGameOptions.h"
#include "Graphics.h"
#include "Interactions.h"
#include "MapMgr.h"
#include "Navi.h"
#include "NaviMgr.h"
#include "Piki.h"
#include "PikiMgr.h"
#include "Shape.h"
#include "Texture.h"
#include "gameflow.h"
#include "gl/pc_gfx.h"
#include "system.h"
#include "teki.h"
#include <cmath>
#include <cstdio>
#include <fstream>
#include <map>
#include <string>
#include <vector>

namespace {
using namespace p2btown;

const char* weaponName(int w) {
    switch (w) {
    case P2BTWEAPON_Elec: return "elec";
    case P2BTWEAPON_Fire: return "fire";
    case P2BTWEAPON_Gas: return "gas";
    case P2BTWEAPON_Water: return "water";
    default: return "louie";
    }
}

struct DroppedWeapon {
    int weapon = -1;
    Vector3f pos, vel;
    bool resting = false;
};

struct Binding {
    unsigned generator = 0;
    Fsm fsm;
    std::vector<Hit> pending;   // hits recorded by interactDefault since the last tick
    float debt = 0.0f;         // 30 Hz source clock debt
    float lastHealth = 0.0f;
    bool deadLogged = false;
    bool escaped = false;
    bool began = false;        // host death funnel ran (corpse pellet)
    float logTimer = 0.0f;
    int attacks = 0;
    bool emitLogged = false;
    int ignored = 0;
    std::vector<DroppedWeapon> dropped;
    std::map<const void*, int> recvCount;
};
std::map<BTeki*, Binding> s;

Params sParams;
Bank sBank = defaultBank();
p2btown::Animator sAnimator;
bool sReady = false;
P2BigTreasureMapTrace* sTrace = nullptr;
std::vector<Shape*> sPoses[AnimCount];
Shape* sPellet[P2BTWEAPON_Count] = {};
bool sPosesLoaded = false;
std::map<BTeki*, int> sDrawLogged;

Binding* find(const BTeki* t) {
    auto i = s.find(const_cast<BTeki*>(t));
    return i == s.end() ? nullptr : &i->second;
}

// Pose bank (Groink/tank pattern): shared materials, bounded bytes.
Shape* loadShape(const std::string& rel, Shape*& shared, std::size_t& total) {
    const std::string path = "assets/dataDir/courses/pikmin2room/" + rel;
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) return nullptr;
    const auto size = file.tellg();
    if (size <= 0 || size > 2 * 1024 * 1024 || total + std::size_t(size) > 40u * 1024 * 1024) return nullptr;
    total += std::size_t(size);
    file.seekg(0);
    std::vector<unsigned char> bytes(std::size_t(size), 0), resources;
    if (!file.read(reinterpret_cast<char*>(bytes.data()), size) || !p2animation::resources(bytes, resources)) return nullptr;
    Shape* shape = gameflow.loadShape(("courses/pikmin2room/" + rel).c_str(), true);
    if (!shape) return nullptr;
    if (!shared) {
        shared = shape;
        for (int t = 0; t < shape->mTexAttrCount; ++t)
            if (shape->mTexAttrList[t].mTexture) shape->mTexAttrList[t].mTexture->attach();
        return shape;
    }
    if (shape->mMaterialCount != shared->mMaterialCount || shape->mTexAttrCount != shared->mTexAttrCount
        || shape->mTevInfoCount != shared->mTevInfoCount) return nullptr;
    for (int j = 0; j < shape->mTotalMatpolyCount; ++j) {
        auto* poly = shape->mMatpolyList[j];
        if (!poly || !poly->mMaterial) continue;
        int material = -1;
        for (int m = 0; m < shape->mMaterialCount; ++m)
            if (poly->mMaterial == &shape->mMaterialList[m]) material = m;
        if (material < 0) return nullptr;
        poly->mMaterial = &shared->mMaterialList[material];
    }
    shape->mMaterialList = shared->mMaterialList;
    shape->mTexAttrList = shared->mTexAttrList;
    shape->mTevInfoList = shared->mTevInfoList;
    return shape;
}

Shape* loadPellet(const std::string& rel) {
    const std::string path = "assets/dataDir/courses/pikmin2room/" + rel;
    std::ifstream probe(path, std::ios::binary);
    if (!probe) return nullptr;
    Shape* shape = gameflow.loadShape(("courses/pikmin2room/" + rel).c_str(), true);
    if (shape)
        for (int t = 0; t < shape->mTexAttrCount; ++t)
            if (shape->mTexAttrList[t].mTexture) shape->mTexAttrList[t].mTexture->attach();
    return shape;
}

bool loadInputs(bool bridge) {
    sParams = Params{};
    {
        std::ifstream in("p2-bigtreasure-parms.txt");
        std::string error;
        if (!in) {
            std::printf("P2_BIGTREASURE_PARMS_MISSING fallback=disc_defaults\n");
        } else if (!parseEnemyParm(in, sParams, error)) {
            std::printf("P2_BIGTREASURE_PARMS_INVALID reason=%s\n", error.c_str());
            return !pc_p2_setup_skip(bridge, "BigTreasure", "parms_invalid");
        }
    }
    std::printf("P2_BIGTREASURE_PARMS source_id=73 retail=%d health=%.1f move=%.1f turn=%.1f private=%.1f "
                "territory=%.1f sight=%.1f attack=%.1f ik_base=%.2f wait_e=%.2f wait_f=%.2f/%.2f wait_g=%.2f "
                "wait_w=%.2f atk_max=%.1f/%.1f/%.1f/%.1f\n",
                sParams.retail ? 1 : 0, sParams.health, sParams.moveSpeed, sParams.maxTurnAngle, sParams.privateRadius,
                sParams.territoryRadius, sParams.sightRadius, sParams.attackDamage, sParams.baseFactor, sParams.elecWait,
                sParams.fireWait1, sParams.fireWait2, sParams.gasWait, sParams.waterWait, sParams.elecAttackMax,
                sParams.fireAttackMax, sParams.gasAttackMax, sParams.waterAttackMax);
    {
        std::ifstream in("p2_bigtreasure_events.txt", std::ios::binary);
        std::string error;
        if (!in) {
            std::printf("P2_BIGTREASURE_EVENTS_MISSING\n");
            return !pc_p2_setup_skip(bridge, "BigTreasure", "events_missing");
        }
        try {
            const p2retail::Table table = p2retail::read(in);
            if (!sAnimator.load(table, error)) {
                std::printf("P2_BIGTREASURE_EVENTS_INVALID reason=%s\n", error.c_str());
                return !pc_p2_setup_skip(bridge, "BigTreasure", "events_invalid");
            }
        } catch (const std::exception&) {
            std::printf("P2_BIGTREASURE_EVENTS_INVALID reason=table\n");
            return !pc_p2_setup_skip(bridge, "BigTreasure", "events_invalid");
        }
    }
    sBank = defaultBank();
    {
        std::ifstream in("p2-bigtreasure-bank.txt");
        std::string error;
        if (in && !parseBank(in, sBank, error)) {
            std::printf("P2_BIGTREASURE_BANK_INVALID reason=%s fallback=bind_pose draw=host\n", error.c_str());
            sBank = defaultBank();
        }
    }
    // Poses: bigtreasure_<clip>_<ii>.mod; slot 29 (Walk) reuses wait2's.
    for (auto& v : sPoses) v.clear();
    for (Shape*& p : sPellet) p = nullptr;
    sPosesLoaded = false;
    Shape* shared = nullptr;
    std::size_t total = 0, poses = 0;
    bool ok = sBank.staged;
    for (int a = 0; ok && a < AnimCount; ++a) {
        if (a == AnimWait2_2) continue;
        const ClipBank& clip = sBank.clip[a];
        for (std::size_t i = 0; i < clip.poses.size(); ++i) {
            char rel[160];
            std::snprintf(rel, sizeof(rel), "bigtreasure_%s_%02u.mod", clip.name.c_str(), unsigned(i));
            Shape* shape = loadShape(rel, shared, total);
            if (!shape) { ok = false; break; }
            sPoses[a].push_back(shape);
            ++poses;
        }
    }
    if (ok) sPoses[AnimWait2_2] = sPoses[AnimWait2];
    if (!ok) for (auto& v : sPoses) v.clear();
    sPosesLoaded = ok && poses > 0;
    int pellets = 0;
    for (int w = 0; w < P2BTWEAPON_Count && sPosesLoaded; ++w) {
        char rel[96];
        std::snprintf(rel, sizeof(rel), "bigtreasure_pellet_%s.mod", weaponName(w));
        sPellet[w] = loadPellet(rel);
        if (sPellet[w]) ++pellets;
    }
    std::printf("P2_BIGTREASURE_BANK staged=%d legs_staged=%d leg_distance=%.1f poses=%zu bytes=%zu pellets=%d draw=%s\n",
                sBank.staged ? 1 : 0, sBank.legs.staged ? 1 : 0, sBank.legs.distance, sPosesLoaded ? poses : std::size_t(0),
                total, pellets, sPosesLoaded ? "p2_model" : "host");
    std::fflush(stdout);
    return true;
}

// Snapshot in source manager order: captains, then Piki.
struct Snapshot {
    std::vector<Candidate> c;
    std::vector<Creature*> who;
};
void buildSnapshot(BTeki* self, Snapshot& snap) {
    snap.c.clear();
    snap.who.clear();
    auto add = [&](Creature* cr, Candidate cand) {
        const Vector3f p = cr->getPosition();
        cand.id = std::uint64_t(snap.who.size() + 1);
        cand.pos = {p.x, p.y, p.z};
        cand.alive = cr->isAlive();
        snap.c.push_back(cand);
        snap.who.push_back(cr);
    };
    for (Navi* n : pc_p2_navis()) {
        if (!n) continue;
        Candidate cand;
        cand.navi = true;
        add(n, cand);
    }
    if (pikiMgr) {
        Iterator it(pikiMgr);
        CI_LOOP(it) {
            Piki* p = static_cast<Piki*>(*it);
            if (!p) continue;
            Candidate cand;
            cand.pikmin = true;
            Creature* stick = p->getStickObject();
            cand.stuckToSelf = stick == self;
            cand.stuckElsewhere = stick && stick != self;
            cand.blue = p->mColor == Blue;
            cand.buried = p->isBuried() || p->isStickToMouth();
            add(p, cand);
        }
    }
}
Creature* creatureFor(const Snapshot& snap, std::uint64_t id) {
    return id >= 1 && id <= snap.who.size() ? snap.who[std::size_t(id - 1)] : nullptr;
}

void logState(const Binding& b, const Transition& tr, BTeki* t) {
    const Vector3f p = t->getPosition();
    std::printf("P2_BIGTREASURE_FSM generator=%u source_id=73 from=%s state=%s x=%.1f z=%.1f health=%.1f weapons=%d "
                "hp=%.0f/%.0f/%.0f/%.0f anim=%s\n",
                b.generator, stateName(tr.from), stateName(tr.to), p.x, p.z, t->mHealth, b.fsm.ownership().weaponCount(),
                b.fsm.ownership().weaponHealth(0), b.fsm.ownership().weaponHealth(1), b.fsm.ownership().weaponHealth(2),
                b.fsm.ownership().weaponHealth(3), animClipName(b.fsm.animator().anim()) ? animClipName(b.fsm.animator().anim()) : "-");
}

bool stimulateElement(BTeki* t, Creature* c, const ElementHit& eh) {
    // BigTreasureAttack.cpp receivers: Pikmin take the element stimulus;
    // captains fall back to a flick/zero-damage attack when they refuse it.
    bool accepted = false;
    const P2BigTreasureReceiverHit& hit = eh.hit;
    switch (hit.stimulus) {
    case P2BigTreasureReceiverStimulus::Fire: accepted = c->stimulate(InteractFire(t, hit.damage)); break;
    case P2BigTreasureReceiverStimulus::Gas: accepted = c->stimulate(InteractGas(t, hit.damage)); break;
    case P2BigTreasureReceiverStimulus::Water: accepted = c->stimulate(InteractBubble(t, 0.0f)); break;
    case P2BigTreasureReceiverStimulus::Elec: {
        Vector3f dir(hit.direction.x, hit.direction.y, hit.direction.z);
        accepted = c->stimulate(InteractDenki(t, hit.damage, &dir));
        break;
    }
    case P2BigTreasureReceiverStimulus::None: return false;
    }
    if (!accepted && eh.navi && hit.stimulus != P2BigTreasureReceiverStimulus::Gas) {
        c->stimulate(InteractAttack(t, nullptr, 0.0f, false));
    }
    return accepted;
}

void applyOutput(BTeki* t, Binding& b, const Snapshot& snap, const TickOutput& o) {
    for (const Transition& tr : o.entered) {
        logState(b, tr, t);
        if (tr.to == State::Dead && !b.deadLogged) {
            b.deadLogged = true;
            // StateDead::init -> deathProcedure -> setAlive(false): stuck
            // Pikmin let go (aiAttack drops a non-alive stick target) and no
            // further hit lands. The host death funnel runs at KEYEVENT_END.
            t->clearTekiOption(TEKIOPT_Alive);
            std::printf("P2_BIGTREASURE_DEAD generator=%u source_id=73 health=%.1f weapons=%d\n", b.generator,
                        t->mHealth, b.fsm.ownership().weaponCount());
        }
        if (tr.to == State::Attack) b.emitLogged = false;
    }
    for (int w = 0; w < P2BTWEAPON_Count; ++w) {
        if (!o.weaponHits[w]) continue;
        std::printf("P2_BIGTREASURE_DAMAGE generator=%u source_id=73 part=%s hits=%d damage=%.1f hp=%.1f state=%s%s\n",
                    b.generator, weaponName(w), o.weaponHits[w], o.weaponDamage[w], b.fsm.ownership().weaponHealth(w),
                    stateName(b.fsm.state()), o.pinchSmoke[w] ? " pinch=1" : "");
    }
    if (o.ignoredHits) b.ignored += o.ignoredHits;
    if (!o.flick.empty() || (o.flickReason && o.flick.empty())) {
        int done = 0;
        for (auto id : o.flick)
            if (Creature* c = creatureFor(snap, id))
                if (c->isAlive() && c->getStickObject() == t
                    && c->stimulate(InteractFlick(t, o.flickKnockback, o.flickDamage, o.flickAngle))) ++done;
        if (o.flickReason)
            std::printf("P2_BIGTREASURE_FLICK generator=%u source_id=73 reason=%s flicked=%d/%zu knockback=%.1f\n",
                        b.generator, o.flickReason, done, o.flick.size(), o.flickKnockback);
    }
    if (o.pickedWeapon >= 0)
        std::printf("P2_BIGTREASURE_PICK generator=%u source_id=73 weapon=%s hp=%.0f/%.0f/%.0f/%.0f\n", b.generator,
                    weaponName(o.pickedWeapon), b.fsm.ownership().weaponHealth(0), b.fsm.ownership().weaponHealth(1),
                    b.fsm.ownership().weaponHealth(2), b.fsm.ownership().weaponHealth(3));
    if (o.attackStarted >= 0) {
        ++b.attacks;
        b.recvCount.clear();
        std::printf("P2_BIGTREASURE_ATTACK_START generator=%u source_id=73 weapon=%s variant=%d hp=%.1f n=%d\n",
                    b.generator, weaponName(o.attackStarted), o.fireVariant,
                    b.fsm.ownership().weaponHealth(o.attackStarted), b.attacks);
    }
    if (o.attackNodes > 0 && !b.emitLogged) {
        b.emitLogged = true;
        std::printf("P2_BIGTREASURE_ATTACK_EMIT generator=%u source_id=73 weapon=%s nodes=%d\n", b.generator,
                    weaponName(b.fsm.elements().activeWeapon()), o.attackNodes);
    }
    if (o.attackFinished) std::printf("P2_BIGTREASURE_ATTACK_FINISH generator=%u source_id=73\n", b.generator);
    for (const ElementHit& eh : o.elementHits) {
        Creature* c = creatureFor(snap, eh.id);
        if (!c || !c->isAlive()) continue;
        const bool accepted = stimulateElement(t, c, eh);
        std::printf("P2_BIGTREASURE_RECV generator=%u source_id=73 weapon=%s target=%s color=%d accepted=%d\n",
                    b.generator, p2_bigtreasure_receiver_stimulus_name(eh.hit.stimulus), eh.navi ? "navi" : "piki",
                    eh.navi ? -1 : int(static_cast<Piki*>(c)->mColor), accepted ? 1 : 0);
    }
    for (const Drop& d : o.drops) {
        if (d.weapon >= 0) {
            int parts = 0;
            for (auto id : o.partFlick)
                if (Creature* c = creatureFor(snap, id))
                    if (c->isAlive() && c->getStickObject() == t
                        && c->stimulate(InteractFlick(t, 10.0f, 0.0f, o.partFlickAngle))) ++parts;
            DroppedWeapon dw;
            dw.weapon = d.weapon;
            dw.pos = Vector3f(d.position.x, d.position.y, d.position.z);
            dw.vel = Vector3f(d.velocity.x, d.velocity.y, d.velocity.z);
            b.dropped.push_back(dw);
            std::printf("P2_BIGTREASURE_WEAPON_DROP generator=%u source_id=73 weapon=%s x=%.1f y=%.1f z=%.1f vy=%.1f "
                        "part_flick=%d/%zu remaining=%d state=%s\n",
                        b.generator, weaponName(d.weapon), d.position.x, d.position.y, d.position.z, d.velocity.y, parts,
                        o.partFlick.size(), b.fsm.ownership().weaponCount(), stateName(b.fsm.state()));
        } else {
            std::printf("P2_BIGTREASURE_RELEASE generator=%u source_id=73 item=louie x=%.1f y=%.1f z=%.1f vy=%.1f "
                        "throwup=%d carryable=0\n",
                        b.generator, d.position.x, d.position.y, d.position.z, d.velocity.y, o.throwupItem ? 1 : 0);
        }
    }
}

void stepDropped(Binding& b, float dt) {
    for (DroppedWeapon& d : b.dropped) {
        if (d.resting) continue;
        d.vel.y -= 490.0f * dt; // visual pop only (not a P1 pellet)
        d.pos.x += d.vel.x * dt;
        d.pos.y += d.vel.y * dt;
        d.pos.z += d.vel.z * dt;
        const float ground = mapMgr ? mapMgr->getMinY(d.pos.x, d.pos.z, true) : 0.0f;
        if (d.pos.y <= ground) {
            d.pos.y = ground;
            d.resting = true;
        }
    }
}

// Live tick. Returns true once the host teardown ran.
bool ownTick(BTeki* t, Binding& b, float dt) {
    b.debt += dt;
    int ticks = int(b.debt / kSourceDelta);
    if (ticks > 4) ticks = 4;
    b.debt -= float(ticks) * kSourceDelta;
    if (b.debt > 1.0f) b.debt = 0.0f;
    // Every InteractAttack stored damage on the suppressed host; the core
    // owns where it goes (weapons or, unarmed, the body). Non-Piki sources are
    // ignored (source damageCallBack requires creature->isPiki()).
    t->mStoredDamage = 0.0f;
    if (ticks <= 0) return false;
    Snapshot snap;
    buildSnapshot(t, snap);
    bool kill = false;
    for (int k = 0; k < ticks && !kill; ++k) {
        TickInput in;
        const Vector3f pos = t->getPosition();
        in.position = {pos.x, pos.y, pos.z};
        in.health = t->mHealth;
        in.groundY = mapMgr ? mapMgr->getMinY(pos.x, pos.z, true) : pos.y;
        in.candidates = snap.c.data();
        in.count = snap.c.size();
        in.hits = k == 0 ? b.pending.data() : nullptr;
        in.hitCount = k == 0 ? b.pending.size() : 0;
        if (sTrace) {
            in.element.context = sTrace;
            in.element.trace = P2BigTreasureMapTrace::trace;
            in.element.ground = P2BigTreasureMapTrace::ground;
        }
        const float before = t->mHealth;
        const TickOutput o = b.fsm.tick(in);
        if (k == 0) b.pending.clear();
        if (!o.valid) break;
        if (o.bodyDamage > 0.0f) {
            t->mStoredDamage = o.bodyDamage;
            t->makeDamaged();
            std::printf("P2_BIGTREASURE_BODY_DAMAGE generator=%u source_id=73 health=%.1f prior=%.1f hits=%d weapons=%d\n",
                        b.generator, t->mHealth, before, o.bodyHits, b.fsm.ownership().weaponCount());
        }
        applyOutput(t, b, snap, o);
        // Movement/facing are the gait's; the P1 host integrates them.
        t->setDirection(o.faceDir);
        const Vector3f drive(o.velocity.x, 0.0f, o.velocity.z);
        t->inputDrive(drive);
        t->mVelocity.x = drive.x;
        t->mVelocity.z = drive.z;
        kill = o.killRequest;
    }
    stepDropped(b, dt);
    if (t->mHealth > 0.0f) t->updateLifeGauge();
    b.logTimer += dt;
    if (b.logTimer >= 1.0f) {
        b.logTimer = 0.0f;
        const Vector3f p = t->getPosition();
        const Fsm& f = b.fsm;
        int stuck = 0;
        for (const Candidate& c : snap.c) stuck += c.stuckToSelf && c.alive ? 1 : 0;
        std::printf("P2_BIGTREASURE_POS generator=%u source_id=73 state=%s anim=%s frame=%.0f x=%.1f z=%.1f face=%.2f "
                    "target=%.1f,%.1f home=%.1f,%.1f health=%.1f weapons=%d hp=%.0f/%.0f/%.0f/%.0f stuck=%d steps=%d "
                    "ik=%d flick=%.0f limit=%.2f ignored=%d\n",
                    b.generator, stateName(f.state()), animClipName(f.animator().anim()) ? animClipName(f.animator().anim()) : "-",
                    f.animator().frame(), p.x, p.z, t->getDirection(), f.targetPosition().x, f.targetPosition().z,
                    f.home().x, f.home().z, t->mHealth, f.ownership().weaponCount(), f.ownership().weaponHealth(0),
                    f.ownership().weaponHealth(1), f.ownership().weaponHealth(2), f.ownership().weaponHealth(3), stuck,
                    f.gait().steps(), f.gait().active() ? 1 : 0, f.flickTimer(), f.attackLimitTimer(), b.ignored);
    }
    std::fflush(stdout);
    if (kill && !b.escaped) {
        // Dead KEYEVENT_END -> kill(). The host death funnel (die + dieSoon)
        // runs in the suppressed doAI, hence pcEscapeNow (Groink pattern).
        b.escaped = true;
        t->inputDrive(Vector3f(0.0f, 0.0f, 0.0f));
        t->mVelocity.x = t->mVelocity.z = 0.0f;
        std::printf("P2_BIGTREASURE_ESCAPE generator=%u source_id=73 native=host_escape_now health=%.1f\n", b.generator,
                    t->mHealth);
        std::fflush(stdout);
        t->pcEscapeNow();
        return true;
    }
    return false;
}
} // namespace

void pc_p2_bigtreasure_teki_reset() {
    const int before = int(s.size());
    s.clear();
    sDrawLogged.clear();
    for (auto& v : sPoses) v.clear();
    for (Shape*& p : sPellet) p = nullptr;
    sPosesLoaded = false;
    sReady = false;
    if (before > 0) {
        std::printf("P2_BIGTREASURE_TEKI_RESET bound_before=%d\n", before);
        std::fflush(stdout);
    }
}

void pc_p2_bigtreasure_teki_forget(BTeki* t) {
    if (!t) return;
    if (s.erase(t) > 0) {
        std::printf("P2_BIGTREASURE_TEKI_FORGET remaining=%d\n", int(s.size()));
        std::fflush(stdout);
    }
    sDrawLogged.erase(t);
}

bool pc_p2_bigtreasure_teki_is_bound(const BTeki* t) { return t && find(t) != nullptr; }

void pc_p2_bigtreasure_attack(BTeki* teki, Creature* attacker, float damage) {
    Binding* b = find(teki);
    if (!b || b->began || !attacker) return;
    const Vector3f p = attacker->getPosition();
    Hit h;
    h.attacker = {p.x, p.y, p.z};
    h.damage = damage;
    h.fromPiki = attacker->isPiki();
    if (b->pending.size() < 512) b->pending.push_back(h);
}

float pc_p2_bigtreasure_teki_param_f(const BTeki* teki, int idx, float fallback) {
    const Binding* b = find(teki);
    if (!b || b->began) return fallback;
    switch (idx) {
    case TPF_Life:
        return b->fsm.params().health;
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

float pc_p2_bigtreasure_teki_effective_health(const BTeki* teki, float fallback) {
    const Binding* b = find(teki);
    if (!b || b->began || b->deadLogged) return fallback;
    float hp = fallback;
    for (int w = 0; w < P2BTWEAPON_Count; ++w) hp += b->fsm.ownership().weaponHealth(w);
    return hp;
}

bool pc_p2_bigtreasure_teki_suppress_ai(const BTeki* teki) {
    const Binding* b = find(teki);
    return b && !b->began;
}

void pc_p2_bigtreasure_teki_setup() {
    pc_p2_bigtreasure_teki_reset();
    const bool bridge = pc_randomizer_p2_bridge();
    if (!bridge || !tekiMgr) return;
    bool any = false;
    {
        Iterator it(tekiMgr);
        CI_LOOP(it) {
            auto* t = static_cast<Teki*>(*it);
            if (t && t->mGenerator && pc_p2_campaign_source(t) == 73) any = true;
        }
    }
    if (!any) return;
    if (!loadInputs(bridge)) return;
    if (!sTrace) sTrace = new P2BigTreasureMapTrace;
    sTrace->reset(mapMgr);
    sReady = true;
    Iterator it(tekiMgr);
    CI_LOOP(it) {
        auto* t = static_cast<Teki*>(*it);
        if (!t || !t->mGenerator || pc_p2_campaign_source(t) != 73) continue;
        const unsigned gen = pc_p2_campaign_token(t);
        Binding& b = s[static_cast<BTeki*>(t)];
        b = Binding{};
        b.generator = gen;
        const Vector3f pos = t->getPosition();
        b.fsm.init(sParams, sBank, sAnimator, {pos.x, pos.y, pos.z}, t->getDirection(), (gen * 2654435761u) | 1u);
        t->mHealth = sParams.health;
        b.lastHealth = t->mHealth;
        const int corpse = t->getParameterI(TPI_CorpseType);
        std::printf("P2_BIGTREASURE_BIND generator=%u source_id=73 host_type=%d health=%.1f weapons=%d louie=1 "
                    "state=%s draw=%s corpse_type=%d x=%.1f z=%.1f\n",
                    gen, int(t->mTekiType), t->mHealth, b.fsm.ownership().weaponCount(), stateName(b.fsm.state()),
                    sPosesLoaded ? "p2_model" : "host", corpse, pos.x, pos.z);
        // Ordinary-delivery bridge (lane 06): the corpse substitute the owner
        // must approve -- the host's LeaveCorpse pellet grants onion:p2:73.
        pc_randomizer_p2_bind_source(static_cast<PelletView*>(t), 73u, gen);
        std::printf("P2_BIGTREASURE_DELIVERY_BIND generator=%u source_id=73 reward=host_corpse_substitute\n", gen);
        std::fflush(stdout);
    }
}

void pc_p2_bigtreasure_teki_tick(BTeki* t) {
    auto i = s.find(t);
    if (i == s.end()) return;
    Binding& b = i->second;
    if (b.began) return;
    const float dt = gsys ? gsys->getFrameTime() : 0.0f;
    if (t->mDeadState == 0) {
        if (!(dt > 0.0f && dt < 0.5f)) return;
        ownTick(t, b, dt); // may run the host teardown; never touch b after it
        return;
    }
    if (!b.escaped) {
        // Something outside the FSM called die(): finish the teardown here or
        // the corpse would never pelletize (dieSoon only runs in doAI).
        b.escaped = true;
        std::printf("P2_BIGTREASURE_ESCAPE generator=%u source_id=73 native=host_die_external\n", b.generator);
        std::fflush(stdout);
        t->pcEscapeNow();
        return;
    }
    if (!b.began && (t->mPellet || t->mHealth <= 0.0f)) {
        b.began = true;
        std::printf("P2_BIGTREASURE_CORPSE generator=%u source_id=73 pellet=%d x=%.1f z=%.1f\n", b.generator,
                    t->mPellet ? 1 : 0, t->mSRT.t.x, t->mSRT.t.z);
        std::fflush(stdout);
    }
}

bool pc_p2_bigtreasure_teki_draw(BTeki* t, Graphics& gfx, const Matrix4f& view, bool corpse) {
    auto i = s.find(t);
    if (i == s.end() || !sPosesLoaded || !gfx.mCamera) return false;
    Binding& b = i->second;
    const bool dead = corpse || b.began || t->mDeadState != 0;
    int anim = b.fsm.animator().anim();
    float frame = b.fsm.animator().frame();
    bool last = false;
    if (dead) {
        anim = AnimDead;
        last = true;
    }
    if (anim < 0 || anim >= AnimCount || sPoses[anim].empty()) {
        anim = AnimWait1;
        frame = 0.0f;
        if (sPoses[anim].empty()) return false;
    }
    const auto& poses = sBank.clip[anim].poses;
    std::size_t best = last ? sPoses[anim].size() - 1 : 0;
    if (!last)
        for (std::size_t k = 1; k < poses.size() && k < sPoses[anim].size(); ++k)
            if (std::fabs(float(poses[k].frame) - frame) < std::fabs(float(poses[best].frame) - frame)) best = k;
    Shape* shape = sPoses[anim][best];
    shape->updateAnim(gfx, view, nullptr, t);
    pc_gfx_specular_family_scope(1);
    shape->drawshape(gfx, *gfx.mCamera, nullptr);
    // Captured weapons ride the staged pose's otakara_* joint (model space).
    int drawn = 0;
    if (!dead) {
        for (int w = 0; w < P2BTWEAPON_Count; ++w) {
            if (!sPellet[w] || !b.fsm.ownership().isWeaponAttached(w)) continue;
            const Mat34& jm = b.fsm.jointModel(w);
            Matrix4f joint;
            for (int r = 0; r < 3; ++r)
                for (int c = 0; c < 4; ++c) joint.mMtx[r][c] = jm.m[r][c];
            joint.mMtx[3][0] = joint.mMtx[3][1] = joint.mMtx[3][2] = 0.0f;
            joint.mMtx[3][3] = 1.0f;
            Matrix4f pelletView;
            view.multiplyTo(joint, pelletView);
            sPellet[w]->updateAnim(gfx, pelletView, nullptr, nullptr);
            sPellet[w]->drawshape(gfx, *gfx.mCamera, nullptr);
            ++drawn;
        }
    }
    // Knocked-off weapons rest where they fell (visual; not carryable yet).
    for (const DroppedWeapon& d : b.dropped) {
        if (!sPellet[d.weapon]) continue;
        Matrix4f world, pelletView;
        world.makeSRT(Vector3f(1.0f, 1.0f, 1.0f), Vector3f(0.0f, 0.0f, 0.0f), d.pos);
        gfx.mCamera->mLookAtMtx.multiplyTo(world, pelletView);
        sPellet[d.weapon]->updateAnim(gfx, pelletView, nullptr, nullptr);
        sPellet[d.weapon]->drawshape(gfx, *gfx.mCamera, nullptr);
    }
    pc_gfx_specular_family_scope(0);
    int& logged = sDrawLogged[t];
    const int bit = dead ? 2 : 1;
    if (!(logged & bit)) {
        logged |= bit;
        std::printf("P2_BIGTREASURE_DRAW generator=%u source_id=73 corpse=%d clip=%s pose=%zu weapons_drawn=%d "
                    "model=p2_bigtreasure scale=%.2f\n",
                    b.generator, dead ? 1 : 0, sBank.clip[anim].name.c_str(), best, drawn, t->mSRT.s.x);
        std::fflush(stdout);
    }
    return true;
}
