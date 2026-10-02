#include "pc_midday_projectile.h"
#include "pc_p2_cannon_stone.h"
#include "pc_p2_rock_hazard.h"
#include "pc_p2_kabuto_stone_fleet.h"
#include "pc_p2_groink_volley.h"
#include "pc_p2_bombsarai_bomb.h"
#include <iostream>
#include <cstdlib>
using namespace pc_midday;
struct Resolver:LogicalResolver {
 bool identify(const char*,RefKind,const void*,LogicalRef&,std::string&)override{return false;}
 bool validate(const char*,RefKind,const LogicalRef&r,std::string&)const override{return r.owner!=0;}
 bool validateTyped(const FieldSchema&s,const LogicalRef&r,std::string&)const override{return !s.targetType.empty()&&r.owner!=0;}
 bool resolve(const char*,RefKind,const LogicalRef&,void*&,std::string&)override{return false;}
 bool identifyHandle(const char*,RefKind,u32,LogicalRef&,std::string&)override{return false;}
 bool resolveHandle(const char*,RefKind,const LogicalRef&,u32&,std::string&)override{return false;}
 u64 remap=0;
 bool identifyToken(const char*,RefKind,u64 v,LogicalRef&r,std::string&)override{r.owner=v;return true;}
 bool resolveToken(const char*,RefKind,const LogicalRef&r,u64&v,std::string&)override{v=r.owner? r.owner+remap:0;return true;}
};
void check(bool b,const std::string&e){if(!b){std::cerr<<e<<'\n';std::exit(1);}}
int tests=0;
template<class T>void roundtrip(T&o,ProjectileType kind){Resolver r;ActorBytes a,b;std::string e;bool ok=capture_projectile(o,r,a,e);check(ok,"capture "+std::to_string(int(kind))+": "+e);T restored=o;check(bind_projectile(restored,a,r,e),"bind: "+e);check(capture_projectile(restored,r,b,e)&&a==b,"roundtrip bytes");++tests;
 ActorFields f;check(decode_actor_fields(a,f,e),e);f["projectile.kind"].bits=99;check(encode_actor_fields(f,b,e),e);e.clear();check(!validate_projectile(b,kind,r,e),"wrong factory accepted");++tests;}
int main(){P2CannonStone a;P2RockHazard b;P2CannonStonePool c(16);P2RockHazardPool d(16);p2kabutostone::Fleet e;P2GroinkPolicy f;P2GroinkVolley g;
 roundtrip(a,ProjectileType::CannonStone);roundtrip(b,ProjectileType::RockHazard);roundtrip(c,ProjectileType::CannonPool);roundtrip(d,ProjectileType::RockPool);roundtrip(e,ProjectileType::KabutoFleet);roundtrip(f,ProjectileType::GroinkShell);roundtrip(g,ProjectileType::GroinkVolley);
 Resolver r;ActorBytes bytes;std::string why;check(capture_projectile(g,r,bytes,why),why);ActorFields fields;check(decode_actor_fields(bytes,fields,why),why);fields["projectile.body.slot.1.inactiveIndex"].bits=0;check(encode_actor_fields(fields,bytes,why),why);why.clear();check(!validate_projectile(bytes,ProjectileType::GroinkVolley,r,why),"duplicate FIFO accepted");++tests;

 P2CannonStone live;P2CannonStoneConfig config;config.moveSpeed=250;config.health=100;config.collisionRadius=40;live.reset(config);check(live.birth({1,2,3},0,true,0x100000001ULL,0x200000002ULL),"live birth");
 roundtrip(live,ProjectileType::CannonStone);
 check(capture_projectile(live,r,bytes,why),why);Resolver relocated;relocated.remap=0x100000000ULL;P2CannonStone destination;check(bind_projectile(destination,bytes,relocated,why),why);check(destination.sourceToken()==0x200000001ULL&&destination.selfToken()==0x300000002ULL,"token truncation/remap");++tests;
 check(decode_actor_fields(bytes,fields,why),why);fields["projectile.body.self"].target={};check(encode_actor_fields(fields,bytes,why),why);why.clear();check(!validate_projectile(bytes,ProjectileType::CannonStone,r,why),"missing active self accepted");++tests;
 P2GroinkMuzzle muzzle;muzzle.column0={1,0,0};muzzle.column1={0,1,0};muzzle.column2={0,0,1};muzzle.column3={0,100,0};std::array<P2GroinkVec3,3> samples{};check(g.emit(muzzle,100,samples).valid,"volley birth");roundtrip(g,ProjectileType::GroinkVolley);
 P2BombSaraiBomb bomb;roundtrip(bomb,ProjectileType::BombSarai);
 P2BombSaraiBombPool bp(16);roundtrip(bp,ProjectileType::BombSaraiPool);
 std::cout<<tests<<" projectile component controls PASS (not gameplay)\n";
}
