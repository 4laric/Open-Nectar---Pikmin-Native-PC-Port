#include "pc_midday_reference_catalog.h"
#include <cstdlib>
#include <iostream>
using namespace pc_midday;
int checks=0;void check(bool b){++checks;if(!b){std::cerr<<"catalog check failed "<<checks<<"\n";std::exit(1);}}
struct Left {virtual ~Left()=default;int value=1;};struct Right {virtual ~Right()=default;int value=2;};struct Multi:Left,Right{};
int main(){
 std::string e;SceneReferenceCatalog catalog({1,2});Multi actor;Right* adjusted=static_cast<Right*>(&actor);
 const LogicalRef root{1,0,0},other{2,0,0},part{1,0,4},foreignPart{2,0,4},resource{0,55,0};
 check(catalog.declare({root,RefKind::Creature,FieldCategory::Reference,"Multi",CatalogOrigin::ActorRoot,&actor,0,true},e));
 check(catalog.declare({root,RefKind::Creature,FieldCategory::Reference,"Right",CatalogOrigin::ActorRoot,adjusted,0,true},e));
 check(catalog.declare({other,RefKind::Creature,FieldCategory::Reference,"Multi",CatalogOrigin::ActorRoot,nullptr,0,false},e));
 check(catalog.declare({part,RefKind::AnimListener,FieldCategory::Reference,"Listener",CatalogOrigin::ActorSubobject,nullptr,0,false},e));
 check(catalog.declare({foreignPart,RefKind::AnimListener,FieldCategory::Reference,"Listener",CatalogOrigin::ActorSubobject,nullptr,0,false},e));
 check(catalog.declare({resource,RefKind::Animation,FieldCategory::Reference,"Clip",CatalogOrigin::Content,nullptr,0,false},e));
 auto role=[](u64,const FieldSchema&,const LogicalRef&,std::string& err){err.clear();return true;};
 std::vector<FieldSchema> schema={FieldSchema::ref("self",RefKind::Creature,false,"Right",ReferenceOwnership::Self),
  FieldSchema::ref("optional",RefKind::Creature,true,"Multi"),FieldSchema::ref("part",RefKind::AnimListener,false,"Listener",ReferenceOwnership::ActorSubobject),
  FieldSchema::ref("content",RefKind::Animation,false,"Clip",ReferenceOwnership::Content),
  FieldSchema::ref("linked",RefKind::AnimListener,false,"Listener",ReferenceOwnership::ActorSubobject,"captain")};
 SceneReferenceResolver resolver(1,catalog,schema,role,{{"captain",other}});
 check(resolver.validateTyped(schema[0],root,e));check(!resolver.validateTyped(schema[0],other,e));
 void* output=nullptr;check(resolver.resolve("self",RefKind::Creature,root,output,e)&&output==adjusted&&output!=static_cast<void*>(&actor));
 check(!resolver.resolve("unknown",RefKind::Creature,root,output,e));check(!resolver.resolve("self",RefKind::Animation,root,output,e));
 check(!resolver.validateTyped(FieldSchema::ref("self",RefKind::Creature,false,"Multi",ReferenceOwnership::Self),root,e));
 check(resolver.validateTyped(schema[1],{},e));check(!resolver.validateTyped(schema[0],{},e));
 check(resolver.validateTyped(schema[2],part,e));check(!resolver.validateTyped(schema[2],foreignPart,e));
 check(resolver.validateTyped(schema[4],foreignPart,e));check(!resolver.validateTyped(schema[4],part,e));
 SceneReferenceResolver missingOwner(1,catalog,schema,role);check(!missingOwner.validateTyped(schema[4],foreignPart,e));
 check(resolver.validateTyped(schema[3],resource,e));check(!resolver.resolve("content",RefKind::Animation,resource,output,e));
 SceneReferenceResolver unavailable(1,catalog,schema,{});check(!unavailable.validateTyped(schema[0],root,e));
 RestoreGate g{true,true,true,true,true,true,true};int listener=1,clip=2;
 RestoreGate bad=g;bad.paused=false;check(!catalog.bind(part,RefKind::AnimListener,FieldCategory::Reference,"Listener",&listener,0,bad,e));
 check(!resolver.resolve("part",RefKind::AnimListener,part,output,e));
 check(catalog.bind(part,RefKind::AnimListener,FieldCategory::Reference,"Listener",&listener,0,g,e));
 check(resolver.resolve("part",RefKind::AnimListener,part,output,e)&&output==&listener);
 check(!catalog.bind(part,RefKind::AnimListener,FieldCategory::Reference,"Listener",&listener,0,g,e));
 check(catalog.bind(resource,RefKind::Animation,FieldCategory::Reference,"Clip",&clip,0,g,e));
 check(resolver.resolve("content",RefKind::Animation,resource,output,e)&&output==&clip);
 const LogicalRef handle{1,0,8},token{1,0,9};
 check(catalog.declare({handle,RefKind::Path,FieldCategory::Handle,"Client",CatalogOrigin::ActorSubobject,nullptr,12,true},e));
 check(catalog.declare({token,RefKind::SlotListener,FieldCategory::Token64,"Serial",CatalogOrigin::ActorSubobject,nullptr,0xfedcba9876543210ULL,true},e));
 auto handles=FieldSchema::handle("handle",RefKind::Path,true,"Client",ReferenceOwnership::ActorSubobject);
 auto tokens=FieldSchema::token64("token",RefKind::SlotListener,false,"Serial",ReferenceOwnership::ActorSubobject);
 SceneReferenceResolver numbers(1,catalog,{handles,tokens},role);u32 h=0;u64 t=0;LogicalRef found;
 check(numbers.resolveHandle("handle",RefKind::Path,handle,h,e)&&h==12);
 check(numbers.resolveToken("token",RefKind::SlotListener,token,t,e)&&t==0xfedcba9876543210ULL);
 check(numbers.identifyToken("token",RefKind::SlotListener,t,found,e)&&found.owner==1&&found.slot==9);
 check(numbers.resolveHandle("handle",RefKind::Path,{},h,e)&&!h);
 check(!numbers.resolveToken("token",RefKind::SlotListener,{},t,e));
 check(!catalog.declare({{3,0,0},RefKind::Creature,FieldCategory::Reference,"Unknown",CatalogOrigin::ActorRoot,nullptr,0,false},e));
 const LogicalRef collision{2,0,11};check(catalog.declare({collision,RefKind::Creature,FieldCategory::Reference,"Other",CatalogOrigin::ActorSubobject,adjusted,0,true},e));
 check(!resolver.identify("self",RefKind::Creature,adjusted,found,e));
 check(catalog.declare({{2,0,9},RefKind::SlotListener,FieldCategory::Token64,"Serial",CatalogOrigin::ActorSubobject,nullptr,0xfedcba9876543210ULL,true},e));
 check(numbers.identifyToken("token",RefKind::SlotListener,0xfedcba9876543210ULL,found,e)&&found.owner==1);
 SceneReferenceCatalog transactional({1});CatalogAlias a{{1,0,0},RefKind::Creature,FieldCategory::Reference,"Multi",CatalogOrigin::ActorRoot,&actor,0,true};
 check(!transactional.declareAll({a,a},e)&&!transactional.lookup(root,RefKind::Creature,FieldCategory::Reference,"Multi"));
 auto invalid=schema[1];invalid.ownership=ReferenceOwnership(999);SceneReferenceResolver capture(1,catalog,{},role);
 check(!capture.validateTyped(invalid,{},e));
 std::cout<<checks<<" scene catalog controls PASS\n";
}
