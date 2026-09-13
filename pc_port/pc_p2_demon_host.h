#pragma once
#include "Creature.h"
#include "pc_p2_demon_attack_window.h"
#include "pc_p2_demon_pose_bank.h"
#include "pc_p2_retail_player.h"
#include "pc_p2_demon_catchfly_policy.h"

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
    bool loadMouthPoses(const char* path);
    bool applyMouthFrame(int frame);
    bool loadPoseMeshes(const char* profile);
    bool preloadPoseMeshes(const char* profile);
    bool switchPoseMeshes(const char* profile);
    bool applyPoseFrame(int frame);
    int renderedPoseFrame() const { return mRenderedFrame; }
    bool beginAttack();
    bool beginTimedAttack(const p2retail::Motion& motion);
    P2DemonAttackDecision tickTimedAttack(Navi*, float sourceFrames, bool floorContact);
    bool beginCatchFly(const p2retail::Motion& motion);
    P2DemonAttackDecision tickCatchFly(Navi* target, float sourceFrames, p2demon::CatchFlyInput input);
    bool beginFallMeck(const p2retail::Motion& motion);
    P2DemonAttackDecision tickFallMeck(Navi* target, float sourceFrames, float damage, float speed);
    bool updateAttack(Navi* target, float sourceFrame, bool floorContact);
    bool endAttack(Navi* target);
    bool forceDrop(Navi* target, float damage, float speed);
    void release(Navi* target);
    bool occupied() const;
    Vector3f mouthCentre(unsigned slot) const;
    void sceneExit();

    void refresh(Graphics&) override;
    void update() override;
    void doKill() override;

private:
    struct PoseSet {
        std::string profile;
        P2DemonPoseBank bank;
        std::vector<Shape*> meshes;
    };
    Shape* mShape;
    CollPart* mMouths[2];
    Matrix4f mMouthLocal[2];
    P2DemonAttackWindow mWindow;
    p2retail::Player mAttackPlayer;
    P2DemonPoseBank mPoseBank;
    std::vector<Shape*> mPoseMeshes;
    std::vector<PoseSet> mPoseSets;
    int mRenderedFrame = -1;
    bool mLoaded;
    bool mAttackActive;
    int mClockMode = 0;
    float mCatchElapsedFrames = 0.0f;
    bool mClockFinished = false;
    bool mClockReleased = false;
    unsigned mOccupied;
    std::uint64_t mOwnerToken;
    void updateMouths();
};
