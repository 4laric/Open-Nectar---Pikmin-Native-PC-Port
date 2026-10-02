#include "pc_midday_item_factory.h"
#include <cstdlib>
#include <iostream>
using namespace pc_midday;
int checks=0;void check(bool ok){++checks;if(!ok){std::cerr<<"itemfactory55 control "<<checks<<" failed\n";std::exit(1);}}
int main(){
 int objects[3]{},prototype=0;PolyPoolView fresh;fresh.capacity=3;fresh.stride=128;fresh.statuses={-1,-1,-1};fresh.objects={&objects[0],&objects[1],&objects[2]};fresh.templates={{7,64,&prototype}};
 PolyPoolPlan plan;plan.capacity=3;plan.count=2;plan.classes={7,11};plan.slots={{SlotLife::Active,1,7},{SlotLife::Retained,2,11},{}};
 std::vector<ItemPlacement>tasks;std::string e;check(preflightItemPlacement(plan,fresh,{7,11},{7,11},tasks,e));check(tasks.size()==2&&tasks[1].slot==1&&tasks[1].actor==2&&tasks[1].classId==11);
 auto refuse=[&](PolyPoolPlan p,PolyPoolView v,std::set<int>allocated={7,11},std::set<int>supported={7,11}){check(!preflightItemPlacement(p,v,allocated,supported,tasks,e));check(tasks.size()==2&&tasks[1].actor==2);};
 refuse(plan,fresh,{7,11},{7});refuse(plan,fresh,{7},{7,11});auto v=fresh;v.count=1;refuse(plan,v);v=fresh;v.capacity=4;refuse(plan,v);v=fresh;v.stride=0;refuse(plan,v);v=fresh;v.statuses[2]=0;refuse(plan,v);v=fresh;v.statuses[0]=-2;refuse(plan,v);v=fresh;v.objects[2]=v.objects[1];refuse(plan,v);v=fresh;v.objects[2]=nullptr;refuse(plan,v);v=fresh;v.objects[2]=&prototype;refuse(plan,v);v=fresh;v.objects.pop_back();refuse(plan,v);
 auto p=plan;p.slots[1].actor=1;refuse(p,fresh);p=plan;p.slots[2].actor=3;refuse(p,fresh);p=plan;p.slots[1].classId=17;refuse(p,fresh);p=plan;p.slots[1].life=SlotLife(99);refuse(p,fresh);p=plan;p.count=1;refuse(p,fresh);p=plan;p.slots.pop_back();refuse(p,fresh);
 p=plan;p.count=0;p.slots.assign(3,{});check(preflightItemPlacement(p,fresh,{7,11},{},tasks,e)&&tasks.empty());
 // All twelve source ObjType IDs from compiled ItemMgr::initialise. This
 // exercises all-slot transaction ordering, not actual native constructors.
 std::set<int> matrix={1,2,3,4,5,6,13,14,17,18,27,28};int backing[12]{};
 PolyPoolView full;full.capacity=12;full.stride=4096;full.statuses.assign(12,-1);
 PolyPoolPlan all;all.capacity=12;all.count=12;all.classes=matrix;
 unsigned index=0;for(int id:matrix){full.objects.push_back(&backing[index]);all.slots.push_back({index%2?SlotLife::Retained:SlotLife::Active,index+1,id});++index;}
 check(preflightItemPlacement(all,full,matrix,matrix,tasks,e)&&tasks.size()==12);
 for(int excluded:matrix){auto partial=matrix;partial.erase(excluded);const auto before=tasks;check(!preflightItemPlacement(all,full,matrix,partial,tasks,e));check(tasks.size()==12&&tasks.back().actor==before.back().actor);}
 std::cout<<checks<<" item factory all-before-placement preflight controls PASS (no native construction)\n";
}
