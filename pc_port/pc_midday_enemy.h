#pragma once
#include "pc_midday_actor_archive.h"
class Teki;
class BTeki;
class Boss;
struct PaniAnimator;
namespace p2sampled { struct Clip; }
namespace pc_midday {
// Sub-visitors compose into complete family records; these are not standalone
// save adapters. The caller must also serialize the concrete family host graph.
enum class EnemyHost : int { Frog=1, Kochappy=2, Catfish=3 };
bool enemy_schema(const ActorFields&,EnemyHost,std::vector<FieldSchema>&,std::string&);
bool capture_enemy(Teki&,EnemyHost,LogicalResolver&,double,ActorBytes&,std::string&);
bool validate_enemy(const ActorBytes&,EnemyHost,const LogicalResolver&,std::string&);
bool bind_enemy(Teki&,EnemyHost,const ActorBytes&,LogicalResolver&,double,std::string&);
struct EnemyRegistration { int id; const char* name; const char* strategy; };
const EnemyRegistration* enemy_registration(int id);
int boss_object_type(int bossId);
bool enemy_catfish_fields(BTeki&,ActorArchive&);
// The scene content resolver MUST call this during validateTyped for
// enemy.p2.catfish.clock.content, using the actual content-bound clip. This
// closes clip-dependent clock bounds before scene allocation, not during bind.
bool enemy_catfish_clock_valid(const ActorFields&,const p2sampled::Clip&,std::string&);
bool enemy_catfish_schema(const ActorFields&,std::vector<FieldSchema>&,std::string&);
bool enemy_stun_fields(BTeki&,ActorArchive&);
bool enemy_stun_schema(const ActorFields&,std::vector<FieldSchema>&,std::string&);
bool enemy_kochappy_fields(BTeki&,ActorArchive&);
bool enemy_kochappy_schema(const ActorFields&,std::vector<FieldSchema>&,std::string&);
bool enemy_frog_fields(BTeki&,ActorArchive&);
bool enemy_frog_schema(const ActorFields&,std::vector<FieldSchema>&,std::string&);
bool enemy_subobject_fields(Teki&,ActorArchive&);
void enemy_subobject_schema(std::vector<FieldSchema>&);
bool enemy_base_fields(Teki&,ActorArchive&);
bool enemy_base_schema(const ActorFields&,std::vector<FieldSchema>&,std::string&);
bool boss_small_family_fields(Boss&,int bossId,ActorArchive&);
bool boss_small_family_schema(const ActorFields&,std::vector<FieldSchema>&,std::string&);
bool boss_base_fields(Boss&,ActorArchive&);
bool boss_base_schema(const ActorFields&,std::vector<FieldSchema>&,std::string&);
bool enemy_animation_fields(PaniAnimator&,ActorArchive&);
void enemy_animation_schema(const std::string&,std::vector<FieldSchema>&);
}
