#pragma once
#include "pc_midday_actor_archive.h"
#include "pc_midday_restore.h"
namespace pc_midday {
enum class CatalogOrigin {ActorRoot,ActorSubobject,Content};
struct CatalogAlias {
 LogicalRef id;RefKind kind;FieldCategory category;std::string type;CatalogOrigin origin;
 void* address=nullptr;u64 number=0;bool bound=false;
};
// Source/factory metadata only, never saved RTTI. Aliases use actual adjusted
// pointers. Declare the entire content/actor/subobject topology before begin;
// install fresh addresses/handles only after the native factory allocates them.
class SceneReferenceCatalog {
 std::set<u64> actors_;std::vector<CatalogAlias> aliases_;
public:
 explicit SceneReferenceCatalog(std::set<u64> actors):actors_(std::move(actors)){}
 bool declare(const CatalogAlias&,std::string&);
 bool declareAll(const std::vector<CatalogAlias>&,std::string&);
 bool bind(const LogicalRef&,RefKind,FieldCategory,const std::string& type,void*,u64,const RestoreGate&,std::string&);
 const CatalogAlias* lookup(const LogicalRef&,RefKind,FieldCategory,const std::string&)const;
 bool identify(RefKind,FieldCategory,const void*,u64,LogicalRef&,std::string&,u64 numericOwner=0)const;
 bool live(u64 id)const {return id&&actors_.count(id);}
};
// Relationship checks are required even after concrete alias validation: e.g.
// formation plate belongs to the action's saved Navi, not just any live actor.
using SceneRoleCheck=std::function<bool(u64 subject,const FieldSchema&,const LogicalRef&,std::string&)>;
class SceneReferenceResolver final:public LogicalResolver {
 u64 subject_;const SceneReferenceCatalog& catalog_;std::map<std::string,FieldSchema> schema_;SceneRoleCheck role_;
 std::map<std::string,LogicalRef> ownerRoots_;
 bool valid_=true;
 const FieldSchema* field(const char*,RefKind,FieldCategory,std::string&)const;
 bool typed(const FieldSchema&,const LogicalRef&,std::string&)const;
 bool number(const char*,RefKind,FieldCategory,const LogicalRef&,u64&,std::string&);
public:
 // ownerRoots comes from exact pinned payload owner-link fields, or actual
 // read-only native owner pointers during capture; never guessed from IDs.
 SceneReferenceResolver(u64,const SceneReferenceCatalog&,const std::vector<FieldSchema>&,SceneRoleCheck,
                        std::map<std::string,LogicalRef> ownerRoots={});
 // Capture may identify unique adjusted pointers before its dynamic schema is
 // known; the completed typed schema must then pass validateTyped. Restore
 // constructs a fresh resolver with the complete compiled schema pinned.
 bool identify(const char*,RefKind,const void*,LogicalRef&,std::string&)override;
 bool identifyHandle(const char*,RefKind,u32,LogicalRef&,std::string&)override;
 bool identifyToken(const char*,RefKind,u64,LogicalRef&,std::string&)override;
 bool validate(const char*,RefKind,const LogicalRef&,std::string&)const override;
 bool validateTyped(const FieldSchema&,const LogicalRef&,std::string&)const override;
 bool resolve(const char*,RefKind,const LogicalRef&,void*&,std::string&)override;
 bool resolveHandle(const char*,RefKind,const LogicalRef&,u32&,std::string&)override;
 bool resolveToken(const char*,RefKind,const LogicalRef&,u64&,std::string&)override;
};
// Typed aliases from native factory roots; static_cast performs real adjustment.
bool declareNaviRoot(SceneReferenceCatalog&,u64,Navi*,std::string&);
bool declarePikiRoot(SceneReferenceCatalog&,u64,Piki*,std::string&);
}
