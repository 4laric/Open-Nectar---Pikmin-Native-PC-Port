#include "pc_p2_kurage_own_host.h"

#include "pc_p2_kurage_receiver.h"
#include "pc_p2_navi_select.h"
#include "pc_p2_sfx.h"
#include "Collision.h"
#include "Interactions.h"
#include "MapMgr.h"
#include "Navi.h"
#include "NaviMgr.h"
#include "Piki.h"
#include "PikiMgr.h"
#include "Shape.h"
#include "system.h"
#include "teki.h"
#include <cmath>
#include <cstdio>

namespace {
constexpr float kSourceDelta = 1.0f / 30.0f;
// EnemyFunc.h:9 FLICK_BACKWARD_ANGLE; flickStick* add PI and wrap.
constexpr float kFlickBackwardAngle = -1000.0f;
// Creature Property accel (enemyparm s003): fraction of the velocity error
// closed per source tick.
constexpr float kAccel = 0.1f;
} // namespace

float P2KurageOwn::rand01()
{
    mRng = mRng * 1664525u + 1013904223u;
    return float((mRng >> 8) & 0xFFFFu) / 65536.0f;
}

bool P2KurageOwn::init(BTeki* actor, unsigned generator, unsigned source, CollPart* mouth, Shape* restShape)
{
    if (!actor || !mouth) return false;
    mActive = true;
    mEscaped = false;
    mGenerator = generator;
    mSource = source;
    mVariant = source == 72 ? p2kurage::Variant::Greater : p2kurage::Variant::Lesser;
    mMouth = mouth;
    mFsm = p2kurage::Fsm(p2kurageown::flightParms(mVariant), mVariant);
    mFsm.spawn();
    mLast = p2kurage::Out{};
    mState = p2kurage::State::Wait;
    mMotion = p2kurage::Motion::None;
    mUntargetable = true;
    mClock.reset();
    mPlayer.cancel();
    mPendingKey = p2kurage::KeyEvent::None;
    mPendingEnd = false;
    mRng = (generator * 2654435761u) | 1u;
    const Vector3f pos = actor->mSRT.t;
    mHomeX = pos.x;
    mHomeZ = pos.z;
    mTargetX = pos.x;
    mTargetZ = pos.z;
    mHasTarget = false;
    mYaw = actor->getDirection();
    mVelX = mVelY = mVelZ = 0.0f;
    mSucked = 0;
    mSuckFull = false;
    actor->mHealth = p2kurageown::general(mVariant).life;
    mLastHealth = actor->mHealth;

    // Body joint offset: centroid of the rest mesh when loaded (the model root
    // sits at the bell underside), else the measured fallback.
    mBody = p2kurageown::defaultBodyOffset(mVariant);
    bool measured = false;
    if (restShape && restShape->mVertexList && restShape->mVertexCount > 0) {
        const Vector3f* verts = restShape->mVertexList;
        measured = p2flyer::bodyOffsetFromMesh(std::size_t(restShape->mVertexCount),
            [verts](std::size_t i) { return p2flyer::Vec3{verts[i].x, verts[i].y, verts[i].z}; }, mBody);
    }
    const bool collBound = mColl.bind(actor, p2kurageown::spheres(mVariant), p2kurageown::kSphereCount);
    if (collBound) mColl.follow(actor, p2flyer::Vec3{pos.x, pos.y, pos.z}, mYaw, mBody, actor->mSRT.s.y);
    actor->startFlying();
    std::printf("P2_KURAGE_OWN_BIND generator=%u source_id=%u health=%.1f retail_parms=1 coll=%d "
                "body=%.1f,%.1f,%.1f measured=%d stick_bottom=%.1f flight_height=%.1f territory=%.1f\n",
                generator, source, actor->mHealth, int(collBound), mBody.x, mBody.y, mBody.z, int(measured),
                p2flyer::stickableBottom(p2kurageown::spheres(mVariant), p2kurageown::kSphereCount, mBody),
                p2kurageown::flightParms(mVariant).flightHeight, p2kurageown::general(mVariant).territoryRadius);
    std::fflush(stdout);
    return true;
}

void P2KurageOwn::detach(BTeki* actor)
{
    if (!mActive) return;
    mColl.detach(actor);
    if (actor && !mEscaped && actor->isFlying()) actor->finishFlying();
    mActive = false;
}

void P2KurageOwn::startMotion(p2kurage::Motion m)
{
    mMotion = m;
    mPlayer.cancel();
    mPlayer.start(p2kurageown::clipFor(m));
    mPendingKey = p2kurage::KeyEvent::None;
    mPendingEnd = false;
}

// Kurage::getSearchedTarget(offset) + isSuck(offset, target/nullptr).
Piki* P2KurageOwn::search(BTeki* actor, float altitude, bool& suckTarget, bool& suckAny) const
{
    suckTarget = false;
    suckAny = false;
    if (!pikiMgr) return nullptr;
    const auto g = p2kurageown::general(mVariant);
    const Vector3f pos = actor->mSRT.t;
    const float minY = pos.y - altitude - 50.0f;
    const float attackRange = g.maxAttackRange * g.maxAttackRange;
    const bool territory = p2kurageown::insideTerritory(pos.x - mHomeX, pos.z - mHomeZ, g);
    const float fov = p2flyer::kPi * (p2flyer::kPi / 180.0f * g.viewAngle);
    float maxDist = g.sightRadius * g.sightRadius;
    Piki* target = nullptr;
    Piki* closeHit = nullptr;
    Iterator it(pikiMgr);
    CI_LOOP(it) {
        Piki* p = static_cast<Piki*>(*it);
        if (!p || !p->isAlive() || p->isBuried() || p->getStickObject() == actor
            || pc_p2_kurage_receiver_controls(p)) continue;
        const Vector3f q = p->mSRT.t;
        if (!(q.y > minY && q.y < pos.y)) continue;
        const float dx = q.x - pos.x, dz = q.z - pos.z;
        const float d2 = dx * dx + dz * dz;
        if (d2 < attackRange) {
            suckAny = true;
            if (territory && !closeHit) closeHit = p; // getSearchedTarget returns the first in range
        }
        if (territory && !closeHit && d2 < maxDist) {
            const float ang = p2flyer::angleDist(mYaw, dx, dz);
            if (std::fabs(ang) <= fov) { target = p; maxDist = d2; }
        }
    }
    Piki* found = closeHit ? closeHit : target;
    if (found) {
        const Vector3f q = found->mSRT.t;
        const float dx = q.x - pos.x, dz = q.z - pos.z;
        suckTarget = q.y > minY && q.y < pos.y && dx * dx + dz * dz < attackRange;
    }
    return found;
}

// mStuckPikminCount: Pikmin stuck to the body (mouth-carried stomach Pikmin are
// the receiver's, not body-stuck).
int P2KurageOwn::countStuck(BTeki* actor, bool& purple) const
{
    purple = false;
    if (!pikiMgr) return 0;
    int n = 0;
    Iterator it(pikiMgr);
    CI_LOOP(it) {
        Piki* p = static_cast<Piki*>(*it);
        if (!p || !p->isAlive() || p->getStickObject() != actor) continue;
        if (pc_p2_kurage_receiver_controls(p)) continue;
        ++n;
        if (p->mP2Purple) purple = true;
    }
    return n;
}

// Kurage::suckPikmin(offset): per source tick, every eligible Pikmin under the
// bell rolls fp12 and enters the suction. Returns via mSuckFull (>= ip11).
void P2KurageOwn::suckPikmin(BTeki* actor, float mapY)
{
    if (!pikiMgr) return;
    const auto g = p2kurageown::general(mVariant);
    const p2kurage::Parms parms = p2kurageown::flightParms(mVariant);
    const Vector3f pos = actor->mSRT.t;
    const float minY = mapY - 50.0f; // currY - offset(altitude) - 50
    const float range = g.attackRadius * g.attackRadius;
    Iterator it(pikiMgr);
    CI_LOOP(it) {
        Piki* p = static_cast<Piki*>(*it);
        if (!p || !p->isAlive() || p->isBuried() || p->getStickObject() == actor) continue;
        if (!(mSucked < parms.maxSuckPiki && rand01() < parms.suckChance)) continue;
        const Vector3f q = p->mSRT.t;
        if (!(q.y > minY && q.y < pos.y)) continue;
        const float dx = q.x - pos.x, dz = q.z - pos.z;
        if (dx * dx + dz * dz >= range) continue;
        if (pc_p2_kurage_receiver_admit_for(actor, p)) {
            ++mSucked;
            pc_p2_sfx(mSource, mGenerator, p2sfx::Event::Attack, actor);
        }
    }
    mSuckFull = mSucked >= parms.maxSuckPiki;
}

// EnemyFunc::flickStickPikmin(creature, chance, knockback, damage, FLICK_BACKWARD_ANGLE).
void P2KurageOwn::flickStuck(BTeki* actor, float chance, float knockback, float damage)
{
    if (!pikiMgr) return;
    const float angle = p2flyer::roundAng(kFlickBackwardAngle + p2flyer::kPi);
    int flicked = 0, seen = 0;
    Iterator it(pikiMgr);
    CI_LOOP(it) {
        Piki* p = static_cast<Piki*>(*it);
        if (!p || !p->isAlive() || p->getStickObject() != actor || pc_p2_kurage_receiver_controls(p)) continue;
        ++seen;
        if (chance > rand01() && p->stimulate(InteractFlick(actor, knockback, damage, angle))) ++flicked;
    }
    std::printf("P2_KURAGE_OWN_FLICK generator=%u source_id=%u kind=stick stuck=%d flicked=%d\n", mGenerator,
                mSource, seen, flicked);
}

// EnemyFunc::flickNearbyNavi / flickNearbyPikmin (GroundFlick KEYEVENT_3).
void P2KurageOwn::flickNearby(BTeki* actor, float radius, float knockback, float damage)
{
    const float angle = p2flyer::kPi + kFlickBackwardAngle;
    const float r2 = radius * radius;
    const Vector3f me = actor->mSRT.t;
    int navi = 0, piki = 0;
    for (Navi* n : pc_p2_navis()) {
        if (!n || !n->isAlive()) continue;
        const Vector3f d = n->mSRT.t - me;
        if (d.x * d.x + d.y * d.y + d.z * d.z < r2 && n->stimulate(InteractFlick(actor, knockback, damage, angle))) ++navi;
    }
    if (pikiMgr) {
        Iterator it(pikiMgr);
        CI_LOOP(it) {
            Piki* p = static_cast<Piki*>(*it);
            if (!p || !p->isAlive() || p->getStickObject() == actor) continue;
            const Vector3f d = p->mSRT.t - me;
            if (d.x * d.x + d.y * d.y + d.z * d.z < r2 && p->stimulate(InteractFlick(actor, knockback, damage, angle))) ++piki;
        }
    }
    std::printf("P2_KURAGE_OWN_FLICK generator=%u source_id=%u kind=nearby navi=%d piki=%d\n", mGenerator, mSource,
                navi, piki);
}

void P2KurageOwn::sourceTick(BTeki* actor)
{
    const auto g = p2kurageown::general(mVariant);
    const Vector3f pos = actor->mSRT.t;
    const float mapY = mapMgr ? mapMgr->getMinY(pos.x, pos.z, false) : 0.0f;
    const float altitude = pos.y - mapY;

    bool suckTarget = false, suckAny = false;
    Piki* target = search(actor, altitude, suckTarget, suckAny);
    bool purple = false;
    const int stuck = countStuck(actor, purple);
    if (stuck > mStuckPrev)
        std::printf("P2_KURAGE_OWN_LATCH generator=%u source_id=%u stuck=%d airborne=%d alt=%.1f state=%d health=%.1f\n",
                    mGenerator, mSource, stuck, int(mUntargetable), altitude, int(mState), actor->mHealth);
    mStuckPrev = stuck;

    p2kurage::In in;
    in.deltaTime = kSourceDelta;
    in.health = actor->mHealth;
    in.stuckPikminCount = stuck;
    in.purpleStuck = purple;
    in.isFlying = mUntargetable; // EnemyBase::isFlying() == EB_Untargetable
    in.targetFound = target != nullptr;
    in.suckTarget = suckTarget;
    in.suckAny = suckAny;
    in.mapY = mapY;
    in.positionY = pos.y;
    in.distToTargetXZ = mDistGoal;
    in.motionFrame = mPlayer.frame();
    in.keyEvent = mPendingKey;
    in.motionFinished = mPendingEnd;
    in.suckFull = mSuckFull;
    mPendingKey = p2kurage::KeyEvent::None;
    mPendingEnd = false;

    const p2kurage::State before = mState;
    const p2kurage::Out out = mFsm.tick(in);
    mLast = out;
    mAltitude = out.altitude;
    ++mTicks;
    if (out.motionChanged) startMotion(out.motion);
    if (out.state != before) {
        mState = out.state;
        std::printf("P2_KURAGE_OWN_STATE generator=%u source_id=%u from=%d state=%d motion=%d alt=%.1f stuck=%d "
                    "untargetable=%d health=%.1f x=%.1f z=%.1f\n",
                    mGenerator, mSource, int(before), int(out.state), int(out.motion), altitude, stuck,
                    int(out.flags.untargetable), actor->mHealth, pos.x, pos.z);
        if (out.state == p2kurage::State::Dead && !mDeadLogged) {
            mDeadLogged = true;
            pc_p2_sfx(mSource, mGenerator, p2sfx::Event::Dead, actor);
            std::printf("P2_KURAGE_OWN_DEAD generator=%u source_id=%u health=%.1f\n", mGenerator, mSource,
                        actor->mHealth);
        }
        if (out.state == p2kurage::State::Land) {
            pc_p2_sfx(mSource, mGenerator, p2sfx::Event::Land, actor);
            std::printf("P2_KURAGE_OWN_LAND generator=%u source_id=%u alt=%.1f stuck=%d\n", mGenerator, mSource,
                        altitude, stuck);
        }
        if (out.state == p2kurage::State::FlyFlick || out.state == p2kurage::State::GroundFlick)
            pc_p2_sfx(mSource, mGenerator, p2sfx::Event::Flick, actor);
    }
    if (out.state == p2kurage::State::Attack && out.motionChanged) { mSucked = 0; mSuckFull = false; }
    if (out.state == p2kurage::State::Move && out.motionChanged) {
        // StateMove::init -> setRandTarget()
        const float u1 = rand01(), u2 = rand01();
        p2kurageown::randTarget(u1, u2, pos.x, pos.z, mHomeX, mHomeZ, g, mTargetX, mTargetZ);
        mHasTarget = true;
    }

    // Movement: walkToTarget in Move (patrol point) and Chase (searched Pikmin);
    // every other state zeroes the target velocity.
    float tvx = 0.0f, tvz = 0.0f;
    mDistGoal = 1.0e9f;
    if (out.state == p2kurage::State::Move && mHasTarget) {
        const float dx = mTargetX - pos.x, dz = mTargetZ - pos.z;
        mDistGoal = std::sqrt(dx * dx + dz * dz);
        if (!out.finishing) {
            mYaw = p2flyer::turnStep(mYaw, dx, dz, g.turnSpeed, g.maxTurnAngle);
            p2flyer::walkVelocity(mYaw, g.moveSpeed, tvx, tvz);
        }
    } else if (out.state == p2kurage::State::Chase && target && !suckTarget && !out.finishing) {
        const Vector3f q = target->mSRT.t;
        mYaw = p2flyer::turnStep(mYaw, q.x - pos.x, q.z - pos.z, g.turnSpeed, g.maxTurnAngle);
        p2flyer::walkVelocity(mYaw, g.moveSpeed, tvx, tvz);
    }
    mVelX += (tvx - mVelX) * kAccel;
    mVelZ += (tvz - mVelZ) * kAccel;

    // Vertical: setHeightVelocity while EB_Untargetable (flying). The Fall
    // descent frame drops EB_Untargetable and kicks -100 into the velocity.
    const bool wasUntargetable = mUntargetable;
    mUntargetable = out.flags.untargetable;
    const bool flyPhys = p2flyer::airborne(mUntargetable) && out.state != p2kurage::State::Dead;
    if (flyPhys) mVelY = out.heightVelocity;
    if (wasUntargetable && !mUntargetable && before == p2kurage::State::Fall) actor->mVelocity.y += p2flyer::kFallKick;
    // Grounded phases and death stand still horizontally (mTargetVelocity = 0).
    if (out.state == p2kurage::State::Dead || out.state == p2kurage::State::Land
        || out.state == p2kurage::State::Ground || out.state == p2kurage::State::GroundFlick) {
        mVelX = mVelZ = 0.0f;
    }
    // CF_IsFlying mirror + gravity switch: a pure function of FSM state.
    if (flyPhys && !actor->isFlying()) actor->startFlying();
    else if (!flyPhys && actor->isFlying()) actor->finishFlying();

    if (out.isSucking) suckPikmin(actor, mapY);

    if (out.flickStick || out.flickNearby) {
        if (out.state == p2kurage::State::Dead) flickStuck(actor, 1.0f, 100.0f, 0.0f);
        else flickStuck(actor, g.shakeChance, g.shakeKnockback, g.shakeDamage);
    }
    if (out.flickNearby) flickNearby(actor, g.shakeRange, g.shakeKnockback, g.shakeDamage);

    // Retail clip clock: one animation frame per 30 Hz source tick.
    mPlayer.finishMotion(out.finishing);
    mPlayer.advance(1.0f, [this](const p2retail::Event& e) {
        if (e.type == 1000) mPendingEnd = true;
        else if (e.type >= 1 && e.type <= 3) mPendingKey = p2kurageown::keyFor(e.type);
    });

    if (out.kill && !mEscaped) {
        // StateDead KEYEVENT_END -> kill(): the host death funnel births the
        // LeaveCorpse pellet (dieSoon only runs inside the suppressed doAI, so
        // finalise it here: groink/long-legs pcEscapeNow pattern).
        mEscaped = true;
        const Vector3f p = actor->mSRT.t;
        mColl.release(actor, p2flyer::Vec3{p.x, p.y, p.z});
        pc_p2_kurage_receiver_release_all_for(actor);
        if (actor->isFlying()) actor->finishFlying();
        actor->inputDrive(Vector3f(0.0f, 0.0f, 0.0f));
        actor->mVelocity.x = actor->mVelocity.z = 0.0f;
        std::printf("P2_KURAGE_OWN_ESCAPE generator=%u source_id=%u native=host_escape_now health=%.1f\n",
                    mGenerator, mSource, actor->mHealth);
        std::fflush(stdout);
        actor->pcEscapeNow();
    }
}

bool P2KurageOwn::tick(BTeki* actor, float dt)
{
    if (!mActive || mEscaped || !actor) return false;
    // The suppressed P1 strategy normally applies stored damage through its
    // damage reaction; drain it so Pikmin hits reach mHealth (natural death).
    if (actor->mStoredDamage > 0.0f) actor->makeDamaged();
    if (actor->mHealth < mLastHealth && actor->mHealth > 0.0f) {
        pc_p2_sfx(mSource, mGenerator, p2sfx::Event::Damage, actor);
        std::printf("P2_KURAGE_OWN_DAMAGE generator=%u source_id=%u health=%.1f prior=%.1f airborne=%d state=%d\n",
                    mGenerator, mSource, actor->mHealth, mLastHealth, int(mUntargetable), int(mState));
    }
    mLastHealth = actor->mHealth;

    const int ticks = mClock.step(double(dt), true);
    for (int k = 0; k < ticks && !mEscaped; ++k) sourceTick(actor);
    if (mEscaped) return true;

    // Per real frame: the host integrates the velocity the FSM set; facing,
    // collision spheres, suction mouth and life gauge follow.
    const bool flyPhys = actor->isFlying();
    actor->setDirection(mYaw);
    if (flyPhys) {
        actor->inputDrive(Vector3f(mVelX, mVelY, mVelZ));
        actor->mVelocity.set(mVelX, mVelY, mVelZ);
        pc_p2_sfx(mSource, mGenerator, p2sfx::Event::Hover, actor);
    } else {
        actor->inputDrive(Vector3f(mVelX, 0.0f, mVelZ));
        actor->mVelocity.x = mVelX;
        actor->mVelocity.z = mVelZ;
    }
    const Vector3f pos = actor->mSRT.t;
    if (mColl.bound())
        mColl.follow(actor, p2flyer::Vec3{pos.x, pos.y, pos.z}, mYaw, mBody, actor->mSRT.s.y);
    if (mMouth) {
        mMouth->mPartType = PART_BoundSphere;
        mMouth->mRadius = p2kurageown::mouthRadius(mVariant);
        mMouth->mCentre = pos;
        mMouth->mJointMatrix.makeIdentity();
    }
    pc_p2_kurage_receiver_update_for(actor, dt, true, actor->mHealth > 0.0f, false);
    if (actor->mHealth > 0.0f) actor->updateLifeGauge();

    mLogTimer += dt;
    if (mLogTimer >= 1.0f) {
        mLogTimer = 0.0f;
        std::printf("P2_KURAGE_OWN_POS generator=%u source_id=%u state=%d motion=%d x=%.1f y=%.1f z=%.1f alt=%.1f "
                    "health=%.1f stuck=%d held=%d untargetable=%d\n",
                    mGenerator, mSource, int(mState), int(mMotion), pos.x, pos.y, pos.z, mAltitude,
                    actor->mHealth, mStuckPrev, pc_p2_kurage_receiver_count_for(actor), int(mUntargetable));
    }
    std::fflush(stdout);
    return false;
}
