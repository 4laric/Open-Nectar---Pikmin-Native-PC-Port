#include "pc_midday_poly_pool.h"
#include <cstdlib>
#include <iostream>
using namespace pc_midday;
int checks=0;void check(bool b){++checks;if(!b){std::cerr<<"poly49 check "<<checks<<" failed\n";std::exit(1);}}
struct Types:PolyConcreteTypes {
 std::map<int,PolyTemplateView> layouts;
 bool validateTemplate(const PolyTemplateView&t,std::string&e)const override {auto i=layouts.find(t.classId);if(i==layouts.end()||i->second.bytes!=t.bytes||i->second.prototype!=t.prototype){e="compiled factory layout mismatch";return false;}return true;}
 std::map<const void*,int> actual;bool ambiguous=false,refuse=false;mutable unsigned calls=0;
 bool matches(int c,const void*p,bool&match,std::string&e)const override {++calls;if(refuse){e="factory refused";return false;}auto i=actual.find(p);if(i==actual.end()){e="free object inspected";return false;}match=ambiguous||i->second==c;return true;}
};
int main(){
 int roots[3]{},templates[2]{};std::string e;InventoryScope scope;scope.generation=9;scope.frame=80;scope.binding.seed[0]=1;scope.binding.session[0]=2;scope.binding.content[0]=3;scope.binding.schema[0]=4;
 InventoryFactory f;std::vector<InventoryChannel> channels;
 for(uint32_t n=uint32_t(Family::Captain);n<=uint32_t(Family::Projectile);++n){auto key=std::to_string(n);f[key]={Family(n)};channels.push_back({scope,key,true,true,true,80,{}});}
 channels[4].roots={{&roots[0],Family::Cargo,SlotLife::Active},{&roots[1],Family::Cargo,SlotLife::Retained}};
 CheckpointInventory inventory;check(buildCheckpointInventory(scope,f,channels,inventory,e));
 PolyPoolView view;view.capacity=3;view.count=2;view.stride=128;view.statuses={7,-2,-1};view.objects={&roots[0],&roots[1],&roots[2]};view.templates={{7,64,&templates[0]},{11,96,&templates[1]}};
 Types types;for(const auto&t:view.templates)types.layouts[t.classId]=t;types.actual={{&roots[0],7},{&roots[1],11}};PolyPoolPlan plan;
 check(planPolyPool(view,inventory,scope,types,plan,e));check(plan.slots[1].life==SlotLife::Retained&&plan.slots[1].classId==11&&plan.slots[1].actor==2&&types.calls==4);check(plan.slots[2].classId==-1&&!plan.slots[2].actor);
 check(validatePolyPool(plan,{1,2},{7,11},e));check(!validatePolyPool(plan,{1},{7,11},e));check(!validatePolyPool(plan,{1,2},{7},e));
 Bytes encoded;check(encodePolyPool(plan,encoded,e));PolyPoolPlan decoded;check(decodePolyPool(encoded,decoded,e)&&decoded.slots[1].classId==11&&decoded.slots[1].life==SlotLife::Retained);Bytes again;check(encodePolyPool(decoded,again,e)&&again==encoded);
 auto reject=[&](PolyPoolView bad){check(!planPolyPool(bad,inventory,scope,types,plan,e));check(plan.count==2&&plan.slots[1].classId==11);};
 auto bad=view;bad.count=1;reject(bad);bad=view;bad.objects[2]=bad.objects[0];reject(bad);bad=view;bad.statuses[0]=11;reject(bad);bad=view;bad.statuses[0]=99;reject(bad);bad=view;bad.statuses[2]=-3;reject(bad);bad=view;bad.statuses[0]=-1;reject(bad);bad=view;bad.statuses[1]=7;reject(bad);bad=view;bad.templates[1].classId=7;reject(bad);bad=view;bad.templates[0].bytes=129;reject(bad);bad=view;bad.templates[1].prototype=&templates[0];reject(bad);bad=view;bad.objects.pop_back();reject(bad);
 bad=view;bad.templates[0].bytes=63;reject(bad);bad=view;bad.objects[2]=&templates[0];reject(bad);
 types.ambiguous=true;reject(view);types.ambiguous=false;types.refuse=true;reject(view);types.refuse=false;PolyConcreteTypes unavailable;check(!planPolyPool(view,inventory,scope,unavailable,plan,e));auto wrong=scope;++wrong.generation;check(!planPolyPool(view,inventory,wrong,types,plan,e));
 for(size_t i=0;i<encoded.size();++i){Bytes truncated(encoded.begin(),encoded.begin()+i);check(!decodePolyPool(truncated,decoded,e));check(decoded.slots[1].classId==11);}
 auto corrupt=encoded;corrupt[8]=2;check(!decodePolyPool(corrupt,decoded,e));corrupt=encoded;corrupt[28]=7;check(!decodePolyPool(corrupt,decoded,e));corrupt=encoded;corrupt.push_back(0);check(!decodePolyPool(corrupt,decoded,e));
 auto broken=plan;broken.slots[1].actor=1;again={42};check(!encodePolyPool(broken,again,e)&&again==Bytes{42});broken=plan;broken.slots[2].classId=7;check(!encodePolyPool(broken,again,e));
 std::cout<<checks<<" scoped Poly pool controls PASS (including retained -2, no gameplay)\n";
}
