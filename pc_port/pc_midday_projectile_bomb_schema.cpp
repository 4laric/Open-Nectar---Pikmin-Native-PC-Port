#include "pc_midday_projectile.h"
#include "pc_midday_creature.h"
#include "pc_midday_world.h"
#include "pc_midday_collision.h"
#include "ObjType.h"
namespace pc_midday {
bool bomb_item_schema(const ActorFields&f,std::vector<FieldSchema>&out,std::string&e){int type=0,version=0;if(!actor_i32(f,"creature.objectType",type,e)||type!=OBJTYPE_Bomb||!actor_i32(f,"bomb.version",version,e)||version!=1){if(e.empty())e="Bomb concrete factory mismatch";return false;}
 auto a=f.find("creature.mCollInfo"),b=f.find("bomb.collision.identity");if(a==f.end()||b==f.end()||a->second.target.owner!=b->second.target.owner||a->second.target.resource!=b->second.target.resource||a->second.target.slot!=b->second.target.slot){if(e.empty())e="Bomb collider alias mismatch";return false;}
 std::vector<FieldSchema>s;if(!creature_schema(f,s,e)||!world_item_schema(f,s,e)||!collision_schema(f,"bomb.collision",s,e))return false;s.push_back(FieldSchema::value("bomb.version",ScalarKind::S32));out.swap(s);return true;}
bool validate_bomb_item(const ActorBytes&b,const LogicalResolver&r,std::string&e){ActorFields f;std::vector<FieldSchema>s;return decode_actor_fields(b,f,e)&&bomb_item_schema(f,s,e)&&validate_actor_fields(f,s,r,e);}
}
