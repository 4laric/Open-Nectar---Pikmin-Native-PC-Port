#if defined(PIKI_PC_PORT)
#include "pc_midday_projectile.h"
#include "pc_midday_creature.h"
#include "pc_midday_world.h"
#include "pc_midday_collision.h"
#include "BombItem.h"
#include "ObjType.h"
namespace pc_midday {
bool allocate_bomb_item_subobjects(BombItem&o){return collision_bind_storage(o.mBombColl,o.mCollParts,o.mCollPartIdList,10);}
bool bomb_item_fields(BombItem&o,ActorArchive&a){
 int version=1;if(o.mObjType!=OBJTYPE_Bomb||!a.scalar("bomb.version",ScalarKind::S32,&version)||version!=1)return a.fail("Bomb concrete factory/version mismatch");
 if(!creature_fields(o,a)||!world_item_fields(o,a))return false;
 // mSound is presentation state. Its stable SeContext address is registered by
 // the scene resource factory, never copied as audio backend memory.
 PrefixArchive c(a,"bomb.collision");return collision_fields(o.mBombColl,c);
}
bool capture_bomb_item(BombItem&o,LogicalResolver&r,double now,ActorBytes&b,std::string&e){ActorFields f;FieldArchive a(Mode::Capture,f,r,e,now);std::vector<FieldSchema>s;return bomb_item_fields(o,a)&&a.finish()&&bomb_item_schema(f,s,e)&&validate_actor_fields(f,s,r,e)&&encode_actor_fields(f,b,e);}
bool bind_bomb_item(BombItem&o,const ActorBytes&b,LogicalResolver&r,double now,std::string&e){
 if(!validate_bomb_item(b,r,e))return false;ActorFields f;if(!decode_actor_fields(b,f,e))return false;
 // Stable storage is allocated/registered by the scene BEFORE reference bind.
 // This only checks allocation identity; it does not initialize AI or geometry.
 FieldArchive v(Mode::Validate,f,r,e,now);if(!bomb_item_fields(o,v)||!v.finish())return false;
 FieldArchive a(Mode::Apply,f,r,e,now);return bomb_item_fields(o,a)&&a.finish();
}
}
#endif
