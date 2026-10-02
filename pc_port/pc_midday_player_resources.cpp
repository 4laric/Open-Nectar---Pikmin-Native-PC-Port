#include "pc_midday_player_resources.h"
#include "pc_midday_world.h"
#include "pc_midday_world_resources.h"
#include <set>
namespace pc_midday {
namespace {
bool bad(std::string& e,const char* s){e=s;return false;}
bool scalar(const ActorFields& f,const std::string& k,ScalarKind kind,u64& v,std::string& e){auto it=f.find(k);if(it==f.end()||it->second.category!=FieldCategory::Scalar||it->second.scalar!=kind)return bad(e,"player resource discriminator type");v=it->second.bits;return true;}
void val(std::vector<FieldSchema>& s,const std::string& k,ScalarKind t){s.push_back(FieldSchema::value(k.c_str(),t));}
void vec(std::vector<FieldSchema>& s,const std::string& k){for(auto a:{".x",".y",".z"})val(s,k+a,ScalarKind::F32);}
bool animation(const ActorFields& f,const std::string& prefix,std::vector<FieldSchema>& s,std::string& e){
 const auto begin=s.size();if(!world_animation_schema(f,prefix+".",s,e))return false;
 // This visitor's subject is a PlayerState resource, not a Creature. Nullable
 // listeners still require exact registered resource/interface membership.
 for(size_t i=begin;i<s.size();++i)if(s[i].reference==RefKind::AnimListener&&s[i].category==FieldCategory::Reference)s[i].ownership=ReferenceOwnership::ResourceSubobject;
 return true;
}
}
bool player_resources_schema(const ActorFields& f,std::vector<FieldSchema>& out,std::string& e){
 std::vector<FieldSchema> s;int version=0,total=0,registered=0,repair=0;
 if(!actor_i32(f,"player.resources.version",version,e)||version!=1||!actor_i32(f,"player.resources.total",total,e)||total!=30||!actor_i32(f,"player.resources.registered",registered,e)||registered<0||registered>total||!actor_i32(f,"player.resources.repair",repair,e)||repair< -1||repair>=registered)return bad(e,"player resource registration/repair bounds");
 for(auto k:{"version","total","registered","repair"})val(s,std::string("player.resources.")+k,ScalarKind::S32);
 val(s,"player.resources.preload",ScalarKind::Bool);
 for(int i=0;i<30;++i){const auto p="player.resources.part."+std::to_string(i)+".";u64 vis=0;if(!scalar(f,p+"visibility",ScalarKind::U8,vis,e)||vis>2)return bad(e,"UfoParts visibility invalid");val(s,p+"visibility",ScalarKind::U8);val(s,"player.resources.replay."+std::to_string(i),ScalarKind::Bool);}
 std::set<u32> models,pellets;
 for(int i=0;i<registered;++i){const auto p="player.resources.part."+std::to_string(i)+".";int joint=0;u32 model=0,pellet=0;u64 shaped=0;
  if(!actor_i32(f,(p+"joint").c_str(),joint,e)||joint< -1||!actor_u32(f,(p+"model").c_str(),model,e)||!actor_u32(f,(p+"pellet").c_str(),pellet,e)||!models.insert(model).second||!pellets.insert(pellet).second||!scalar(f,p+"shaped",ScalarKind::Bool,shaped,e)||shaped>1)return bad(e,"UfoParts factory descriptor invalid");
  val(s,p+"joint",ScalarKind::S32);val(s,p+"model",ScalarKind::U32);val(s,p+"pellet",ScalarKind::U32);val(s,p+"shaped",ScalarKind::Bool);vec(s,p+"repairPosition");
  s.push_back(FieldSchema::ref((p+"listenerIdentity").c_str(),RefKind::AnimListener,false,"PlayerState::UfoParts",ReferenceOwnership::ResourceSubobject));
  s.push_back(FieldSchema::ref((p+"shape").c_str(),RefKind::ItemShape,!shaped,"PelletShapeObject",ReferenceOwnership::Content));
  const auto it=f.find(p+"shape");if(it==f.end()||bool(it->second.target.owner||it->second.target.resource||it->second.target.slot)!=bool(shaped))return bad(e,"UfoParts shape discriminator mismatch");
  if(!world_materials_schema(f,p+"materials",s,e))return false;
  if(shaped){val(s,p+"motionSpeed",ScalarKind::F32);if(!animation(f,p+"lower",s,e)||!animation(f,p+"upper",s,e))return false;
   auto identity=f.find(p+"listenerIdentity");if(identity==f.end())return bad(e,"UfoParts listener identity missing");
   for(auto name:{"lower","upper"}){auto listener=f.find(p+name+".mListener");if(listener==f.end())continue;const auto& id=identity->second.target;const auto& got=listener->second.target;
    if((got.owner||got.resource||got.slot)&&(got.owner!=id.owner||got.resource!=id.resource||got.slot!=id.slot))return bad(e,"UfoParts animator listener belongs to different part");
    for(auto& entry:s)if(entry.key==listener->first)entry.targetType="PlayerState::UfoParts";
   }
  }
 }
 s.push_back(FieldSchema::ref("player.resources.olimarShape",RefKind::Shape,false,"PikiShapeObject",ReferenceOwnership::Content));
 val(s,"player.resources.olimarSpeed",ScalarKind::F32);
 if(!animation(f,"player.resources.olimarLower",s,e)||!animation(f,"player.resources.olimarUpper",s,e))return false;
 for(auto key:{"light","lightGlow"})s.push_back(FieldSchema::ref((std::string("player.resources.")+key).c_str(),RefKind::Effect,false,"PermanentEffect",ReferenceOwnership::AnyLive));
 vec(s,"player.resources.lightPosition");out=std::move(s);e.clear();return true;
}
bool validatePlayerResources(const ActorBytes& bytes,const LogicalResolver& resolver,std::string& e){ActorFields f;std::vector<FieldSchema> s;return decode_actor_fields(bytes,f,e)&&player_resources_schema(f,s,e)&&validate_actor_fields(f,s,resolver,e);}
}
