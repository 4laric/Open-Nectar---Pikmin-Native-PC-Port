#include "pc_midday_item_factory.h"
namespace pc_midday {
bool preflightItemPlacement(const PolyPoolPlan&p,const PolyPoolView&v,const std::set<int>&allocated,const std::set<int>&constructible,std::vector<ItemPlacement>&out,std::string&e){
 if(v.capacity!=p.capacity||v.count||!v.stride||v.objects.size()!=v.capacity||v.statuses.size()!=v.capacity){e="item placement destination geometry is not fresh";return false;}
 std::set<uint64_t>ids;for(const auto&s:p.slots)if(s.actor)ids.insert(s.actor);
 if(!validatePolyPool(p,ids,allocated,e))return false;
 std::set<const void*>unique,prototypes;for(const auto&t:v.templates)if(t.prototype)prototypes.insert(t.prototype);std::vector<ItemPlacement>tasks;
 for(size_t i=0;i<v.capacity;++i){
  if(v.statuses[i]!=-1||!v.objects[i]||prototypes.count(v.objects[i])||!unique.insert(v.objects[i]).second){e="item placement backing is not free/distinct";return false;}
  const auto&s=p.slots[i];if(s.actor){if(!constructible.count(s.classId)){e="item constructor resources/subobjects not implemented for this class";return false;}tasks.push_back({uint32_t(i),s.actor,s.classId});}
 }
 out=std::move(tasks);e.clear();return true;
}
}
