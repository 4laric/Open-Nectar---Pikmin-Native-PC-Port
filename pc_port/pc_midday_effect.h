#pragma once
#include "pc_midday_actor_archive.h"
struct BurnEffect;
struct FreeLightEffect;
struct GoalEffect;
struct RippleEffect;
struct SimpleEffect;
struct SmokeGrassEffect;
struct SmokeRockEffect;
struct SmokeSoilEffect;
struct SmokeTreeEffect;
struct UfoSuikomiEffect;
struct BombEffect;
struct BombEffectLight;
struct WhistleTemplate;
struct UfoSuckEffect;
struct SlimeEffect;
namespace pc_midday {
enum class EffectRecordKind:int {BurnEffect=1,FreeLightEffect=2,GoalEffect=3,RippleEffect=4,SimpleEffect=5,SmokeGrassEffect=6,SmokeRockEffect=7,SmokeSoilEffect=8,SmokeTreeEffect=9,UfoSuikomiEffect=10,BombEffect=11,BombEffectLight=12,WhistleTemplate=13,UfoSuckEffect=14,SlimeEffect=15};
bool effect_fields(BurnEffect&,ActorArchive&);
bool effect_fields(FreeLightEffect&,ActorArchive&);
bool effect_fields(GoalEffect&,ActorArchive&);
bool effect_fields(RippleEffect&,ActorArchive&);
bool effect_fields(SimpleEffect&,ActorArchive&);
bool effect_fields(SmokeGrassEffect&,ActorArchive&);
bool effect_fields(SmokeRockEffect&,ActorArchive&);
bool effect_fields(SmokeSoilEffect&,ActorArchive&);
bool effect_fields(SmokeTreeEffect&,ActorArchive&);
bool effect_fields(UfoSuikomiEffect&,ActorArchive&);
bool effect_fields(BombEffect&,ActorArchive&);
bool effect_fields(BombEffectLight&,ActorArchive&);
bool effect_fields(WhistleTemplate&,ActorArchive&);
bool effect_fields(UfoSuckEffect&,ActorArchive&);
bool effect_fields(SlimeEffect&,ActorArchive&);
bool effect_schema(const ActorFields&,EffectRecordKind,const std::string&,std::vector<FieldSchema>&,std::string&);
}
