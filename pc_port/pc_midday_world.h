#pragma once
#include "pc_midday_creature.h"
class ItemCreature;
class AICreature;
class PaniAnimator;
template<class T> class StateMachine;
namespace pc_midday {
// Factory-derived family/subtype, never echo these from untrusted bytes.
enum class WorldKind : int { Item=1, Pellet=2, Bridge=3, HinderRock=4, Plant=5, Rope=6, Seed=7, Key=8 };
enum class WorldSAIKind:int { Goal=1,Head=2,Gate=3,Bomb=4,Water=5,Plant=6 };
bool world_sai_registered(WorldSAIKind,int);
// One canonical record per native machine allocation; never repeat in actors.
bool world_sai_machine_fields(StateMachine<AICreature>&,WorldSAIKind,ActorArchive&);
bool world_sai_machine_schema(const ActorFields&,WorldSAIKind,const std::string&,std::vector<FieldSchema>&,std::string&);
// Shared inherited ItemCreature payload (not Creature); also used by Bomb adapter.
bool world_item_fields(ItemCreature&,ActorArchive&);
bool world_item_schema(const ActorFields&,std::vector<FieldSchema>&,std::string&);
bool world_ai_fields(AICreature&,ActorArchive&);
bool world_ai_schema(const ActorFields&,std::vector<FieldSchema>&,std::string&);
bool world_animation_fields(PaniAnimator&,ActorArchive&);
bool world_animation_schema(const ActorFields&,const std::string&,std::vector<FieldSchema>&,std::string&);
bool world_fields(Creature&,WorldKind,ActorArchive&);
bool world_schema(const ActorFields&,int expectedObjectType,WorldKind,std::vector<FieldSchema>&,std::string&);
bool capture_world(Creature&,WorldKind,LogicalResolver&,double,ActorBytes&,std::string&);
bool validate_world(const ActorBytes&,const LogicalResolver&,int expectedObjectType,WorldKind,std::string&);
bool bind_world(Creature&,WorldKind,const ActorBytes&,LogicalResolver&,double,std::string&);
}
