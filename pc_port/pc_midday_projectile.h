#pragma once
#include "pc_midday_actor_archive.h"
namespace p2kabutostone { class Fleet; }
class BombItem;
class P2BombSaraiBomb; class P2BombSaraiBombPool;
class P2GroinkPolicy; class P2GroinkVolley;
class P2CannonStone; class P2RockHazard; class P2CannonStonePool; class P2RockHazardPool;
namespace pc_midday {
enum class ProjectileType : int { CannonStone=1, RockHazard=2, CannonPool=3, RockPool=4, KabutoFleet=5, GroinkShell=6, GroinkVolley=7, BombSarai=8, BombSaraiPool=9 };
bool allocate_bomb_item_subobjects(BombItem&);
bool bomb_item_fields(BombItem&,ActorArchive&);
bool bomb_item_schema(const ActorFields&,std::vector<FieldSchema>&,std::string&);
bool capture_bomb_item(BombItem&,LogicalResolver&,double,ActorBytes&,std::string&);
bool validate_bomb_item(const ActorBytes&,const LogicalResolver&,std::string&);
bool bind_bomb_item(BombItem&,const ActorBytes&,LogicalResolver&,double,std::string&);
// Scene family 0x6808/version1. Factory discriminator is supplied independently.
bool projectile_schema(const ActorFields&,ProjectileType,std::vector<FieldSchema>&,std::string&);
// Appends a nested schema; prefix has no trailing dot. Caller validates the whole record.
bool projectile_nested_schema(const ActorFields&,ProjectileType,const std::string& prefix,std::vector<FieldSchema>&,std::string&);
bool validate_projectile(const ActorBytes&,ProjectileType,const LogicalResolver&,std::string&);
#define PC_PROJECTILE_API(T) \
bool projectile_fields(T&,ActorArchive&); \
bool capture_projectile(T&,LogicalResolver&,ActorBytes&,std::string&); \
bool bind_projectile(T&,const ActorBytes&,LogicalResolver&,std::string&);
PC_PROJECTILE_API(P2CannonStone)
PC_PROJECTILE_API(P2RockHazard)
PC_PROJECTILE_API(P2CannonStonePool)
PC_PROJECTILE_API(P2RockHazardPool)
PC_PROJECTILE_API(p2kabutostone::Fleet)
PC_PROJECTILE_API(P2GroinkPolicy)
PC_PROJECTILE_API(P2GroinkVolley)
PC_PROJECTILE_API(P2BombSaraiBomb)
PC_PROJECTILE_API(P2BombSaraiBombPool)
#undef PC_PROJECTILE_API
}
