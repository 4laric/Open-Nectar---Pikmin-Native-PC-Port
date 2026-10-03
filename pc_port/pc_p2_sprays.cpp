#include "pc_p2_sprays.h"
#include "pc_p2_original_resource_state.h"
#include "PikiState.h"
#include "Piki.h"
#include "Navi.h"
#include "NaviState.h"
#include "CPlate.h"
#include "Controller.h"
#include "Kontroller.h"
#include "Interface.h"
#include "MoviePlayer.h"
#include "PlayerState.h"
#include "gameflow.h"
#include "sysNew.h"
#include "timing/pc_render_phase.h"
#include <cstdio>
#include <string>
namespace {
p2originalresource::ResourceState* inventory = nullptr;
bool gameplay() {
    return gsys && playerState && !playerState->mInDayEnd
        && gameflow.mMoviePlayer && !gameflow.mMoviePlayer->mIsActive
        && !gameflow.mPauseAll && !gameflow.mIsUIOverlayActive;
}
class DopeState final : public PikiState {
    float delay = 0;
    bool started = false;
public:
    DopeState() : PikiState(PIKISTATE_P2Dope, "P2_SPICY") {}
    void init(Piki* p) override {
        delay = 0.3f * gsys->getRand(1.0f); started = false;
        p->mTargetVelocity.set(0,0,0);
    }
    void exec(Piki* p) override {
        p->mTargetVelocity.set(0,0,0); p->mVelocity.x = p->mVelocity.z = 0;
        if (!started) {
            delay -= gsys->getFrameTime();
            if (delay <= 0) {
                // Source draws twice even though both choices are GROWUP1.
                (void)gsys->getRand(1.0f);
                started = true;
                p->startMotion(PaniMotionInfo(PIKIANIM_GrowUp1,p),PaniMotionInfo(PIKIANIM_GrowUp1));
            }
        } else if (p->getUpperMotionIndex() != PIKIANIM_GrowUp1) {
            transit(p,PIKISTATE_Normal);
        }
    }
    void procAnimMsg(Piki* p, MsgAnim* msg) override {
        // Host GROWUP1 growth action maps to source GROWUP1 KEYEVENT_2.
        // This uses the actual native animation callback, not a timer commit.
        if (msg->mKeyEvent->mEventType == KEY_Action0) {
            p->mP2Spicy.begin();
            std::printf("P2_SPICY_BEGIN piki=%p duration=40 maturity=%d\n",static_cast<void*>(p),p->mHappa);
        } else if (msg->mKeyEvent->mEventType == KEY_Finished) transit(p,PIKISTATE_Normal);
    }
};
}
PikiState* pc_p2_spicy_state() { return new DopeState; }
void pc_p2_sprays_bind(p2originalresource::ResourceState* state) { inventory = state; }
bool pc_p2_spicy_active(const Piki* p) { return p && p->mP2Spicy.active(); }
void pc_p2_spicy_tick(Piki* p) {
    if (!pc_render_is_authoritative()) return;
    if (!p->isAlive()) { p->mP2Spicy.clear(); return; }
    if (p->mP2Spicy.tick(gsys->getFrameTime(),gameplay()))
        std::printf("P2_SPICY_END piki=%p maturity=%d\n",static_cast<void*>(p),p->mHappa);
}
bool pc_p2_spicy_accept(Piki* p) {
    if (!p || !p->isAlive()) return false;
    if (p->mP2Spicy.active()) { p->mP2Spicy.begin(); return false; }
    // Source only PikiWalkState::dopable; host Normal is its counterpart.
    if (p->getState() != PIKISTATE_Normal) return false;
    p->mFSM->transit(p,PIKISTATE_P2Dope); return true;
}
bool pc_p2_spicy_use(Navi* n) {
    if (!inventory || !inventory->ready() || !gameplay() || !n || !n->isAlive()
        || !n->getCurrState() || n->getCurrState()->getID() != NAVISTATE_Walk || !n->mPlateMgr) return false;
    std::string error;
    if (!inventory->useSpray(p2originalresource::HoneyKind::Spicy,error)) return false;
    int accepted = 0;
    Iterator squad(n->mPlateMgr);
    CI_LOOP(squad) {
        Creature* c = *squad;
        if (c && c->isPiki() && pc_p2_spicy_accept(static_cast<Piki*>(c))) ++accepted;
    }
    std::printf("P2_SPICY_USE captain=%d stock=%d accepted=%d\n",n->getNaviIndex(),inventory->sprayCount(p2originalresource::HoneyKind::Spicy),accepted);
    return true;
}
bool pc_p2_sprays_input(Navi* n) {
    return n && n->mKontroller && n->mKontroller->keyClick(KBBTN_DPAD_UP) && pc_p2_spicy_use(n);
}
