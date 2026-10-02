#include "pc_midday_world_resources.h"
#include <cstdio>
#include <cstdlib>
using namespace pc_midday;
namespace {
int tests=0;void check(bool ok,const char*m){++tests;if(!ok){std::fprintf(stderr,"FAIL %s\n",m);std::exit(1);}}
void value(ActorFields& f,const char*k,ScalarKind t,u64 bits){ActorField v;v.scalar=t;v.bits=bits;f[k]=v;}
using Schema=bool(*)(const ActorFields&,const std::string&,std::vector<FieldSchema>&,std::string&);
struct Resolver:LogicalResolver {
 bool identify(const char*,RefKind,const void*,LogicalRef&,std::string&)override{return false;}
 bool validate(const char*,RefKind,const LogicalRef&,std::string&)const override{return false;}
 bool validateTyped(const FieldSchema& s,const LogicalRef&r,std::string&)const override{return !s.targetType.empty()&&((s.ownership==ReferenceOwnership::Content&&r.owner==0&&r.resource==1)||(s.ownership!=ReferenceOwnership::Content&&r.owner==1&&r.resource==0))&&r.slot==static_cast<u32>(s.reference)+1;}
 bool resolve(const char*,RefKind,const LogicalRef&,void*&,std::string&)override{return false;}
 bool identifyHandle(const char*,RefKind,u32,LogicalRef&,std::string&)override{return false;}
 bool resolveHandle(const char*,RefKind,const LogicalRef&,u32&,std::string&)override{return false;}
};
ActorFields complete(ActorFields f,Schema fn){std::vector<FieldSchema>s;std::string e;check(fn(f,"",s,e),e.c_str());for(auto&d:s)if(!f.count(d.key)){ActorField v;v.category=d.category;v.scalar=d.scalar;v.reference=d.reference;if(d.category==FieldCategory::Reference&&!d.nullable)v.target=d.ownership==ReferenceOwnership::Content?LogicalRef{0,1,static_cast<u32>(d.reference)+1}:LogicalRef{1,0,static_cast<u32>(d.reference)+1};f[d.key]=v;}return f;}
bool valid(const ActorFields&f,Schema fn){std::vector<FieldSchema>s;std::string e;Resolver r;ActorBytes bytes;ActorFields back;return fn(f,"",s,e)&&validate_actor_fields(f,s,r,e)&&encode_actor_fields(f,bytes,e)&&decode_actor_fields(bytes,back,e)&&validate_actor_fields(back,s,r,e);}
}
int main(){
 ActorFields f;value(f,"count",ScalarKind::S32,2);f=complete(f,world_materials_schema);check(valid(f,world_materials_schema),"material list roundtrip");auto bad=f;value(bad,"count",ScalarKind::S32,257);check(!valid(bad,world_materials_schema),"material bounds");bad=f;bad.erase("material.1");check(!valid(bad,world_materials_schema),"missing material identity");
 f={};value(f,"mFlags",ScalarKind::U32,1);value(f,"pvwFlags",ScalarKind::U32,1);value(f,"textureCount",ScalarKind::S32,2);f=complete(f,world_material_schema);check(valid(f,world_material_schema),"PVW material aliases roundtrip");bad=f;value(bad,"pvwFlags",ScalarKind::U32,0);check(!valid(bad,world_material_schema),"duplicate discriminator consistency");bad=f;bad["texture.1"].target={};check(!valid(bad,world_material_schema),"texture alias required");
 for(int initialized=0;initialized<2;++initialized){f={};value(f,"mIsMatrixDirty",ScalarKind::Bool,initialized);value(f,"matrixReady",ScalarKind::Bool,initialized);f=complete(f,world_texture_data_schema);check(valid(f,world_texture_data_schema),"texture initialized/uninitialized matrix payload");bad=f;value(bad,"matrixReady",ScalarKind::Bool,1-initialized);check(!valid(bad,world_texture_data_schema),"texture readiness drift");}
 f=complete({},world_tev_schema);check(valid(f,world_tev_schema),"TEV animated color roundtrip");bad=f;bad.erase("mTevColRegs.2.mCurrentAnimFrame");check(!valid(bad,world_tev_schema),"missing TEV frame");
 f={};value(f,"hasModel",ScalarKind::Bool,1);value(f,"vertices",ScalarKind::S32,3);value(f,"triangles",ScalarKind::S32,1);value(f,"joints",ScalarKind::S32,1);value(f,"groups",ScalarKind::S32,1);value(f,"triangle.0.room",ScalarKind::S16,0);for(int i=0;i<3;++i){value(f,("triangle.0.vertex."+std::to_string(i)).c_str(),ScalarKind::U32,i);value(f,("triangle.0.adjacent."+std::to_string(i)).c_str(),ScalarKind::S16,65535);}value(f,"group.0.count",ScalarKind::S32,1);value(f,"group.0.farCount",ScalarKind::S16,1);value(f,"group.0.distances",ScalarKind::Bool,1);value(f,"group.0.verticesBound",ScalarKind::Bool,1);value(f,"group.0.joint",ScalarKind::S32,0);value(f,"group.0.triangle.0",ScalarKind::S32,0);f=complete(f,world_dyn_shape_schema);check(valid(f,world_dyn_shape_schema),"dynamic collision geometry roundtrip");
 for(auto mutation:{"vertices","triangles","joints","groups","triangle.0.vertex.0","triangle.0.adjacent.0","group.0.triangle.0","group.0.farCount","group.0.joint"}){bad=f;bad[mutation].bits=999999;check(!valid(bad,world_dyn_shape_schema),mutation);}bad=f;value(bad,"group.0.distances",ScalarKind::Bool,0);check(!valid(bad,world_dyn_shape_schema),"far culling requires distance storage");bad=f;bad.erase("visible.0");check(!valid(bad,world_dyn_shape_schema),"missing joint visibility");
 f=complete({},world_joint_schema);check(valid(f,world_joint_schema),"joint visibility and matrices roundtrip");bad=f;bad.erase("inverse.3.3");check(!valid(bad,world_joint_schema),"joint inverse matrix missing");
 f={};value(f,"count",ScalarKind::S32,0);f=complete(f,world_platform_schema);check(valid(f,world_platform_schema),"empty platform manager");bad=f;value(bad,"count",ScalarKind::S32,17);check(!valid(bad,world_platform_schema),"platform fixed capacity");
 std::printf("PASS world resources %d controls\n",tests);
}
