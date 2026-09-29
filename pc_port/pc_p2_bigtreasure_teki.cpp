#include "pc_p2_bigtreasure_teki.h"
#include "pc_p2_bigtreasure_own.h"
#include "pc_p2_bigtreasure_map_trace.h"
#include "pc_p2_campaign_actor.h"
#include "pc_p2_setup_failsafe.h"
#include "pc_p2_navi_select.h"
#include "pc_p2_species.h"
#include "pc_p2_animation.h"
#include "pc_bbft.h"
#include "Collision.h"
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
#include <set>
#include <string>
#include <vector>

extern Matrix4f invCamMat; // collInfo.cpp: camera inverse used by CollPart::getMatrix

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

// Own collision (#246 fix stage): the retail enemycoll.txt tree as real P1
// CollParts on the Titan, replacing the Swallow host's CollInfo while bound.
struct OwnColl {
    CollInfo* host = nullptr;          // the host's CollInfo, restored on forget
    CollInfo* own = nullptr;           // never freed (see restoreHostColl)
    std::vector<ObjCollInfo*> nodes;   // sColl order
    std::vector<CollPart*> parts;      // sColl order (nullptr if not built)
    bool unarmedCodes = false;         // tam1/tam2 switched to 'st__'
};

struct Binding {
    unsigned generator = 0;
    Fsm fsm;
    OwnColl coll;
    bool anchorValid = false;  // hardConstraintOn: the host must not be pushed
    Vector3f anchor;
    bool moving = false;
    int recvStim = 0, recvAccepted = 0, recvNaviFallback = 0;
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
std::vector<CollNode> sColl;          // retail enemycoll.txt (pre-order)
bool sSetupDone = false;              // late spawns bind lazily after setup
std::set<BTeki*> sConsidered;         // actors already checked for a lazy bind
// Host CollInfo per actor that currently wears an own CollInfo. Survives
// reset() so a pooled actor always gets its host tree back on forget.
std::map<BTeki*, CollInfo*> sHostColl;

u32 fourcc(const std::string& id) {
    u32 v = 0;
    for (int i = 0; i < 4; ++i) v = (v << 8) | u32(static_cast<unsigned char>(i < int(id.size()) ? id[i] : '_'));
    return v;
}

int partOf(const Binding& b, const CollPart* part) {
    if (!part) return PartNone;
    for (std::size_t i = 0; i < b.coll.parts.size(); ++i)
        if (b.coll.parts[i] == part) {
            const int w = weaponForPartId(sColl[i].id);
            return w >= 0 ? w : PartOther;
        }
    return PartOther;
}

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
    {
        std::ifstream in("p2-bigtreasure-coll.txt");
        std::string error;
        if (!in) {
            std::printf("P2_BIGTREASURE_COLL_MISSING\n");
            return !pc_p2_setup_skip(bridge, "BigTreasure", "coll_missing");
        }
        if (!parseCollTree(in, sColl, error)) {
            std::printf("P2_BIGTREASURE_COLL_INVALID reason=%s\n", error.c_str());
            return !pc_p2_setup_skip(bridge, "BigTreasure", "coll_invalid");
        }
        std::printf("P2_BIGTREASURE_COLL nodes=%zu source=enemycoll.txt\n", sColl.size());
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
void buildSnapshot(BTeki* self, const Binding& b, Snapshot& snap) {
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
            cand.stuckPart = stick == self ? partOf(b, p->getStickPart()) : PartNone;
            cand.blue = p->mColor == Blue;
            cand.buried = p->isBuried() || p->isStickToMouth();
            add(p, cand);
        }
    }
}
Creature* creatureFor(const Snapshot& snap, std::uint64_t id) {
    return id >= 1 && id <= snap.who.size() ? snap.who[std::size_t(id - 1)] : nullptr;
}

// Diagnostic census of the field Pikmin around the Titan (mode/state/stick),
// logged at Dead entry and when the corpse forms.
void pikiCensus(const Binding& b, BTeki* t, const char* when) {
    if (!pikiMgr) return;
    int alive = 0, stuck = 0, near = 0, formation = 0, free = 0, transport = 0, other = 0;
    std::map<int, int> states;
    const Vector3f me = t->getPosition();
    Iterator it(pikiMgr);
    CI_LOOP(it) {
        Piki* p = static_cast<Piki*>(*it);
        if (!p || !p->isAlive()) continue;
        ++alive;
        if (p->getStickObject() == t) ++stuck;
        const Vector3f q = p->getPosition();
        const float dx = q.x - me.x, dz = q.z - me.z;
        if (dx * dx + dz * dz < 300.0f * 300.0f) ++near;
        if (p->mMode == PikiMode::FormationMode) ++formation;
        else if (p->mMode == PikiMode::FreeMode) ++free;
        else if (p->mMode == PikiMode::TransportMode) ++transport;
        else ++other;
        ++states[p->getState()];
    }
    std::printf("P2_BIGTREASURE_PIKI_CENSUS generator=%u source_id=73 when=%s alive=%d stuck=%d near=%d formation=%d "
                "free=%d transport=%d other=%d states=",
                b.generator, when, alive, stuck, near, formation, free, transport, other);
    for (const auto& kv : states) std::printf("%d:%d,", kv.first, kv.second);
    std::printf("\n");
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
    if (!accepted && eh.navi) {
        // Every element (gas included): flick with *_NAVI_FLICK_CHANCE, else a
        // 0-damage attack (BigTreasureAttack.cpp fire/gas/bubble/elec loops).
        if (eh.naviFlick) c->stimulate(InteractFlick(t, 0.0f, 0.0f, FLICK_BACKWARDS_ANGLE));
        else c->stimulate(InteractAttack(t, nullptr, 0.0f, false));
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
            pikiCensus(b, t, "dead_enter");
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
    if (o.attackFinished) {
        std::printf("P2_BIGTREASURE_ATTACK_FINISH generator=%u source_id=73 stimulated=%d accepted=%d navi_fallback=%d\n",
                    b.generator, b.recvStim, b.recvAccepted, b.recvNaviFallback);
        b.recvStim = b.recvAccepted = b.recvNaviFallback = 0;
    }
    for (const ElementHit& eh : o.elementHits) {
        Creature* c = creatureFor(snap, eh.id);
        if (!c || !c->isAlive()) continue;
        const bool accepted = stimulateElement(t, c, eh);
        ++b.recvStim;
        if (accepted) ++b.recvAccepted;
        if (!accepted && eh.navi) ++b.recvNaviFallback;
        // Source re-stimulates every update; log the first contact and the
        // first acceptance per target per attack.
        int& seen = b.recvCount[c];
        const int bit = accepted ? 2 : 1;
        if (seen & bit) continue;
        seen |= bit;
        std::printf("P2_BIGTREASURE_RECV generator=%u source_id=73 weapon=%s target=%s color=%d accepted=%d%s\n",
                    b.generator, p2_bigtreasure_receiver_stimulus_name(eh.hit.stimulus), eh.navi ? "navi" : "piki",
                    eh.navi ? -1 : int(static_cast<Piki*>(c)->mColor), accepted ? 1 : 0,
                    !accepted && eh.navi ? (eh.naviFlick ? " fallback=flick" : " fallback=attack0") : "");
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

// setupBigTreasureCollision (BigTreasure.cpp:671-700): a dropped weapon's
// part turns '_t__' with radius 0 (radius applied in updateColl); tam1/tam2
// are '_t__' while any weapon is captured and 'st__' once none is.
void setupCollisionCodes(Binding& b) {
    if (!b.coll.own) return;
    const bool unarmed = b.fsm.ownership().weaponCount() == 0;
    for (std::size_t i = 0; i < b.coll.nodes.size(); ++i) {
        ObjCollInfo* n = b.coll.nodes[i];
        const std::string& id = sColl[i].id;
        const int w = weaponForPartId(id);
        if (w >= 0 && !b.fsm.ownership().isWeaponAttached(w)) n->mCode.setID('_t__');
        if (id == "tam1" || id == "tam2") n->mCode.setID(unarmed ? 'st__' : '_t__');
    }
    if (unarmed && !b.coll.unarmedCodes) {
        b.coll.unarmedCodes = true;
        std::printf("P2_BIGTREASURE_COLL_UNARMED generator=%u source_id=73 tam=st__\n", b.generator);
    }
}

void updateColl(Binding& b) {
    if (!b.coll.own) return;
    for (std::size_t i = 0; i < b.coll.parts.size(); ++i) {
        CollPart* part = b.coll.parts[i];
        if (!part) continue;
        const CollNode& n = sColl[i];
        const Vec3 c = b.fsm.collCentre(n);
        part->mCentre.set(c.x, c.y, c.z);
        const int w = weaponForPartId(n.id);
        part->mRadius = w >= 0 && !b.fsm.ownership().isWeaponAttached(w) ? 0.0f : n.radius;
    }
}

bool buildColl(BTeki* t, Binding& b) {
    if (sColl.empty() || !t->mCollInfo) return false;
    OwnColl& oc = b.coll;
    oc.nodes.clear();
    oc.parts.clear();
    for (const CollNode& n : sColl) {
        auto* node = new ObjCollInfo();
        node->mId.setID(fourcc(n.id));
        node->mCode.setID(fourcc(n.code));
        node->mRadius = n.radius;
        node->mCentrePosition.set(n.offset.x, n.offset.y, n.offset.z);
        node->mJointIndex = n.joint;
        oc.nodes.push_back(node);
    }
    for (std::size_t i = 0; i < sColl.size(); ++i)
        if (sColl[i].parent >= 0) oc.nodes[std::size_t(sColl[i].parent)]->add(oc.nodes[i]);
    const int capacity = int(sColl.size()) + 1 > 32 ? int(sColl.size()) + 1 : 32; // >= any host tree (22)
    oc.own = new CollInfo(capacity);
    oc.own->initInfoTree(oc.nodes[0]);
    int tubes = 0;
    for (std::size_t i = 0; i < sColl.size(); ++i) {
        CollPart* part = oc.own->getSphere(fourcc(sColl[i].id));
        if (part) {
            part->mIsUpdateActive = false; // no parent shape: updateColl owns centre/radius
            part->mJointMatrix = Matrix4f::ident;
        }
        oc.parts.push_back(part);
    }
    // setupCollision: makeTubeTree on the four leg roots (child chains).
    for (const char* leg : {"lft1", "lht1", "rft1", "rht1"}) {
        int depth = 0;
        for (CollPart* p = oc.own->getSphere(fourcc(leg)); p && p->getChild(); p = p->getChild()) ++depth;
        if (depth > 0) {
            oc.own->makeTubesChild(fourcc(leg), depth);
            ++tubes;
        }
    }
    oc.host = t->mCollInfo;
    t->mCollInfo = oc.own;
    // onInit disableEvent(EB_PlatformCollEnabled): the Titan has no platform
    // collision. The Swallow host's back platforms would otherwise report
    // contacts whose part our tree cannot resolve (null CollEvent part).
    t->mPlatMgr.release();
    sHostColl[t] = oc.host;
    setupCollisionCodes(b);
    updateColl(b);
    std::printf("P2_BIGTREASURE_COLL_BIND generator=%u source_id=73 parts=%zu tubes=%d host_parts_replaced=1\n",
                b.generator, oc.parts.size(), tubes);
    return true;
}

void restoreHostColl(BTeki* t) {
    // The own CollInfo is intentionally never freed: stuck Pikmin may still
    // hold CollPart pointers into it, and a pooled actor re-inits whatever
    // mCollInfo it holds.
    auto i = sHostColl.find(t);
    if (i == sHostColl.end()) return;
    if (i->second) t->mCollInfo = i->second;
    sHostColl.erase(i);
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
    // hardConstraintOn: the source Titan is never pushed. Weight 0 already
    // stops P1 collision impulses (getiMass); a standing Titan is also held
    // on its anchor against anything else that moves the host.
    if (!b.moving) {
        if (!b.anchorValid) {
            b.anchor = t->mSRT.t;
            b.anchorValid = true;
        } else {
            t->mSRT.t.x = b.anchor.x;
            t->mSRT.t.z = b.anchor.z;
            t->mVolatileVelocity.x = t->mVolatileVelocity.z = 0.0f;
        }
    } else {
        b.anchorValid = false;
    }
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
    buildSnapshot(t, b, snap);
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
        b.moving = drive.x != 0.0f || drive.z != 0.0f;
        t->inputDrive(drive);
        t->mVelocity.x = drive.x;
        t->mVelocity.z = drive.z;
        kill = o.killRequest;
        if (!o.drops.empty()) setupCollisionCodes(b);
    }
    updateColl(b);
    stepDropped(b, dt);
    if (t->mHealth > 0.0f) t->updateLifeGauge();
    b.logTimer += dt;
    if (b.logTimer >= 1.0f) {
        b.logTimer = 0.0f;
        const Vector3f p = t->getPosition();
        const Fsm& f = b.fsm;
        int stuck = 0, onWeapon = 0, onBody = 0;
        for (const Candidate& c : snap.c) {
            if (!(c.stuckToSelf && c.alive)) continue;
            ++stuck;
            if (c.stuckPart >= 0) ++onWeapon;
            else if (c.stuckPart == PartOther) ++onBody;
        }
        std::printf("P2_BIGTREASURE_POS generator=%u source_id=73 state=%s anim=%s frame=%.0f x=%.1f z=%.1f face=%.2f "
                    "target=%.1f,%.1f home=%.1f,%.1f health=%.1f weapons=%d hp=%.0f/%.0f/%.0f/%.0f stuck=%d steps=%d "
                    "stuck_weapon=%d stuck_body=%d ik=%d flick=%.0f limit=%.2f ignored=%d\n",
                    b.generator, stateName(f.state()), animClipName(f.animator().anim()) ? animClipName(f.animator().anim()) : "-",
                    f.animator().frame(), p.x, p.z, t->getDirection(), f.targetPosition().x, f.targetPosition().z,
                    f.home().x, f.home().z, t->mHealth, f.ownership().weaponCount(), f.ownership().weaponHealth(0),
                    f.ownership().weaponHealth(1), f.ownership().weaponHealth(2), f.ownership().weaponHealth(3), stuck,
                    f.gait().steps(), onWeapon, onBody, f.gait().active() ? 1 : 0, f.flickTimer(), f.attackLimitTimer(), b.ignored);
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
    sConsidered.clear();
    sSetupDone = false;
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
    restoreHostColl(t);
    sConsidered.erase(t);
    if (s.erase(t) > 0) {
        std::printf("P2_BIGTREASURE_TEKI_FORGET remaining=%d\n", int(s.size()));
        std::fflush(stdout);
    }
    sDrawLogged.erase(t);
}

bool pc_p2_bigtreasure_teki_is_bound(const BTeki* t) { return t && find(t) != nullptr; }

void pc_p2_bigtreasure_attack(BTeki* teki, Creature* attacker, float damage, CollPart* part) {
    Binding* b = find(teki);
    if (!b || b->began || !attacker) return;
    Hit h;
    h.part = partOf(*b, part);
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
    case TPF_Weight:
        return 0.0f; // hardConstraintOn: getiMass() == 0, never pushed
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

namespace {
bool bindActor(BTeki* t, const char* when) {
    const unsigned gen = pc_p2_campaign_token(t);
    Binding& b = s[t];
    b = Binding{};
    b.generator = gen;
    const Vector3f pos = t->getPosition();
    b.fsm.init(sParams, sBank, sAnimator, {pos.x, pos.y, pos.z}, t->getDirection(), (gen * 2654435761u) | 1u);
    t->mHealth = sParams.health;
    b.lastHealth = t->mHealth;
    if (!buildColl(t, b)) {
        std::printf("P2_BIGTREASURE_COLL_BIND_FAILED generator=%u source_id=73\n", gen);
        s.erase(t);
        return false;
    }
    const int corpse = t->getParameterI(TPI_CorpseType);
    std::printf("P2_BIGTREASURE_BIND generator=%u source_id=73 host_type=%d health=%.1f weapons=%d louie=1 "
                "state=%s draw=%s corpse_type=%d x=%.1f z=%.1f when=%s\n",
                gen, int(t->mTekiType), t->mHealth, b.fsm.ownership().weaponCount(), stateName(b.fsm.state()),
                sPosesLoaded ? "p2_model" : "host", corpse, pos.x, pos.z, when);
    // Ordinary-delivery bridge (lane 06): the corpse substitute the owner
    // must approve -- the host's LeaveCorpse pellet grants onion:p2:73.
    pc_randomizer_p2_bind_source(static_cast<PelletView*>(t), 73u, gen);
    std::printf("P2_BIGTREASURE_DELIVERY_BIND generator=%u source_id=73 reward=host_corpse_substitute\n", gen);
    std::fflush(stdout);
    return true;
}

bool prepare(bool bridge) {
    if (sReady) return true;
    if (!loadInputs(bridge)) return false;
    if (!sTrace) sTrace = new P2BigTreasureMapTrace;
    sTrace->reset(mapMgr);
    sReady = true;
    return true;
}
} // namespace

void pc_p2_bigtreasure_teki_setup() {
    pc_p2_bigtreasure_teki_reset();
    const bool bridge = pc_randomizer_p2_bridge();
    if (!bridge || !tekiMgr) return;
    sSetupDone = true;
    bool any = false;
    {
        Iterator it(tekiMgr);
        CI_LOOP(it) {
            auto* t = static_cast<Teki*>(*it);
            if (t && t->mGenerator && pc_p2_campaign_source(t) == 73) any = true;
        }
    }
    if (!any) return;
    if (!prepare(bridge)) return;
    Iterator it(tekiMgr);
    CI_LOOP(it) {
        auto* t = static_cast<Teki*>(*it);
        if (!t || !t->mGenerator || pc_p2_campaign_source(t) != 73) continue;
        sConsidered.insert(t);
        bindActor(t, "setup");
    }
}

void pc_p2_bigtreasure_teki_tick(BTeki* t) {
    auto i = s.find(t);
    if (i == s.end()) {
        // Late spawn (respawn / later generator): bind on its first live tick
        // after setup. Each actor lifetime is considered once.
        if (!sSetupDone || !t || t->mTekiType != TEKI_Swallow || t->mDeadState != 0 || !t->isAlive()) return;
        if (!sConsidered.insert(t).second) return;
        if (!t->mGenerator || pc_p2_campaign_source(t) != 73) return;
        if (!prepare(pc_randomizer_p2_bridge())) return;
        bindActor(t, "late_spawn");
        return;
    }
    Binding& b = i->second;
    const float dt = gsys ? gsys->getFrameTime() : 0.0f;
    if (b.began) {
        // Corpse phase: periodic Pikmin census for the carry diagnosis.
        b.logTimer += dt;
        if (b.logTimer >= 5.0f) {
            b.logTimer = 0.0f;
            pikiCensus(b, t, "corpse_phase");
            std::fflush(stdout);
        }
        return;
    }
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
        pikiCensus(b, t, "corpse");
        std::fflush(stdout);
    }
}

bool pc_p2_bigtreasure_teki_draw(BTeki* t, Graphics& gfx, const Matrix4f& view, bool corpse) {
    auto i = s.find(t);
    if (i == s.end() || !sPosesLoaded || !gfx.mCamera) return false;
    Binding& b = i->second;
    if (b.coll.own && gfx.mCamera) {
        // CollPart::getMatrix() = invCamMat * mJointMatrix with the centre as
        // translation: give our parts the Titan yaw in the same camera frame.
        invCamMat = gfx.mCamera->mInverseLookAtMtx;
        Matrix4f yaw, camYaw;
        yaw.makeSRT(Vector3f(1.0f, 1.0f, 1.0f), Vector3f(0.0f, t->getDirection(), 0.0f), Vector3f(0.0f, 0.0f, 0.0f));
        gfx.mCamera->mLookAtMtx.multiplyTo(yaw, camYaw);
        for (CollPart* part : b.coll.parts)
            if (part) part->mJointMatrix = camYaw;
    }
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
