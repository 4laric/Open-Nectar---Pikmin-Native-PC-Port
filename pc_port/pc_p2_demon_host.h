#pragma once
#include "Creature.h"
#include "pc_p2_demon_attack_window.h"

class Graphics;
class Shape;
class Navi;
class CollPart;

// Private host for the staged P2 Demon model. It is deliberately not registered
// as a P1 teki or a replacement for Sarai's full FSM.
class P2DemonHost final : public Creature {
public:
    P2DemonHost();
    ~P2DemonHost() = default;

    bool load(const char* modelPath, const Vector3f& mouthA, const Vector3f& mouthB);
    void setPosition(const Vector3f& position);
    // Atomic pose update: source-local joint bases, before the native mouth twist.
    bool setMouthPose(const Matrix4f& mouthA, const Matrix4f& mouthB);
    bool beginAttack();
    bool updateAttack(Navi* target, float sourceFrame, bool floorContact);
    bool endAttack(Navi* target);
    bool forceDrop(Navi* target, float damage, float speed);
    void release(Navi* target);
    bool occupied() const;
    Vector3f mouthCentre(unsigned slot) const;
    void sceneExit();

    void refresh(Graphics&) override;
    void doKill() override;

private:
    Shape* mShape;
    CollPart* mMouths[2];
    Matrix4f mMouthLocal[2];
    P2DemonAttackWindow mWindow;
    bool mLoaded;
    bool mAttackActive;
    unsigned mOccupied;
    std::uint64_t mOwnerToken;
    void updateMouths();
};