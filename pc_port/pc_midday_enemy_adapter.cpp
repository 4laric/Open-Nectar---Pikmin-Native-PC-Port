#if defined(PIKI_PC_PORT)
#include "pc_midday_enemy.h"
#include "pc_midday_creature.h"
#include "pc_midday_collision.h"
#include "teki.h"
namespace pc_midday {
namespace {
bool fields(Teki& actor,EnemyHost expected,ActorArchive& a){
 int host=a.mode()==Mode::Capture?int(expected):0;
 if(!a.scalar("enemy.host",ScalarKind::S32,&host)||host!=int(expected)||host<1||host>3)return a.fail("unsupported explicit enemy host");
 if(!actor.mCollInfo)return a.fail("enemy collider allocation missing");
 if(!creature_fields(actor,a))return false;
 PrefixArchive collision(a,"enemy.collision");
 if(!collision_fields(*actor.mCollInfo,collision)||!enemy_base_fields(actor,a)||!enemy_subobject_fields(actor,a))return false;
 switch(expected){case EnemyHost::Frog:return enemy_frog_fields(actor,a);case EnemyHost::Kochappy:return enemy_kochappy_fields(actor,a)&&enemy_stun_fields(actor,a);case EnemyHost::Catfish:return enemy_catfish_fields(actor,a);}
 return a.fail("unsupported enemy host");
}
}
bool capture_enemy(Teki& actor,EnemyHost host,LogicalResolver& resolver,double now,ActorBytes& bytes,std::string& e){ActorFields f;FieldArchive a(Mode::Capture,f,resolver,e,now);std::vector<FieldSchema>s;return fields(actor,host,a)&&a.finish()&&enemy_schema(f,host,s,e)&&validate_actor_fields(f,s,resolver,e)&&encode_actor_fields(f,bytes,e);}
bool bind_enemy(Teki& actor,EnemyHost host,const ActorBytes& bytes,LogicalResolver& resolver,double now,std::string& e){
 // Bind only into a disposable preallocated scene, after all closures passed
 // pure validation. No strategy transition, birth or animation start occurs.
 if(!validate_enemy(bytes,host,resolver,e))return false;ActorFields f;if(!decode_actor_fields(bytes,f,e))return false;
 FieldArchive check(Mode::Validate,f,resolver,e,now);if(!fields(actor,host,check)||!check.finish())return false;
 FieldArchive apply(Mode::Apply,f,resolver,e,now);return fields(actor,host,apply)&&apply.finish();
}
}
#endif
