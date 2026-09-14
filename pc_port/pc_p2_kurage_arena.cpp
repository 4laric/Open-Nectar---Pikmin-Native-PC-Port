#include "pc_p2_kurage_arena.h"

#include "Camera.h"
#include "Collision.h"
#include "Creature.h"
#include "Graphics.h"
#include "MapMgr.h"
#include "Piki.h"
#include "PikiMgr.h"
#include "Shape.h"
#include "gameflow.h"
#include "system.h"
#include "pc_p2_kurage_fsm.h"
#include "pc_p2_kurage_receiver.h"
#include "pc_p2_retail_player.h"
#include "pc_p2_kurage_visual.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

namespace {
class LesserHost final : public Creature {
public:
    LesserHost() : Creature(nullptr) { }
    void refresh(Graphics&) override { }
    void doKill() override { }
};
struct Host {
    Shape* shape = nullptr;
    Shape* attackShape = nullptr;
    LesserHost owner;
    CollPart mouth{};
    Vector3f position;
    float height = 90.0f;
    float radius = 35.0f;
    Vector3f mouthJointTranslation;
    bool sourceJointAvailable = false;
    float phase = 0.0f;
    bool ready = false;
    bool alive = false;
    // The event clock is supplied by Kurage attack.bca.  The converted MOD is
    // deliberately a static attack pose; it does not provide skeletal BCA/BTK
    // playback.
    p2retail::Player attackPlayer;
    bool attackPlaying = false;
    bool sucking = false;
    // Test-only frame injection retained for focused unit/fixture seams.  It
    // never drives the live host update path.
    bool attackFrameSeam = false;
    float attackFrame = 0.0f;
    // Opt-in source flight-lifecycle authority (pc_p2_kurage_fsm.h).
    p2kurage::Fsm fsm;
    bool fsmEnabled = false;
    int fsmTicks = 0;
    int lastFsmState = -1;
    float fsmAltitude = 0.0f;
    float fsmHealth = 100.0f;
    bool ownerHasHealth = true;
    bool ownerBittered = false;
    int autoAdmissions = 0;
    p2kurage::KeyEvent pendingKey = p2kurage::KeyEvent::None;
    bool fsmMotionFinished = false;
    // Bounded animation-END stand-in while the converted MOD is a static pose.
    // The real source motion-end event belongs to the #431 animation clock.
    int fsmMotionTimer = 0;
};
Host sHost;
// Retail Lesser Kurage suckPikmin() queries collision part ID 'suck'; hire1 is
// a BMD visual joint, not the source suction collision location.
constexpr char kSourceSuctionPart[] = "suck";
constexpr char kVisualHireJoint[] = "hire1";
constexpr int kSourceSuctionJoint = 4;
constexpr float kSourceSuctionRadius = 15.0f;
// Bounded host approximation of Kurage::mAttackRadius for getSearchedTarget/
// isSuck; the source ProperParms value is asset-supplied and not yet imported.
constexpr float kSourceAttackRadius = 35.0f;
constexpr int kMaxAutoAdmissions = 10; // Kurage.h ip11 maxSuckPiki
constexpr float kFsmLiveHealth = 100.0f;
// Bounded host animation-END period (0.5 s at 60 Hz) used only while the
// converted MOD cannot supply a real motion-end event (#431 owns that bridge).
constexpr int kFsmMotionFrames = 30;
// p2retail::Player timers are animation frames. Kurage attack.bca is a 30 fps
// clip, so a real-time host advances the clock by delta * 30.
constexpr float kAttackFramesPerSecond = 30.0f;
// Retail Kurage/attack.bca SHA-256 302660c6ba9c86fee11cc6aca98bd514e201a80d8530be3a5cce867a9dc74a4e.
// ANF1 is big-endian: loop attribute 2, duration 0x0078 (120), 12 joints.
// enemyanimmgr.txt supplies the source event table below.
const p2retail::Motion kAttackMotion{
    "attack.bca", "302660c6ba9c86fee11cc6aca98bd514e201a80d8530be3a5cce867a9dc74a4e",
    120, 2, {{37, 2}, {60, 0}, {67, 1}}
};

bool attackPoseActive()
{
    return sHost.attackFrameSeam
        ? sHost.attackFrame >= 37.0f && sHost.attackFrame < 67.0f
        : sHost.sucking;
}
bool suckingActive()
{
    return sHost.attackFrameSeam
        ? sHost.attackFrame >= 37.0f && sHost.attackFrame < 67.0f
        : sHost.sucking;
}

bool finite(float value) { return std::isfinite(value); }
bool valid(Vector3f value)
{
    return finite(value.x) && finite(value.y) && finite(value.z)
        && std::fabs(value.x) < 100000.0f && std::fabs(value.y) < 100000.0f
        && std::fabs(value.z) < 100000.0f;
}

void updateHostCollision()
{
    sHost.owner.mSRT.t = sHost.position;
    sHost.owner.mSRT.s.set(1.0f, 1.0f, 1.0f);
    sHost.owner.mSRT.r.set(0.0f, 0.0f, 0.0f);
    sHost.mouth.mPartType = PART_BoundSphere;
    sHost.mouth.mRadius = kSourceSuctionRadius;
    // enemycoll.txt binds suck to JNT1 index 4 with a zero offset.  The
    // converted preview MOD currently omits that JNT1 hierarchy, so the
    // bounded host uses the source offset from its origin until a collision-
    // tree world-matrix bridge can supply that joint's animated position.
    sHost.mouth.mCentre.set(sHost.position.x + sHost.mouthJointTranslation.x,
        sHost.position.y + sHost.mouthJointTranslation.y,
        sHost.position.z + sHost.mouthJointTranslation.z);
    sHost.mouth.mJointMatrix.makeIdentity();
}

// Bounded host approximation of Kurage::getSearchedTarget(altitude): return the
// first live Pikmin inside the source vertical suction window and attack
// radius whose sticker is not this owner.  View-angle rejection and per-family
// sight radius belong to the one-consumer receiver, not this scan.
Piki* findSuctionTarget()
{
    if (!pikiMgr || !sHost.ready) return nullptr;
    ObjectMgr* manager = static_cast<ObjectMgr*>(pikiMgr);
    const float radiusSqr = kSourceAttackRadius * kSourceAttackRadius;
    for (int it = manager->getFirst(); !manager->isDone(it); it = manager->getNext(it)) {
        Piki* piki = static_cast<Piki*>(manager->getCreature(it));
        if (!piki || !piki->isAlive() || piki->getStickObject() == &sHost.owner || !piki->mayIstick()) continue;
        if (!p2kurage::inSuctionWindow(sHost.position.y, 0.0f, piki->mSRT.t.y)) continue;
        const float dx = piki->mSRT.t.x - sHost.position.x;
        const float dz = piki->mSRT.t.z - sHost.position.z;
        if (dx * dx + dz * dz >= radiusSqr) continue;
        return piki;
    }
    return nullptr;
}
}

void pc_p2_kurage_arena_reset()
{
    // Detach while the concrete host and its CollPart still exist.
    pc_p2_kurage_receiver_owner_invalidated(&sHost.owner);
    sHost.shape = nullptr; sHost.ready = sHost.alive = false; sHost.phase = 0.0f;
    sHost.attackPlayer.cancel(); sHost.attackPlaying = sHost.sucking = false;
    sHost.attackFrameSeam = false; sHost.attackFrame = 0.0f;
    sHost.fsmEnabled = false; sHost.fsmTicks = 0; sHost.lastFsmState = -1; sHost.fsmAltitude = 0.0f;
    sHost.fsmHealth = kFsmLiveHealth; sHost.ownerHasHealth = true; sHost.ownerBittered = false;
    sHost.autoAdmissions = 0; sHost.pendingKey = p2kurage::KeyEvent::None; sHost.fsmMotionFinished = false; sHost.fsmMotionTimer = 0;
}

bool pc_p2_kurage_arena_setup(const char* profilePath)
{
    pc_p2_kurage_arena_reset();
    if (!profilePath || !*profilePath) return false;
    std::ifstream profile(profilePath);
    std::string header;
    if (!(profile >> header) || header != "P2_KURAGE_ARENA_1") return false;
    Host parsed;
    std::string key;
    if (!(profile >> key >> parsed.position.x >> parsed.position.y >> parsed.position.z)
        || key != "position" || !valid(parsed.position)) return false;
    if (!(profile >> key >> parsed.height >> parsed.radius)
        || key != "params" || !finite(parsed.height) || !finite(parsed.radius)
        || parsed.height <= 0.0f || parsed.height > 1000.0f || parsed.radius < 0.0f
        || parsed.radius > 1000.0f) return false;
    if (profile >> key) return false;
    if (!pc_p2_kurage_visual_setup()) return false;
    parsed.shape = pc_p2_kurage_visual_wait_shape();
    parsed.attackShape = pc_p2_kurage_visual_attack_shape();
    if (!parsed.shape || !parsed.attackShape) return false;
    parsed.sourceJointAvailable = parsed.shape->mJointCount > kSourceSuctionJoint;
    if (parsed.sourceJointAvailable)
        parsed.mouthJointTranslation = parsed.shape->mJointList[kSourceSuctionJoint].mTranslation;
    else
        parsed.mouthJointTranslation.set(0.0f, 0.0f, 0.0f);
    parsed.ready = parsed.alive = true;
    sHost.shape = parsed.shape; sHost.attackShape = parsed.attackShape; sHost.position = parsed.position; sHost.height = parsed.height;
    sHost.radius = parsed.radius; sHost.mouthJointTranslation = parsed.mouthJointTranslation;
    sHost.sourceJointAvailable = parsed.sourceJointAvailable;
    sHost.phase = 0.0f; sHost.ready = sHost.alive = true;
    sHost.fsmEnabled = false; sHost.fsmTicks = 0; sHost.lastFsmState = -1; sHost.fsmAltitude = 0.0f;
    sHost.fsmHealth = kFsmLiveHealth; sHost.ownerHasHealth = true; sHost.ownerBittered = false;
    sHost.autoAdmissions = 0; sHost.pendingKey = p2kurage::KeyEvent::None; sHost.fsmMotionFinished = false; sHost.fsmMotionTimer = 0;
    sHost.fsm = p2kurage::Fsm(); sHost.fsm.spawn();
    sHost.owner.mStickListHead = nullptr;
    updateHostCollision();
    std::printf("P2_KURAGE_ARENA_READY species=Kurage id=57 visual=converted_wait source_part=%s joint=%d radius=%.1f offset=0,0,0 joint_translation=%s visual_joint=%s host_receiver=bounded\n", kSourceSuctionPart, kSourceSuctionJoint, kSourceSuctionRadius, sHost.sourceJointAvailable ? "wait_pose" : "unavailable_origin", kVisualHireJoint);
    return true;
}

bool pc_p2_kurage_arena_update(float delta, bool ownerAlive)
{
    if (!sHost.ready || !sHost.alive || !finite(delta) || delta < 0.0f || delta > 1.0f) return false;
    if (!ownerAlive) {
        // This private host is the lifecycle authority for its bounded receiver.
        // Release while the concrete owner and mouth still exist; callers must
        // not substitute this for P2's full Kurage health/bitter/FSM path.
        pc_p2_kurage_receiver_update(0.0f, false, true, false);
        return false;
    }
    if (sHost.fsmEnabled) {
        // Source Kurage StateWait/Move/Chase/Attack vertical authority.  The
        // bounded host supplies real map height/position and the real attack.bca
        // event clock; target facts come from a live Pikmin scan, not a
        // fabricated target.
        const float mapY = mapMgr ? mapMgr->getMinY(sHost.position.x, sHost.position.z, false) : 0.0f;
        p2kurage::In in;
        in.deltaTime = delta;
        in.health = sHost.fsmHealth;
        in.stuckPikminCount = pc_p2_kurage_receiver_count();
        in.isFlying = true;
        in.mapY = mapY;
        in.positionY = sHost.position.y;
        in.targetFound = findSuctionTarget() != nullptr || pc_p2_kurage_receiver_count() > 0;
        in.suckTarget = in.targetFound;
        in.suckAny = in.targetFound;
        in.motionFrame = sHost.attackPlaying ? sHost.attackPlayer.frame() : 0.0f;
        // Bounded animation-END: the attack clock owns the Attack interval; the
        // other states use a fixed period until the #431 motion-event bridge
        // supplies real clip completion.
        bool motionEnd = false;
        if (!sHost.attackPlaying && ++sHost.fsmMotionTimer >= kFsmMotionFrames) {
            sHost.fsmMotionTimer = 0;
            motionEnd = true;
        }
        in.motionFinished = sHost.fsmMotionFinished || motionEnd;
        in.keyEvent = sHost.pendingKey;
        sHost.pendingKey = p2kurage::KeyEvent::None;
        sHost.fsmMotionFinished = false;

        const p2kurage::Out out = sHost.fsm.tick(in);
        sHost.position.y += out.heightVelocity * delta;
        sHost.fsmAltitude = out.altitude;
        if ((int)out.state != sHost.lastFsmState) {
            sHost.lastFsmState = (int)out.state;
            std::printf("P2_KURAGE_FSM state=%d motion=%d altitude=%.3f vy=%.3f ticks=%d\n",
                (int)out.state, (int)out.motion, out.altitude, out.heightVelocity, sHost.fsmTicks);
        }
        // Entering the source Attack state starts the retail attack.bca clock.
        if (out.state == p2kurage::State::Attack && out.motionChanged && !sHost.attackPlaying) {
            if (pc_p2_kurage_arena_begin_attack()) { sHost.autoAdmissions = 0; sHost.fsmMotionTimer = 0; }
        }
        // The clock supplies KeyEvent 2/1 and the open suction interval.  The
        // retail Player advances in animation frames, not seconds.
        if (sHost.attackPlaying) pc_p2_kurage_arena_tick_attack(delta * kAttackFramesPerSecond);
        if (!valid(sHost.position)) { sHost.alive = false; return false; }
        updateHostCollision();
        // Source Attack suction: the ordinary Attack state autonomously admits
        // eligible Pikmin while its window is open.
        if ((out.isSucking || sHost.sucking)
            && pc_p2_kurage_receiver_scan_admit(0.0f, kSourceAttackRadius, kMaxAutoAdmissions, true) > 0)
            ++sHost.autoAdmissions;
        sHost.fsmTicks++;
        pc_p2_kurage_receiver_update(delta, true, sHost.ownerHasHealth, sHost.ownerBittered);
        return true;
    }
    sHost.phase += delta * 0.8f;
    if (!finite(sHost.phase)) { sHost.alive = false; return false; }
    sHost.position.y += std::sin(sHost.phase) * sHost.height * delta;
    if (!valid(sHost.position)) { sHost.alive = false; return false; }
    updateHostCollision();
    // Ordering contract: the Piki frame observes the previous receiver
    // velocity; this host tick refreshes the moving `suck` target and drives
    // the following frame.  Piki::doAI suppresses ordinary action writes for
    // receiver-owned Piki.  Health/bitter are intentionally unavailable on
    // this bounded host and remain the source-adapter defaults here.
    pc_p2_kurage_receiver_update(delta, true, true, false);
    return true;
}

bool pc_p2_kurage_arena_begin_attack()
{
    if (!sHost.ready || !sHost.alive || !sHost.attackPlayer.start(kAttackMotion)) return false;
    sHost.attackFrameSeam = false;
    sHost.attackFrame = 0.0f;
    sHost.attackPlaying = true;
    sHost.sucking = false;
    return true;
}

bool pc_p2_kurage_arena_tick_attack(float delta)
{
    if (!sHost.ready || !sHost.alive || !sHost.attackPlaying) return false;
    const auto result = sHost.attackPlayer.advance(delta, [](const p2retail::Event& event) {
        if (event.type == 2) {
            sHost.sucking = true;
            sHost.pendingKey = p2kurage::KeyEvent::Key2; // Kurage KEYEVENT_2 suck start
        } else if (event.type == 1 || event.type == 1000) {
            // Kurage StateAttack leaves its sucking interval on type 1.  This
            // bounded static-pose host closes there instead of re-looping 60..67.
            sHost.sucking = false;
            sHost.attackPlaying = false;
            sHost.attackPlayer.cancel();
            sHost.pendingKey = p2kurage::KeyEvent::Key1; // Kurage KEYEVENT_1 suck end
            sHost.fsmMotionFinished = true;
        }
    });
    if (result == p2retail::Update::Ok && sHost.attackPlayer.completed()) sHost.fsmMotionFinished = true;
    return result == p2retail::Update::Ok || result == p2retail::Update::Replaced;
}

Creature* pc_p2_kurage_arena_owner()
{
    return sHost.ready && sHost.alive ? &sHost.owner : nullptr;
}

CollPart* pc_p2_kurage_arena_mouth()
{
    return sHost.ready && sHost.alive ? &sHost.mouth : nullptr;
}

int pc_p2_kurage_arena_scan_admit(float verticalOffset, float attackRadius, int maxAdmissions, bool admitEligible)
{
    // Retail attack.bca events: 37=KEYEVENT_2 starts suck, 67=type1 ends it.
    if (!sHost.ready || !sHost.alive || !suckingActive()) return 0;
    return pc_p2_kurage_receiver_scan_admit(verticalOffset, attackRadius, maxAdmissions, admitEligible);
}
void pc_p2_kurage_arena_set_attack_frame(float frame)
{
    sHost.attackFrameSeam = true;
    sHost.attackFrame = finite(frame) && frame >= 0.0f ? frame : 0.0f;
}
bool pc_p2_kurage_arena_attack_pose_active()
{
    return sHost.ready && attackPoseActive();
}

void pc_p2_kurage_arena_fsm_enable(bool enable)
{
    if (sHost.ready && sHost.alive) sHost.fsmEnabled = enable;
}
bool pc_p2_kurage_arena_fsm_enabled()
{
    return sHost.ready && sHost.alive && sHost.fsmEnabled;
}
int pc_p2_kurage_arena_fsm_state()
{
    return sHost.ready ? (int)sHost.fsm.state() : -1;
}
float pc_p2_kurage_arena_fsm_altitude()
{
    return sHost.fsmAltitude;
}
void pc_p2_kurage_arena_set_owner_facts(bool hasHealth, bool bittered)
{
    sHost.ownerHasHealth = hasHealth;
    sHost.ownerBittered = bittered;
    sHost.fsmHealth = hasHealth ? kFsmLiveHealth : 0.0f;
}
int pc_p2_kurage_arena_auto_admissions()
{
    return sHost.autoAdmissions;
}

void pc_p2_kurage_arena_draw(Graphics& gfx)
{
    if (!sHost.ready || !sHost.alive || !sHost.shape || !gfx.mCamera) return;
    gfx.setPerspective(gfx.mCamera->mPerspectiveMatrix.mMtx, gfx.mCamera->mFov,
        gfx.mCamera->mAspectRatio, gfx.mCamera->mNear, gfx.mCamera->mFar, 1.f);
    gfx.useMaterial(nullptr);
    gfx.setDepth(true);
    Matrix4f world;
    world.makeIdentity();
    world.mMtx[0][3] = sHost.position.x;
    world.mMtx[1][3] = sHost.position.y;
    world.mMtx[2][3] = sHost.position.z;
    Matrix4f matrix;
    gfx.mCamera->mLookAtMtx.multiplyTo(world, matrix);
    Shape* shape = attackPoseActive() ? sHost.attackShape : sHost.shape;
    shape->updateAnim(gfx, matrix, nullptr, nullptr);
    shape->drawshape(gfx, *gfx.mCamera, nullptr);
    static bool logged = false;
    if (!logged) { std::printf("P2_KURAGE_DRAW position=%.2f,%.2f,%.2f\n", sHost.position.x, sHost.position.y, sHost.position.z); logged = true; }
}
