#include "pc_p2_original_captain_water.h"
#include <cmath>
namespace p2original {namespace captain {namespace water {
namespace {
bool fail(std::string& e,const char* message){e=message;return false;}
bool same(Owner a,Owner b){return a.scene==b.scene&&a.actor==b.actor&&a.sceneIncarnation==b.sceneIncarnation&&a.actorIncarnation==b.actorIncarnation;}
bool valid(Owner a){return a.scene&&a.actor&&a.sceneIncarnation&&a.actorIncarnation;}
bool valid(Handle a){return a.pointer&&a.lifetime;}
bool live(const Provider& p,Handle a,std::string& e){return (valid(a)||fail(e,"Actual source WaterBox handle is invalid"))&&p.liveWater(a,e);}
bool same(ColdInitialization a,ColdInitialization b){return same(a.owner,b.owner)&&a.event==b.event&&a.serial==b.serial;}
bool receipt(const Provider& p,ColdInitialization& r,std::string& e){
 Owner owner;
 if(!p.currentOwner(owner,e)||!p.coldInitialization(r,e))return false;
 if(!valid(owner)||!same(owner,r.owner)||!r.serial||(r.event!=ColdEvent::Constructor&&r.event!=ColdEvent::InitFakePiki))return fail(e,"Actual source water initialization/owner is unavailable");
 Owner after;
 if(!p.currentOwner(after,e))return false;
 return same(owner,after)||fail(e,"Source water owner changed during initialization query");
}
}
struct Access {
 static bool guard(const Cache& c,const Provider& p,std::uint64_t revision,std::string& e){
  if(c.provider_!=&p||!c.initialization_||c.revision_!=revision)return fail(e,"Source water cache is unknown or changed during callback");
  ColdInitialization current;
  if(!receipt(p,current,e))return false;
  if(c.provider_!=&p||!c.initialization_||c.revision_!=revision||!same(*c.initialization_,current))return fail(e,"Source water actor/scene/provider lifecycle expired");
  return true;
 }
};
bool initialize(Cache& c,const Provider& p,std::string& e){
 const auto revision=c.revision_;ColdInitialization current;
 if(!receipt(p,current,e))return false;
 if(c.revision_!=revision)return fail(e,"Source water initialization changed reentrantly");
 if(c.provider_==&p&&c.initialization_&&same(*c.initialization_,current)){e.clear();return true;}
 c.provider_=&p;c.initialization_=current;c.water_.reset();++c.revision_;e.clear();return true;
}
bool cachedWater(const Cache& c,const Provider& p,std::optional<Handle>& out,std::string& e){
 const auto revision=c.revision_;
 if(!Access::guard(c,p,revision,e))return false;
 const auto result=c.water_;
 if(result&&(!live(p,*result,e)||!Access::guard(c,p,revision,e)))return false;
 out=result;e.clear();return true;
}
bool checkWater(Cache& c,Provider& p,std::string& e){
 const auto revision=c.revision_;if(!Access::guard(c,p,revision,e))return false;
 const auto old=c.water_;Sphere sphere;
 if(!p.boundingSphere(sphere,e)||!Access::guard(c,p,revision,e))return false;
 if(!std::isfinite(sphere.x)||!std::isfinite(sphere.y)||!std::isfinite(sphere.z)||!std::isfinite(sphere.radius)||sphere.radius<0)return fail(e,"Actual source bounding sphere is invalid");
 if(old){
  if(!live(p,*old,e)||!Access::guard(c,p,revision,e))return false;
  bool inside=false;
  if(!p.containsSphere(*old,sphere,inside,e)||!Access::guard(c,p,revision,e))return false;
  if(inside){e.clear();return true;}
 }
 std::optional<bool> map;
 if(!p.mapPresent(map,e)||!Access::guard(c,p,revision,e))return false;
 if(!map)return fail(e,"Actual source mapMgr presence is unknown");
 if(!*map){e.clear();return true;} // Source retains old nonnull outside water.
 std::optional<Handle> candidate;
 if(!p.findWater(sphere,candidate,e)||!Access::guard(c,p,revision,e))return false;
 if(candidate&&(!live(p,*candidate,e)||!Access::guard(c,p,revision,e)))return false;
 // Creature local changes before callback, but FakePiki mWaterBox assignment
 // occurs only on return. Nonnull-to-nonnull has no entry or exit callback.
 if(!old&&candidate){if(!p.inWaterCallback(*candidate,e)||!Access::guard(c,p,revision,e))return false;}
 else if(old&&!candidate){if(!p.outWaterCallback(e)||!Access::guard(c,p,revision,e))return false;}
 if(candidate&&(!live(p,*candidate,e)||!Access::guard(c,p,revision,e)))return false;
 c.water_=candidate;++c.revision_;e.clear();return true;
}
}}}
