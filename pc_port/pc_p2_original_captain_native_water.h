#pragma once
#include "pc_p2_original_captain_water.h"
#include "pc_p2_original_captain_body_phases.h"
#include <memory>
namespace p2retail {class SceneContext;}
namespace p2original {namespace captain {namespace bodyphases {class NativeComposition;}}}
namespace p2original {namespace captain {namespace water {
// One concrete retained child of the real captain phase owner. Borrows actual
// Scene930 registration and FakePiki fields, never a positional receiver query.
class NativeCache final:private Provider {
public:
 bool cached(std::optional<Handle>&,std::string&)const;
private:
 friend class bodyphases::NativeComposition;
 static std::unique_ptr<NativeCache> create(const p2retail::SceneContext&,Navi*,bodyphases::Owner&,std::string&);
 bool animation(const bodyphases::Sphere&,std::string&);
 NativeCache(const p2retail::SceneContext&,Navi*,bodyphases::Owner&);
 bool currentOwner(Owner&,std::string&)const override;
 bool coldInitialization(ColdInitialization&,std::string&)const override;
 bool boundingSphere(Sphere&,std::string&)const override;
 bool mapPresent(std::optional<bool>&,std::string&)const override;
 bool liveWater(Handle,std::string&)const override;
 bool containsSphere(Handle,const Sphere&,bool&,std::string&)const override;
 bool findWater(const Sphere&,std::optional<Handle>&,std::string&)const override;
 bool inWaterCallback(Handle,std::string&)override;
 bool outWaterCallback(std::string&)override;
 const p2retail::SceneContext* context_;Navi* actor_;bodyphases::Owner* phases_;
 const LoadedScene* scene_;std::uint64_t serial_,revision_,birth_;
 std::string campaign_,session_,catalog_;Cache cache_;
};
}}}
