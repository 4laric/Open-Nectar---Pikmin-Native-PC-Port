#include "pc_midday_inventory.h"
namespace pc_midday {
namespace {
bool known(Family f){return uint32_t(f)>=uint32_t(Family::Captain)&&uint32_t(f)<=uint32_t(Family::Projectile);}
bool fail(std::string& e,const char* text){e=text;return false;}
bool nonzero(const Digest& d){for(auto b:d)if(b)return true;return false;}
bool valid(const InventoryScope& s){return s.generation&&nonzero(s.binding.seed)&&nonzero(s.binding.session)&&nonzero(s.binding.content)&&nonzero(s.binding.schema);}
}
bool sameInventoryScope(const InventoryScope& a,const InventoryScope& b){return a.generation==b.generation&&a.frame==b.frame&&a.binding.seed==b.binding.seed&&a.binding.session==b.binding.session&&a.binding.content==b.binding.content&&a.binding.schema==b.binding.schema;}
const BirthLedger* CheckpointInventory::identities(const InventoryScope& s,std::string& e)const{if(!ready_||!sameInventoryScope(scope_,s)){e="inventory consumer belongs to another checkpoint epoch";return nullptr;}e.clear();return &identities_;}
bool CheckpointInventory::root(const InventoryScope& s,uint64_t id,InventoryRoot& out,std::string& e)const{if(!identities(s,e))return false;auto it=roots_.find(id);if(it==roots_.end())return fail(e,"actor ordinal absent from checkpoint inventory");out=it->second;e.clear();return true;}
bool CheckpointInventory::ids(const InventoryScope& s,std::set<uint64_t>& out,std::string& e)const{if(!identities(s,e))return false;std::set<uint64_t> ids;for(const auto& entry:roots_)ids.insert(entry.first);out.swap(ids);e.clear();return true;}
bool buildCheckpointInventory(const InventoryScope& scope,const InventoryFactory& factory,const std::vector<InventoryChannel>& channels,CheckpointInventory& out,std::string& e){
 if(!valid(scope)||factory.empty()||factory.size()>1024||channels.size()!=factory.size())return fail(e,"invalid epoch or incomplete source inventory channels");
 std::set<Family> families;
 for(const auto& entry:factory){if(entry.first.empty()||entry.first.size()>128||entry.first.find('\0')!=std::string::npos||entry.second.empty())return fail(e,"invalid source factory channel");for(auto f:entry.second){if(!known(f))return fail(e,"unknown factory family");families.insert(f);}}
 for(uint32_t f=uint32_t(Family::Captain);f<=uint32_t(Family::Projectile);++f)if(!families.count(Family(f)))return fail(e,"factory inventory omits an actor family");
 std::map<std::string,const InventoryChannel*> sorted;std::set<const void*> unique;
 for(const auto& channel:channels){auto contract=factory.find(channel.source);if(contract==factory.end()||!sorted.emplace(channel.source,&channel).second||!channel.initialized||!channel.complete||!channel.stopped||channel.frameAfter!=scope.frame||!sameInventoryScope(channel.scope,scope))return fail(e,"mixed epoch/tick or incomplete native inventory channel");
  if(channel.roots.size()>MaxActors)return fail(e,"native source channel exceeds bound");
  for(const auto& root:channel.roots){if(!root.address||!contract->second.count(root.family)||(root.life!=SlotLife::Active&&root.life!=SlotLife::Retained)||!unique.insert(root.address).second||unique.size()>MaxActors)return fail(e,"unknown/duplicate/null native actor ownership");}
 }
 if(unique.empty())return fail(e,"empty inventory cannot prove a playable world");
 CheckpointInventory stage;stage.scope_=scope;
 for(const auto& channel:sorted)for(const auto& root:channel.second->roots){uint64_t id=0;if(!stage.identities_.birth(root.address,root.family,id,e))return false;stage.roots_.emplace(id,root);}
 stage.ready_=true;out=std::move(stage);e.clear();return true;
}
bool monoInventoryRoots(const MonoPoolView& view,Family family,std::vector<InventoryRoot>& out,std::string& e){
 if(!known(family)||view.capacity>MaxActors||view.count>view.capacity||view.statuses.size()!=view.capacity||view.objects.size()!=view.capacity)return fail(e,"incomplete native mono inventory view");
 std::vector<InventoryRoot> roots;std::set<const void*> backing;
 for(size_t i=0;i<view.capacity;++i){if(!view.objects[i]||!backing.insert(view.objects[i]).second)return fail(e,"missing/aliased native pool backing");switch(view.statuses[i]){case -1:break;case 0:roots.push_back({view.objects[i],family,SlotLife::Active});break;case -2:roots.push_back({view.objects[i],family,SlotLife::Retained});break;default:return fail(e,"unknown mono pool lifecycle");}}
 if(roots.size()!=view.count)return fail(e,"mono active/retained inventory count mismatch");
 out=std::move(roots);e.clear();return true;
}
}
