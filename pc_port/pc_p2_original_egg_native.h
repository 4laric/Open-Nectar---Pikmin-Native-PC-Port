#pragma once
#include "pc_p2_original_egg.h"
#include "pc_p2_original_resource_contents.h"
#include "pc_p2_original_egg_snapshot.h"
class BTeki;class Graphics;class CollEvent;struct Matrix4f;
namespace p2original { namespace egg {
// Literal ShapeMapMgr/getCurrTri highest positive-Y static source triangle.
// Egg's query carries y+20, but Sys::Triangle::insideXZ projects the plane and
// GridDivider chooses highest Y without a below-ceiling filter.
bool sourceGroundHeight(Position,float&,std::string&);
// Exact source contents births/absorption and source Egg effects are supplied
// by their owners. Missing receivers/assets refuse admission, never proxy.
class Services : public p2originalresource::Engine {
public:
 virtual bool contentsReady(const P2EggConfig&,const p2originalresource::ContentsRequirements&,std::string&)=0;
 // Actual NativeHost supplies this authority scope before a fresh genItem draw.
 // Preflight mutable metadata/retained limits here; callback births can inspect
 // the same journal's pending record. False must leave no active scope.
 // rootSource is the real field37 or captured parent16, never a new cargo UID.
 virtual bool beginContents(const Host&,const p2originalresource::SourceIdentity&,
                            unsigned rootSource,const P2EggConfig& config,
                            const P2EggVec3&,const p2originalresource::EggContents&,
                            std::string& e){
  p2originalresource::ContentsRequirements required;
  if(!p2originalresource::requirements(config,required,e))return false;
  if(rootSource!=16&&rootSource!=37){e="Egg contents scope source is invalid";return false;}
  if(required.pelletOne||required.pelletFive){e="Egg numeric scoped consumer is not installed";return false;}
  e.clear();return true;
 }
 // Called exactly once after admitted generation, including exceptional exit.
 // Drop borrowed host/journal scope references; do not replay/retire rewards.
 virtual void endContents()noexcept{}
 virtual bool breakEffectsReady(std::string& e){e.clear();return false;}
 virtual bool capturedIdentity(Creature*,std::string&,std::string&)=0;
 virtual bool capturedSourceIdentity(Creature*,p2originalresource::SourceIdentity&,std::string&)=0;
 virtual bool sourceIdentity(const Host&,p2originalresource::SourceIdentity&,std::string&)=0;
 virtual bool groundHeight(Position query,float& y,std::string& e){return sourceGroundHeight(query,y,e);}
 virtual bool breakEffects(Creature*,std::string& e){e.clear();return true;}
};
class Native {
public:
 explicit Native(Services&);~Native();
 Native(Services&,ActorRegistry&);
 Native(const Native&)=delete;Native& operator=(const Native&)=delete;
 Provider& provider();
 bool owns(const Creature*)const;
 bool tick(BTeki*,float,std::string&);
 bool draw(BTeki*,Graphics&,const Matrix4f& view,std::string&);
 // Actual source world matrix, including a borrowed authored capture matrix.
 bool world(BTeki*,Matrix4f&,std::string&)const;
 bool identity(BTeki*,p2originalresource::SourceIdentity&,std::string&)const;
 // Transient graph inspection only; neither pointer belongs in saved state.
 bool captureLink(BTeki*,Creature*& actualParent,void*& actualMatrix,std::string&)const;
 // Read is atomic. Preflight does not mutate; all actor snapshots can validate
 // before the checkpoint owner applies the transaction. Apply revalidates.
 bool snapshot(BTeki*,Snapshot&,std::string&)const;
 bool preflight(BTeki*,const Snapshot&,std::string&)const;
 bool apply(BTeki*,const Snapshot&,std::string&);
 bool allocateRestored(const CatalogRow&,Generator*,const Snapshot&,BTeki*&,std::string&);
 bool allocateRestoredCargo(const CatalogRow& parentRow,const Snapshot&,BTeki*&,std::string&);
 bool preflightAllocateRestored(const CatalogRow&,Generator*,const Snapshot&,std::string&)const;
 bool preflightAllocateRestoredCargo(const CatalogRow& parentRow,const Snapshot&,std::string&)const;
 bool bindRestoredCapture(BTeki*,Creature* actualParent,void* actualMatrix,std::string&);
 void publishRestored() noexcept;
 bool preflightPublishRestored(std::string&)const;
 bool abortRestored(std::string&);
 void forget(BTeki*);
 p2originalresource::EggContents& contents();
private:
 struct Impl;std::unique_ptr<Impl> m;
};
} }
bool pc_p2_original_egg_update(BTeki*);
bool pc_p2_original_egg_refresh(BTeki*,Graphics&);
// Ownership return suppresses inherited chassis callbacks even on rejection.
bool pc_p2_original_egg_damage(BTeki*,float damage,float flick,bool& accepted);
bool pc_p2_original_egg_press(BTeki*,bool& accepted);
bool pc_p2_original_egg_bounce(BTeki*);
bool pc_p2_original_egg_collision(BTeki*,const CollEvent&);
void pc_p2_original_egg_forget(BTeki*);
