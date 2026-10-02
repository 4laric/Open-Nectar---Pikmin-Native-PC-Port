#include "pc_midday_reference_catalog.h"
#include <limits>
namespace pc_midday {
namespace {
bool absent(const LogicalRef& r){return !r.owner&&!r.resource&&!r.slot;}
bool same(const LogicalRef& a,const LogicalRef& b){return a.owner==b.owner&&a.resource==b.resource&&a.slot==b.slot;}
bool referenceCategory(FieldCategory c){return c==FieldCategory::Reference||c==FieldCategory::Handle||c==FieldCategory::Token64;}
bool ownership(ReferenceOwnership o){return o==ReferenceOwnership::AnyLive||o==ReferenceOwnership::Self||o==ReferenceOwnership::ActorSubobject||o==ReferenceOwnership::Content||o==ReferenceOwnership::ResourceSelf||o==ReferenceOwnership::ResourceSubobject;}
bool fail(std::string& e,const char* m){e=m;return false;}
}
const CatalogAlias* SceneReferenceCatalog::lookup(const LogicalRef& id,RefKind k,FieldCategory c,const std::string& t)const{
 for(const auto& a:aliases_)if(same(a.id,id)&&a.kind==k&&a.category==c&&a.type==t)return &a;
 return nullptr;
}
bool SceneReferenceCatalog::declare(const CatalogAlias& a,std::string& e){
 if(aliases_.size()>=262144||a.type.empty()||a.type.size()>128||static_cast<unsigned>(a.kind)>=static_cast<unsigned>(RefKind::Count)||!referenceCategory(a.category))return fail(e,"invalid/bounded catalog alias");
 bool identity=false;
 switch(a.origin){
 case CatalogOrigin::ActorRoot:identity=live(a.id.owner)&&!a.id.resource&&!a.id.slot;break;
 case CatalogOrigin::ActorSubobject:identity=live(a.id.owner)&&!a.id.resource&&a.id.slot;break;
 case CatalogOrigin::Content:identity=!a.id.owner&&resource(a.id.resource);break;
 case CatalogOrigin::ResourceSelf:identity=!a.id.owner&&resource(a.id.resource);break;
 case CatalogOrigin::ResourceSubobject:identity=!a.id.owner&&resource(a.id.resource)&&a.id.slot;break;
 default:break;
 }
 if(!identity||lookup(a.id,a.kind,a.category,a.type))return fail(e,"invalid or duplicate catalog identity/type");
 if(a.category==FieldCategory::Reference){if(a.number||(a.bound&&!a.address)||(!a.bound&&a.address))return fail(e,"invalid pointer alias binding");}
 else if(a.address||(!a.bound&&a.number)||(a.category==FieldCategory::Handle&&a.number>std::numeric_limits<u32>::max()))return fail(e,"invalid numeric alias binding");
 aliases_.push_back(a);e.clear();return true;
}
bool SceneReferenceCatalog::declareAll(const std::vector<CatalogAlias>& aliases,std::string& e){
 SceneReferenceCatalog stage=*this;
 for(const auto& alias:aliases)if(!stage.declare(alias,e))return false;
 *this=std::move(stage);e.clear();return true;
}
bool SceneReferenceCatalog::bind(const LogicalRef& id,RefKind k,FieldCategory c,const std::string& t,void* pointer,u64 n,const RestoreGate& g,std::string& e){
 if(!g.freshProcess||!g.paused||!g.zeroInput||!g.birthEffectsSuppressed||!g.rewardsSuppressed||!g.rngDrawsSuppressed||!g.audioVoicesSuppressed)return fail(e,"reference catalog binding requires full fresh fence");
 for(auto& a:aliases_)if(same(a.id,id)&&a.kind==k&&a.category==c&&a.type==t){
  if(a.bound)return fail(e,"catalog alias already bound");
  if(c==FieldCategory::Reference ? (!pointer||n) : (pointer||(c==FieldCategory::Handle&&n>std::numeric_limits<u32>::max())))return fail(e,"invalid fresh catalog address/handle");
  a.address=pointer;a.number=n;a.bound=true;e.clear();return true;
 }
 return fail(e,"fresh reference has no declared concrete alias");
}
bool SceneReferenceCatalog::identify(RefKind k,FieldCategory c,const void* pointer,u64 n,LogicalRef& out,std::string& e,SceneSubject numericSubject)const{
 if(!referenceCategory(c)||static_cast<unsigned>(k)>=static_cast<unsigned>(RefKind::Count))return fail(e,"invalid reference capture category/kind");
 if((numericSubject.actor&&numericSubject.resource)||(numericSubject.actor&&!live(numericSubject.actor))||(numericSubject.resource&&!resource(numericSubject.resource))||
    (c!=FieldCategory::Reference&&!numericSubject.actor&&!numericSubject.resource))return fail(e,"numeric capture requires one registered subject");
 bool found=false;LogicalRef selected;
 for(const auto& a:aliases_)if(a.bound&&a.kind==k&&a.category==c&&(!numericSubject.actor||a.id.owner==numericSubject.actor)&&(!numericSubject.resource||(!a.id.owner&&a.id.resource==numericSubject.resource))&&(c==FieldCategory::Reference?a.address==pointer:a.number==n)){
  if(found&&!same(selected,a.id))return fail(e,"ambiguous native reference identity");
  selected=a.id;found=true;
 }
 if(!found){if(c!=FieldCategory::Reference&&!n){out={};e.clear();return true;}return fail(e,"native reference missing from complete catalog");}
 out=selected;e.clear();return true;
}
SceneReferenceResolver::SceneReferenceResolver(u64 subject,const SceneReferenceCatalog& c,const std::vector<FieldSchema>& schema,SceneRoleCheck role,std::map<std::string,LogicalRef> owners):SceneReferenceResolver(SceneSubject{subject,0},c,schema,std::move(role),std::move(owners)){}
SceneReferenceResolver::SceneReferenceResolver(SceneSubject subject,const SceneReferenceCatalog& c,const std::vector<FieldSchema>& schema,SceneRoleCheck role,std::map<std::string,LogicalRef> owners):subject_(subject),catalog_(c),role_(std::move(role)),ownerRoots_(std::move(owners)){
 if((bool(subject.actor)==bool(subject.resource))||(subject.actor&&!catalog_.live(subject.actor))||(subject.resource&&!catalog_.resource(subject.resource)))valid_=false;
 for(const auto& owner:ownerRoots_)if(owner.first.empty()||!catalog_.live(owner.second.owner)||owner.second.resource||owner.second.slot)valid_=false;
 for(const auto& s:schema)if(s.key.empty()||!schema_.emplace(s.key,s).second)valid_=false;
}
const FieldSchema* SceneReferenceResolver::field(const char* key,RefKind k,FieldCategory c,std::string& e)const{
 if(!valid_||!key){fail(e,"invalid scene subject/schema");return nullptr;}
 auto i=schema_.find(key);if(i==schema_.end()||i->second.reference!=k||i->second.category!=c){fail(e,"reference key/category/kind not in compiled schema");return nullptr;}
 return &i->second;
}
bool SceneReferenceResolver::typed(const FieldSchema& s,const LogicalRef& id,std::string& e)const{
 if(!valid_||!referenceCategory(s.category)||s.targetType.empty()||!ownership(s.ownership)||static_cast<unsigned>(s.reference)>=static_cast<unsigned>(RefKind::Count))return fail(e,"missing or invalid compiled reference contract");
 if(((s.ownership==ReferenceOwnership::Self||s.ownership==ReferenceOwnership::ActorSubobject)&&!subject_.actor)||
    ((s.ownership==ReferenceOwnership::ResourceSelf||s.ownership==ReferenceOwnership::ResourceSubobject)&&!subject_.resource))return fail(e,"compiled reference requires different subject context");
 if(!s.ownerLink.empty()&&(s.ownership!=ReferenceOwnership::ActorSubobject||!ownerRoots_.count(s.ownerLink)))return fail(e,"compiled owner link unavailable or invalid");
 if(absent(id))return s.nullable?(e.clear(),true):fail(e,"required typed reference absent");
 const auto* a=catalog_.lookup(id,s.reference,s.category,s.targetType);
 if(!a)return fail(e,"wrong concrete reference type or unknown identity");
 u64 expectedOwner=subject_.actor;
 if(!s.ownerLink.empty()){
  auto owner=ownerRoots_.find(s.ownerLink);
  if(s.ownership!=ReferenceOwnership::ActorSubobject||owner==ownerRoots_.end())return fail(e,"compiled owner link unavailable or invalid");
  expectedOwner=owner->second.owner;
 }
 if((id.owner&&!catalog_.live(id.owner))||
    (s.ownership==ReferenceOwnership::Self&&(a->origin!=CatalogOrigin::ActorRoot||id.owner!=subject_.actor))||
    (s.ownership==ReferenceOwnership::ActorSubobject&&(a->origin!=CatalogOrigin::ActorSubobject||id.owner!=expectedOwner))||
    (s.ownership==ReferenceOwnership::Content&&a->origin!=CatalogOrigin::Content)||
    (s.ownership==ReferenceOwnership::ResourceSelf&&(a->origin!=CatalogOrigin::ResourceSelf||id.owner||id.resource!=subject_.resource))||
    (s.ownership==ReferenceOwnership::ResourceSubobject&&(a->origin!=CatalogOrigin::ResourceSubobject||id.owner||id.resource!=subject_.resource)))return fail(e,"typed reference ownership mismatch");
 if(!role_)return fail(e,"complete scene relationship check unavailable");
 if(!role_(subject_,s,id,e)){if(e.empty())e="scene relationship check refused";return false;}
 e.clear();return true;
}
bool SceneReferenceResolver::validateTyped(const FieldSchema& s,const LogicalRef& id,std::string& e)const{
 if(!schema_.empty()){
  const auto* expected=field(s.key.c_str(),s.reference,s.category,e);if(!expected)return false;
  if(expected->targetType!=s.targetType||expected->ownership!=s.ownership||expected->nullable!=s.nullable||expected->ownerLink!=s.ownerLink)return fail(e,"compiled reference contract changed");
 }
 return typed(s,id,e);
}
bool SceneReferenceResolver::validate(const char* key,RefKind k,const LogicalRef& id,std::string& e)const{
 if(!valid_||!key)return fail(e,"invalid scene reference request");
 auto i=schema_.find(key);if(i==schema_.end()||i->second.reference!=k)return fail(e,"unknown compiled reference key/kind");
 return typed(i->second,id,e);
}
bool SceneReferenceResolver::identify(const char*,RefKind k,const void* p,LogicalRef& out,std::string& e){if(!valid_)return fail(e,"invalid capture subject/schema");return catalog_.identify(k,FieldCategory::Reference,p,0,out,e);}
bool SceneReferenceResolver::identifyHandle(const char*,RefKind k,u32 n,LogicalRef& out,std::string& e){if(!valid_)return fail(e,"invalid capture subject/schema");return catalog_.identify(k,FieldCategory::Handle,nullptr,n,out,e,subject_);}
bool SceneReferenceResolver::identifyToken(const char*,RefKind k,u64 n,LogicalRef& out,std::string& e){if(!valid_)return fail(e,"invalid capture subject/schema");return catalog_.identify(k,FieldCategory::Token64,nullptr,n,out,e,subject_);}
bool SceneReferenceResolver::resolve(const char* key,RefKind k,const LogicalRef& id,void*& out,std::string& e){
 const auto* s=field(key,k,FieldCategory::Reference,e);if(!s||!typed(*s,id,e))return false;
 if(absent(id)){out=nullptr;return true;}
 const auto* a=catalog_.lookup(id,k,s->category,s->targetType);if(!a->bound)return fail(e,"typed pointer not yet allocated/bound");
 out=a->address;e.clear();return true;
}
bool SceneReferenceResolver::number(const char* key,RefKind k,FieldCategory c,const LogicalRef& id,u64& out,std::string& e){
 const auto* s=field(key,k,c,e);if(!s||!typed(*s,id,e))return false;
 if(absent(id)){out=0;return true;}
 const auto* a=catalog_.lookup(id,k,c,s->targetType);if(!a->bound)return fail(e,"typed native handle/token not yet allocated/bound");
 out=a->number;e.clear();return true;
}
bool SceneReferenceResolver::resolveHandle(const char* key,RefKind k,const LogicalRef& id,u32& out,std::string& e){u64 n=0;if(!number(key,k,FieldCategory::Handle,id,n,e))return false;out=u32(n);return true;}
bool SceneReferenceResolver::resolveToken(const char* key,RefKind k,const LogicalRef& id,u64& out,std::string& e){return number(key,k,FieldCategory::Token64,id,out,e);}
}
