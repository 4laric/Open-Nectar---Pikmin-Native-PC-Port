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
    mMouthLocal[0] = mouthA;
    mMouthLocal[1] = mouthB;
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

void P2DemonHost::updateMouths()
{
    for (int i = 0; i < 2; ++i) {
        const Vector3f centre = mSRT.t + mMouthLocal[i];
        mMouths[i]->mCentre = centre;
        mMouths[i]->mJointMatrix.makeSRT(Vector3f(1, 1, 1), Vector3f(0, 0, 0), centre);
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
void P2DemonHost::sceneExit() { pc_demon_owner_lost(mOwnerToken); mOccupied = 0; mAttackActive = false; }
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