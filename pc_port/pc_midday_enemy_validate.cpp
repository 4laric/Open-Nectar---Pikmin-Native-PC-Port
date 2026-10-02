#include "pc_midday_enemy.h"
#include "pc_midday_creature.h"
#include "pc_midday_collision.h"
#include "ObjType.h"
namespace pc_midday {
bool enemy_schema(const ActorFields& f,EnemyHost expected,std::vector<FieldSchema>& out,std::string& e) {
 auto bad=[&](const char* reason){e=reason;return false;};
 int host=0,type=0,object=0;
 if(!actor_i32(f,"enemy.host",host,e)||host!=int(expected)||host<1||host>3)return bad("unsupported or mismatched explicit enemy host");
 if(!actor_i32(f,"creature.objectType",object,e)||object!=OBJTYPE_Teki||!actor_i32(f,"enemy.base.type",type,e))return bad("enemy allocation discriminator");
 if((expected==EnemyHost::Frog&&type!=0&&type!=33)||(expected==EnemyHost::Kochappy&&type!=3)||(expected==EnemyHost::Catfish&&type!=30))return bad("enemy host vehicle mismatch");
 const char* presence=expected==EnemyHost::Frog?"enemy.p2.frog.present":expected==EnemyHost::Kochappy?"enemy.p2.kochappy.present":"enemy.p2.catfish.present";
 auto present=f.find(presence);if(present==f.end()||present->second.category!=FieldCategory::Scalar||present->second.scalar!=ScalarKind::Bool||present->second.bits!=1)return bad("selected enemy host is absent");
 std::vector<FieldSchema> s;if(!creature_schema(f,s,e))return false;
 s.push_back(FieldSchema::value("enemy.host",ScalarKind::S32));
 if(!collision_schema(f,"enemy.collision",s,e)||!enemy_base_schema(f,s,e))return false;
 auto collider=f.find("creature.mCollInfo"),identity=f.find("enemy.collision.identity");
 if(collider==f.end()||identity==f.end()||collider->second.target.owner!=identity->second.target.owner||collider->second.target.resource!=identity->second.target.resource||collider->second.target.slot!=identity->second.target.slot)return bad("enemy collision alias mismatch");
 enemy_subobject_schema(s);
 for(const char* k:{"bite","runAway","stay","flying","timer","choke","footEffect","attack.moving"}){u32 v=0;auto key=std::string("enemy.subobjects.")+k;if(!actor_u32(f,key.c_str(),v,e)||v>(std::string(k)=="footEffect"?15u:1u))return bad("enemy switch bounds");}
 bool ok=expected==EnemyHost::Frog?enemy_frog_schema(f,s,e):expected==EnemyHost::Kochappy?(enemy_kochappy_schema(f,s,e)&&enemy_stun_schema(f,s,e)):enemy_catfish_schema(f,s,e);
 if(!ok)return false;out.swap(s);return true;
}
bool validate_enemy(const ActorBytes& bytes,EnemyHost host,const LogicalResolver& resolver,std::string& e){ActorFields f;std::vector<FieldSchema>s;return decode_actor_fields(bytes,f,e)&&enemy_schema(f,host,s,e)&&validate_actor_fields(f,s,resolver,e);}
}

