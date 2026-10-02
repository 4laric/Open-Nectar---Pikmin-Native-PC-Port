#include "pc_p2_kochappy_stun.h"
#include "pc_p2_purple_impact_policy.h"
#include "TAI/Action.h"
#include "teki.h"

#include <cstdio>
#include <map>

namespace {
constexpr int PurpleImpactState = 16;
struct Runtime {
    p2purpleimpact::Lifetime lifetime = 0;
    float fitDuration = p2purpleimpact::RedFitDuration;
    p2purpleimpact::ReceiverState state;
};
std::map<BTeki*, Runtime> actors;
p2purpleimpact::Lifetime nextLifetime = 0;
}

void pc_p2_kochappy_stun_reset()
{
    actors.clear();
}

void pc_p2_kochappy_stun_register(BTeki* actor, float fitDuration)
{
    if (!actor) return;
    Runtime runtime;
    runtime.lifetime = ++nextLifetime;
    runtime.fitDuration = fitDuration > 0.0f ? fitDuration : p2purpleimpact::RedFitDuration;
    actors[actor] = runtime;
}

void pc_p2_kochappy_stun_forget(BTeki* actor)
{
    actors.erase(actor);
}

bool pc_p2_kochappy_stun_receive(BTeki* actor, const p2purpleimpact::Event& event, float bounceRoll)
{
    auto found = actors.find(actor);
    if (found == actors.end()) return false;
    const char* reason = nullptr;
    if (actor->mDeadState || !actor->isAlive()) reason = "dead";
    else if (!actor->mGroundTriangle) reason = "airborne";
    else if (actor->isFlying()) reason = "flying";
    else if (actor->getTekiOption(BTeki::TEKI_OPTION_INVINCIBLE)) reason = "invincible";
    else if (actor->mStateID < 4 || actor->mStateID == 13 || actor->mStateID == 14 || actor->mStateID > PurpleImpactState) reason = "state";
    if (reason) {
        std::printf("P2_PURPLE_QUAKE token=%llu target=%p accepted=0 reason=%s health=%.1f\n",
            static_cast<unsigned long long>(event.attackToken), static_cast<void*>(actor), reason, actor->mHealth);
        return false;
    }

    TaiStrategy* strategy = static_cast<TaiStrategy*>(actor->getStrategy());
    if (!strategy || !strategy->transit(*static_cast<Teki*>(actor), PurpleImpactState)) {
        std::printf("P2_PURPLE_QUAKE token=%llu target=%p accepted=0 reason=transition health=%.1f\n",
            static_cast<unsigned long long>(event.attackToken), static_cast<void*>(actor), actor->mHealth);
        return false;
    }
    Runtime& runtime = found->second;
    p2purpleimpact::receive(runtime.state, runtime.lifetime);
    actor->mVelocity.x = actor->mVelocity.z = 0.0f;
    actor->mTargetVelocity.x = actor->mTargetVelocity.z = 0.0f;
    actor->mVelocity.y = p2purpleimpact::bounceVelocity(1.0f, bounceRoll);
    actor->mTargetVelocity.y = actor->mVelocity.y;
    std::printf("P2_PURPLE_QUAKE token=%llu target=%p accepted=1 reason=red_dwarf health=%.1f vy=%.1f\n",
        static_cast<unsigned long long>(event.attackToken), static_cast<void*>(actor), actor->mHealth, actor->mVelocity.y);
    return true;
}

bool pc_p2_kochappy_stun_step(BTeki* actor, float deltaTime, float fitRoll)
{
    auto found = actors.find(actor);
    if (found == actors.end()) return true;
    auto& state = found->second.state;
    if (state.phase == p2purpleimpact::Phase::Bounce) {
        actor->mTargetVelocity.x = actor->mTargetVelocity.z = 0.0f;
        if (!p2purpleimpact::updateBounce(state, actor->mGroundTriangle != nullptr, fitRoll)) {
            return state.phase == p2purpleimpact::Phase::None;
        }
        actor->mVelocity.set(0.0f, 0.0f, 0.0f);
        actor->mTargetVelocity.set(0.0f, 0.0f, 0.0f);
    }
    if (state.phase == p2purpleimpact::Phase::Fit) {
        actor->mVelocity.set(0.0f, 0.0f, 0.0f);
        actor->mTargetVelocity.set(0.0f, 0.0f, 0.0f);
        return !p2purpleimpact::updateFit(state, deltaTime, false, found->second.fitDuration);
    }
    return true;
}

bool pc_p2_kochappy_stun_needs_fit_roll(const BTeki* actor)
{
    auto found = actors.find(const_cast<BTeki*>(actor));
    return found != actors.end()
        && found->second.state.phase == p2purpleimpact::Phase::Bounce
        && found->second.state.bounceUpdates >= 3
        && found->second.state.fitElapsed <= 0.0f
        && actor->mGroundTriangle;
}

void pc_p2_kochappy_stun_interrupt(BTeki* actor)
{
    auto found = actors.find(actor);
    if (found != actors.end()) found->second.state = {};
}

bool pc_p2_kochappy_stun_active(const BTeki* actor)
{
    auto found = actors.find(const_cast<BTeki*>(actor));
    return found != actors.end() && found->second.state.phase != p2purpleimpact::Phase::None;
}

#include "pc_midday_enemy.h"
namespace pc_midday {
bool enemy_stun_fields(BTeki& actor,ActorArchive& outer) {
 PrefixArchive ar(outer,"enemy.p2.purpleStun");auto it=actors.find(&actor);bool present=ar.mode()==Mode::Capture&&it!=actors.end();
 if(!ar.scalar("present",ScalarKind::Bool,&present))return false;if(!present){if(ar.mode()==Mode::Apply)actors.erase(&actor);return true;}
 Runtime value=ar.mode()==Mode::Capture?it->second:Runtime{};
 // Actor registration incarnations are remapped by the staged logical catalog;
 // never restore an old process's numeric allocation counter.
 if(!ar.token64("lifetime",RefKind::Creature,value.lifetime)||!ar.token64("targetLifetime",RefKind::Creature,value.state.targetLifetime))return false;
 int phase=ar.mode()==Mode::Capture?static_cast<int>(value.state.phase):0;
 if(!ar.scalar("phase",ScalarKind::S32,&phase)||phase<0||phase>2)return ar.fail("invalid Purple stun phase");value.state.phase=static_cast<p2purpleimpact::Phase>(phase);
 if(!ar.field("fitDuration",value.fitDuration)||!ar.field("fitElapsed",value.state.fitElapsed)||!ar.field("bounceUpdates",value.state.bounceUpdates))return false;
 if(ar.mode()==Mode::Apply){if(!value.lifetime||value.fitDuration<=0||value.state.fitElapsed<0)return ar.fail("invalid remapped Purple stun runtime");actors[&actor]=value;}
 return true;
}
}
