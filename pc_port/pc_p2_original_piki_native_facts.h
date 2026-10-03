#pragma once
#include "pc_p2_original_piki_runtime.h"
#include "pc_p2_original_captain_damage.h"
namespace p2original { namespace piki {
// Concrete source startup/actor owners implement this read-only boundary.
// Missing retail fields cannot be inferred from P1 flags, HP, or movieMode.
struct PhysicalFacts {
 bool frozen=false,movieExtra=false,movieActor=false,pikiManagerFlag1=false;
 bool naviManagerFlag1=false,mapAvailable=false,stuck=false,targetCollision=false;
};
class PhysicalSource {
public:
 virtual ~PhysicalSource()=default;
 virtual const captain::LoadedScene& scene()const=0;
 virtual bool readPhysical(Handle,PhysicalFacts&,std::string&)const=0;
};
struct NativeBodyFacts {
 Handle handle; unsigned species=0,happa=0; Vector3f position;
 OriginalPikiBody body;
};
struct SceneBinding {
 const captain::LoadedScene* scene=nullptr;
 std::uint64_t incarnation=0;
 Navi* captains[2]={nullptr,nullptr};
 std::string campaign,fingerprint,catalog;
};
// Loading is admitted only for initialization; inactive only for exact
// lifetime cleanup. Canonical source descriptor and both roster slots required.
bool nativeSceneBinding(SceneBinding&,bool cleanup,std::string&);
bool nativeSceneCurrent(const SceneBinding&,bool cleanup,std::string&);
bool nativeBodyFacts(Handle,NativeBodyFacts&,std::string&);
bool nativeCaptainFacts(const SceneBinding&,const Navi*,bool cleanup,std::string&);
bool nativePhysicalFacts(Handle,const PhysicalSource*,PhysicalFacts&,std::string&);
} }
