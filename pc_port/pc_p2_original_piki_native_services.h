#pragma once
#include "pc_p2_original_piki_captain_reader.h"
#include "pc_p2_original_piki_native_bank.h"
#include "pc_p2_original_piki_animator.h"
#include "pc_p2_original_piki_plate.h"

namespace p2original { namespace piki {
// Concrete body effect owner: selected JPA/PSM registrations remain retained
// on failed creation/cleanup, tied to the exact native body lifetime. No P1
// effect/sound delegate or successful empty presentation implementation.
class NativeEffects {
public:
 struct Ownership {
  std::uint64_t freeContexts=0,throwContexts=0,voiceObjects=0,inFlightOperations=0;
 };
 virtual ~NativeEffects()=default;
 virtual const captain::LoadedScene& scene()const=0;
 // Actual retained Piki contexts, including failed/partial registration and
 // pending voice cleanup. Unknown observation refuses; runtime flag counts
 // cannot substitute for this producer's independent ownership census.
 virtual bool ownership(Ownership&,std::string&)const=0;
 virtual bool canRetire(std::string&)const=0;
 virtual bool retire(std::string&)=0;
 virtual bool canRemoveFree(Handle,std::string&)const=0;
 virtual bool free(Handle,bool enabled,std::string&)=0;
 virtual bool canRemoveThrow(Handle,std::string&)const=0;
 virtual bool thrown(Handle,bool enabled,std::string&)=0;
 virtual bool hangSound(Handle,std::string&)=0;
 virtual bool landSound(Handle,std::string&)=0;
 virtual bool calledSound(Handle,std::string&)=0;
 virtual bool nudgeRumble(Handle,Navi*,std::string&)=0;
 virtual bool boreVoice(Handle,bool sleep,std::string&)=0;
};
// The actual scene/entity owner supplies a complete source task census and
// source simulation environment. Missing census is refusal, not no-task.
class NativeTaskEnvironment {
public:
 virtual ~NativeTaskEnvironment()=default;
 virtual const captain::LoadedScene& scene()const=0;
 virtual bool gravity(float&,std::string&)const=0;
 virtual bool random(float&,std::string&)=0;
 virtual bool task(Handle,FreeSearch,bool&,std::string&)const=0;
};

// One real delegate composition, owned for the process lifetime by the body
// factory. Installation is separate from admission and grants no World phase.
// All delegate objects are process-lifetime stable (the runtime has no Services
// uninstall). Their per-scene buffers/descriptors stay retained through checked
// body/runtime retirement and can be rebound only after that scene is retired.
class NativeServices final : public Services {
public:
 NativeServices(NativeBodyBank&,NativeAnimator&,Plate&,NativeCaptainReader&,
                NativeEffects&,NativeTaskEnvironment&,PhysicalSource&);
 const captain::LoadedScene& scene()const override;
 const std::string& pikiParameterBytes()const override;
 const std::string& naviParameterBytes()const override;
 bool gravity(float&,std::string&)const override;
 bool captainFrame(const Navi*,CaptainFrame&,std::string&)const override;
 bool bodyAlive(Handle,bool&,std::string&)const override;
 bool supports(Handle,Motion,std::string&)const override;
 bool motion(Handle,Motion,std::string&)override;
 bool animate(Handle,float,std::string&)override;
 bool currentMotion(Handle,Motion&,std::string&)const override;
 bool canRemoveFreeEffects(Handle,std::string&)const override;
 bool freeEffects(Handle,bool,std::string&)override;
 bool canRemoveThrowEffects(Handle,std::string&)const override;
 bool throwEffects(Handle,bool,std::string&)override;
 bool hangSound(Handle,std::string&)override;
 bool landSound(Handle,std::string&)override;
 bool calledSound(Handle,std::string&)override;
 bool nudgeRumble(Handle,Navi*,std::string&)override;
 bool allocateSlot(Handle,Navi*,int&,std::string&)override;
 bool canReleaseSlot(Handle,Navi*,int,std::string&)const override;
 bool releaseSlot(Handle,Navi*,int,std::string&)override;
 bool slotPosition(Handle,Navi*,int,Vector3f&,std::string&)const override;
 bool formed(Handle,Navi*,std::string&)override;
 bool sortSlot(Handle,Navi*,int,int,std::string&)override;
 bool freeTaskAvailable(Handle,FreeSearch,bool&,std::string&)const override;
 bool animationStatus(Handle,Motion&,float&,bool&,std::string&)const override;
 bool animationSpeed(Handle,float,std::string&)override;
 bool finishMotion(Handle,std::string&)override;
 bool loopStart(Handle,std::string&)override;
 bool boreVoice(Handle,bool,std::string&)override;
 bool random(float&,std::string&)override;
private:
 NativeBodyBank& bank;NativeAnimator& animator;Plate& plate;
 NativeCaptainReader& captains;NativeEffects& effects;
 NativeTaskEnvironment& environment;
 PhysicalSource& physical;
 struct Observation;
 bool beginObservation(Observation&,std::string&)const;
 bool finishObservation(const Observation&,std::string&)const;
 bool finishBodyObservation(const Observation&,Handle,std::string&)const;
 bool bound(std::string&)const;
};
} }
