#include "pc_midday_collision.h"
#include <iostream>
#include <cstdlib>
using namespace pc_midday;
struct Resolver:LogicalResolver{
 bool identify(const char*,RefKind,const void*,LogicalRef&,std::string&)override{return false;}
 bool validate(const char*,RefKind,const LogicalRef&r,std::string&)const override{return r.owner==1;}
 bool validateTyped(const FieldSchema&s,const LogicalRef&r,std::string&)const override{return !s.targetType.empty()&&r.owner==1;}
 bool resolve(const char*,RefKind,const LogicalRef&,void*&,std::string&)override{return false;}
 bool identifyHandle(const char*,RefKind,u32,LogicalRef&,std::string&)override{return false;}
 bool resolveHandle(const char*,RefKind,const LogicalRef&,u32&,std::string&)override{return false;}
};
void check(bool b,const char*m){if(!b){std::cerr<<m<<'\n';std::exit(1);}}
ActorField num(ScalarKind k,u64 n){ActorField f;f.scalar=k;f.bits=n;return f;}
int main(){ActorFields f;f["c.count"]=num(ScalarKind::U16,2);f["c.capacity"]=num(ScalarKind::U16,10);ActorField id;id.category=FieldCategory::Reference;id.reference=RefKind::CollInfo;id.target={1,0,7};f["c.identity"]=id;
 for(int i=0;i<2;++i){auto p="c.part."+std::to_string(i)+".";f[p+"type"]=num(ScalarKind::U8,0);f[p+"next"]=num(ScalarKind::S16,65535);f[p+"child"]=num(ScalarKind::S16,65535);f[p+"parent"]=id;}
 std::string e;std::vector<FieldSchema>s;check(collision_schema(f,"c",s,e),"schema seed");for(auto&k:s){if(f.count(k.key))continue;ActorField v;v.category=k.category;v.scalar=k.scalar;v.reference=k.reference;if(k.category!=FieldCategory::Scalar&&!k.nullable)v.target.owner=1;f[k.key]=v;}
 Resolver r;check(validate_actor_fields(f,s,r,e),"valid collision payload");int n=1;
 auto reject=[&](ActorFields b){std::string why;std::vector<FieldSchema> bs;check(!collision_schema(b,"c",bs,why)||!validate_actor_fields(b,bs,r,why),"invalid collision accepted");++n;};
 auto b=f;b["c.part.0.child"].bits=0;reject(b);b=f;b["c.part.0.child"].bits=1;b["c.part.1.next"].bits=0;reject(b);b=f;b["c.part.0.next"].bits=2;reject(b);b=f;b["c.part.0.type"].bits=7;reject(b);b=f;b["c.part.0.parent"].target.slot=8;reject(b);b=f;b["c.capacity"].bits=1025;reject(b);b=f;b["c.part.0.descriptor"].target={};reject(b);b=f;b["c.part.0.radius"].bits=0x7fc00000;reject(b);
 std::cout<<n<<" collision pure controls PASS (not gameplay)\n";
}
