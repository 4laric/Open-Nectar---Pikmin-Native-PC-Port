#pragma once
#include <cstdint>
#include <optional>
#include <string>
class Navi;
namespace Game {struct WaterBox;}
namespace p2original {namespace captain {
struct LoadedScene;
namespace water {
struct Owner {
 const LoadedScene* scene=nullptr;Navi* actor=nullptr;
 std::uint64_t sceneIncarnation=0,actorIncarnation=0;
};
struct Handle {Game::WaterBox* pointer=nullptr;std::uint64_t lifetime=0;};
struct Sphere {float x=0,y=0,z=0,radius=0;};
enum class ColdEvent {Constructor,InitFakePiki};
// Internal mechanical lifecycle identity from the real SDK constructor/init
// event; no registry, workflow record or caller-generated admission token.
struct ColdInitialization {Owner owner;ColdEvent event=ColdEvent::Constructor;std::uint64_t serial=0;};
// The concrete source SDK owner retains this provider and its actor/scene for
// the whole call. Queries attest actual current identities, not caller readiness.
// Unknown map presence is distinct from the actual source mapMgr == nullptr.
class Provider {
public:
 virtual ~Provider()=default;
 virtual bool currentOwner(Owner&,std::string&)const=0;
 virtual bool coldInitialization(ColdInitialization&,std::string&)const=0;
 // Actual getBoundingSphere's cached mBoundingSphere at doAnimation. Constructor
 // radius is 8.5; initFakePiki does not reconstruct/reset the cached sphere.
 virtual bool boundingSphere(Sphere&,std::string&)const=0;
 virtual bool mapPresent(std::optional<bool>&,std::string&)const=0;
 virtual bool liveWater(Handle,std::string&)const=0;
 virtual bool containsSphere(Handle,const Sphere&,bool&,std::string&)const=0;
 // Literal mapMgr->findWater: first actual SeaMgr list match, possibly null.
 virtual bool findWater(const Sphere&,std::optional<Handle>&,std::string&)const=0;
 virtual bool inWaterCallback(Handle,std::string&)=0;
 virtual bool outWaterCallback(std::string&)=0;
};
class Cache {
private:
 const Provider* provider_=nullptr;std::optional<ColdInitialization> initialization_;
 std::optional<Handle> water_;std::uint64_t revision_=0;
 friend bool initialize(Cache&,const Provider&,std::string&);
 friend bool cachedWater(const Cache&,const Provider&,std::optional<Handle>&,std::string&);
 friend bool checkWater(Cache&,Provider&,std::string&);
 friend struct Access;
};
// Query a genuine source constructor/initFakePiki event. Re-observing the same
// event identity does not reset the field. There is no public cache/phase setter.
bool initialize(Cache&,const Provider&,std::string&);
// Reports only FakePiki's cached pointer. A cached null is not physical dryness;
// default unknown and expired lifecycle refuse without modifying the output.
bool cachedWater(const Cache&,const Provider&,std::optional<Handle>&,std::string&);
// Concrete source doAnimation calls here after movement/getBoundingSphere,
// before gravity/updateTrMatrix. This policy cannot establish that frame event.
// Callbacks observe the old cache. Refusal/ownership change never publishes the
// candidate; reentrant genuine initialization is preserved rather than undone.
bool checkWater(Cache&,Provider&,std::string&);
}}}
