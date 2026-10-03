#pragma once
#include "pc_p2_original_piki_native_facts.h"
#include <array>
namespace p2retail {class SceneContext;}
class StageInfo;class MapMgr;
namespace p2original {namespace piki {
class NativeBodyFactory;
struct PikiUpdateScheduleState {
 unsigned currentIndex=0,clientCount=0,retainedContexts=0;
 std::array<unsigned,10> clients{},activeClients{};
};
// Retail PikiMgr::mUpdateMgr2/create(10), NOT Creature::mUpdateContext.
// This owns only source Free/look scheduling, never physical/world admission.
// The actual singular factory constructs/binds this owner and invokes private
// lifecycle calls. There is no standalone frame tick or external flag setter.
class PikiUpdateSchedule final {
public:
 PikiUpdateSchedule(const PikiUpdateSchedule&)=delete;
 PikiUpdateSchedule& operator=(const PikiUpdateSchedule&)=delete;
 bool updatable(Handle,bool&,std::string&)const;
 bool state(PikiUpdateScheduleState&,std::string&)const;
private:
 friend class NativeBodyFactory;
 PikiUpdateSchedule()=default;
 bool bind(const p2retail::SceneContext&,std::string&);
 bool initContext(Handle,std::string&);
 bool exitContext(Handle,std::string&);
 // Exactly once at actual source PikiMgr::doAnimation entry, before actors.
 bool updateFromPikiMgr(std::string&);
 bool release(std::string&);
 struct Context {Handle handle;int clientIndex=-1;bool active=false,forced=false;};
 const p2retail::SceneContext* mStage=nullptr;
 StageInfo* mStageInfo=nullptr;MapMgr* mMap=nullptr;
 SceneBinding mBinding;std::uint64_t mSerial=0,mRevision=0;
 std::array<Context,100> mContexts{};
 PikiUpdateScheduleState mState;
 mutable bool mBusy=false,mReentered=false;
 class Operation;
 bool exact(bool cleanup,std::string&)const;
 bool body(Handle,bool cleanup,std::string&)const;
 bool validate(bool cleanup,std::string&)const;
};
} }
