#include "pc_midday_player_resource_stage.h"
namespace pc_midday {
bool planPlayerResources(const ActorBytes& bytes,const PlayerCoreFields& core,const LogicalResolver& resolver,PlayerResourcePlan& out,std::string& e){
 ActorFields fields;std::vector<FieldSchema> schema;PlayerResourcePlan staged;
 if(!decode_actor_fields(bytes,fields,e)||!player_resources_schema(fields,schema,e)||!validate_actor_fields(fields,schema,resolver,e))return false;
 int total=0;if(!actor_i32(fields,"player.resources.total",total,e)||!actor_i32(fields,"player.resources.registered",staged.registered,e)||!actor_i32(fields,"player.resources.repair",staged.repair,e))return false;
 if(core.totalParts!=total||core.totalRegisteredParts!=staged.registered){e="PlayerCore and course resource registration disagree";return false;}
 for(size_t i=0;i<30;++i){const auto prefix="player.resources.part."+std::to_string(i)+".";
  staged.visibility[i]=uint8_t(fields.at(prefix+"visibility").bits);
  if(i<size_t(staged.registered)&&!actor_i32(fields,(prefix+"materials.count").c_str(),staged.materials[i],e))return false;
 }
 out=staged;e.clear();return true;
}
}
