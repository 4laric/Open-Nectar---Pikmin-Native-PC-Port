#include "pc_midday_animation_stage.h"
namespace pc_midday {
bool planCanonicalContexts(const std::vector<CanonicalContextRecord>& records,const std::set<u64>& required,const ContextResolverFactory& factory,const AnimationContentCheck& content,std::vector<PlannedContext>& out,std::string& e){
 if(records.size()>4096||required.size()>4096||required.count(0)||!factory||!content){e="canonical animation context inventory/factory invalid";return false;}
 std::set<u64> seen;for(const auto& record:records)if(!record.id||!required.count(record.id)||!seen.insert(record.id).second){e="canonical animation context ID absent/foreign/duplicated";return false;}
 if(seen!=required){e="canonical animation context inventory incomplete";return false;}
 std::vector<PlannedContext> planned;planned.reserve(records.size());
 for(const auto& record:records){
  auto* resolver=factory(record.id);if(!resolver){e="canonical animation context has no compiled resolver";return false;}
  PlannedContext next;next.id=record.id;
  if(!decodeAnimationContext(record.payload,*resolver,content,next.state,e))return false;
  ActorFields fields;if(!decode_actor_fields(record.payload,fields,e)||!next.pointers.prepare(fields,animationContextSchema(),*resolver,{},e))return false;
  planned.push_back(std::move(next));
 }
 out=std::move(planned);e.clear();return true;
}
}
