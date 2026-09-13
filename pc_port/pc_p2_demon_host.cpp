#include "pc_p2_demon_host.h"
#include "pc_p2_demon_bridge.h"
#include "Collision.h"
#include "Navi.h"
#include "Shape.h"
#include "Graphics.h"
#include "Texture.h"
#include "gameflow.h"
#include "sysNew.h"
#include <cmath>
#include <utility>

namespace { std::uint64_t nextHostToken = 0; }

P2DemonHost::P2DemonHost()
    : Creature(nullptr)
    , mShape(nullptr)
    , mMouths { new CollPart, new CollPart }
    , mLoaded(false)
    , mAttackActive(false)
    , mOccupied(0)
    , mOwnerToken(++nextHostToken)
{
    mStickListHead = nullptr;
}

bool P2DemonHost::load(const char* modelPath, const Vector3f& mouthA, const Vector3f& mouthB)
{
    if (!modelPath || !std::isfinite(mouthA.x) || !std::isfinite(mouthA.y) || !std::isfinite(mouthA.z)
        || !std::isfinite(mouthB.x) || !std::isfinite(mouthB.y) || !std::isfinite(mouthB.z))
        return false;
    const int previousHeap = gsys->setHeap(SYSHEAP_App);
    mShape = gameflow.loadShape(modelPath, true);
    gsys->setHeap(previousHeap);
    if (!mShape)
        return false;
    for (int i = 0; i < mShape->mTexAttrCount; ++i)
        if (mShape->mTexAttrList[i].mTexture) mShape->mTexAttrList[i].mTexture->attach();
    mMouthLocal[0].makeSRT(Vector3f(1, 1, 1), Vector3f(0, 0, 0), mouthA);
    mMouthLocal[1].makeSRT(Vector3f(1, 1, 1), Vector3f(0, 0, 0), mouthB);
    for (auto* mouth : mMouths) {
        mouth->mPartType = PART_BoundSphere;
        mouth->mRadius = 15.0f;
        mouth->mJointMatrix.makeIdentity();
    }
    mLoaded = true;
    updateMouths();
    return true;
}

void P2DemonHost::setPosition(const Vector3f& position)
{
    mSRT.t = position;
    updateMouths();
}

bool P2DemonHost::setMouthPose(const Matrix4f& mouthA, const Matrix4f& mouthB)
{
    if (!mLoaded) return false;
    const Matrix4f* poses[2] = { &mouthA, &mouthB };
    for (const Matrix4f* pose : poses) {
        for (int r = 0; r < 4; ++r)
            for (int c = 0; c < 4; ++c)
                if (!std::isfinite(pose->mMtx[r][c]) || std::fabs(pose->mMtx[r][c]) > 1.0e6f) return false;
        if (pose->mMtx[3][0] != 0 || pose->mMtx[3][1] != 0 || pose->mMtx[3][2] != 0 || pose->mMtx[3][3] != 1) return false;
    }
    mMouthLocal[0] = mouthA;
    mMouthLocal[1] = mouthB;
    updateMouths();
    return true;
}

void P2DemonHost::updateMouths()
{
    Matrix4f world;
    world.makeSRT(mSRT.s, mSRT.r, mSRT.t);
    for (int i = 0; i < 2; ++i) {
        world.multiplyTo(mMouthLocal[i], mMouths[i]->mJointMatrix);
        const Matrix4f& joint = mMouths[i]->mJointMatrix;
        mMouths[i]->mCentre.set(joint.mMtx[0][3], joint.mMtx[1][3], joint.mMtx[2][3]);
    }
}

bool P2DemonHost::beginAttack()
{
    if (!mLoaded || mAttackActive)
        return false;
    mWindow.reset();
    mOccupied = 0;
    mAttackActive = true;
    return true;
}

bool P2DemonHost::updateAttack(Navi* target, float sourceFrame, bool floorContact)
{
    if (!mAttackActive || !target)
        return false;
    updateMouths();
    const auto decision = mWindow.step(sourceFrame, true, floorContact);
    if (!decision.valid)
        return false;
    if (decision.attemptCapture && !mOccupied) {
        const Vector3f delta = target->mSRT.t - mMouths[0]->mCentre;
        if (delta.squaredLength() < 15.0f * 15.0f && pc_demon_capture(target, this, mMouths[0], mOwnerToken, 0))
            mOccupied = 1;
    }
    return mOccupied != 0;
}

bool P2DemonHost::endAttack(Navi* target)
{
    if (!mAttackActive)
        return false;
    if (mOccupied && (!target || !pc_demon_owned_by(target, this))) mOccupied = 0;
    const auto decision = P2DemonAttackWindow::eventDecision(target != nullptr, true,
        P2DemonAttackEvent::End, mOccupied);
    mAttackActive = false;
    return decision.next == P2DemonAttackNext::CatchFly && mOccupied != 0;
}

bool P2DemonHost::forceDrop(Navi* target, float damage, float speed)
{
    if (mOccupied && (!target || !pc_demon_owned_by(target, this)))
        mOccupied = 0;
    if (!mOccupied || !target)
        return false;
    const bool released = pc_demon_forced_release(target, damage, speed);
    if (released)
        mOccupied = 0;
    return released;
}
void P2DemonHost::release(Navi* target)
{
    if (target && pc_demon_owned_by(target, this))
        pc_demon_release(target);
    mOccupied = 0;
    mAttackActive = false;
}

bool P2DemonHost::occupied() const { return mOccupied != 0; }
Vector3f P2DemonHost::mouthCentre(unsigned slot) const { return slot < 2 ? mMouths[slot]->mCentre : Vector3f(0, 0, 0); }
void P2DemonHost::sceneExit() { mAttackPlayer.cancel(); pc_demon_owner_lost(mOwnerToken); mOccupied = 0; mAttackActive = false; }
void P2DemonHost::doKill() { sceneExit(); }

void P2DemonHost::refresh(Graphics& gfx)
{
    if (!mShape)
        return;
    Matrix4f world, view;
    world.makeSRT(mSRT.s, mSRT.r, mSRT.t);
    gfx.mCamera->mLookAtMtx.multiplyTo(world, view);
    mShape->updateAnim(gfx, view, nullptr, nullptr);
    mShape->drawshape(gfx, *gfx.mCamera, nullptr);
}
bool P2DemonHost::loadMouthPoses(const char* path) { return mPoseMeshes.empty() && mPoseBank.load(path); }
bool P2DemonHost::applyMouthFrame(int frame)
{
    const auto* pose = mPoseBank.exact(frame);
    if (!pose) return false;
    Matrix4f mouths[2];
    for (int slot = 0; slot < 2; ++slot) {
        mouths[slot].makeIdentity();
        for (int r = 0; r < 3; ++r)
            for (int c = 0; c < 4; ++c)
                mouths[slot].mMtx[r][c] = pose->values[slot * 12 + r * 4 + c];
    }
    return setMouthPose(mouths[0], mouths[1]);
}

// Load sampled meshes once in the App heap. Switching only changes pointers and
// the active bank, so a motion transition cannot allocate or mutate global state.
bool P2DemonHost::preloadPoseMeshes(const char* profile)
{
    if (!mLoaded || !profile || !*profile) return false;
    for (const auto& set : mPoseSets)
        if (set.profile == profile) return true;

    PoseSet set;
    set.profile = profile;
    if (!set.bank.load(profile)) return false;
    for (const auto& pose : set.bank.samples()) if (pose.model.empty()) return false;
    const int previousHeap = gsys->setHeap(SYSHEAP_App);
    for (const auto& pose : set.bank.samples()) {
        const std::string path = "courses/pikmin2room/" + pose.model;
        Shape* shape = gameflow.loadShape(path.c_str(), true);
        if (!shape) { gsys->setHeap(previousHeap); return false; }
        for (int i = 0; i < shape->mTexAttrCount; ++i)
            if (shape->mTexAttrList[i].mTexture) shape->mTexAttrList[i].mTexture->attach();
        set.meshes.push_back(shape);
    }
    gsys->setHeap(previousHeap);
    mPoseSets.push_back(std::move(set));
    if (mPoseMeshes.empty()) return switchPoseMeshes(profile);
    return true;
}

bool P2DemonHost::switchPoseMeshes(const char* profile)
{
    if (!mLoaded || !profile || !*profile) return false;
    for (const auto& set : mPoseSets) {
        if (set.profile != profile) continue;
        if (set.bank.samples().size() != set.meshes.size() || set.meshes.empty()) return false;
        mPoseBank = set.bank;
        mPoseMeshes = set.meshes;
        return true;
    }
    // Transitions must use a bank explicitly preloaded during setup; this keeps
    // the host clock free of allocations and makes missing assets observable.
    return false;
}

bool P2DemonHost::loadPoseMeshes(const char* profile)
{
    if (!mLoaded || !mPoseMeshes.empty()) return false;
    return preloadPoseMeshes(profile);
}
bool P2DemonHost::applyPoseFrame(int frame)
{
    const auto& samples = mPoseBank.samples();
    if (samples.size() != mPoseMeshes.size()) return false;
    for (unsigned i=0; i<samples.size(); ++i) {
        if (samples[i].frame != frame) continue;
        if (!applyMouthFrame(frame)) return false;
        mShape = mPoseMeshes[i];
        mRenderedFrame = frame;
        return true;
    }
    return false;
}

// The caller supplies animation-frame deltas from its simulation clock.
// Keep updates <=1 frame so the continuous capture window cannot be skipped.
bool P2DemonHost::beginTimedAttack(const p2retail::Motion& motion)
{
    if (motion.name != "attack1.bca" || mPoseMeshes.empty() || mAttackActive) return false;
    if (!mAttackPlayer.start(motion)) return false;
    if (!beginAttack()) { mAttackPlayer.cancel(); return false; }
    return true;
}

bool P2DemonHost::beginCatchFly(const p2retail::Motion& motion)
{
    if (!mOccupied || mClockMode != 0 || !mAttackPlayer.start(motion)) return false;
    mClockMode = 1;
    mClockFinished = false;
    return true;
}

P2DemonAttackDecision P2DemonHost::tickCatchFly(float delta, bool targetWithin25)
{
    P2DemonAttackDecision result;
    if (mClockMode != 1 || !std::isfinite(delta) || delta <= 0 || delta > 1) return result;
    if (!mClockFinished && (mAttackPlayer.frame() > 300.0f || targetWithin25)) {
        mAttackPlayer.finishMotion();
        mClockFinished = true;
    }
    bool ended = false;
    if (mAttackPlayer.advance(delta, [&](p2retail::Event event) { ended |= event.type == 1000; }) != p2retail::Update::Ok)
        return result;
    const auto& samples = mPoseBank.samples();
    const P2DemonMouthFrame* selected = nullptr;
    for (const auto& pose : samples) if (pose.frame <= mAttackPlayer.frame()) selected = &pose;
    if (!selected || !applyPoseFrame(selected->frame)) return result;
    result.valid = true;
    if (ended) { mClockMode = 2; result.next = P2DemonAttackNext::FallMeck; }
    return result;
}

bool P2DemonHost::beginFallMeck(const p2retail::Motion& motion)
{
    if (mClockMode != 2 || !mAttackPlayer.start(motion)) return false;
    mClockMode = 3;
    mClockReleased = false;
    return true;
}

P2DemonAttackDecision P2DemonHost::tickFallMeck(Navi* target, float delta, float damage, float speed)
{
    P2DemonAttackDecision result;
    if (mClockMode != 3 || !std::isfinite(delta) || delta <= 0 || delta > 1) return result;
    bool ended = false;
    if (mAttackPlayer.advance(delta, [&](p2retail::Event event) {
            if (event.type == 3 && !mClockReleased && mAttackPlayer.frame() == 20.0f)
                mClockReleased = forceDrop(target, damage, speed);
            if (event.type == 1000) ended = true;
        }) != p2retail::Update::Ok) return result;
    const auto& samples = mPoseBank.samples();
    const P2DemonMouthFrame* selected = nullptr;
    for (const auto& pose : samples) if (pose.frame <= mAttackPlayer.frame()) selected = &pose;
    if (!selected || !applyPoseFrame(selected->frame)) return result;
    result.valid = true;
    if (ended) { mClockMode = 4; result.next = P2DemonAttackNext::Move; }
    return result;
}

P2DemonAttackDecision P2DemonHost::tickTimedAttack(Navi* target, float delta, bool floorContact)
{
    P2DemonAttackDecision result;
    if (!mAttackActive || !std::isfinite(delta) || delta <= 0 || delta > 1) return result;
    std::vector<p2retail::Event> events;
    if (mAttackPlayer.advance(delta, [&](p2retail::Event event){ events.push_back(event); }) != p2retail::Update::Ok) return result;
    const float frame = mAttackPlayer.frame();
    const P2DemonMouthFrame* selected = nullptr;
    for (const auto& pose : mPoseBank.samples()) if (pose.frame <= frame) selected = &pose;
    if (!selected || !applyPoseFrame(selected->frame)) return result;
    if (mOccupied && (!target || !pc_demon_owned_by(target, this))) mOccupied = 0;
    updateAttack(target, frame, floorContact);
    result.valid = true;
    if (!target) result.next = P2DemonAttackNext::Move;
    for (const auto& event : events) {
        P2DemonAttackEvent mapped = P2DemonAttackEvent::None;
        if (event.type == 2) mapped = P2DemonAttackEvent::Dash;
        else if (event.type == 3) mapped = P2DemonAttackEvent::Interruptible;
        else if (event.type == 4) mapped = P2DemonAttackEvent::CaptureCheck;
        else if (event.type == 1000) mapped = P2DemonAttackEvent::End;
        const auto decision = P2DemonAttackWindow::eventDecision(target != nullptr, true, mapped, mOccupied);
        result.dash |= decision.dash;
        result.clearNoInterrupt |= decision.clearNoInterrupt;
        if (decision.next != P2DemonAttackNext::None) result.next = decision.next;
    }
    if (result.next != P2DemonAttackNext::None) { mAttackActive = false; mAttackPlayer.cancel(); }
    return result;
}
