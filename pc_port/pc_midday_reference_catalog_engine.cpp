#include "pc_midday_reference_catalog.h"
#include "Navi.h"
#include "Piki.h"
namespace pc_midday {
bool declareNaviRoot(SceneReferenceCatalog& c,u64 id,Navi* p,std::string& e){
 return c.declareAll({{{id,0,0},RefKind::Creature,FieldCategory::Reference,"Navi",CatalogOrigin::ActorRoot,p,0,p!=nullptr},
                     {{id,0,0},RefKind::Creature,FieldCategory::Reference,"Creature",CatalogOrigin::ActorRoot,static_cast<Creature*>(p),0,p!=nullptr}},e);
}
bool declarePikiRoot(SceneReferenceCatalog& c,u64 id,Piki* p,std::string& e){
 return c.declareAll({{{id,0,0},RefKind::Creature,FieldCategory::Reference,"Piki",CatalogOrigin::ActorRoot,p,0,p!=nullptr},
                     {{id,0,0},RefKind::Creature,FieldCategory::Reference,"Creature",CatalogOrigin::ActorRoot,static_cast<Creature*>(p),0,p!=nullptr}},e);
}
}
