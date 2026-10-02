#include "pc_midday_effect.h"
namespace pc_midday {
bool effect_schema(const ActorFields&f,EffectRecordKind expected,const std::string&prefix,std::vector<FieldSchema>&s,std::string&e){
 const std::string p=prefix.empty()?"":prefix+".";int kind=0,version=0;if(!actor_i32(f,(p+"kind").c_str(),kind,e)||!actor_i32(f,(p+"version").c_str(),version,e)||kind!=int(expected)||kind<1||kind>15||version!=1){e="effect concrete factory/version mismatch";return false;}
 auto val=[&](const std::string&k,ScalarKind t){s.push_back(FieldSchema::value((p+k).c_str(),t));};auto vec=[&](const std::string&k){for(auto x:{"x","y","z"})val(k+"."+x,ScalarKind::F32);};
 auto ref=[&](const std::string&k,RefKind r,bool n,const char*t,ReferenceOwnership own=ReferenceOwnership::AnyLive){s.push_back(FieldSchema::ref((p+k).c_str(),r,n,t,own));};auto gen=[&](const char*k){ref(k,RefKind::ParticleGenerator,true,"zen::particleGenerator");};
 val("kind",ScalarKind::S32);val("version",ScalarKind::S32);
 ref("callback.generator",RefKind::ParticleCallback,false,"zen::CallBack1<zen::particleGenerator*>",ReferenceOwnership::ResourceSelf);ref("callback.pair",RefKind::ParticleCallback,false,"zen::CallBack2<zen::particleGenerator*,zen::particleMdl*>",ReferenceOwnership::ResourceSelf);ref("callback.model",RefKind::ParticleCallback,false,"zen::CallBack1<zen::particleMdl*>",ReferenceOwnership::ResourceSelf);
 auto id=[&](const char*k){val(k,ScalarKind::S32);int n=0;return actor_i32(f,(p+k).c_str(),n,e)&&n>=0&&n<332;};
 switch(expected){
 case EffectRecordKind::BurnEffect:ref("positionRef",RefKind::Vector3,true,"Vector3f");gen("generatorA");gen("generatorB");break;
 case EffectRecordKind::FreeLightEffect:val("color",ScalarKind::U16);val("scale",ScalarKind::F32);gen("generator");break;
 case EffectRecordKind::RippleEffect:gen("generatorA");gen("generatorB");gen("generatorC");break;
 case EffectRecordKind::SimpleEffect:if(!id("effectId")){e="effect ID bound";return false;}gen("generator");break;
 case EffectRecordKind::UfoSuikomiEffect:vec("start");vec("end");gen("generator");break;
 case EffectRecordKind::WhistleTemplate:case EffectRecordKind::UfoSuckEffect:vec("start");vec("end");gen("generatorA");gen("generatorB");if(!id("effectIdA")||!id("effectIdB")){e="effect ID bound";return false;}break;
 case EffectRecordKind::SlimeEffect:{gen("generator");auto g=f.find(p+"generator");bool has=g!=f.end()&&(g->second.target.owner||g->second.target.resource||g->second.target.slot);ref("owner",RefKind::Creature,!has,"Creature");break;}
 default:break; // Stateless concrete emitters: no hidden gameplay payload.
 }return true;
}
}
