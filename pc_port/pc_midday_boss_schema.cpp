#include "pc_midday_enemy.h"
namespace pc_midday {
int boss_object_type(int id){static const int types[]={39,41,44,43,45,46,47,48,48,42,49,49};return id>=0&&id<12?types[id]:-1;}
bool boss_base_schema(const ActorFields&,std::vector<FieldSchema>& out,std::string&) {
 const std::string p="boss.base.";
#define FIELD(k,x) out.push_back(FieldSchema::value((p+#x).c_str(),ScalarKind::k));
#define VECTOR(x) for(const char* c:{".x",".y",".z"})out.push_back(FieldSchema::value((p+#x+c).c_str(),ScalarKind::F32));
#include "pc_midday_boss_base_fields.inc"
#undef FIELD
#undef VECTOR
 out.push_back(FieldSchema::ref((p+"target").c_str(),RefKind::Creature,true,"Creature"));out.push_back(FieldSchema::ref((p+"wallObject").c_str(),RefKind::DynCollObject,true,"DynCollObject"));
 for(const char* c:{"x","y","z"})out.push_back(FieldSchema::value((p+"wall.normal."+c).c_str(),ScalarKind::F32));
 out.push_back(FieldSchema::value((p+"wall.offset").c_str(),ScalarKind::F32));out.push_back(FieldSchema::value((p+"pelletID").c_str(),ScalarKind::U32));
 enemy_animation_schema(p+"animation.",out);return true;
}
bool boss_small_family_schema(const ActorFields& f,std::vector<FieldSchema>& out,std::string& error) {
 const std::string p="boss.family.";int id=0,state=0,next=0;
 if(!actor_i32(f,(p+"id").c_str(),id,error)||(id!=4&&id!=5&&id!=10&&id!=11)){error="boss derived family not implemented";return false;}
 const int maxState=id==4?4:id==5?5:2;
 if(!actor_i32(f,"boss.base.mCurrentStateID",state,error)||!actor_i32(f,"boss.base.mNextStateID",next,error)||state<0||state>maxState||next<0||next>maxState){error="invalid concrete boss state";return false;}
 if(id==5){
  int color=0,previous=0,released=0,maximum=0;
  if(!actor_i32(f,(p+"color").c_str(),color,error)||color<0||color>2||!actor_i32(f,(p+"previousPiki").c_str(),previous,error)||previous<0||previous>4096||!actor_i32(f,(p+"released").c_str(),released,error)||released<0||released>4096||!actor_i32(f,(p+"maximum").c_str(),maximum,error)||maximum<0||maximum>4096){error="invalid Pom conversion counters";return false;}
 }
 if(id==4){int drops=0;if(!actor_i32(f,(p+"dropCount").c_str(),drops,error)||drops<0){error="invalid Kogane drop count";return false;}}
 auto field=[&](const char* key,ScalarKind k){out.push_back(FieldSchema::value((p+key).c_str(),k));};
 auto ref=[&](const char* key,RefKind k,bool nullable,const char* type,ReferenceOwnership owner){out.push_back(FieldSchema::ref((p+key).c_str(),k,nullable,type,owner));};
 field("id",ScalarKind::S32);
 if(id==4){for(const char* k:{"appear","pelletPending","inWater"})field(k,ScalarKind::Bool);for(const char* k:{"dropCount","effectType"})field(k,ScalarKind::S32);for(const char* k:{"appearTimer","idleDuration"})field(k,ScalarKind::F32);ref("owner",RefKind::Creature,false,"Kogane",ReferenceOwnership::Self);ref("rippleOwner",RefKind::Creature,true,"Kogane",ReferenceOwnership::Self);}
 if(id==5){for(const char* k:{"touching","collided","playSound","opening","callbackBound"})field(k,ScalarKind::Bool);for(const char* k:{"color","previousPiki","released","maximum"})field(k,ScalarKind::S32);for(const char* k:{"deformAmount","currentDeform"})field(k,ScalarKind::F32);ref("owner",RefKind::Creature,false,"Pom",ReferenceOwnership::Self);}
 if(id==10||id==11){field("visible",ScalarKind::Bool);ref("owner",RefKind::Creature,false,"Mizu",ReferenceOwnership::Self);for(const char* k:{"bubble","mist","puff"})ref(k,RefKind::ParticleGenerator,true,"zen::particleGenerator",ReferenceOwnership::ActorSubobject);}
 return true;
}

}
