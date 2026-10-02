#pragma once
#include "pc_midday_actor_graph.h"
#include "pc_midday_constructor.h"
#include "pc_midday_piki_action_factory.h"
#include "Navi.h"
#include "NaviState.h"
#include "ViewPiki.h"
#include "PikiState.h"
#include "PikiAI.h"
#include "sysNew.h"
#include <set>
#include <algorithm>
#include <typeinfo>
#include <iostream>
#include <limits>
#include <cstdio>
// Actual-engine allocation checks. Caller supplies captured live payloads and
// a held physical fence, prevents ticks, and independently snapshots managers
// and globals. This does not bind a saved world or certify reusable pool slots.
namespace pc_midday_graph_checks {
inline bool fail(std::string& e,const char* text){e=text;return false;}
inline bool heapEqual(const PikiPcAllocationStats& before,const PikiPcAllocationStats& after,std::string& e){
 if(before.liveBlocks!=after.liveBlocks||before.liveBytes!=after.liveBytes||before.unknownFrees!=after.unknownFrees){
  char message[384];
  std::snprintf(message,sizeof(message),"owned graph heap changed blocks=%zu->%zu bytes=%zu->%zu unknownFrees=%zu->%zu (leak or concurrent allocation noise)",
   before.liveBlocks,after.liveBlocks,before.liveBytes,after.liveBytes,before.unknownFrees,after.unknownFrees);
  return fail(e,message);
 }
 return true;
}
template<class Actor> bool topology(StateMachine<Actor>* machine,int slots,int registered,std::string& e) {
 if(!machine||machine->mStateLimit!=slots||machine->mStateCount!=registered||machine->mLastStateID!=-1||
    !machine->mStates||!machine->mStateIDs||!machine->mStateIndexes)return fail(e,"FSM allocation shape mismatch");
 std::set<const void*> addresses;std::set<int> ids;
 for(int i=0;i<slots;++i){
  if(i>=registered){if(machine->mStates[i]||machine->mStateIDs[i]!=-1)return fail(e,"unregistered FSM storage not empty");continue;}
  auto* state=machine->mStates[i];if(!state||!addresses.insert(state).second)return fail(e,"missing/aliased state");
  int id=state->getID();if(id<0||id>=slots||!ids.insert(id).second||machine->mStateIDs[i]!=id||machine->mStateIndexes[id]!=i)return fail(e,"FSM inverse mapping mismatch");
 }
 for(int id=0;id<slots;++id)if(!ids.count(id)&&machine->mStateIndexes[id]!=-1)return fail(e,"unregistered state became available");
 return true;
}
inline bool navi(Navi& live,Navi* staged,std::string& e){
 if(!staged||staged==&live||typeid(*staged)!=typeid(Navi)||staged->mCurrState||!topology<Navi>(staged->mStateMachine,38,38,e))return fail(e,"fresh Navi FSM mismatch");
 auto& machine=*staged->mStateMachine;
 if(typeid(*machine.mStates[2])!=typeid(NaviWalkState))return fail(e,"Navi registered concrete type/order mismatch");
 if(typeid(*machine.mStates[3])!=typeid(NaviStuckState))return fail(e,"Navi registered concrete type/order mismatch");
 if(typeid(*machine.mStates[4])!=typeid(NaviFlickState))return fail(e,"Navi registered concrete type/order mismatch");
 if(typeid(*machine.mStates[5])!=typeid(NaviGeyzerState))return fail(e,"Navi registered concrete type/order mismatch");
 if(typeid(*machine.mStates[6])!=typeid(NaviContainerState))return fail(e,"Navi registered concrete type/order mismatch");
 if(typeid(*machine.mStates[7])!=typeid(NaviUfoState))return fail(e,"Navi registered concrete type/order mismatch");
 if(typeid(*machine.mStates[8])!=typeid(NaviPartsAccessState))return fail(e,"Navi registered concrete type/order mismatch");
 if(typeid(*machine.mStates[9])!=typeid(NaviUfoAccessState))return fail(e,"Navi registered concrete type/order mismatch");
 if(typeid(*machine.mStates[10])!=typeid(NaviPikiZeroState))return fail(e,"Navi registered concrete type/order mismatch");
 if(typeid(*machine.mStates[11])!=typeid(NaviPickState))return fail(e,"Navi registered concrete type/order mismatch");
 if(typeid(*machine.mStates[12])!=typeid(NaviRopeState))return fail(e,"Navi registered concrete type/order mismatch");
 if(typeid(*machine.mStates[13])!=typeid(NaviRopeExitState))return fail(e,"Navi registered concrete type/order mismatch");
 if(typeid(*machine.mStates[14])!=typeid(NaviGatherState))return fail(e,"Navi registered concrete type/order mismatch");
 if(typeid(*machine.mStates[15])!=typeid(NaviReleaseState))return fail(e,"Navi registered concrete type/order mismatch");
 if(typeid(*machine.mStates[16])!=typeid(NaviThrowWaitState))return fail(e,"Navi registered concrete type/order mismatch");
 if(typeid(*machine.mStates[17])!=typeid(NaviThrowState))return fail(e,"Navi registered concrete type/order mismatch");
 if(typeid(*machine.mStates[18])!=typeid(NaviFunbariState))return fail(e,"Navi registered concrete type/order mismatch");
 if(typeid(*machine.mStates[19])!=typeid(NaviPushState))return fail(e,"Navi registered concrete type/order mismatch");
 if(typeid(*machine.mStates[20])!=typeid(NaviPushPikiState))return fail(e,"Navi registered concrete type/order mismatch");
 if(typeid(*machine.mStates[21])!=typeid(NaviBuryState))return fail(e,"Navi registered concrete type/order mismatch");
 if(typeid(*machine.mStates[22])!=typeid(NaviNukuState))return fail(e,"Navi registered concrete type/order mismatch");
 if(typeid(*machine.mStates[23])!=typeid(NaviNukuAdjustState))return fail(e,"Navi registered concrete type/order mismatch");
 if(typeid(*machine.mStates[24])!=typeid(NaviWaterState))return fail(e,"Navi registered concrete type/order mismatch");
 if(typeid(*machine.mStates[25])!=typeid(NaviSowState))return fail(e,"Navi registered concrete type/order mismatch");
 if(typeid(*machine.mStates[26])!=typeid(NaviPressedState))return fail(e,"Navi registered concrete type/order mismatch");
 if(typeid(*machine.mStates[27])!=typeid(NaviClearState))return fail(e,"Navi registered concrete type/order mismatch");
 if(typeid(*machine.mStates[28])!=typeid(NaviLockState))return fail(e,"Navi registered concrete type/order mismatch");
 if(typeid(*machine.mStates[29])!=typeid(NaviAttackState))return fail(e,"Navi registered concrete type/order mismatch");
 if(typeid(*machine.mStates[30])!=typeid(NaviIroIroState))return fail(e,"Navi registered concrete type/order mismatch");
 if(typeid(*machine.mStates[31])!=typeid(NaviDeadState))return fail(e,"Navi registered concrete type/order mismatch");
 if(typeid(*machine.mStates[32])!=typeid(NaviIdleState))return fail(e,"Navi registered concrete type/order mismatch");
 if(typeid(*machine.mStates[33])!=typeid(NaviStartingState))return fail(e,"Navi registered concrete type/order mismatch");
 if(typeid(*machine.mStates[34])!=typeid(NaviDemoWaitState))return fail(e,"Navi registered concrete type/order mismatch");
 if(typeid(*machine.mStates[35])!=typeid(NaviDemoInfState))return fail(e,"Navi registered concrete type/order mismatch");
 if(typeid(*machine.mStates[36])!=typeid(NaviPelletState))return fail(e,"Navi registered concrete type/order mismatch");
 if(typeid(*machine.mStates[37])!=typeid(NaviDemoSunsetState))return fail(e,"Navi registered concrete type/order mismatch");
 auto* sunset=dynamic_cast<NaviDemoSunsetState*>(machine.mStates[machine.mStateIndexes[NAVISTATE_DemoSunset]]);
 if(!sunset||sunset->mCurrentState||!topology<NaviDemoSunsetState>(sunset->mStateMachine,36,5,e))return false;
 if(typeid(*sunset->mStateMachine->mStates[0])!=typeid(NaviDemoSunsetState::GoState))return fail(e,"Sunset concrete topology mismatch");
 if(typeid(*sunset->mStateMachine->mStates[1])!=typeid(NaviDemoSunsetState::LookState))return fail(e,"Sunset concrete topology mismatch");
 if(typeid(*sunset->mStateMachine->mStates[2])!=typeid(NaviDemoSunsetState::WhistleState))return fail(e,"Sunset concrete topology mismatch");
 if(typeid(*sunset->mStateMachine->mStates[3])!=typeid(NaviDemoSunsetState::WaitState))return fail(e,"Sunset concrete topology mismatch");
 if(typeid(*sunset->mStateMachine->mStates[4])!=typeid(NaviDemoSunsetState::SitState))return fail(e,"Sunset concrete topology mismatch");
 if(!staged->mCollInfo||staged->mCollInfo==live.mCollInfo||!staged->mPlateMgr||staged->mPlateMgr==live.mPlateMgr||
    !staged->mKontroller||staged->mKontroller==live.mKontroller||!staged->mShadowCaster.mShadowDrawer||
    !staged->mBurnEffect||!staged->mRippleEffect||!staged->mSlimeEffect||!staged->mNaviLightEfx||
    !staged->mNaviLightGlowEfx||!staged->mCursorTrailEfx||!staged->_780)return fail(e,"missing/aliased Navi owned graph");
 return true;
}
inline bool piki(ViewPiki& live,ViewPiki* staged,std::string& e){
 if(!staged||staged==&live||typeid(*staged)!=typeid(ViewPiki)||staged->mCurrentState||!topology<Piki>(staged->mFSM,37,35,e))return fail(e,"fresh ViewPiki FSM mismatch");
 auto& machine=*staged->mFSM;
 if(typeid(*machine.mStates[0])!=typeid(PikiNormalState))return fail(e,"Piki registered concrete type/order mismatch");
 if(typeid(*machine.mStates[1])!=typeid(PikiFlickState))return fail(e,"Piki registered concrete type/order mismatch");
 if(typeid(*machine.mStates[2])!=typeid(PikiFlownState))return fail(e,"Piki registered concrete type/order mismatch");
 if(typeid(*machine.mStates[3])!=typeid(PikiEmitState))return fail(e,"Piki registered concrete type/order mismatch");
 if(typeid(*machine.mStates[4])!=typeid(PikiFallState))return fail(e,"Piki registered concrete type/order mismatch");
 if(typeid(*machine.mStates[5])!=typeid(PikiCliffState))return fail(e,"Piki registered concrete type/order mismatch");
 if(typeid(*machine.mStates[6])!=typeid(PikiLookAtState))return fail(e,"Piki registered concrete type/order mismatch");
 if(typeid(*machine.mStates[7])!=typeid(PikiBulletState))return fail(e,"Piki registered concrete type/order mismatch");
 if(typeid(*machine.mStates[8])!=typeid(PikiBubbleState))return fail(e,"Piki registered concrete type/order mismatch");
 if(typeid(*machine.mStates[9])!=typeid(PikiFiredState))return fail(e,"Piki registered concrete type/order mismatch");
 if(typeid(*machine.mStates[10])!=typeid(PikiSwallowedState))return fail(e,"Piki registered concrete type/order mismatch");
 if(typeid(*machine.mStates[11])!=typeid(PikiHangedState))return fail(e,"Piki registered concrete type/order mismatch");
 if(typeid(*machine.mStates[12])!=typeid(PikiWaterHangedState))return fail(e,"Piki registered concrete type/order mismatch");
 if(typeid(*machine.mStates[13])!=typeid(PikiGoHangState))return fail(e,"Piki registered concrete type/order mismatch");
 if(typeid(*machine.mStates[14])!=typeid(PikiFlyingState))return fail(e,"Piki registered concrete type/order mismatch");
 if(typeid(*machine.mStates[15])!=typeid(PikiGrowState))return fail(e,"Piki registered concrete type/order mismatch");
 if(typeid(*machine.mStates[16])!=typeid(PikiGrowupState))return fail(e,"Piki registered concrete type/order mismatch");
 if(typeid(*machine.mStates[17])!=typeid(PikiBuryState))return fail(e,"Piki registered concrete type/order mismatch");
 if(typeid(*machine.mStates[18])!=typeid(PikiWaveState))return fail(e,"Piki registered concrete type/order mismatch");
 if(typeid(*machine.mStates[19])!=typeid(PikiPushState))return fail(e,"Piki registered concrete type/order mismatch");
 if(typeid(*machine.mStates[20])!=typeid(PikiPushPikiState))return fail(e,"Piki registered concrete type/order mismatch");
 if(typeid(*machine.mStates[21])!=typeid(PikiNukareState))return fail(e,"Piki registered concrete type/order mismatch");
 if(typeid(*machine.mStates[22])!=typeid(PikiNukareWaitState))return fail(e,"Piki registered concrete type/order mismatch");
 if(typeid(*machine.mStates[23])!=typeid(PikiAutoNukiState))return fail(e,"Piki registered concrete type/order mismatch");
 if(typeid(*machine.mStates[24])!=typeid(PikiPressedState))return fail(e,"Piki registered concrete type/order mismatch");
 if(typeid(*machine.mStates[25])!=typeid(PikiAbsorbState))return fail(e,"Piki registered concrete type/order mismatch");
 if(typeid(*machine.mStates[26])!=typeid(PikiDyingState))return fail(e,"Piki registered concrete type/order mismatch");
 if(typeid(*machine.mStates[27])!=typeid(PikiDeadState))return fail(e,"Piki registered concrete type/order mismatch");
 if(typeid(*machine.mStates[28])!=typeid(PikiDenkiDyingState))return fail(e,"Piki registered concrete type/order mismatch");
 if(typeid(*machine.mStates[29])!=typeid(PikiPanicState))return fail(e,"Piki registered concrete type/order mismatch");
 if(typeid(*machine.mStates[30])!=typeid(PikiKinokoState))return fail(e,"Piki registered concrete type/order mismatch");
 if(typeid(*machine.mStates[31])!=typeid(PikiDrownState))return fail(e,"Piki registered concrete type/order mismatch");
 if(typeid(*machine.mStates[32])!=typeid(PikiEmotionState))return fail(e,"Piki registered concrete type/order mismatch");
 if(typeid(*machine.mStates[33])!=typeid(PikiKinokoChangeState))return fail(e,"Piki registered concrete type/order mismatch");
 if(typeid(*machine.mStates[34])!=typeid(PikiFallMeckState))return fail(e,"Piki registered concrete type/order mismatch");
 if(!staged->mCollInfo||staged->mCollInfo==live.mCollInfo||!staged->mActiveAction||staged->mActiveAction==live.mActiveAction||
    staged->mActiveAction->mPiki!=staged||staged->mActiveAction->mChildCount!=31||
    !staged->mBurnEffect||!staged->mRippleEffect||!staged->mFreeLightEffect||!staged->mSlimeEffect||!staged->mPanickedEffect)
    return fail(e,"missing/aliased Piki owned graph");
 return pc_midday::inspect_piki_action_graph(*staged,*staged->mActiveAction,e);
}
// Observe must verify the caller's immutable live-manager/global snapshot. It
// executes after preparation AND after reset, including injected failures.
template<class Prepare,class Inspect,class Observe>
bool run(Prepare prepare,Inspect inspect,Observe observe,pc_midday::ConstructorFence& fence,std::string& e){
 if(!fence.held())return fail(e,"owned graph test requires held constructor fence");
 e.reserve(512); // Keep failure diagnostics outside the measured allocation set.
 size_t attempts=0;
 {
  std::string detail;detail.reserve(512);
  const auto before=piki_pc_allocation_stats();
  bool good=false,unchanged=false,retired=false;
  {
  pc_midday::ActorAllocationGraph graph;
  const bool prepared=prepare(graph,std::numeric_limits<size_t>::max(),e);
  attempts=graph.allocationAttempts();
  good=prepared&&attempts>0&&!graph.empty()&&inspect(graph,e);
  if(!good)detail.assign(e.data(),std::min(e.size(),size_t(511)));
  unchanged=observe(e);
  if(!unchanged&&detail.empty())detail.assign(e.data(),std::min(e.size(),size_t(511)));
  graph.reset();
  retired=graph.empty()&&!graph.stagedNavi()&&!graph.stagedPiki();
  unchanged=observe(e)&&unchanged;
  if(!unchanged&&detail.empty())detail.assign(e.data(),std::min(e.size(),size_t(511)));
  }
  if(!heapEqual(before,piki_pc_allocation_stats(),e))return false;
  if(!retired||!good||!unchanged){if(!detail.empty()){e=detail;return false;}return fail(e,"baseline owned graph/cleanup failed");}
 }
 for(size_t at=0;at<attempts;++at){
  std::string rejected;rejected.reserve(512);
  const auto before=piki_pc_allocation_stats();
  bool good=false,unchanged=false,retired=false;
  {
  pc_midday::ActorAllocationGraph graph;
  bool accepted=prepare(graph,at,rejected);
  good=!accepted&&!rejected.empty()&&graph.empty()&&!graph.stagedNavi()&&!graph.stagedPiki()&&graph.allocationAttempts()==at+1;
  unchanged=observe(e);
  graph.reset();
  retired=graph.empty()&&!graph.stagedNavi()&&!graph.stagedPiki();
  unchanged=observe(e)&&unchanged;
  }
  const bool heap=heapEqual(before,piki_pc_allocation_stats(),e);
  if(!heap||!retired||!unchanged||!good){
   char message[512];
   std::snprintf(message,sizeof(message),"allocation injection index=%zu: %.400s",at,
    !heap||!unchanged?e.c_str():!rejected.empty()?rejected.c_str():"accepted or failed to retire entire graph");
   return fail(e,message);
  }
 }
 {
  std::string detail;detail.reserve(512);
  const auto before=piki_pc_allocation_stats();
  bool good=false,unchanged=false,retired=false;
  {
  pc_midday::ActorAllocationGraph graph;
  const bool prepared=prepare(graph,std::numeric_limits<size_t>::max(),e);
  good=prepared&&graph.allocationAttempts()==attempts&&inspect(graph,e);
  if(!good)detail.assign(e.data(),std::min(e.size(),size_t(511)));
  unchanged=observe(e);
  if(!unchanged&&detail.empty())detail.assign(e.data(),std::min(e.size(),size_t(511)));
  graph.reset();
  retired=graph.empty()&&!graph.stagedNavi()&&!graph.stagedPiki();
  unchanged=observe(e)&&unchanged;
  if(!unchanged&&detail.empty())detail.assign(e.data(),std::min(e.size(),size_t(511)));
  }
  if(!heapEqual(before,piki_pc_allocation_stats(),e))return false;
  if(!retired||!good||!unchanged){if(!detail.empty()){e=detail;return false;}return fail(e,"fresh success after injection failed");}
 }
 std::cout<<"MIDDAY_OWNED_GRAPH allocation_boundaries="<<attempts<<" failure_sweep=PASS topology=PASS cleanup=PASS heap_baseline=PASS STL_internal_allocations=not_injected\n";
 return true;
}
}
template<class Observe> bool pc_midday_check_owned_navi_graph(Navi& live,const pc_midday::ActorBytes& base,
 pc_midday::LogicalResolver& resolver,pc_midday::ConstructorFence& fence,int port,Observe observe,std::string& e){
 return pc_midday_graph_checks::run([&](pc_midday::ActorAllocationGraph& g,size_t at,std::string& error){return g.prepareNavi(base,resolver,fence,port,error,at);},
  [&](pc_midday::ActorAllocationGraph& g,std::string& error){return pc_midday_graph_checks::navi(live,g.stagedNavi(),error);},observe,fence,e);
}
template<class Observe> bool pc_midday_check_owned_piki_graph(ViewPiki& live,const pc_midday::ActorBytes& base,const pc_midday::ActorBytes& subtype,
 pc_midday::LogicalResolver& resolver,pc_midday::ConstructorFence& fence,Observe observe,std::string& e){
 return pc_midday_graph_checks::run([&](pc_midday::ActorAllocationGraph& g,size_t at,std::string& error){return g.prepareViewPiki(base,subtype,resolver,fence,error,at);},
  [&](pc_midday::ActorAllocationGraph& g,std::string& error){return pc_midday_graph_checks::piki(live,g.stagedPiki(),error);},observe,fence,e);
}
