// Actual-header/production-helper controls. No ViewPiki object or engine is fabricated.
#include "ViewPiki.h"
#include "pc_midday_view_piki.h"
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <type_traits>
using namespace pc_midday;
static_assert(std::is_same<decltype(ViewPiki::mPikiShape),PikiShapeObject*>::value,"actual shape member type");
static_assert(std::is_same<decltype(ViewPiki::mHappaModel),Shape*>::value,"actual leaf member type");
static_assert(std::is_same<decltype(ViewPiki::mLastEffectPosition),Vector3f>::value,"actual position member type");
namespace {
int checks=0;
void check(bool ok,const char* why){++checks;if(!ok){std::fprintf(stderr,"FAIL ViewPiki fields %s\n",why);std::exit(1);}}
// Address-only resolver witnesses. Never dereferenced or claimed to be engine resources.
// The test exercises typed pointer transfer; actual content allocation remains scene-owned.
alignas(std::max_align_t) unsigned char witnesses[4][64]{};
struct Resolver final:LogicalResolver {
 PikiShapeObject* sourceShape=reinterpret_cast<PikiShapeObject*>(witnesses[0]);
 Shape* sourceLeaf=reinterpret_cast<Shape*>(witnesses[1]);
 PikiShapeObject* targetShape=reinterpret_cast<PikiShapeObject*>(witnesses[2]);
 Shape* targetLeaf=reinterpret_cast<Shape*>(witnesses[3]);
 bool failLeafResolve=false;
 bool role(const char* key,RefKind kind,const LogicalRef& ref)const{
  return kind==RefKind::Shape&&!ref.owner&&!ref.slot&&
   ((std::string(key)=="viewPiki.pikiShape"&&ref.resource==11)||(std::string(key)=="viewPiki.happaModel"&&ref.resource==22));
 }
 bool identify(const char* key,RefKind kind,const void* p,LogicalRef& r,std::string& e)override{
  if(std::string(key)=="viewPiki.pikiShape"&&(p==sourceShape||p==targetShape))r={0,11,0};
  else if(std::string(key)=="viewPiki.happaModel"&&(p==sourceLeaf||p==targetLeaf))r={0,22,0};
  else {e="unregistered typed resource address";return false;}
  return role(key,kind,r);
 }
 bool validate(const char* key,RefKind kind,const LogicalRef& r,std::string& e)const override{if(role(key,kind,r))return true;e="wrong declared resource role";return false;}
 bool validateTyped(const FieldSchema& d,const LogicalRef& r,std::string& e)const override{
  if(d.reference!=RefKind::Shape||d.ownership!=ReferenceOwnership::Content||
    (d.key=="viewPiki.pikiShape"?d.targetType!="PikiShapeObject":d.key!="viewPiki.happaModel"||d.targetType!="Shape")){e="wrong compiled resource descriptor";return false;}
  if(!r.owner&&!r.resource&&!r.slot)return d.nullable;
  return validate(d.key.c_str(),d.reference,r,e);
 }
 bool resolve(const char* key,RefKind kind,const LogicalRef& r,void*& p,std::string& e)override{
  if(!validate(key,kind,r,e))return false;
  if(r.resource==22&&failLeafResolve){e="late leaf resolution refused";return false;}
  p=r.resource==11?static_cast<void*>(targetShape):static_cast<void*>(targetLeaf);return true;
 }
 bool identifyHandle(const char*,RefKind,u32,LogicalRef&,std::string&)override{return false;}
 bool resolveHandle(const char*,RefKind,const LogicalRef&,u32&,std::string&)override{return false;}
};
bool same(const Vector3f& a,const Vector3f& b){return a.x==b.x&&a.y==b.y&&a.z==b.z;}
}
int main(){
 Resolver resolver;auto* shape=resolver.sourceShape;auto* leaf=resolver.sourceLeaf;Vector3f position;position.set(1.25f,-2.5f,3.75f);
 ActorFields original;std::string error;FieldArchive capture(Mode::Capture,original,resolver,error,0);
 check(view_piki_subtype_fields(shape,leaf,position,capture)&&capture.finish(),"production helper Capture");
 std::vector<FieldSchema> schema;check(view_piki_schema(original,schema,error)&&validate_actor_fields(original,schema,resolver,error),"actual helper keys/types equal schema");check(original.size()==6,"exact six fields");
 auto walk=[&](ActorFields& fields,Mode mode){error.clear();FieldArchive archive(mode,fields,resolver,error,0);return view_piki_subtype_fields(shape,leaf,position,archive)&&archive.finish();};
 Vector3f before;before.set(9,8,7);position=before;
 check(walk(original,Mode::Validate),"Validate valid record");check(shape==resolver.sourceShape&&leaf==resolver.sourceLeaf&&same(position,before),"Validate changes no fields");
 check(walk(original,Mode::Apply),"Apply valid record");check(shape==resolver.targetShape&&leaf==resolver.targetLeaf&&position.x==1.25f&&position.y==-2.5f&&position.z==3.75f,"different-address typed resource resolution and exact vector");
 ActorFields recaptured;FieldArchive again(Mode::Capture,recaptured,resolver,error,0);check(view_piki_subtype_fields(shape,leaf,position,again)&&again.finish(),"recapture rebound fields");ActorBytes a,b;check(encode_actor_fields(original,a,error)&&encode_actor_fields(recaptured,b,error)&&a==b,"stable identity bytes across resource addresses");
 auto refused=[&](ActorFields fields,const char* why){shape=resolver.sourceShape;leaf=resolver.sourceLeaf;position=before;check(!walk(fields,Mode::Apply),why);check(shape==resolver.sourceShape&&leaf==resolver.sourceLeaf&&same(position,before),"failed helper Apply is atomic");};
 auto bad=original;bad["viewPiki.lastEffectPosition.z"].bits=0x7fc00000;refused(bad,"late NaN refusal");
 check(!walk(bad,Mode::Validate)&&shape==resolver.sourceShape&&leaf==resolver.sourceLeaf&&same(position,before),"late Validate refusal leaves fields unchanged");
 bad=original;bad.erase("viewPiki.lastEffectPosition.z");refused(bad,"late missing component refusal");
 bad=original;bad["viewPiki.lastEffectPosition.z"].scalar=ScalarKind::U32;refused(bad,"late wrong scalar type refusal");
 bad=original;bad["viewPiki.happaModel"].target.resource=11;refused(bad,"leaf cannot bind PikiShapeObject resource");
 resolver.failLeafResolve=true;refused(original,"late resolver failure after shape resolution");resolver.failLeafResolve=false;
 bad=original;bad["viewPiki.happaModel"].target={};check(walk(bad,Mode::Apply)&&leaf==nullptr,"legitimate absent leaf binds");
 bad=original;bad["viewPiki.pikiShape"].target={};check(!validate_actor_fields(bad,schema,resolver,error),"required shape rejected before binding");
 bad=original;bad["viewPiki.pikiShape"].target.resource=22;check(!validate_actor_fields(bad,schema,resolver,error),"foreign declared type rejected before binding");
 shape=nullptr;ActorFields fresh;FieldArchive empty(Mode::Capture,fresh,resolver,error,0);check(!view_piki_subtype_fields(shape,leaf,position,empty)&&fresh.empty(),"uninitialized shape capture refuses before reading fields");
 std::printf("PASS ViewPiki actual-header helper controls %d engine_objects=0\n",checks);
}
