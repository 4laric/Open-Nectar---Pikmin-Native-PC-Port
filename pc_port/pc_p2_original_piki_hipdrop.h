#pragma once
#include "pc_p2_original_piki_runtime.h"
#include <memory>
class Creature;
class CollPart;
namespace p2original { namespace piki {
struct HipTarget {Creature* body=nullptr;std::uint64_t lifetime=0;};
struct HipContact {CollPart* part=nullptr;std::uint64_t lifetime=0;};
struct HipTargetFacts {Vector3f position;bool piki=false,teki=false,alive=false,living=false;};
struct HipParameters {float gravity=0,poundDamage=0,earthquakeRange=0;};
bool parseHipParameters(const std::string& selectedPikiBytes,float sourceGravity,HipParameters&,std::string&);
// Actual source target/contact/effect/Brain owners supply this required view.
// All reads authenticate source scene and native lifetimes. No P1 enemy/HP,
// empty-census, audio or particle fallback may implement these operations.
class HipDropServices {
public:
 virtual ~HipDropServices()=default;
 // Cleanup permits exact inactive retained owners; actions require Active.
 virtual bool current(Handle,bool cleanup,std::string&)const=0;
 virtual bool stateIsHipDrop(Handle,bool&,std::string&)const=0;
 virtual bool parameters(Handle,HipParameters&,std::string&)const=0;
 virtual bool supportsFall(Handle,std::string&)const=0;
 virtual bool fallMotion(Handle,std::string&)=0;
 virtual bool killThrow(Handle,std::string&)=0;
 virtual bool blackDown(Handle,bool create,std::string&)=0;
 // Literal custom-radius CellIterator; preserve source iteration order and
 // include every cell candidate. A missing census is an error, not empty.
 virtual bool cellCensus(Handle,const Vector3f&,float,std::vector<HipTarget>&,std::string&)const=0;
 virtual bool target(HipTarget,HipTargetFacts&,std::string&)const=0;
 virtual bool contact(HipContact,bool& stickable,std::string&)const=0;
 // Invocation result differs from the target's legitimate accepted=false.
 virtual bool hipdrop(Handle,HipTarget,float,HipContact,bool& accepted,std::string&)=0;
 virtual bool press(Handle,HipTarget,float,HipContact,bool& accepted,std::string&)=0;
 virtual bool earthquake(Handle,HipTarget,float,bool& accepted,std::string&)=0;
 virtual bool stick(Handle,HipTarget,HipContact,std::string&)=0;
 virtual bool attachSound(Handle,HipTarget,std::string&)=0;
 virtual bool blackDrop(Handle,const Vector3f&,std::string&)=0;
 virtual bool rumbleBoth(Handle,const Vector3f&,std::string&)=0;
 virtual bool vibrationBoth(Handle,const Vector3f&,std::string&)=0;
 virtual bool dosunSound(Handle,bool hit,std::string&)=0;
 virtual bool walk(Handle,std::string&)=0;
 // Actual source invokeAI (optional contact,true on collision) and Free.
 virtual bool invokeAI(Handle,const HipTarget*,const HipContact*,bool collision,bool& selected,std::string&)=0;
 virtual bool free(Handle,std::string&)=0;
 virtual bool forceActive(Handle,bool,std::string&)=0;
};
enum class HipPhase { Pause, Descent, Recovery };
struct HipSnapshot {HipPhase phase=HipPhase::Pause;float timer=0;bool blackDownOwned=false,forceOwned=false;};
// Isolated genuine PikiHipDropState port. Existing SourcePiki Flying continues
// refusing Purple until a real runtime composes these delegates and state.
// The instance and Services are process-lifetime retained owners; destruction
// while owned() is true is prohibited. No implicit destructor cleanup exists.
class NativeHipDrop final {
public:
 explicit NativeHipDrop(HipDropServices&);
 ~NativeHipDrop();
 NativeHipDrop(const NativeHipDrop&)=delete;
 NativeHipDrop& operator=(const NativeHipDrop&)=delete;
 bool begin(Handle,std::string&);
 bool update(Handle,float seconds,std::string&);
 bool bounce(Handle,std::string&);
 bool platform(Handle,std::string&);
 bool collision(Handle,HipTarget,HipContact,std::string&);
 bool snapshot(Handle,HipSnapshot&)const noexcept;
 bool retire(Handle,std::string&);
 bool retireScene(std::string&);
 bool owned()const noexcept;
private:
 struct Impl;std::unique_ptr<Impl> impl;
};
} }
