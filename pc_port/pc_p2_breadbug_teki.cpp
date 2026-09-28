// Campaign OWN driver for the Breadbug (PanModoki 38, #898). See the header.
#include "pc_p2_breadbug_teki.h"
#include "pc_p2_breadbug_fsm.h"
#include "pc_p2_campaign_actor.h"
#include "pc_p2_groink_clock.h"
#include "pc_p2_animation.h"
#include "pc_p2_purple.h"
#include "pc_randomizer.h"
#include "Interactions.h"
#include "Pellet.h"
#include "PelletState.h"
#include "Piki.h"
#include "PikiMgr.h"
#include "PikiState.h"
#include "Route.h"
#include "Shape.h"
#include "Stickers.h"
#include "Texture.h"
#include "Graphics.h"
#include "gameflow.h"
#include "gl/pc_gfx.h"
#include "system.h"
#include "teki.h"
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace {
namespace bb = p2breadbugfsm;
constexpr unsigned kSource = 38;

struct Binding {
    unsigned generator = 0;
    bb::Fsm fsm;
    P2GroinkSourceClock clock;
    bool escaped = false;       // host death funnel ran (pcEscapeNow)
    bool corpseLogged = false;
    bool deadLogged = false;
    int pendingPresses = 0;
    bool pendingBounce = false;
    bool heldInGoal = false;    // last update: the held cargo was being sucked
    Pellet* held = nullptr;     // last update's stick object (for suck-finish)
    std::set<Piki*> pressedFlight;  // one press per thrown Pikmin flight
    int taiState = -1;          // P1 Collec mStateID at bind (must never change)
    int taiChanges = 0;
    int attacksIgnored = 0;
    int eventsConsumed = 0;
    int presses = 0, pressesRejected = 0;
    int flyContactsRising = 0;  // thrown Pikmin that touched it while still rising (no press)
    bool hidden = false;
    float logTimer = 0.0f;
    float corpseTimer = 0.0f;
    float lastContestPiki = -1.0f;
    bool lastCanBack = true;
};
std::map<BTeki*, Binding> s;

bb::Params sParams;
bb::Bank sBank = bb::defaultBank();
std::vector<Shape*> sPoses[bb::AnimCount];
bool sPosesLoaded = false;
std::map<BTeki*, int> sDrawLogged;

// Wall-clock milliseconds, so frame dumps (file mtimes) can be matched to markers.
long long wallMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

Binding* find(const BTeki* t) {
    auto i = s.find(const_cast<BTeki*>(t));
    return i == s.end() ? nullptr : &i->second;
}

// P1 carry-route graph ('test' handle) as the source WayPoint graph; the
// synchronous path is a breadth-first search over the same links.
struct P1Route : bb::Route {
    int nearest(const bb::Vec3& p) const override {
        if (!routeMgr || routeMgr->getNumWayPoints('test') <= 0) return -1;
        WayPoint* wp = routeMgr->findNearestWayPoint('test', Vector3f(p.x, p.y, p.z), false);
        return wp ? wp->mIndex : -1;
    }
    bool get(int index, bb::WayPointInfo& out) const override {
        if (!routeMgr || index < 0 || index >= routeMgr->getNumWayPoints('test')) return false;
        WayPoint* wp = routeMgr->getWayPoint('test', index);
        if (!wp) return false;
        out = bb::WayPointInfo{};
        out.index = wp->mIndex;
        out.pos = {wp->mPosition.x, wp->mPosition.y, wp->mPosition.z};
        out.open = wp->mIsOpen;
        for (int l = 0; l < wp->mLinkCount && l < 8; ++l)
            if (wp->mLinkIndices[l] >= 0) out.links[out.linkCount++] = wp->mLinkIndices[l];
        return true;
    }
    bool path(int from, int to, std::vector<int>& out) const override {
        out.clear();
        if (!routeMgr) return false;
        const int n = routeMgr->getNumWayPoints('test');
        if (from < 0 || to < 0 || from >= n || to >= n) return false;
        std::vector<int> prev(std::size_t(n), -2);
        std::vector<int> queue{from};
        prev[std::size_t(from)] = -1;
        for (std::size_t q = 0; q < queue.size(); ++q) {
            const int cur = queue[q];
            if (cur == to) break;
            WayPoint* wp = routeMgr->getWayPoint('test', cur);
            if (!wp) continue;
            for (int l = 0; l < wp->mLinkCount && l < 8; ++l) {
                const int next = wp->mLinkIndices[l];
                if (next < 0 || next >= n || prev[std::size_t(next)] != -2) continue;
                WayPoint* nw = routeMgr->getWayPoint('test', next);
                if (!nw || (!nw->mIsOpen && next != to)) continue;  // PATHFLAG_RequireOpen
                prev[std::size_t(next)] = cur;
                queue.push_back(next);
            }
        }
        if (prev[std::size_t(to)] == -2) return false;
        for (int at = to; at != -1; at = prev[std::size_t(at)]) out.insert(out.begin(), at);
        return true;
    }
};
P1Route sRoute;

void loadParams() {
    sParams = bb::Params{};
    std::ifstream in("p2-breadbug-parms.txt");
    std::string error;
    if (in && !bb::parseEnemyParm(in, sParams, error)) {
        std::printf("P2_BREADBUG_PARMS_INVALID reason=%s fallback=source_defaults\n", error.c_str());
        sParams = bb::Params{};
    }
    std::printf("P2_BREADBUG_PARMS source_id=38 retail=%d health=%.1f move=%.1f search=%.1f angle=%.1f "
                "press=%.1f suck=%.1f carry=%.1f hide=%.1f wait=%.1f weight=%d walk_anim=%.2f\n",
                sParams.retail ? 1 : 0, sParams.health, sParams.moveSpeed, sParams.searchDistance,
                sParams.searchAngle, sParams.pressDamage, sParams.suckDamage, sParams.carrySpeed,
                sParams.hideTime, sParams.waitTime, sParams.maxCarryWeight, sParams.walkAnimSpeed);
}

// Pose bank with shared materials (Groink/tank pattern), bounded bytes.
Shape* loadShape(const std::string& rel, Shape*& shared, std::size_t& total) {
    const std::string path = "assets/dataDir/courses/pikmin2room/" + rel;
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) return nullptr;
    const auto size = file.tellg();
    if (size <= 0 || size > 1024 * 1024 || total + std::size_t(size) > 24u * 1024 * 1024) return nullptr;
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
    } else {
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
    }
    return shape;
}

void loadBank() {
    sBank = bb::defaultBank();
    for (auto& v : sPoses) v.clear();
    sPosesLoaded = false;
    std::ifstream in("p2-breadbug-bank.txt");
    std::string error;
    if (in && !bb::parseBank(in, sBank, error)) {
        std::printf("P2_BREADBUG_BANK_INVALID reason=%s fallback=builtin_timing draw=host\n", error.c_str());
        sBank = bb::defaultBank();
    }
    int staged = 0;
    for (const auto& c : sBank.clip) staged += c.staged ? 1 : 0;
    Shape* shared = nullptr;
    std::size_t total = 0, poses = 0;
    bool ok = staged > 0;
    for (int a = 0; ok && a < bb::AnimCount; ++a) {
        const auto& clip = sBank.clip[a];
        if (!clip.staged) continue;
        for (std::size_t i = 0; i < clip.poses.size(); ++i) {
            char rel[160];
            std::snprintf(rel, sizeof(rel), "breadbug_%s_%02u.mod", clip.name.c_str(), unsigned(i));
            Shape* shape = loadShape(rel, shared, total);
            if (!shape) { ok = false; break; }
            sPoses[a].push_back(shape);
            ++poses;
        }
    }
    if (!ok) for (auto& v : sPoses) v.clear();
    sPosesLoaded = ok && poses > 0;
    std::printf("P2_BREADBUG_BANK staged_clips=%d poses=%zu bytes=%zu draw=%s\n", staged,
                sPosesLoaded ? poses : std::size_t(0), total, sPosesLoaded ? "p2_model" : "host");
}

std::uint64_t idOf(const Pellet* p) { return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(p)); }

float pikiStrength(Pellet* p) {
    float sum = 0.0f;
    Stickers stuck(p);
    Iterator it(&stuck);
    CI_LOOP(it) {
        Creature* c = *it;
        if (c && c->isPiki()) sum += float(pc_piki_carry_strength(static_cast<Piki*>(c)));
    }
    return sum;
}

bool otherTekiStuck(Pellet* p, const BTeki* self) {
    Stickers stuck(p);
    Iterator it(&stuck);
    CI_LOOP(it) {
        Creature* c = *it;
        if (c && c->isTeki() && c != self) return true;
    }
    return false;
}

int sPelletsAlive = 0;
float sPelletNearest = -1.0f;
void snapshot(BTeki* t, std::vector<bb::PelletInfo>& out, std::vector<Pellet*>& who) {
    out.clear();
    who.clear();
    sPelletsAlive = 0;
    sPelletNearest = -1.0f;
    if (!pelletMgr) return;
    const Vector3f me = t->getPosition();
    Iterator it(pelletMgr);
    CI_LOOP(it) {
        Pellet* p = static_cast<Pellet*>(*it);
        if (!p || !p->isAlive() || !p->mConfig) continue;
        const Vector3f& pos = p->mSRT.t;
        const float dx = pos.x - me.x, dz = pos.z - me.z;
        ++sPelletsAlive;
        if (sPelletNearest < 0.0f || std::sqrt(dx * dx + dz * dz) < sPelletNearest) sPelletNearest = std::sqrt(dx * dx + dz * dz);
        if (dx * dx + dz * dz > 1000.0f * 1000.0f && t->getStickObject() != p) continue;
        bb::PelletInfo info;
        info.id = idOf(p);
        info.pos = {pos.x, pos.y, pos.z};
        // findNearestPellet compares the pellet's base with the Breadbug's
        // feet (retail: centre - 0.5 * cylinder height). A P1 pellet's origin
        // already is its base (Pellet::update puts the life gauge at
        // mSRT.t.y + height + 5), so the base is mSRT.t.y itself.
        info.bottomY = pos.y;
        info.radius = p->getBottomRadius();
        info.carryMin = p->mConfig->mCarryMinPikis();
        info.carryMax = p->mConfig->mCarryMaxPikis();
        if (info.carryMax < info.carryMin) info.carryMax = info.carryMin;
        info.pikiStrength = pikiStrength(p);
        info.alive = p->isAlive();
        info.inGoal = p->getState() == PELSTATE_Goal;
        // panmodokiCarryable + P1 exclusions: UFO parts, captains, pellets in
        // the goal / swallowed, pellets held in a mouth.
        info.captured = p->mStuckMouthPart != nullptr;
        info.pickable = !p->isUfoParts() && !p->mConfig->mModelId.match('NAVI') && !info.inGoal
                     && p->getState() != PELSTATE_Swallowed && p->getState() != PELSTATE_Dead;
        info.otherTekiStuck = otherTekiStuck(p, t);
        info.carcass = p->mPelletView != nullptr;
        info.slotFree = true;
        info.velocity = {p->mVelocity.x, p->mVelocity.y, p->mVelocity.z};
        out.push_back(info);
        who.push_back(p);
    }
}

Pellet* pelletFor(const std::vector<bb::PelletInfo>& infos, const std::vector<Pellet*>& who, std::uint64_t id) {
    for (std::size_t i = 0; i < infos.size(); ++i) if (infos[i].id == id) return who[i];
    return nullptr;
}

void logState(const Binding& b, bb::State from, bb::State to, BTeki* t) {
    const Vector3f p = t->getPosition();
    std::printf("P2_BREADBUG_OWN_STATE generator=%u source_id=38 from=%s to=%s x=%.1f z=%.1f health=%.1f wall=%lld\n",
                b.generator, bb::stateName(from), bb::stateName(to), p.x, p.z, b.fsm.health(), wallMs());
}

void setHidden(BTeki* t, Binding& b, bool hidden) {
    if (hidden == b.hidden) return;
    b.hidden = hidden;
    if (hidden) t->clearTekiOption(TEKIOPT_Atari | TEKIOPT_ShadowVisible);
    else t->setTekiOption(TEKIOPT_Atari | TEKIOPT_ShadowVisible);
}

// Kill the Pikmin still stuck to a consumed cargo (endCarry InteractKill),
// then the cargo itself (P1 Collec putting recipe: InteractKill).
void consumeCargo(BTeki* t, Binding& b, Pellet* p) {
    int killed = 0;
    {
        Stickers stuck(p);
        Iterator it(&stuck);
        CI_LOOP(it) {
            Creature* c = *it;
            if (c && c->isPiki() && c->isAlive()) {
                c->stimulate(InteractKill(t, 0));
                ++killed;
                it.dec();
            }
        }
    }
    if (t->getStickObject() == p) p->endStickTeki(t);
    const bool carcass = p->mPelletView != nullptr;
    p->stimulate(InteractKill(t, 0));
    std::printf("P2_BREADBUG_OWN_CONSUME generator=%u source_id=38 cargo_carcass=%d pikmin_killed=%d\n",
                b.generator, carcass ? 1 : 0, killed);
}

bool ownTick(BTeki* t, Binding& b, float dt) {
    // doAI tail the suppressed strategy no longer runs: gravity, life gauge.
    if (t->getTekiOption(TEKIOPT_Gravitatable)) t->gravitate(t->getGravity());
    // The P1 Collec TAI must stay frozen (negative evidence).
    if (t->mStateID != b.taiState) {
        ++b.taiChanges;
        std::printf("P2_BREADBUG_OWN_TAI_TRANSITION generator=%u source_id=38 from=%d to=%d\n",
                    b.generator, b.taiState, int(t->mStateID));
        b.taiState = t->mStateID;
    }
    // Stored damage reaches mStoredDamage only through InteractBomb (ordinary
    // attacks are refused at InteractAttack::actTeki): EnemyBase::bombCallBack.
    const float external = t->mStoredDamage > 0.0f ? t->mStoredDamage : 0.0f;
    t->mStoredDamage = 0.0f;
    // Prune the per-flight press memory once a Pikmin is no longer flying.
    for (auto it = b.pressedFlight.begin(); it != b.pressedFlight.end();) {
        if (!*it || !(*it)->isAlive() || (*it)->getState() != PIKISTATE_Flying) it = b.pressedFlight.erase(it);
        else ++it;
    }
    const int ticks = b.clock.step(double(dt), true);
    if (ticks <= 0) {
        if (external > 0.0f) t->mStoredDamage += external;  // keep for the next source update
        return false;
    }
    std::vector<bb::PelletInfo> infos;
    std::vector<Pellet*> who;
    bool kill = false;
    for (int k = 0; k < ticks && !kill; ++k) {
        snapshot(t, infos, who);
        Creature* stick = t->getStickObject();
        Pellet* held = stick && stick->isObjType(OBJTYPE_Pellet) ? static_cast<Pellet*>(stick) : nullptr;
        bb::TickInput in;
        const Vector3f pos = t->getPosition();
        in.position = {pos.x, pos.y, pos.z};
        in.externalDamage = k == 0 ? external : 0.0f;
        in.presses = k == 0 ? b.pendingPresses : 0;
        in.bounced = k == 0 && b.pendingBounce;
        // InteractSuckFinish: the cargo we held while it was in the Onion goal
        // is gone (PelletGoalState::exec kills it and frees its stickers).
        in.suckFinished = b.held && b.heldInGoal && held != b.held;
        in.held = held ? idOf(held) : 0;
        in.pellets = infos.data();
        in.count = infos.size();
        in.route = &sRoute;
        const bb::State before = b.fsm.state();
        const float hpBefore = b.fsm.health();
        const bb::TickOutput o = b.fsm.tick(in);
        if (!o.valid) break;
        if (in.suckFinished)
            std::printf("P2_BREADBUG_OWN_SUCKED generator=%u source_id=38 health=%.1f\n", b.generator, hpBefore);
        if (k == 0) {
            b.presses += in.presses;
            b.pendingPresses = 0;
            b.pendingBounce = false;
        }
        bb::State shown = before;
        for (bb::State e : o.entered) {
            logState(b, shown, e, t);
            shown = e;
            if (e == bb::State::Pulled)
                std::printf("P2_BREADBUG_OWN_PULLED generator=%u source_id=38 carriers=%.1f self=%.1f\n",
                            b.generator, o.contestPiki, o.contestSelf);
            if (e == bb::State::Dead && !b.deadLogged) {
                b.deadLogged = true;
                std::printf("P2_BREADBUG_OWN_DEAD generator=%u source_id=38 health=%.1f wall=%lld\n", b.generator, b.fsm.health(), wallMs());
            }
        }
        if (o.damageKind != bb::DamageKind::None) {
            const char* kind = o.damageKind == bb::DamageKind::Press ? "P2_BREADBUG_OWN_PRESS"
                             : o.damageKind == bb::DamageKind::Suck ? "P2_BREADBUG_OWN_SUCK_DAMAGE"
                                                                     : "P2_BREADBUG_OWN_EXTERNAL_DAMAGE";
            std::printf("%s generator=%u source_id=38 hp_before=%.1f hp_after=%.1f state_before=%s wall=%lld\n", kind,
                        b.generator, o.hpBefore, o.hpAfter, bb::stateName(before), wallMs());
        }
        if (o.pressRejected) {
            ++b.pressesRejected;
            std::printf("P2_BREADBUG_OWN_PRESS_IGNORED generator=%u source_id=38 state=%s\n", b.generator,
                        bb::stateName(b.fsm.state()));
        }
        if (o.refilled)
            std::printf("P2_BREADBUG_OWN_HIDE_REFILL generator=%u source_id=38 health=%.1f\n", b.generator, b.fsm.health());
        // Commands.
        if (o.release && held) {
            if (o.releaseReverse) { held->mVelocity.x = -held->mVelocity.x; held->mVelocity.z = -held->mVelocity.z; }
            held->endStickTeki(t);
            std::printf("P2_BREADBUG_OWN_RELEASE generator=%u source_id=38 reverse=%d\n", b.generator, o.releaseReverse ? 1 : 0);
            held = nullptr;
        }
        if (o.stickTo) {
            Pellet* p = pelletFor(infos, who, o.stickTo);
            const bool ok = p && p->startStickTeki(t, 1.0f + t->getTekiCollisionSize());
            std::printf("P2_BREADBUG_OWN_STICK generator=%u source_id=38 ok=%d carcass=%d carry_min=%d carry_max=%d "
                        "strength=%.1f carriers=%.1f\n",
                        b.generator, ok ? 1 : 0, p && p->mPelletView ? 1 : 0, p ? int(p->mConfig->mCarryMinPikis()) : 0,
                        p ? int(p->mConfig->mCarryMaxPikis()) : 0, b.fsm.carryStrength(), p ? pikiStrength(p) : 0.0f);
            if (ok) held = p;
        }
        if (held && o.stopCargo) { held->mVelocity.x = 0.0f; held->mVelocity.z = 0.0f; }
        if (held && o.contest) {
            // P1 Pellet::doCarry keeps the carrier whose count is strictly
            // higher; the source PelletCarry already decided the tug, so the
            // winning Breadbug presents one more than the current crew.
            const int crew = int(std::ceil(o.contestPiki));
            if (o.pulled) {
                held->doCarry(t, Vector3f(o.pullVelocity.x, 0.0f, o.pullVelocity.z), u16(crew + 1));
            }
            if (o.contestPiki != b.lastContestPiki || o.canBack != b.lastCanBack) {
                b.lastContestPiki = o.contestPiki;
                b.lastCanBack = o.canBack;
                std::printf("P2_BREADBUG_OWN_CONTEST generator=%u source_id=38 carriers=%.1f self=%.1f breadbug_wins=%d state=%s\n",
                            b.generator, o.contestPiki, o.contestSelf, o.canBack ? 1 : 0, bb::stateName(b.fsm.state()));
            }
        }
        if (held && o.holdCargo) {
            // CarryEnd/Hide: the cargo is in the mouth (updateCaptureMatrix);
            // the home nudge moves the cargo and the Breadbug riding it.
            held->doCarry(t, Vector3f(0.0f, 0.0f, 0.0f), u16(int(std::ceil(pikiStrength(held))) + 1));
            held->mVelocity.x = held->mVelocity.z = 0.0f;
            held->mSRT.t.x += o.homeNudge.x;
            held->mSRT.t.z += o.homeNudge.z;
        } else if (!held && (o.homeNudge.x != 0.0f || o.homeNudge.z != 0.0f)) {
            Vector3f p = t->getPosition();
            p.x += o.homeNudge.x;
            p.z += o.homeNudge.z;
            t->mSRT.t = p;
        }
        if (o.consumeCargo && held) {
            consumeCargo(t, b, held);
            held = nullptr;
        }
        b.heldInGoal = held && held->getState() == PELSTATE_Goal;
        b.held = held;
        setHidden(t, b, o.hidden);
        // Movement/facing (free Breadbug only; a stuck one rides its cargo).
        if (!held) {
            t->setDirection(o.faceDir);
            const Vector3f drive(o.velocity.x, 0.0f, o.velocity.z);
            t->inputDrive(drive);
            t->mVelocity.x = drive.x;
            t->mVelocity.z = drive.z;
        } else {
            t->inputDrive(Vector3f(0.0f, 0.0f, 0.0f));
        }
        t->mHealth = b.fsm.health();
        kill = o.killRequest;
    }
    if (t->mHealth > 0.0f) t->updateLifeGauge();
    b.logTimer += dt;
    if (b.logTimer >= 1.0f) {
        b.logTimer = 0.0f;
        const Vector3f p = t->getPosition();
        const bb::Vec3& wp = b.fsm.nextWayPoint();
        std::printf("P2_BREADBUG_OWN_POS generator=%u source_id=38 state=%s anim=%d frame=%.0f x=%.1f z=%.1f "
                    "home=%.1f,%.1f next=%.1f,%.1f health=%.1f target=%d held=%d pellets=%zu tai_changes=%d "
                    "attacks_ignored=%d events_consumed=%d presses=%d fly_rising=%d pellets_alive=%d nearest_pellet=%.0f wall=%lld\n",
                    b.generator, bb::stateName(b.fsm.state()), b.fsm.animator().anim(), b.fsm.animator().frame(),
                    p.x, p.z, b.fsm.home().x, b.fsm.home().z, wp.x, wp.z, b.fsm.health(), b.fsm.target() ? 1 : 0,
                    b.held ? 1 : 0, infos.size(), b.taiChanges, b.attacksIgnored, b.eventsConsumed, b.presses,
                    b.flyContactsRising, sPelletsAlive, sPelletNearest, wallMs());
    }
    std::fflush(stdout);
    if (kill && !b.escaped) {
        // Dead KEYEVENT_END -> kill(): the host death funnel (die + dieSoon)
        // births the LeaveCorpse pellet; dieSoon only runs inside the
        // suppressed doAI, hence pcEscapeNow (Groink/long-legs pattern).
        b.escaped = true;
        if (Creature* stick = t->getStickObject())
            if (stick->isObjType(OBJTYPE_Pellet)) static_cast<Pellet*>(stick)->endStickTeki(t);
        setHidden(t, b, false);
        t->inputDrive(Vector3f(0.0f, 0.0f, 0.0f));
        t->mVelocity.x = t->mVelocity.z = 0.0f;
        std::printf("P2_BREADBUG_OWN_ESCAPE generator=%u source_id=38 native=host_escape_now tai_changes=%d "
                    "attacks_ignored=%d presses=%d presses_rejected=%d\n",
                    b.generator, b.taiChanges, b.attacksIgnored, b.presses, b.pressesRejected);
        std::fflush(stdout);
        t->pcEscapeNow();
        return true;
    }
    return false;
}
} // namespace

void pc_p2_breadbug_teki_reset() {
    s.clear();
    sDrawLogged.clear();
    for (auto& poses : sPoses) poses.clear();
    sPosesLoaded = false;
}

void pc_p2_breadbug_teki_forget(BTeki* t) {
    if (!t) return;
    auto i = s.find(t);
    if (i == s.end()) return;
    std::printf("P2_BREADBUG_OWN_FORGET generator=%u source_id=38 dead_state=%d\n", i->second.generator, t->mDeadState);
    std::fflush(stdout);
    s.erase(i);
    sDrawLogged.erase(t);
}

bool pc_p2_breadbug_teki_is_bound(const BTeki* t) { return t && find(t); }

bool pc_p2_breadbug_teki_suppress_ai(const BTeki* t) {
    const Binding* b = find(t);
    return b && !b->escaped;
}

void pc_p2_breadbug_teki_setup() {
    pc_p2_breadbug_teki_reset();
    if (!pc_randomizer_p2_bridge() || !tekiMgr) return;
    bool loaded = false;
    Iterator it(tekiMgr);
    CI_LOOP(it) {
        auto* t = static_cast<Teki*>(*it);
        if (!t || !t->mGenerator || pc_p2_campaign_source(t) != kSource) continue;
        const unsigned gen = pc_p2_campaign_token(t);
        if (t->mTekiType != TEKI_Collec) {
            std::printf("P2_SETUP_SKIP Breadbug host_type_mismatch generator=%u type=%d\n", gen, t->mTekiType);
            continue;
        }
        if (t->getParameterI(TPI_CorpseType) != TEKICORPSE_LeaveCorpse) {
            std::printf("P2_SETUP_SKIP Breadbug no_corpse generator=%u\n", gen);
            continue;
        }
        if (!loaded) {
            loaded = true;
            loadParams();
            loadBank();
            // One-shot census of the stage's pellets (read-only diagnosis of
            // what a wandering Breadbug can find; fp14 search is 500).
            if (pelletMgr) {
                Iterator pit(pelletMgr);
                CI_LOOP(pit) {
                    Pellet* p = static_cast<Pellet*>(*pit);
                    if (!p || !p->isAlive() || !p->mConfig) continue;
                    std::printf("P2_BREADBUG_OWN_PELLET_CENSUS model=%s min=%d max=%d x=%.0f y=%.0f z=%.0f ufo=%d state=%d\n",
                                p->mConfig->mModelId.mStringID, int(p->mConfig->mCarryMinPikis()),
                                int(p->mConfig->mCarryMaxPikis()), p->mSRT.t.x, p->mSRT.t.y, p->mSRT.t.z,
                                p->isUfoParts() ? 1 : 0, p->getState());
                }
            }
        }
        Binding& b = s[static_cast<BTeki*>(t)];
        b = Binding{};
        b.generator = gen;
        const Vector3f pos = t->getPosition();
        b.fsm.init(sParams, sBank, {pos.x, pos.y, pos.z}, t->getDirection(), (gen * 2654435761u) | 1u, &sRoute);
        t->mHealth = b.fsm.health();
        b.taiState = t->mStateID;
        // Retail PanModoki is not a living thing while unbittered (isLivingThing):
        // no Pikmin latch. The P1 Collec already clears ORGANIC at strategy
        // start; keep it cleared for the bound actor.
        t->clearTekiOption(TEKIOPT_Organic);
        std::printf("P2_BREADBUG_OWN_BIND generator=%u source_id=38 host_type=%d health=%.1f retail_parms=%d draw=%s "
                    "home=%.1f,%.1f wp=%d state=%s tai_state=%d\n",
                    gen, t->mTekiType, t->mHealth, sParams.retail ? 1 : 0, sPosesLoaded ? "p2_model" : "host", pos.x,
                    pos.z, sRoute.nearest({pos.x, pos.y, pos.z}), bb::stateName(b.fsm.state()), b.taiState);
        // Ordinary-delivery bridge: GoalItem::suckMe grants onion:p2:38 once
        // for the delivered corpse of THIS generator token.
        pc_randomizer_p2_bind_source(static_cast<PelletView*>(static_cast<BTeki*>(t)), kSource, gen);
        std::printf("P2_BREADBUG_DELIVERY_BIND generator=%u source_id=38\n", gen);
        std::fflush(stdout);
    }
}

void pc_p2_breadbug_teki_tick(BTeki* t) {
    auto i = s.find(t);
    if (i == s.end()) return;
    Binding& b = i->second;
    const float dt = gsys ? gsys->getFrameTime() : 0.0f;
    if (!b.escaped && t->mDeadState == 0) {
        if (!(dt > 0.0f && dt < 0.5f)) return;
        ownTick(t, b, dt);  // may run the host teardown; never touch b after a kill
        return;
    }
    if (!b.escaped && t->mDeadState == 1) {
        // Something outside the FSM called die(): finish the teardown so the
        // corpse pelletizes (dieSoon only runs in the suppressed doAI).
        b.escaped = true;
        std::printf("P2_BREADBUG_OWN_ESCAPE generator=%u source_id=38 native=host_die_external\n", b.generator);
        std::fflush(stdout);
        t->pcEscapeNow();
        return;
    }
    // Corpse phase: observe the carcass carry (read-only).
    if (t->mPellet && t->mPellet->isAlive()) {
        Pellet* c = t->mPellet;
        if (!b.corpseLogged) {
            b.corpseLogged = true;
            std::printf("P2_BREADBUG_OWN_CORPSE generator=%u source_id=38 x=%.1f z=%.1f carry_min=%d carry_max=%d\n",
                        b.generator, c->mSRT.t.x, c->mSRT.t.z, int(c->mConfig->mCarryMinPikis()),
                        int(c->mConfig->mCarryMaxPikis()));
        }
        b.corpseTimer += dt;
        if (b.corpseTimer >= 2.0f) {
            b.corpseTimer = 0.0f;
            std::printf("P2_BREADBUG_OWN_CORPSE_CARRY generator=%u source_id=38 x=%.1f z=%.1f carriers=%.1f state=%d wall=%lld\n",
                        b.generator, c->mSRT.t.x, c->mSRT.t.z, pikiStrength(c), c->getState(), wallMs());
        }
        std::fflush(stdout);
    }
}

bool pc_p2_breadbug_teki_event(BTeki* t, const TekiEvent& event) {
    Binding* b = find(t);
    if (!b || b->escaped) return false;
    ++b->eventsConsumed;
    if (event.mEventType == TekiEventType::Ground) {
        b->pendingBounce = true;
        t->mActionVelocity.y = 0.0f;  // landed: gravitate's fall speed resets
        return true;
    }
    if (event.mEventType == TekiEventType::Entity && event.mOther && event.mOther->isPiki()) {
        // PikiFlyingState::collisionCallback (pikiState.cpp:2322-2330): a
        // thrown Pikmin touching the enemy while falling sends InteractPress.
        Piki* piki = static_cast<Piki*>(event.mOther);
        if (piki->isAlive() && piki->getState() == PIKISTATE_Flying && piki->mVelocity.y >= 0.0f
            && !b->pressedFlight.count(piki)) {
            ++b->flyContactsRising;  // rising contact: retail sends no press (vel.y < 0 only)
        }
        if (piki->isAlive() && piki->getState() == PIKISTATE_Flying && piki->mVelocity.y < 0.0f
            && !b->pressedFlight.count(piki)) {
            b->pressedFlight.insert(piki);
            ++b->pendingPresses;
            std::printf("P2_BREADBUG_OWN_PRESS_CONTACT generator=%u source_id=38 vy=%.1f state=%s wall=%lld\n", b->generator,
                        piki->mVelocity.y, bb::stateName(b->fsm.state()), wallMs());
            std::fflush(stdout);
        }
    }
    return true;  // the P1 Collec TAI never sees an OWN Breadbug's events
}

bool pc_p2_breadbug_teki_attack(BTeki* t, float damage) {
    Binding* b = find(t);
    if (!b || b->escaped) return false;
    ++b->attacksIgnored;
    if (b->attacksIgnored <= 5 || b->attacksIgnored % 50 == 0)
        std::printf("P2_BREADBUG_OWN_ATTACK_IGNORED generator=%u source_id=38 damage=%.1f health=%.1f count=%d\n",
                    b->generator, damage, b->fsm.health(), b->attacksIgnored);
    std::fflush(stdout);
    return true;
}

float pc_p2_breadbug_teki_param_f(const BTeki* t, int idx, float fallback) {
    const Binding* b = find(t);
    if (!b || b->escaped) return fallback;
    if (idx == TPF_Life) return b->fsm.params().health;
    if (idx == TPF_LifeRecoverRate) return 0.0f;
    return fallback;
}

bool pc_p2_breadbug_teki_draw(BTeki* t, Graphics& gfx, const Matrix4f& view, bool corpse) {
    auto i = s.find(t);
    if (i == s.end() || !sPosesLoaded || !gfx.mCamera) return false;
    Binding& b = i->second;
    const bool dead = corpse || b.escaped || t->mDeadState != 0;
    int anim = b.fsm.animator().anim();
    float frame = b.fsm.animator().frame();
    if (dead) {
        // startCarcassMotion: the carcass clip (type5), loop start pose.
        anim = bb::AnimCarry;
        frame = 10.0f;
        if (sPoses[anim].empty()) { anim = bb::AnimDead; frame = 1e9f; }
    }
    if (anim < 0 || anim >= bb::AnimCount || sPoses[anim].empty()) {
        anim = !sPoses[bb::AnimWalk].empty() ? int(bb::AnimWalk) : int(bb::AnimWait);
        frame = 0.0f;
        if (sPoses[anim].empty()) return false;
    }
    const auto& poses = sBank.clip[anim].poses;
    std::size_t best = 0;
    for (std::size_t k = 1; k < poses.size() && k < sPoses[anim].size(); ++k)
        if (std::fabs(float(poses[k]) - frame) < std::fabs(float(poses[best]) - frame)) best = k;
    Shape* shape = sPoses[anim][best];
    shape->updateAnim(gfx, view, nullptr, t);
    pc_gfx_specular_family_scope(1);
    shape->drawshape(gfx, *gfx.mCamera, nullptr);
    pc_gfx_specular_family_scope(0);
    int& logged = sDrawLogged[t];
    const int bit = dead ? 2 : 1;
    if (!(logged & bit)) {
        logged |= bit;
        std::printf("P2_BREADBUG_OWN_DRAW generator=%u source_id=38 corpse=%d clip=%s pose=%zu model=p2_panmodoki\n",
                    b.generator, dead ? 1 : 0, sBank.clip[anim].name.c_str(), best);
        std::fflush(stdout);
    }
    return true;
}
