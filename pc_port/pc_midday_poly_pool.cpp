#include "pc_midday_poly_pool.h"
#include <algorithm>
namespace pc_midday {
namespace {
bool fail(std::string&e,const char*text){e=text;return false;}
bool valid(const PolyPoolPlan&p,std::set<uint64_t>&ids,std::string&e){
 if(p.capacity>MaxActors||p.count>p.capacity||p.slots.size()!=p.capacity||p.classes.empty()||p.classes.size()>4096)return fail(e,"poly plan geometry outside bounds");
 for(int c:p.classes)if(c<0)return fail(e,"poly factory class invalid");
 uint32_t count=0;
 for(const auto&s:p.slots){
  if(s.life==SlotLife::Free){if(s.actor||s.classId!=-1)return fail(e,"free poly slot has identity/class");}
  else if(s.life==SlotLife::Active||s.life==SlotLife::Retained){if(!s.actor||!ids.insert(s.actor).second||!p.classes.count(s.classId))return fail(e,"poly occupied identity/class invalid");++count;}
  else return fail(e,"poly lifecycle invalid");
 }
 return count==p.count?true:fail(e,"poly count omits active/retained slots");
}
void put(Bytes&b,uint64_t n,unsigned w){for(unsigned i=0;i<w;++i)b.push_back(uint8_t(n>>(8*i)));}
uint64_t get(const Bytes&b,size_t a,unsigned w){uint64_t n=0;for(unsigned i=0;i<w;++i)n|=uint64_t(b[a+i])<<(8*i);return n;}
}
bool planPolyPool(const PolyPoolView&v,const CheckpointInventory&inventory,const InventoryScope&scope,const PolyConcreteTypes&types,PolyPoolPlan&out,std::string&e){
 const auto*ledger=inventory.identities(scope,e);if(!ledger)return false;
 if(v.capacity>MaxActors||v.count>v.capacity||!v.stride||v.statuses.size()!=v.capacity||v.objects.size()!=v.capacity||v.templates.empty()||v.templates.size()>4096)return fail(e,"poly native view incomplete");
 PolyPoolPlan p;p.capacity=v.capacity;p.count=v.count;
 std::set<const void*>prototypes;std::set<int>registered;
 for(const auto&t:v.templates){
  if(t.classId<0||t.bytes>v.stride||!registered.insert(t.classId).second||bool(t.bytes)!=bool(t.prototype))return fail(e,"poly factory templates invalid/aliased");
  if(!types.validateTemplate(t,e))return false;
  if(t.bytes){if(!prototypes.insert(t.prototype).second)return fail(e,"poly allocated prototypes aliased");p.classes.insert(t.classId);}
  // Explicit source-validated forwarding entries own no Poly backing objects.
  // Their separately managed actors still require full scene channel capture.
 }
 if(p.classes.empty())return fail(e,"poly backing has no compiled allocated classes");
 std::set<const void*>addresses;
 for(size_t i=0;i<v.capacity;++i){
  if(!v.objects[i]||prototypes.count(v.objects[i])||!addresses.insert(v.objects[i]).second)return fail(e,"poly backing slots missing/aliased");
  PolySlot s;
  if(v.statuses[i]==-1){if(ledger->lookup(v.objects[i]))return fail(e,"free poly slot appears in actor inventory");}
  else {
   if(v.statuses[i]<-2)return fail(e,"unknown poly source status");
   if(v.statuses[i]!=-2&&!p.classes.count(v.statuses[i]))return fail(e,"active poly class is not registered");
   const auto*life=ledger->lookup(v.objects[i]);if(!life)return fail(e,"active/retained poly root missing from scoped inventory");
   InventoryRoot root;if(!inventory.root(scope,life->id,root,e))return false;
   s.life=v.statuses[i]==-2?SlotLife::Retained:SlotLife::Active;
   if(root.address!=v.objects[i]||root.life!=s.life)return fail(e,"poly lifecycle disagrees with scoped census");
   unsigned matches=0;
   for(int c:p.classes){bool match=false;if(!types.matches(c,v.objects[i],match,e))return false;if(match){s.classId=c;++matches;}}
   if(matches!=1||(s.life==SlotLife::Active&&s.classId!=v.statuses[i]))return fail(e,"poly concrete class absent/ambiguous/index mismatch");
   s.actor=life->id;
  }
  p.slots.push_back(s);
 }
 std::set<uint64_t>ids;if(!valid(p,ids,e))return false;out=std::move(p);e.clear();return true;
}
bool validatePolyPool(const PolyPoolPlan&p,const std::set<uint64_t>&expected,const std::set<int>&classes,std::string&e){
 std::set<uint64_t>ids;if(!valid(p,ids,e))return false;
 if(ids!=expected||p.classes!=classes)return fail(e,"poly saved actor/factory inventory mismatch");
 e.clear();return true;
}
bool encodePolyPool(const PolyPoolPlan&p,Bytes&out,std::string&e){
 std::set<uint64_t>ids;if(!valid(p,ids,e))return false;
 Bytes b={'P','C','P','O','L','Y','0','1'};put(b,1,4);put(b,p.capacity,4);put(b,p.count,4);put(b,p.classes.size(),4);
 for(int c:p.classes)put(b,uint32_t(c),4);
 for(const auto&s:p.slots){put(b,uint8_t(s.life),1);put(b,s.actor,8);put(b,uint32_t(s.classId),4);}
 out=std::move(b);e.clear();return true;
}
bool decodePolyPool(const Bytes&b,PolyPoolPlan&out,std::string&e){
 const Bytes magic={'P','C','P','O','L','Y','0','1'};
 if(b.size()<24||!std::equal(magic.begin(),magic.end(),b.begin())||get(b,8,4)!=1)return fail(e,"poly framing/version invalid");
 PolyPoolPlan p;p.capacity=uint32_t(get(b,12,4));p.count=uint32_t(get(b,16,4));auto n=get(b,20,4);
 if(p.capacity>MaxActors||!n||n>4096||b.size()!=24+size_t(n)*4+size_t(p.capacity)*13)return fail(e,"poly wire geometry invalid");
 for(size_t i=0;i<n;++i){auto c=get(b,24+i*4,4);if(c>0x7fffffff||!p.classes.insert(int(c)).second)return fail(e,"poly wire factory duplicate/invalid");}
 const size_t start=24+size_t(n)*4;
 for(size_t i=0;i<p.capacity;++i){const auto a=start+i*13;auto c=get(b,a+9,4);if(c!=0xffffffff&&c>0x7fffffff)return fail(e,"poly wire slot class invalid");p.slots.push_back({SlotLife(b[a]),get(b,a+1,8),c==0xffffffff?-1:int(c)});}
 std::set<uint64_t>ids;if(!valid(p,ids,e))return false;out=std::move(p);e.clear();return true;
}
}
