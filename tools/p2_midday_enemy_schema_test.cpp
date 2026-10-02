#include "pc_midday_enemy.h"
#include "pc_p2_catfish_events.h"
#include "ObjType.h"
#include "pc_midday_creature.h"
#include "pc_midday_collision.h"
#include <cstdio>
#include <cstring>
#include <stdexcept>
using namespace pc_midday;
namespace {
int checks=0;void check(bool ok,const char* what){++checks;if(!ok)throw std::runtime_error(what);}
struct Resolver:LogicalResolver {
 bool identify(const char*,RefKind,const void*,LogicalRef&,std::string&)override{return false;}
 bool validate(const char*,RefKind,const LogicalRef& r,std::string&)const override{return r.owner==7;}
 bool validateTyped(const FieldSchema& d,const LogicalRef& r,std::string& e)const override{return !d.targetType.empty()&&((!r.owner&&!r.resource&&!r.slot)?d.nullable:validate(d.key.c_str(),d.reference,r,e));}
 bool resolve(const char*,RefKind,const LogicalRef&,void*&,std::string&)override{return false;}
 bool identifyHandle(const char*,RefKind,u32,LogicalRef&,std::string&)override{return false;}
 bool resolveHandle(const char*,RefKind,const LogicalRef&,u32&,std::string&)override{return false;}
};
void set(ActorFields& f,const std::string& k,ScalarKind t,u64 n){ActorField v;v.scalar=t;v.bits=n;f[k]=v;}
void fill(ActorFields& f,const std::vector<FieldSchema>& schema){for(const auto& d:schema)if(!f.count(d.key)){ActorField v;v.category=d.category;v.scalar=d.scalar;v.reference=d.reference;if(d.category!=FieldCategory::Scalar&&!d.nullable)v.target={7,0,0};f[d.key]=v;}}
// Synthetic initialized fields exercise the combined structural contract only;
// they are never bound to an engine actor or advanced through gameplay.
void composed(){
 struct Catalog:Resolver {
  bool validate(const char*,RefKind,const LogicalRef& r,std::string&)const override{return r.owner==7||(r.owner==0&&r.resource==900);}
  bool validateTyped(const FieldSchema& d,const LogicalRef& r,std::string& e)const override{
   if(d.targetType.empty())return false;
   if(!r.owner&&!r.resource&&!r.slot)return d.nullable;
   if(!validate(d.key.c_str(),d.reference,r,e))return false;
   if(d.ownership==ReferenceOwnership::Content)return r.owner==0&&r.resource==900;
   if(d.ownership==ReferenceOwnership::Self||d.ownership==ReferenceOwnership::ActorSubobject)return r.owner==7&&r.resource==0;
   return true;
  }
 } resolver;
 for(auto host:{EnemyHost::Frog,EnemyHost::Kochappy,EnemyHost::Catfish}){
  ActorFields f;std::vector<FieldSchema> parts;std::string e;
  set(f,"enemy.host",ScalarKind::S32,int(host));set(f,"creature.objectType",ScalarKind::S32,OBJTYPE_Teki);
  set(f,"creature.search.capacity",ScalarKind::S16,0);set(f,"creature.search.count",ScalarKind::S16,0);set(f,"creature.search.last",ScalarKind::S32,0xffffffffu);
  check(creature_schema(f,parts,e),"composed Creature schema seed");fill(f,parts);parts.clear();
  set(f,"enemy.collision.capacity",ScalarKind::U16,1);set(f,"enemy.collision.count",ScalarKind::U16,0);
  ActorField collider;collider.category=FieldCategory::Reference;collider.reference=RefKind::CollInfo;collider.target={7,0,12};
  f["enemy.collision.identity"]=collider;f["creature.mCollInfo"]=collider;
  check(collision_schema(f,"enemy.collision",parts,e),"composed canonical collision schema");fill(f,parts);parts.clear();
  set(f,"enemy.base.type",ScalarKind::S32,host==EnemyHost::Frog?0:host==EnemyHost::Kochappy?3:30);
  set(f,"enemy.base.state",ScalarKind::S32,2);set(f,"enemy.base.mRouteWayPointMax",ScalarKind::S32,0);set(f,"enemy.base.mRouteWayPointCount",ScalarKind::S32,0);
  ActorField strategy;strategy.category=FieldCategory::Reference;strategy.reference=RefKind::Action;strategy.target={0,900,2};f["enemy.base.strategyState"]=strategy;
  check(enemy_base_schema(f,parts,e),"composed registered strategy schema");enemy_subobject_schema(parts);fill(f,parts);parts.clear();
  const std::string p=host==EnemyHost::Frog?"enemy.p2.frog.":host==EnemyHost::Kochappy?"enemy.p2.kochappy.":"enemy.p2.catfish.";
  set(f,p+"present",ScalarKind::Bool,1);set(f,p+"state",ScalarKind::S32,0);set(f,p+"stateTime",ScalarKind::F32,0);
  if(host==EnemyHost::Frog){set(f,p+"clip",ScalarKind::S32,0);set(f,p+"kind",ScalarKind::S32,0);set(f,p+"phase",ScalarKind::F32,0);set(f,p+"flight.elapsed",ScalarKind::F32,0);check(enemy_frog_schema(f,parts,e),"composed Frog host schema");}
  else if(host==EnemyHost::Kochappy){set(f,p+"returnState",ScalarKind::S32,0);set(f,p+"sourceId",ScalarKind::S32,1);set(f,"enemy.p2.purpleStun.present",ScalarKind::Bool,0);check(enemy_kochappy_schema(f,parts,e)&&enemy_stun_schema(f,parts,e),"composed Kochappy host schema");}
  else {set(f,p+"clip",ScalarKind::S32,0);set(f,p+"consumed.count",ScalarKind::U32,0);set(f,p+"phase",ScalarKind::F32,0);set(f,p+"clock.clock.frame",ScalarKind::F64,0);check(enemy_catfish_schema(f,parts,e),"composed Catfish host schema");}
  fill(f,parts);std::vector<FieldSchema> schema;
  check(enemy_schema(f,host,schema,e),"positive whole composed schema");
  for(const auto& d:schema)if(d.category!=FieldCategory::Scalar&&!d.nullable&&d.ownership==ReferenceOwnership::Content){auto& r=f[d.key].target;r.owner=0;r.resource=900;}
  check(validate_actor_fields(f,schema,resolver,e),"positive whole composed typed closure");
  ActorBytes bytes;ActorFields decoded;check(encode_actor_fields(f,bytes,e)&&validate_enemy(bytes,host,resolver,e)&&decode_actor_fields(bytes,decoded,e),"positive composed wire factory validation");
  ActorBytes again;check(encode_actor_fields(decoded,again,e)&&again==bytes,"composed canonical wire roundtrip");
  auto bad=f;bad["creature.mCollInfo"].target.slot=13;ActorBytes corrupt;
  check(encode_actor_fields(bad,corrupt,e)&&!validate_enemy(corrupt,host,resolver,e),"composed collider alias corruption refused");
  bad=f;bad["enemy.base.strategyState"].target.slot=3;
  check(encode_actor_fields(bad,corrupt,e)&&!validate_enemy(corrupt,host,resolver,e),"composed strategy catalog slot corruption refused");
 }
}
void run(){
 composed();
 {ActorFields f;std::vector<FieldSchema> schema;std::string e;
 set(f,"enemy.host",ScalarKind::S32,99);check(!enemy_schema(f,static_cast<EnemyHost>(99),schema,e),"unknown host refuses before scene");
 set(f,"enemy.host",ScalarKind::S32,1);check(!enemy_schema(f,EnemyHost::Catfish,schema,e),"explicit host mismatch refuses");
 set(f,"creature.objectType",ScalarKind::S32,OBJTYPE_Teki);set(f,"enemy.base.type",ScalarKind::S32,3);
 check(!enemy_schema(f,EnemyHost::Frog,schema,e),"proxy vehicle cannot authorize Frog");
 set(f,"enemy.base.type",ScalarKind::S32,0);set(f,"enemy.p2.frog.present",ScalarKind::Bool,0);
 check(!enemy_schema(f,EnemyHost::Frog,schema,e),"absent selected host refuses");
 }

 {
  ActorFields f;std::string e;const std::string p="enemy.p2.catfish.clock.";
  for(const char* k:{"active","clock.active","clock.entry","clock.paused"})set(f,p+k,ScalarKind::Bool,0);
  set(f,p+"clock.generation",ScalarKind::U64,1);set(f,p+"clock.cycle",ScalarKind::U64,0);
  auto frame=[&](double n){u64 b=0;std::memcpy(&b,&n,8);set(f,p+"clock.frame",ScalarKind::F64,b);};
  auto clip=p2catfishevents::makeClip(p2catfishevents::catfishRows().front());
  frame(clip.poses.duration);set(f,p+"active",ScalarKind::Bool,1);set(f,p+"clock.active",ScalarKind::Bool,1);
  check(enemy_catfish_clock_valid(f,clip,e),"nonloop exact end validates before allocation");
  frame(clip.poses.duration+0.25);check(!enemy_catfish_clock_valid(f,clip,e),"resolved clip duration overflow refused prebegin");
  clip.loopEnd=clip.poses.duration;frame(clip.loopEnd);check(!enemy_catfish_clock_valid(f,clip,e),"loop end exclusive bound refused prebegin");
  frame(clip.loopEnd-0.25);check(enemy_catfish_clock_valid(f,clip,e),"loop interior validates");
  set(f,p+"clock.active",ScalarKind::Bool,0);check(!enemy_catfish_clock_valid(f,clip,e),"active receiver inactive clock refused prebegin");
  set(f,p+"active",ScalarKind::Bool,0);frame(10001);check(enemy_catfish_clock_valid(f,clip,e),"inactive finite historical frame preserved");
 }
 auto row=p2catfishevents::catfishRows().front();auto clip=p2catfishevents::makeClip(row);
 p2catfishevents::Receiver a,b;check(a.start(clip,"attack"),"actual Catfish event clock start");
 auto bite=a.advance(18.0/30.0);check(bite.size()==1&&bite[0].action==p2catfishevents::Action::Bite,"bite occurs before snapshot");
 auto saved=a.captureState();check(b.restoreState(clip,"attack",saved),"typed event clock bind");
 auto left=a.advance(60.0/30.0),right=b.advance(60.0/30.0);check(left.size()==1&&right.size()==1&&left[0].action==p2catfishevents::Action::Swallow&&right[0].action==left[0].action&&right[0].cycle==left[0].cycle,"next event preserved without replayed bite");
 saved.clock.frame=-1;check(!b.restoreState(clip,"attack",saved),"bad clock rejected without bind");
 int installed=0;for(int i=0;i<36;++i)if(enemy_registration(i))++installed;
 check(installed==31,"exact registered strategies");for(int id:{26,27,28,29,34,-1,36})check(!enemy_registration(id),"placeholder refused");
 for(int i=0;i<12;++i)check(boss_object_type(i)>=39,"registered boss topology");check(boss_object_type(12)==-1,"invalid boss refused");
 Resolver resolver;std::string error;const std::string p="enemy.p2.frog.";
 for(int state=0;state<10;++state){ActorFields f;set(f,p+"present",ScalarKind::Bool,1);set(f,p+"state",ScalarKind::S32,state);set(f,p+"clip",ScalarKind::S32,0);set(f,p+"kind",ScalarKind::S32,state%2);for(const char* key:{"stateTime","phase","flight.elapsed"})set(f,p+key,ScalarKind::F32,0);
 std::vector<FieldSchema> schema;check(enemy_frog_schema(f,schema,error),"all Frog states have schema");fill(f,schema);check(validate_actor_fields(f,schema,resolver,error),"complete Frog typed record");
 ActorBytes bytes;ActorFields decoded;check(encode_actor_fields(f,bytes,error)&&decode_actor_fields(bytes,decoded,error)&&validate_actor_fields(decoded,schema,resolver,error),"explicit wire roundtrip");
 for(const char* key:{"state","clip","kind"}){auto bad=f;bad[p+key].bits=99;std::vector<FieldSchema> out;check(!enemy_frog_schema(bad,out,error),"bad family discriminator refused");}
 auto bad=f;bad[p+"flight.elapsed"].bits=0xbf800000;std::vector<FieldSchema> out;check(!enemy_frog_schema(bad,out,error),"negative flight time refused");
 bad=f;bad[p+"generator"].target={};check(!validate_actor_fields(bad,schema,resolver,error),"missing generator closure refused");
 }
 ActorFields absent;set(absent,p+"present",ScalarKind::Bool,0);std::vector<FieldSchema> schema;check(enemy_frog_schema(absent,schema,error)&&validate_actor_fields(absent,schema,resolver,error),"absent optional host");absent[p+"state"]=ActorField{};check(!validate_actor_fields(absent,schema,resolver,error),"hidden state forbidden when absent");
}
}
int main(){try{run();std::printf("midday enemy schema: %d checks passed\n",checks);return 0;}catch(const std::exception& e){std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}}
