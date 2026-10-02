#if defined(PIKI_PC_PORT)
#include "pc_midday_state_factory.h"
#include "pc_midday_allocation_owner.h"
#include "Navi.h"
#include "NaviState.h"
#include "Piki.h"
#include "PikiState.h"
#include <algorithm>
#include <exception>
#include <new>
#include <stdexcept>

NaviDemoSunsetState::NaviDemoSunsetState(MiddayRestoreTag)
    : NaviState(NAVISTATE_DemoSunset), mStateMachine(nullptr), mCurrentState(nullptr) {}
struct PcMiddayStateFactoryAccess {
    static NaviDemoSunsetState* sunset(pc_midday::AllocationOwner& owner) {
        return owner.construct<NaviDemoSunsetState>([](void* storage) {
            return new(storage) NaviDemoSunsetState(NaviDemoSunsetState::MiddayRestoreTag{});
        });
    }
};
namespace pc_midday {
namespace {
static_assert(NAVISTATE_Count == 38, "review changed Navi topology");
static_assert(PIKISTATE_Count == 37, "review changed Piki topology");
template<class Actor> void arrays(StateMachine<Actor>& machine, int capacity, AllocationOwner& owner) {
    machine.mStates = owner.array<AState<Actor>*>(capacity);
    machine.mStateIDs = owner.array<int>(capacity);
    machine.mStateIndexes = owner.array<int>(capacity);
    std::fill_n(machine.mStates, capacity, nullptr);
    std::fill_n(machine.mStateIDs, capacity, -1);
    std::fill_n(machine.mStateIndexes, capacity, -1);
    machine.mStateCount = 0; machine.mStateLimit = capacity; machine.mLastStateID = -1;
}
template<class Actor> void add(StateMachine<Actor>& machine, AState<Actor>* state) {
    const int id = state->getID();
    if(id<0 || id>=machine.mStateLimit || machine.mStateCount>=machine.mStateLimit || machine.mStateIndexes[id]!=-1)
        throw std::logic_error("invalid compiled state registration");
    const int index=machine.mStateCount++;
    machine.mStates[index]=state; machine.mStateIDs[index]=id; machine.mStateIndexes[id]=index;
    state->setMachine(&machine);
}
NaviDemoSunsetState* sunset(AllocationOwner& owner) {
    auto* state=PcMiddayStateFactoryAccess::sunset(owner);
    auto* machine=owner.make<NaviDemoSunsetState::DemoStateMachine>();
    arrays<NaviDemoSunsetState>(*machine,36,owner); // Native topology is 36 slots, five registrations.
    add<NaviDemoSunsetState>(*machine,owner.make<NaviDemoSunsetState::GoState>());
    add<NaviDemoSunsetState>(*machine,owner.make<NaviDemoSunsetState::LookState>());
    add<NaviDemoSunsetState>(*machine,owner.make<NaviDemoSunsetState::WhistleState>());
    add<NaviDemoSunsetState>(*machine,owner.make<NaviDemoSunsetState::WaitState>());
    add<NaviDemoSunsetState>(*machine,owner.make<NaviDemoSunsetState::SitState>());
    state->mStateMachine=machine;
    return state;
}
}
bool allocate_navi_state_graph(Navi& actor, AllocationOwner& owner, std::string& error) {
    if(actor.mStateMachine || actor.mCurrState) { error="Navi state graph requires fresh inert root"; return false; }
    try {
        auto* machine=owner.make<NaviStateMachine>();
        arrays<Navi>(*machine,NAVISTATE_Count,owner);
        add<Navi>(*machine,allocate_demon_drop_state(owner));
        add<Navi>(*machine,allocate_demon_escape_state(owner));
        add<Navi>(*machine,owner.make<NaviWalkState>());
        add<Navi>(*machine,owner.make<NaviStuckState>());
        add<Navi>(*machine,owner.make<NaviFlickState>());
        add<Navi>(*machine,owner.make<NaviGeyzerState>());
        add<Navi>(*machine,owner.make<NaviContainerState>());
        add<Navi>(*machine,owner.make<NaviUfoState>());
        add<Navi>(*machine,owner.make<NaviPartsAccessState>());
        add<Navi>(*machine,owner.make<NaviUfoAccessState>());
        add<Navi>(*machine,owner.make<NaviPikiZeroState>());
        add<Navi>(*machine,owner.make<NaviPickState>());
        add<Navi>(*machine,owner.make<NaviRopeState>());
        add<Navi>(*machine,owner.make<NaviRopeExitState>());
        add<Navi>(*machine,owner.make<NaviGatherState>());
        add<Navi>(*machine,owner.make<NaviReleaseState>());
        add<Navi>(*machine,owner.make<NaviThrowWaitState>());
        add<Navi>(*machine,owner.make<NaviThrowState>());
        add<Navi>(*machine,owner.make<NaviFunbariState>());
        add<Navi>(*machine,owner.make<NaviPushState>());
        add<Navi>(*machine,owner.make<NaviPushPikiState>());
        add<Navi>(*machine,owner.make<NaviBuryState>());
        add<Navi>(*machine,owner.make<NaviNukuState>());
        add<Navi>(*machine,owner.make<NaviNukuAdjustState>());
        add<Navi>(*machine,owner.make<NaviWaterState>());
        add<Navi>(*machine,owner.make<NaviSowState>());
        add<Navi>(*machine,owner.make<NaviPressedState>());
        add<Navi>(*machine,owner.make<NaviClearState>());
        add<Navi>(*machine,owner.make<NaviLockState>());
        add<Navi>(*machine,owner.make<NaviAttackState>());
        add<Navi>(*machine,owner.make<NaviIroIroState>());
        add<Navi>(*machine,owner.make<NaviDeadState>());
        add<Navi>(*machine,owner.make<NaviIdleState>());
        add<Navi>(*machine,owner.make<NaviStartingState>());
        add<Navi>(*machine,owner.make<NaviDemoWaitState>());
        add<Navi>(*machine,owner.make<NaviDemoInfState>());
        add<Navi>(*machine,owner.make<NaviPelletState>());
        add<Navi>(*machine,sunset(owner));
        if(machine->mStateCount!=38) throw std::logic_error("Navi state inventory changed");
        actor.mStateMachine=machine; error.clear(); return true;
    } catch(const std::exception& exception) { error=exception.what(); return false; }
}
bool allocate_piki_state_graph(Piki& actor, AllocationOwner& owner, std::string& error) {
    if(actor.mFSM || actor.mCurrentState) { error="Piki state graph requires fresh inert root"; return false; }
    try {
        auto* machine=owner.make<PikiStateMachine>();
        arrays<Piki>(*machine,PIKISTATE_Count,owner);
        add<Piki>(*machine,owner.make<PikiNormalState>());
        add<Piki>(*machine,owner.make<PikiFlickState>());
        add<Piki>(*machine,owner.make<PikiFlownState>());
        add<Piki>(*machine,owner.make<PikiEmitState>());
        add<Piki>(*machine,owner.make<PikiFallState>());
        add<Piki>(*machine,owner.make<PikiCliffState>());
        add<Piki>(*machine,owner.make<PikiLookAtState>());
        add<Piki>(*machine,owner.make<PikiBulletState>());
        add<Piki>(*machine,owner.make<PikiBubbleState>());
        add<Piki>(*machine,owner.make<PikiFiredState>());
        add<Piki>(*machine,owner.make<PikiSwallowedState>());
        add<Piki>(*machine,owner.make<PikiHangedState>());
        add<Piki>(*machine,owner.make<PikiWaterHangedState>());
        add<Piki>(*machine,owner.make<PikiGoHangState>());
        add<Piki>(*machine,owner.make<PikiFlyingState>());
        add<Piki>(*machine,owner.make<PikiGrowState>());
        add<Piki>(*machine,owner.make<PikiGrowupState>());
        add<Piki>(*machine,owner.make<PikiBuryState>());
        add<Piki>(*machine,owner.make<PikiWaveState>());
        add<Piki>(*machine,owner.make<PikiPushState>());
        add<Piki>(*machine,owner.make<PikiPushPikiState>());
        add<Piki>(*machine,owner.make<PikiNukareState>());
        add<Piki>(*machine,owner.make<PikiNukareWaitState>());
        add<Piki>(*machine,owner.make<PikiAutoNukiState>());
        add<Piki>(*machine,owner.make<PikiPressedState>());
        add<Piki>(*machine,owner.make<PikiAbsorbState>());
        add<Piki>(*machine,owner.make<PikiDyingState>());
        add<Piki>(*machine,owner.make<PikiDeadState>());
        add<Piki>(*machine,owner.make<PikiDenkiDyingState>());
        add<Piki>(*machine,owner.make<PikiPanicState>());
        add<Piki>(*machine,owner.make<PikiKinokoState>());
        add<Piki>(*machine,owner.make<PikiDrownState>());
        add<Piki>(*machine,owner.make<PikiEmotionState>());
        add<Piki>(*machine,owner.make<PikiKinokoChangeState>());
        add<Piki>(*machine,owner.make<PikiFallMeckState>());
        if(machine->mStateCount!=35 || machine->mStateIndexes[32]!=-1 || machine->mStateIndexes[34]!=-1)
            throw std::logic_error("Piki state inventory changed");
        actor.mFSM=machine; error.clear(); return true;
    } catch(const std::exception& exception) { error=exception.what(); return false; }
}
}
#endif
