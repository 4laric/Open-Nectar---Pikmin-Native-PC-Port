#pragma once
#include "pc_midday_capture.h"
#include "pc_midday_restore.h"
class Creature;
struct MonoObjectMgr;
namespace pc_midday {
enum class SlotLife:uint8_t { Free,Active,Retained };
struct PoolSlot { SlotLife life=SlotLife::Free;uint64_t actor=0; };
struct MonoPoolPlan {uint32_t capacity=0,count=0;std::vector<PoolSlot> slots;};
// Native pointers are local census keys only. Every slot, including retained-2,
// participates in the plan. Free pool backing objects are not declared actors;
// the scene capability must separately prove their native reuse initialization.
struct MonoPoolView {uint32_t capacity=0,count=0;std::vector<int> statuses;std::vector<const void*> objects;};
bool planMonoPool(const MonoPoolView&,const BirthLedger&,MonoPoolPlan&,std::string&);
bool validateMonoPool(const MonoPoolPlan&,const std::set<uint64_t>& expectedActors,std::string&);
bool encodeMonoPool(const MonoPoolPlan&,Bytes&,std::string&);
bool decodeMonoPool(const Bytes&,MonoPoolPlan&,std::string&);
bool readMonoPoolView(const MonoObjectMgr&,MonoPoolView&,std::string&);
bool captureMonoPool(const MonoObjectMgr&,const BirthLedger&,MonoPoolPlan&,std::string&);
// This operates only on a newly constructed zero-active pool under the complete
// restore fence. All saved slots validate before mutation. No birth/kill calls.
// Returned pointers are constructor-initialized roots for subsequent typed bind.
bool stageMonoPool(MonoObjectMgr&,const MonoPoolPlan&,const RestoreGate&,
                   std::map<uint64_t,Creature*>&,std::string&);
}
