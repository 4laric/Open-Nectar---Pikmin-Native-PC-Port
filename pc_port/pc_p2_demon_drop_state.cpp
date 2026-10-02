#include "pc_p2_demon_drop_state.h"
#include "pc_p2_demon_admission.h"
#include "NaviState.h"
#include "NaviMgr.h"
#include "Interactions.h"
#include "PaniPikiAnimator.h"
#include "system.h"
#include "RumbleMgr.h"
#include <deque>
#include <cstdio>
#include <limits>
namespace {
class DropState;
unsigned gSilentDamage=0; // >0 while the deferred KokeDamage HP deduction runs (no second hit feedback).
struct Listener final : PaniAnimKeyListener {
    DropState* state; Navi* captain; std::uint64_t generation,serial;
    Listener(DropState* s,Navi* n,std::uint64_t g,std::uint64_t a):state(s),captain(n),generation(g),serial(a) {}
    void animationKeyUpdated(immut PaniAnimKeyEvent&) override;
};
class DropState final : public NaviState {
public:
    P2DemonDropPolicy policy;
    Navi* captain=nullptr;
    std::uint64_t generation=0,serial=0,dispatch=0;
    bool delivering=false, retired=false;
    unsigned frame=0; // frames since begin(), for impact/damage timing evidence
    int expected=-1;
    std::deque<Listener> listeners; // Stable issuance tokens; retained until state destruction.
    DropState():NaviState(NAVISTATE_DemonDrop) {}
    bool owns(Navi* n) const { return n&&n==captain&&n->getCurrState()==this; }
    void zero(Navi* n) { n->mVelocity.set(0,0,0); n->mTargetVelocity.set(0,0,0); n->mVolatileVelocity.set(0,0,0); }
    void motion(Navi* n,int id) {
        if(listeners.size()>=4096 || serial==std::numeric_limits<std::uint64_t>::max()) {
            zero(n); n->mStateMachine->transit(n,NAVISTATE_Walk); return;
        }
        expected=id; ++serial;
        listeners.emplace_back(this,n,generation,serial);
        n->startMotion(PaniMotionInfo(id,&listeners.back()),PaniMotionInfo(id));
    }
    void init(Navi* n) override {
        // Registration exposes an ID, but only admitted payloads may own it.
        if(captain!=n||policy.phase()!=P2DemonDropPhase::Falling) {
            policy.cancel(); captain=nullptr; expected=-1; dispatch=0;
            n->mStateMachine->transit(n,NAVISTATE_Walk);
        }
    }
    void cleanup(Navi*) override { policy.cancel(); captain=nullptr; expected=-1; dispatch=0; }
    void resume(Navi* n) override {
        std::printf("DEMON_STATE_RESUME owned_delivery=%d phase=%d\n",int(delivering),int(policy.phase()));
        if(owns(n)&&!delivering) { zero(n); n->mStateMachine->transit(n,NAVISTATE_Walk); }
    }
    void restart(Navi*) override { std::printf("DEMON_STATE_RESTART phase=%d\n",int(policy.phase())); }
    void exec(Navi* n) override {
        if(!owns(n)) return;
        ++frame;
        if(n->mHealth<=1) { n->mStateMachine->transit(n,NAVISTATE_Dead); return; }
        if(policy.phase()!=P2DemonDropPhase::Falling) zero(n);
        auto c=policy.tick(generation,gsys->getFrameTime());
        if(c.startGetUp) motion(n,PIKIANIM_GetUp);
    }
    void procBounceMsg(Navi* n,MsgBounce*) override {
        if(!owns(n)) return;
        auto c=policy.bounce(generation); if(!c.accepted) return;
        std::printf("DEMON_STATE_BOUNCE generation=%llu health=%.3f frame=%u\n",static_cast<unsigned long long>(generation),n->mHealth,frame);
        zero(n);
        if(c.impactZeroDamage) {
            // P2 NaviFallMeckState::bounceCallback -> addDamage(0, true): the hit
            // feedback (SE, camera shake, rumble, damage effect) fires on the
            // ground-impact frame; the HP comes off when JKOKE ends.
            n->startDamageEffect();
            rumbleMgr->start(RUMBLE_Unk1,n->mNaviID,nullptr);
            std::printf("DEMON_STATE_IMPACT_FEEDBACK generation=%llu frame=%u\n",static_cast<unsigned long long>(generation),frame);
        }
        if(c.startKnockdown) motion(n,PIKIANIM_JKoke);
        if(c.resume) n->mStateMachine->transit(n,NAVISTATE_Walk);
    }
    void procAnimMsg(Navi* n,MsgAnim* msg) override {
        if(!owns(n)||!dispatch||dispatch!=serial||msg->mKeyEvent->mEventType!=KEY_Finished||
           n->mNaviAnimMgr.getLowerAnimator().getCurrentMotionIndex()!=expected) return;
        const auto phase=policy.phase();
        auto c=policy.animationEnd(generation,phase);
        if(c.deliverDamage) {
            const auto saved=generation;
            struct Guard { bool& flag; Guard(bool& f):flag(f){flag=true;++gSilentDamage;} ~Guard(){flag=false;--gSilentDamage;} } guard(delivering);
            const float hp=n->mHealth;
            InteractAttack attack(n,nullptr,c.damage,false);
            const bool accepted=n->stimulate(attack);
            std::printf("DEMON_STATE_DAMAGE generation=%llu accepted=%d before=%.3f after=%.3f frame=%u\n",static_cast<unsigned long long>(saved),int(accepted),hp,n->mHealth,frame);
            if(!owns(n)||generation!=saved) return;
            if(n->mHealth<=1) return; // Outer native finishDamage owns the lethal transition.
        }
        if(c.resume && owns(n)) { zero(n); n->mStateMachine->transit(n,NAVISTATE_Walk); }
    }
};
void Listener::animationKeyUpdated(immut PaniAnimKeyEvent& event) {
    // Validate token before dereferencing the captain. State lifetime still host-owned.
    if(state->captain!=captain||state->generation!=generation||state->serial!=serial||!state->owns(captain)) return;
    const auto prior=state->dispatch; state->dispatch=serial;
    captain->animationKeyUpdated(event); // Preserve native finishDamage/restart dispatch.
    state->dispatch=prior;
}
DropState* registered(Navi* n) {
    if(!n||!n->mStateMachine) return nullptr;
    auto* m=n->mStateMachine;
    if(m->mStateLimit<=NAVISTATE_DemonDrop) return nullptr;
    const int index=m->mStateIndexes[NAVISTATE_DemonDrop];
    if(index<0||index>=m->mStateCount) return nullptr;
    return dynamic_cast<DropState*>(m->mStates[index]);
}
}
bool pc_demon_drop_silent_damage() { return gSilentDamage>0; }
NaviState* pc_demon_drop_state_create() { return new DropState(); }
bool pc_demon_drop_begin(Navi* n,std::uint64_t g,float damage,float speed) {
    auto* s=registered(n);
    if(!s||s->retired||!n->isAlive()||n->mHealth<=1||!pc_demon_captain_admission_eligible(n)||n->mRope||n->isStickTo()||
       damage<0||!std::isfinite(n->mSRT.t.x)||!std::isfinite(n->mSRT.t.y)||!std::isfinite(n->mSRT.t.z)||
       n->isCreatureFlag(CF_DisableMovement|CF_IgnoreGravity|CF_IsFlying)||s->listeners.size()>4093) return false;
    auto c=s->policy.begin(g,damage,speed); if(!c.accepted) return false;
    s->captain=n; s->generation=g; s->frame=0;
    n->mStateMachine->transit(n,NAVISTATE_DemonDrop);
    if(!s->owns(n)) { s->policy.cancel(); s->captain=nullptr; return false; }
    n->mGroundTriangle=nullptr; n->mPreviousTriangle=nullptr; n->mCollPlatform=nullptr;
    n->resetCreatureFlag(CF_IsOnGround|CF_IsPositionFixed); n->mFixedPosition=n->mSRT.t;
    n->mVelocity.y=c.actualY; n->mTargetVelocity.set(0,c.targetY,0); n->mVolatileVelocity.set(0,0,0);
    s->motion(n,PIKIANIM_Fall);
    std::printf("DEMON_STATE_BEGIN generation=%llu actual_y=%.3f target_y=%.3f\n",static_cast<unsigned long long>(g),n->mVelocity.y,n->mTargetVelocity.y);
    return s->owns(n);
}
void pc_demon_drop_post_physics(Navi* n) {
    auto* s=registered(n);
    if(s&&s->owns(n)&&s->policy.phase()!=P2DemonDropPhase::Falling) s->zero(n);
}
void pc_demon_drop_reset(Navi* n) {
    auto* s=registered(n); if(!s||!s->owns(n)) return;
    s->zero(n); s->policy.cancel(); s->captain=nullptr; s->expected=-1;
    n->mGroundTriangle=nullptr; n->mPreviousTriangle=nullptr; n->mCollPlatform=nullptr;
    n->resetCreatureFlag(CF_IsOnGround|CF_IsPositionFixed); n->mFixedPosition=n->mSRT.t;
}
P2DemonDropPhase pc_demon_drop_phase(Navi* n) {
    auto* s=registered(n); return s&&s->owns(n)?s->policy.phase():P2DemonDropPhase::Idle;
}

void pc_demon_drop_before_transition(Navi* n,int next) {
    auto* s=registered(n); if(!s||!s->owns(n)) return;
    // Audited P1 receiver contracts: these targets do not accept a caller-supplied
    // vector impulse. Flick intensity and Geyzer destination remain untouched;
    // their own init/exec constructs the new motion after this old drop is quenched.
    switch(next) {
    case NAVISTATE_Walk: case NAVISTATE_Dead: case NAVISTATE_Flick:
    case NAVISTATE_Geyzer: case NAVISTATE_Bury: case NAVISTATE_Pressed:
    case NAVISTATE_DemonDrop:
        s->zero(n);
        std::printf("DEMON_STATE_HANDOFF next=%d quenched=1\n",next);
        break;
    default:
        // Unknown payload/impulse semantics: leave incoming values intact.
        std::printf("DEMON_STATE_HANDOFF next=%d quenched=0 unaudited=1\n",next);
        break;
    }
}

void pc_demon_drop_scene_exit() {
    if(!naviMgr) return;
    unsigned count=0;
    Iterator it(naviMgr);
    for(it.first();!it.isDone();it.next()) {
        auto* n=static_cast<Navi*>(*it);
        auto* s=registered(n); if(!s) continue;
        s->policy.cancel(); s->captain=nullptr; s->expected=-1; s->dispatch=0;
        s->retired=true; ++count;
    }
    std::printf("DEMON_SCENE_REVOKE states=%u before_heap_disposal=1\n",count);
}
// Typed checkpoint access lives beside the private state and issuance tokens.
#include "pc_midday_actor_states.h"
namespace pc_midday {
bool navi_state_subobjects(Navi& n, ActorArchive& a) {
    auto* s=registered(&n);
    if (!s) return a.fail("DemonDrop state missing");
    u32 count=a.mode()==Mode::Capture ? static_cast<u32>(s->listeners.size()) : 0;
    if (!a.scalar("demon.listeners.count",ScalarKind::U32,&count)) return false;
    if (count>4096) return a.fail("Demon listener count exceeds bound");
    if (a.mode()==Mode::Apply) {
        if (!s->listeners.empty()) return a.fail("listener allocation requires fresh actor stage");
        for (u32 i=0;i<count;++i) s->listeners.emplace_back(s,nullptr,0,0);
    }
    return true;
}
bool navi_listener_index(Navi& n, const PaniAnimKeyListener* listener, u32& index) {
    auto* s=registered(&n); if (!s || !listener) return false;
    for (u32 i=0;i<s->listeners.size();++i)
        if (&s->listeners[i]==listener) { index=i; return true; }
    return false;
}
PaniAnimKeyListener* navi_listener_at(Navi& n, u32 index) {
    auto* s=registered(&n);
    return s && index<s->listeners.size() ? &s->listeners[index] : nullptr;
}
bool demon_drop_state(Navi& n, ActorArchive& a) {
    auto* s=registered(&n); if (!s) return a.fail("DemonDrop state missing");
    // These members are initialized at construction and retained across states.
    auto policy=s->policy.captureState();
    int phase=a.mode()==Mode::Capture ? static_cast<int>(policy.phase) : 0;
    if (!a.scalar("policy.phase",ScalarKind::S32,&phase) || phase<0 || phase>4)
        return a.fail("invalid DemonDrop phase");
    if (!a.scalar("policy.generation",ScalarKind::U64,&policy.generation) ||
        !a.scalar("policy.pendingDamage",ScalarKind::F32,&policy.pendingDamage) ||
        !a.scalar("policy.recovery",ScalarKind::F32,&policy.recovery)) return false;
    policy.phase=static_cast<P2DemonDropPhase>(phase);
    P2DemonDropPolicy checked;
    if (!checked.restoreState(policy)) return a.fail("invalid DemonDrop policy");
    if (!a.ref("captain",RefKind::Creature,s->captain) ||
        !a.field("generation",s->generation) || !a.field("serial",s->serial) ||
        !a.field("retired",s->retired) || !a.field("frame",s->frame) ||
        !a.field("expected",s->expected)) return false;
    // Reentrant callbacks cannot be checkpointed at a completed engine tick.
    if (a.mode()==Mode::Capture && (s->delivering || s->dispatch || gSilentDamage))
        return a.fail("Demon callback in progress");
    u32 count=a.mode()==Mode::Capture ? static_cast<u32>(s->listeners.size()) : 0;
    if (!a.scalar("listeners.count",ScalarKind::U32,&count) || count>4096)
        return a.fail("invalid Demon token count");
    if (a.mode()!=Mode::Capture && s->listeners.size()!=count)
        return a.fail("Demon listener allocation does not match checkpoint");
    for (u32 i=0;i<count;++i) {
        PrefixArchive token(a,(std::string("listeners.")+std::to_string(i)).c_str());
        auto& t=s->listeners[i];
        if (!token.ref("captain",RefKind::Creature,t.captain) ||
            !token.field("generation",t.generation) || !token.field("serial",t.serial)) return false;
    }
    if (a.mode()==Mode::Apply) { s->policy=checked; s->dispatch=0; s->delivering=false; }
    return true;
}
}
