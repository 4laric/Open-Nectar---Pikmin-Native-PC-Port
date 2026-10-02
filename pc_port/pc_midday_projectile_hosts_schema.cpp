#include "pc_midday_projectile_hosts.h"
#include <tuple>
#include <cstring>
#include <cmath>
namespace pc_midday {
namespace {
bool bad(std::string&e,const char*m){if(e.empty())e=m;return false;}
void scalar(std::vector<FieldSchema>&s,const std::string&k,ScalarKind t){s.push_back(FieldSchema::value(k.c_str(),t));}
bool same(const LogicalRef&a,const LogicalRef&b){return a.owner==b.owner&&a.resource==b.resource&&a.slot==b.slot;}
}
bool kabuto_projectile_schema(const ActorFields&f,std::vector<FieldSchema>&s,std::string&e){
 if(!projectile_nested_schema(f,ProjectileType::KabutoFleet,"kabutoProjectiles.fleet",s,e))return false;
 scalar(s,"kabutoProjectiles.sourceDebt",ScalarKind::F64);scalar(s,"kabutoProjectiles.traceCalls",ScalarKind::U64);scalar(s,"kabutoProjectiles.traceWalls",ScalarKind::U64);
 for(int i=0;i<16;++i){auto p="kabutoProjectiles.slot."+std::to_string(i)+".";scalar(s,p+"generator",ScalarKind::U32);scalar(s,p+"positionTicks",ScalarKind::S32);}
 u32 n=0;if(!actor_u32(f,"kabutoProjectiles.shooterCount",n,e)||n>4096)return bad(e,"Kabuto shooter count");scalar(s,"kabutoProjectiles.shooterCount",ScalarKind::U32);
 std::set<std::tuple<u64,u64,u32>>seen;
 for(u32 i=0;i<n;++i){auto p="kabutoProjectiles.shooter."+std::to_string(i)+".";
 s.push_back(FieldSchema::token64((p+"token").c_str(),RefKind::ProjectileToken,false,"BTeki",ReferenceOwnership::AnyLive));s.push_back(FieldSchema::ref((p+"actor").c_str(),RefKind::Creature,false,"BTeki",ReferenceOwnership::AnyLive));
 auto a=f.find(p+"actor"),t=f.find(p+"token");if(a==f.end()||t==f.end()||!same(a->second.target,t->second.target)||!seen.emplace(t->second.target.owner,t->second.target.resource,t->second.target.slot).second)return bad(e,"Kabuto shooter identity mismatch");}
 return true;
}
bool longlegs_projectile_schema(const ActorFields&f,std::vector<FieldSchema>&s,std::string&e){
 if(!projectile_nested_schema(f,ProjectileType::CannonPool,"longlegsProjectiles.pool",s,e))return false;
 scalar(s,"longlegsProjectiles.nextSelfToken",ScalarKind::U64);u32 n=0;if(!actor_u32(f,"longlegsProjectiles.shellCount",n,e)||n>10)return bad(e,"Houdai shell count");scalar(s,"longlegsProjectiles.shellCount",ScalarKind::U32);
 std::set<std::tuple<u64,u64,u32>>seen;
 for(u32 i=0;i<n;++i){auto p="longlegsProjectiles.shell."+std::to_string(i)+".";
 s.push_back(FieldSchema::ref((p+"stone").c_str(),RefKind::ProjectileToken,false,"P2CannonStone",ReferenceOwnership::ActorSubobject));s.push_back(FieldSchema::token64((p+"source").c_str(),RefKind::ProjectileToken,false,"Creature",ReferenceOwnership::AnyLive));auto t=f.find(p+"stone");if(t==f.end()||!seen.emplace(t->second.target.owner,t->second.target.resource,t->second.target.slot).second)return bad(e,"duplicate Houdai shell");
 // Resolver must bind stone keys to this global pool's fixed slot array.
 }
 return true;
}
}
