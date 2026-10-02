#include "pc_midday_effect.h"
#include <iostream>
#include <cstdlib>
using namespace pc_midday;
void check(bool b,const char*m){if(!b){std::cerr<<m<<'\n';std::exit(1);}}
struct Resolver:LogicalResolver{
 bool identify(const char*,RefKind,const void*,LogicalRef&,std::string&)override{return false;}
 bool validate(const char*,RefKind,const LogicalRef&r,std::string&)const override{return r.owner==1;}
 bool validateTyped(const FieldSchema&s,const LogicalRef&r,std::string&)const override{return !s.targetType.empty()&&((!r.owner&&!r.resource&&!r.slot)?s.nullable:(((s.ownership==ReferenceOwnership::ResourceSelf||s.ownership==ReferenceOwnership::ResourceSubobject)?(!r.owner&&r.resource==1):r.owner==1)));}
 bool resolve(const char*,RefKind,const LogicalRef&,void*&,std::string&)override{return false;}
 bool identifyHandle(const char*,RefKind,u32,LogicalRef&,std::string&)override{return false;}
 bool resolveHandle(const char*,RefKind,const LogicalRef&,u32&,std::string&)override{return false;}
};
int main(){Resolver r;int tests=0;
for(int kind=1;kind<=15;++kind){ActorFields f;ActorField k;k.scalar=ScalarKind::S32;k.bits=kind;f["kind"]=k;k.bits=1;f["version"]=k;for(auto id:{"effectId","effectIdA","effectIdB"}){k.bits=0;if((kind==5&&std::string(id)=="effectId")||((kind==13||kind==14)&&std::string(id)!="effectId"))f[id]=k;}
 std::string e;std::vector<FieldSchema>s;check(effect_schema(f,EffectRecordKind(kind),"",s,e),"effect schema seed");for(auto&x:s){if(f.count(x.key))continue;ActorField v;v.category=x.category;v.scalar=x.scalar;v.reference=x.reference;if(x.category!=FieldCategory::Scalar&&!x.nullable){if(x.ownership==ReferenceOwnership::ResourceSelf||x.ownership==ReferenceOwnership::ResourceSubobject)v.target.resource=1;else v.target.owner=1;}f[x.key]=v;}check(validate_actor_fields(f,s,r,e),"valid effect fields");++tests;
 auto bad=f;bad["kind"].bits=16;s.clear();e.clear();check(!effect_schema(bad,EffectRecordKind(kind),"",s,e),"wrong callback factory accepted");++tests;
 bad=f;bad["callback.pair"].target={};s.clear();e.clear();check(effect_schema(bad,EffectRecordKind(kind),"",s,e)&&!validate_actor_fields(bad,s,r,e),"missing adjusted callback view accepted");++tests;
}
std::cout<<tests<<" effect schema controls PASS (synthetic, not gameplay)\n";}
