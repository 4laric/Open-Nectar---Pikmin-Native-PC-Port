#pragma once
#include "pc_p2_retail_cave_plan.h"
#include "pc_p2_original_actor.h"
#include <memory>
class Creature;class Generator;struct Vector3f;
namespace p2retail {
struct FamilyOps {
 // Each leaf verifies real resources and whole-family actor/corpse capacity.
 std::function<bool(unsigned,std::string&)> prepare;
 std::function<bool(Generator*,const Vector3f&,float,Creature*&,bool& suppressed,std::string&)> birth;
 std::function<bool(Creature*,unsigned,std::string&)> bind,release;
 std::function<bool(std::string&)> cancel;
 // After registry retirement, before native manager address reuse.
 std::function<bool(Creature*,std::string&)> retired;
 bool retireBeforeRelease=false;
};
// Geometry/stage, Pod/exit and cargo ownership remain with their actual native
// owners. No default/preview implementations exist for any of these callbacks.
class SceneOps {
public:
 virtual ~SceneOps()=default;
 virtual bool mode(const SceneIdentity&,bool& story,bool& inCave,std::string&)const=0;
 virtual bool preflight(const FloorPlan&,const Snapshot&,std::string&)=0;
 virtual bool begin(const FloorPlan&,const Snapshot&,std::string&)=0;
 // Verify selected live/terminal disposition before any native enemy allocation.
 virtual bool prior(const ContentRow&,const BirthIdentity&,const Snapshot&,LiveBinding&,bool& absent,std::string&)=0;
 virtual bool cargo(const Placement&,const ContentRow&,const BirthIdentity&,const Snapshot&,LiveBinding&,std::string&)=0;
 virtual bool absent(const ContentRow&,const BirthIdentity&,const Snapshot&,const LiveBinding&)const=0;
 virtual bool commit(const Snapshot&,std::string&)=0;
 virtual bool release(std::string&)=0;
 virtual bool retired(const BirthIdentity&,const Snapshot&,std::string&)=0;
};
class NativeFloor final:public FloorProvider {
public:
 NativeFloor(FloorPlan,SceneOps&);~NativeFloor();
 NativeFloor(const NativeFloor&)=delete;NativeFloor&operator=(const NativeFloor&)=delete;
 bool family(unsigned source,FamilyOps,std::string&);
 bool preflight(const CaveDescriptor&,const FloorDefinition&,unsigned,const SceneIdentity&,std::string&)override;
 bool install(const CaveDescriptor&,const FloorDefinition&,unsigned,const SceneIdentity&,
              const std::vector<BirthIdentity>&,std::vector<LiveBinding>&,std::string&)override;
 bool verifyAbsent(const CaveDescriptor&,unsigned,const ContentRow&,const SceneIdentity&,
                   const BirthIdentity&,const LiveBinding&)const override;
 bool commit(const Snapshot&,std::string&)override;
 // Trusted installation lookup for cargo/Pod adapters; raw plan remains rechecked.
 bool placement(const SceneIdentity&,unsigned row,unsigned ordinal,Placement&,std::string&)const;
 bool release(std::string&)override;
 // Actual native forget event, before manager address reuse; not a death poll.
 bool retired(Creature*,std::string&);
private:struct Impl;std::unique_ptr<Impl> m;
};
} // namespace p2retail
bool pc_p2_retail_cave_native_retired(Creature*,std::string&);
