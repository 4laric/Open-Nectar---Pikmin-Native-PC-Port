#include "pc_midday_particle.h"
#include <iostream>
#include <cstdlib>
using namespace pc_midday;
void check(bool b,const char*m){if(!b){std::cerr<<m<<'\n';std::exit(1);}}
struct Resolver:LogicalResolver{
 bool identify(const char*,RefKind,const void*,LogicalRef&,std::string&)override{return false;}
 bool validate(const char*,RefKind,const LogicalRef&r,std::string&)const override{return r.owner==1;}
 bool validateTyped(const FieldSchema&s,const LogicalRef&r,std::string&)const override{return !s.targetType.empty()&&((s.ownership==ReferenceOwnership::ResourceSelf||s.ownership==ReferenceOwnership::ResourceSubobject)?(!r.owner&&r.resource==1):r.owner==1);}
 bool resolve(const char*,RefKind,const LogicalRef&,void*&,std::string&)override{return false;}
 bool identifyHandle(const char*,RefKind,u32,LogicalRef&,std::string&)override{return false;}
 bool resolveHandle(const char*,RefKind,const LogicalRef&,u32&,std::string&)override{return false;}
};
int main(){int n=0;std::string e;std::vector<ParticleGraphNode>g={{{1,0,1},{1,0,2},{1,0,2}},{{1,0,2},{1,0,1},{1,0,1}}};check(validate_particle_graph(g,e),"valid ring");++n;auto b=g;b[1].next={1,0,2};check(!validate_particle_graph(b,e),"broken reciprocal accepted");++n;b=g;b[1].identity=b[0].identity;check(!validate_particle_graph(b,e),"duplicate accepted");++n;b=g;b[1].next={1,0,3};check(!validate_particle_graph(b,e),"missing closure accepted");++n;
 Resolver r;for(auto kind:{ParticleRecordKind::Model,ParticleRecordKind::Child,ParticleRecordKind::Permanent,ParticleRecordKind::List,ParticleRecordKind::Generator,ParticleRecordKind::Manager}){
 ActorFields f;std::vector<FieldSchema>s;e.clear();if(kind==ParticleRecordKind::Generator){for(auto k:{"drawCallback","rotationCallback"}){ActorField x;x.scalar=ScalarKind::S32;f[k]=x;}for(auto k:{"mEmissionRateKeyCount","mEmissionRadiusKeyCount","mInitialVelocityKeyCount","animData.maxFrame"}){ActorField x;x.scalar=ScalarKind::U8;f[k]=x;}}
 if(kind==ParticleRecordKind::Manager){for(auto k:{"models","children"}){ActorField x;x.scalar=ScalarKind::U32;x.bits=1;f[k]=x;}}
 check(particle_schema(f,kind,"",s,e),"schema seed");for(auto&k:s){if(f.count(k.key))continue;ActorField v;v.category=k.category;v.scalar=k.scalar;v.reference=k.reference;if(k.category!=FieldCategory::Scalar&&!k.nullable){if(k.ownership==ReferenceOwnership::ResourceSelf||k.ownership==ReferenceOwnership::ResourceSubobject)v.target.resource=1;else v.target.owner=1;}f[k.key]=v;}check(validate_actor_fields(f,s,r,e),"valid field payload");++n;
 if(kind==ParticleRecordKind::Generator){auto bad=f;bad["drawCallback"].bits=3;std::vector<FieldSchema>z;check(!particle_schema(bad,kind,"",z,e),"unknown callback accepted");++n;bad=f;bad["mEmissionRateKeyCount"].bits=2;z.clear();e.clear();check(particle_schema(bad,kind,"",z,e)&&!validate_actor_fields(bad,z,r,e),"missing interpolation array accepted");++n;}
 }
 check(validate_particle_allocations({{1,2},{3,4}},10,e),"valid aggregate");++n;check(!validate_particle_allocations({{1,2},{3,5}},10,e),"aggregate overrun accepted");++n;
 std::cout<<n<<" particle schema/graph controls PASS (synthetic, not gameplay)\n";
}
