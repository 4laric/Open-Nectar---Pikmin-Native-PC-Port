#include "pc_midday_reference_catalog.h"
#include <cstdlib>
#include <iostream>
using namespace pc_midday;
int checks=0;void check(bool b){++checks;if(!b){std::cerr<<"catalog check failed "<<checks<<"\n";std::exit(1);}}
struct Left {virtual ~Left()=default;int value=1;};struct Right {virtual ~Right()=default;int value=2;};struct Multi:Left,Right{};
int main(){
 std::string e;SceneReferenceCatalog catalog({1,2},{55,56});Multi actor;Right* adjusted=static_cast<Right*>(&actor);
 const LogicalRef root{1,0,0},other{2,0,0},part{1,0,4},foreignPart{2,0,4},resource{0,55,0};
 check(catalog.declare({root,RefKind::Creature,FieldCategory::Reference,"Multi",CatalogOrigin::ActorRoot,&actor,0,true},e));
 check(catalog.declare({root,RefKind::Creature,FieldCategory::Reference,"Right",CatalogOrigin::ActorRoot,adjusted,0,true},e));
 check(catalog.declare({other,RefKind::Creature,FieldCategory::Reference,"Multi",CatalogOrigin::ActorRoot,nullptr,0,false},e));
 check(catalog.declare({part,RefKind::AnimListener,FieldCategory::Reference,"Listener",CatalogOrigin::ActorSubobject,nullptr,0,false},e));
 check(catalog.declare({foreignPart,RefKind::AnimListener,FieldCategory::Reference,"Listener",CatalogOrigin::ActorSubobject,nullptr,0,false},e));
 check(catalog.declare({resource,RefKind::Animation,FieldCategory::Reference,"Clip",CatalogOrigin::Content,nullptr,0,false},e));
 auto role=[](SceneSubject,const FieldSchema&,const LogicalRef&,std::string& err){err.clear();return true;};
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
 // Resources are canonical allocations with explicit membership; interfaces
 // retain adjusted addresses and owned array members retain exact slots.
 Multi material;Right* materialInterface=static_cast<Right*>(&material);int member=3;
 const LogicalRef resourceSelf{0,55,1},resourcePart{0,55,2},foreignResource{0,56,1};
 check(catalog.declare({resourceSelf,RefKind::Creature,FieldCategory::Reference,"Right",CatalogOrigin::ResourceSelf,materialInterface,0,true},e));
 check(catalog.declare({resourcePart,RefKind::AnimListener,FieldCategory::Reference,"Listener",CatalogOrigin::ResourceSubobject,&member,0,true},e));
 check(catalog.declare({foreignResource,RefKind::Creature,FieldCategory::Reference,"Right",CatalogOrigin::ResourceSelf,nullptr,0,false},e));
 check(!catalog.declare({{0,999,1},RefKind::Creature,FieldCategory::Reference,"Right",CatalogOrigin::ResourceSelf,nullptr,0,false},e));
 check(!catalog.declare({{1,55,3},RefKind::Creature,FieldCategory::Reference,"Right",CatalogOrigin::ResourceSubobject,nullptr,0,false},e));
 check(!catalog.declare({{0,55,0},RefKind::Creature,FieldCategory::Reference,"Member",CatalogOrigin::ResourceSubobject,nullptr,0,false},e));
 check(!catalog.declare({{1,55,3},RefKind::Creature,FieldCategory::Reference,"Right",CatalogOrigin::ActorSubobject,nullptr,0,false},e));
 auto selfResource=FieldSchema::ref("rself",RefKind::Creature,true,"Right",ReferenceOwnership::ResourceSelf);
 auto partResource=FieldSchema::ref("rpart",RefKind::AnimListener,false,"Listener",ReferenceOwnership::ResourceSubobject);
 auto nextResource=FieldSchema::ref("next",RefKind::Creature,true,"Right",ReferenceOwnership::AnyLive);
 SceneSubject observed{};auto resourceRole=[&](SceneSubject subject,const FieldSchema&,const LogicalRef&,std::string& err){observed=subject;err.clear();return true;};
 SceneReferenceResolver resources(SceneSubject{0,55},catalog,{selfResource,partResource,nextResource},resourceRole);
 check(resources.resolve("rself",RefKind::Creature,resourceSelf,output,e)&&output==materialInterface&&output!=static_cast<void*>(&material));
 check(!observed.actor&&observed.resource==55);
 check(resources.validateTyped(selfResource,{},e));
 check(!resources.validateTyped(selfResource,foreignResource,e));
 check(resources.resolve("rpart",RefKind::AnimListener,resourcePart,output,e)&&output==&member);
 check(!resources.validateTyped(partResource,resourceSelf,e));
 check(!resources.validateTyped(partResource,{0,55,999},e));
 check(resources.validateTyped(nextResource,foreignResource,e));
 check(!resources.resolve("next",RefKind::Creature,foreignResource,output,e));
 check(!resources.validateTyped(FieldSchema::ref("rself",RefKind::Creature,true,"Multi",ReferenceOwnership::ResourceSelf),resourceSelf,e));
 SceneReferenceResolver actorResourceRole(1,catalog,{selfResource},role);
 check(!actorResourceRole.validateTyped(selfResource,resourceSelf,e));
 check(!actorResourceRole.validateTyped(selfResource,{},e));
 SceneReferenceResolver resourceActorRole(SceneSubject{0,55},catalog,{schema[0]},role);
 check(!resourceActorRole.validateTyped(schema[0],root,e));
 SceneReferenceResolver bothSubjects(SceneSubject{1,55},catalog,{selfResource},role);
 check(!bothSubjects.validateTyped(selfResource,{},e));
 SceneReferenceResolver noSubject(SceneSubject{},catalog,{selfResource},role);
 check(!noSubject.validateTyped(selfResource,{},e));
 SceneReferenceResolver unknownResource(SceneSubject{0,999},catalog,{selfResource},role);
 check(!unknownResource.validateTyped(selfResource,{},e));
 SceneReferenceResolver unavailableResource(SceneSubject{0,55},catalog,{selfResource},{});
 check(!unavailableResource.validateTyped(selfResource,resourceSelf,e));
 const LogicalRef resourceToken{0,55,8},foreignToken{0,56,8};
 check(catalog.declare({resourceToken,RefKind::SlotListener,FieldCategory::Token64,"Serial",CatalogOrigin::ResourceSubobject,nullptr,42,true},e));
 check(catalog.declare({foreignToken,RefKind::SlotListener,FieldCategory::Token64,"Serial",CatalogOrigin::ResourceSubobject,nullptr,42,true},e));
 auto rt=FieldSchema::token64("rt",RefKind::SlotListener,false,"Serial",ReferenceOwnership::ResourceSubobject);
 SceneReferenceResolver resourceNumbers(SceneSubject{0,55},catalog,{rt},role);
 check(resourceNumbers.identifyToken("rt",RefKind::SlotListener,42,found,e)&&!found.owner&&found.resource==55);
 check(resourceNumbers.resolveToken("rt",RefKind::SlotListener,resourceToken,t,e)&&t==42);
 check(!resourceNumbers.validateTyped(rt,foreignToken,e));
 auto invalidOwnerLink=selfResource;invalidOwnerLink.ownerLink="captain";
 SceneReferenceResolver invalidResourceLink(SceneSubject{0,55},catalog,{invalidOwnerLink},role,{{"captain",other}});
 check(!invalidResourceLink.validateTyped(invalidOwnerLink,{},e));
 check(!catalog.identify(RefKind::SlotListener,FieldCategory::Token64,nullptr,0,found,e));
 check(!catalog.identify(RefKind::SlotListener,FieldCategory::Token64,nullptr,0,found,e,SceneSubject{1,55}));
 check(!catalog.identify(RefKind::SlotListener,FieldCategory::Token64,nullptr,0,found,e,SceneSubject{0,999}));
 check(!catalog.identify(RefKind::SlotListener,FieldCategory::Scalar,nullptr,0,found,e,SceneSubject{0,55}));
 std::cout<<checks<<" scene catalog controls PASS\n";
}
