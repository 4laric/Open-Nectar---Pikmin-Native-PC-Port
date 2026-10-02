#include "pc_midday_creature.h"
#include <iostream>
#include <cstdlib>
using namespace pc_midday;
struct Resolver:LogicalResolver{
 bool validateTyped(const FieldSchema& s,const LogicalRef& r,std::string&)const override{return !s.targetType.empty()&&r.owner==1;}
 bool identify(const char*,RefKind,const void*,LogicalRef&,std::string&)override{return false;}
 bool validate(const char*,RefKind,const LogicalRef& r,std::string&)const override{return r.owner==1;}
 bool resolve(const char*,RefKind,const LogicalRef&,void*&,std::string&)override{return false;}
 bool identifyHandle(const char*,RefKind,u32,LogicalRef&,std::string&)override{return false;}
 bool resolveHandle(const char*,RefKind,const LogicalRef&,u32&,std::string&)override{return false;}
};
static void check(bool value,const char* m){if(!value){std::cerr<<m<<'\n';std::exit(1);}}
static ActorField num(ScalarKind k,u64 bits){ActorField f;f.scalar=k;f.bits=bits;return f;}
int main(){
 Resolver r;ActorFields f;std::string e;std::vector<FieldSchema>s;
 f["creature.objectType"]=num(ScalarKind::S32,1);f["creature.search.capacity"]=num(ScalarKind::S16,2);f["creature.search.count"]=num(ScalarKind::S16,1);f["creature.search.last"]=num(ScalarKind::S32,0);
 check(creature_schema(f,s,e),"schema seed");
 for(auto& x:s){if(f.count(x.key))continue;ActorField v;v.category=x.category;v.scalar=x.scalar;v.reference=x.reference;if(x.category!=FieldCategory::Scalar&&!x.nullable)v.target.owner=1;f[x.key]=v;}
 ActorBytes bytes;check(encode_actor_fields(f,bytes,e)&&validate_creature(bytes,r,1,e),"valid typed payload");
 int cases=2;std::string wrongType;check(!validate_creature(bytes,r,2,wrongType),"factory mismatch accepted");
 auto reject=[&](ActorFields bad){ActorBytes b;std::string why;check(!encode_actor_fields(bad,b,why)||!validate_creature(b,r,1,why),"malformed payload accepted");++cases;};
 auto g=f;g.erase("creature.world.3.3");reject(g);
 g=f;g["creature.extra"]=num(ScalarKind::U8,0);reject(g);
 g=f;g["creature.mProps"].target={};reject(g);
 g=f;g["creature.search.0.target"].target={};reject(g);
 g=f;g["creature.search.capacity"].bits=4097;reject(g);
 g=f;g["creature.search.count"].bits=3;reject(g);
 g=f;g["creature.search.last"].bits=2;reject(g);
 g=f;g["creature.mStickPart"].target.owner=1;reject(g);
 g=f;g["creature.mNextRopeHolder"].target.owner=1;reject(g);
 g=f;g["creature.search.0.target"].target.owner=2;reject(g);
 g=f;g["creature.mIsBeingDamaged"].bits=2;reject(g);
 g=f;g["creature.mHealth"].bits=0x7fc00000;reject(g);
 g=f;g["creature.mHealth"].scalar=ScalarKind::U32;reject(g);
 std::cout<<cases<<" Creature schema controls PASS (synthetic wire, not gameplay)\n";
}
