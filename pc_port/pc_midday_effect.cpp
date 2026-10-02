#if defined(PIKI_PC_PORT)
#include "pc_midday_effect.h"
#include "UtEffect.h"
static_assert(EffectMgr::EFF_COUNT==332,"update pure effect ID schema");
namespace pc_midday {
namespace {
bool tag(ActorArchive&a,EffectRecordKind k){int kind=int(k),version=1;return a.scalar("kind",ScalarKind::S32,&kind)&&a.scalar("version",ScalarKind::S32,&version)&&kind==int(k)&&version==1;}
template<class T>bool links(T&o,ActorArchive&a){zen::CallBack1<zen::particleGenerator*>*g=&o;zen::CallBack2<zen::particleGenerator*,zen::particleMdl*>*pair=&o;zen::CallBack1<zen::particleMdl*>*mdl=&o;auto eg=g;auto ep=pair;auto em=mdl;return a.ref("callback.generator",RefKind::ParticleCallback,g)&&a.ref("callback.pair",RefKind::ParticleCallback,pair)&&a.ref("callback.model",RefKind::ParticleCallback,mdl)&&g==eg&&pair==ep&&mdl==em;}
}
bool effect_fields(BurnEffect&o,ActorArchive&a){if(!tag(a,EffectRecordKind::BurnEffect)||!links(o,a))return a.fail("effect concrete factory/callback binding");
 return a.ref("positionRef",RefKind::Vector3,o._0C)&&a.ref("generatorA",RefKind::ParticleGenerator,o.mEfxA)&&a.ref("generatorB",RefKind::ParticleGenerator,o.mEfxB);
}
bool effect_fields(FreeLightEffect&o,ActorArchive&a){if(!tag(a,EffectRecordKind::FreeLightEffect)||!links(o,a))return a.fail("effect concrete factory/callback binding");
 return a.field("color",o.mColor)&&a.field("scale",o.mScale)&&a.ref("generator",RefKind::ParticleGenerator,o.mEfx);
}
bool effect_fields(GoalEffect&o,ActorArchive&a){if(!tag(a,EffectRecordKind::GoalEffect)||!links(o,a))return a.fail("effect concrete factory/callback binding");
 return true;
}
bool effect_fields(RippleEffect&o,ActorArchive&a){if(!tag(a,EffectRecordKind::RippleEffect)||!links(o,a))return a.fail("effect concrete factory/callback binding");
 return a.ref("generatorA",RefKind::ParticleGenerator,o.mEfxA)&&a.ref("generatorB",RefKind::ParticleGenerator,o.mEfxB)&&a.ref("generatorC",RefKind::ParticleGenerator,o.mEfxC);
}
bool effect_fields(SimpleEffect&o,ActorArchive&a){if(!tag(a,EffectRecordKind::SimpleEffect)||!links(o,a))return a.fail("effect concrete factory/callback binding");
 return a.field("effectId",o.mEfxId)&&a.ref("generator",RefKind::ParticleGenerator,o.mEfx);
}
bool effect_fields(SmokeGrassEffect&o,ActorArchive&a){if(!tag(a,EffectRecordKind::SmokeGrassEffect)||!links(o,a))return a.fail("effect concrete factory/callback binding");
 return true;
}
bool effect_fields(SmokeRockEffect&o,ActorArchive&a){if(!tag(a,EffectRecordKind::SmokeRockEffect)||!links(o,a))return a.fail("effect concrete factory/callback binding");
 return true;
}
bool effect_fields(SmokeSoilEffect&o,ActorArchive&a){if(!tag(a,EffectRecordKind::SmokeSoilEffect)||!links(o,a))return a.fail("effect concrete factory/callback binding");
 return true;
}
bool effect_fields(SmokeTreeEffect&o,ActorArchive&a){if(!tag(a,EffectRecordKind::SmokeTreeEffect)||!links(o,a))return a.fail("effect concrete factory/callback binding");
 return true;
}
bool effect_fields(UfoSuikomiEffect&o,ActorArchive&a){if(!tag(a,EffectRecordKind::UfoSuikomiEffect)||!links(o,a))return a.fail("effect concrete factory/callback binding");
 return a.field("start",o._0C)&&a.field("end",o._18)&&a.ref("generator",RefKind::ParticleGenerator,o.mEfx);
}
bool effect_fields(BombEffect&o,ActorArchive&a){if(!tag(a,EffectRecordKind::BombEffect)||!links(o,a))return a.fail("effect concrete factory/callback binding");
 return true;
}
bool effect_fields(BombEffectLight&o,ActorArchive&a){if(!tag(a,EffectRecordKind::BombEffectLight)||!links(o,a))return a.fail("effect concrete factory/callback binding");
 return true;
}
bool effect_fields(WhistleTemplate&o,ActorArchive&a){if(!tag(a,EffectRecordKind::WhistleTemplate)||!links(o,a))return a.fail("effect concrete factory/callback binding");
 int first=a.mode()==Mode::Capture?int(o.mEffIDA):0,second=a.mode()==Mode::Capture?int(o.mEffIDB):0;if(!a.scalar("effectIdA",ScalarKind::S32,&first)||!a.scalar("effectIdB",ScalarKind::S32,&second)||first<0||first>=EffectMgr::EFF_COUNT||second<0||second>=EffectMgr::EFF_COUNT)return a.fail("effect ID bounds");if(a.mode()==Mode::Apply){o.mEffIDA=EffectMgr::effTypeTable(first);o.mEffIDB=EffectMgr::effTypeTable(second);}
 return a.field("start",o._0C)&&a.field("end",o._18)&&a.ref("generatorA",RefKind::ParticleGenerator,o.mEfxA)&&a.ref("generatorB",RefKind::ParticleGenerator,o.mEfxB);
}
bool effect_fields(UfoSuckEffect&o,ActorArchive&a){if(!tag(a,EffectRecordKind::UfoSuckEffect)||!links(o,a))return a.fail("effect concrete factory/callback binding");
 int first=a.mode()==Mode::Capture?int(o.mEffIDA):0,second=a.mode()==Mode::Capture?int(o.mEffIDB):0;if(!a.scalar("effectIdA",ScalarKind::S32,&first)||!a.scalar("effectIdB",ScalarKind::S32,&second)||first<0||first>=EffectMgr::EFF_COUNT||second<0||second>=EffectMgr::EFF_COUNT)return a.fail("effect ID bounds");if(a.mode()==Mode::Apply){o.mEffIDA=EffectMgr::effTypeTable(first);o.mEffIDB=EffectMgr::effTypeTable(second);}
 return a.field("start",o._0C)&&a.field("end",o._18)&&a.ref("generatorA",RefKind::ParticleGenerator,o.mEfxA)&&a.ref("generatorB",RefKind::ParticleGenerator,o.mEfxB);
}
bool effect_fields(SlimeEffect&o,ActorArchive&a){if(!tag(a,EffectRecordKind::SlimeEffect)||!links(o,a))return a.fail("effect concrete factory/callback binding");
 if(!a.ref("generator",RefKind::ParticleGenerator,o.mEfxGen))return false;Creature* owner=a.mode()==Mode::Capture&&o.mEfxGen?o.mObj:nullptr;if(!a.ref("owner",RefKind::Creature,owner))return false;if(a.mode()==Mode::Apply)o.mObj=owner;return true;
}
}
#endif
