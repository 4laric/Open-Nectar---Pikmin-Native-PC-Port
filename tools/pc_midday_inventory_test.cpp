#include "pc_midday_inventory.h"
#include <cstdlib>
#include <iostream>
using namespace pc_midday;
int checks=0;void check(bool ok){++checks;if(!ok){std::cerr<<"inventory control failed "<<checks<<"\n";std::exit(1);}}
int main(){
 InventoryScope scope;scope.generation=7;scope.frame=124;scope.binding.seed[0]=1;scope.binding.session[0]=2;scope.binding.content[0]=3;scope.binding.schema[0]=4;
 InventoryFactory factory;std::vector<InventoryChannel> channels;int actors[2]{};
 for(uint32_t f=uint32_t(Family::Captain);f<=uint32_t(Family::Projectile);++f){std::string key="source-"+std::to_string(f);factory[key]={Family(f)};channels.push_back({scope,key,true,true,true,scope.frame,{}});}
 channels[0].roots={{&actors[0],Family::Captain,SlotLife::Active}};channels[1].roots={{&actors[1],Family::Pikmin,SlotLife::Retained}};
 CheckpointInventory out;std::string e;check(buildCheckpointInventory(scope,factory,channels,out,e));std::set<uint64_t> ids;check(out.ids(scope,ids,e)&&ids==std::set<uint64_t>{1,2});InventoryRoot root;check(out.root(scope,2,root,e)&&root.address==&actors[1]&&root.life==SlotLife::Retained);check(out.identities(scope,e)->lookup(&actors[1])->id==2);
 auto wrong=scope;++wrong.generation;InventoryRoot unchanged;unchanged.address=&actors[0];check(!out.root(wrong,2,unchanged,e)&&unchanged.address==&actors[0]);ids={77};check(!out.ids(wrong,ids,e)&&ids==std::set<uint64_t>{77});check(!out.identities(wrong,e));
 for(unsigned i=0;i<5;++i){wrong=scope;Digest* d[]={&wrong.binding.seed,&wrong.binding.session,&wrong.binding.content,&wrong.binding.schema};if(i<4)(*d[i])[4]=1;else ++wrong.frame;check(!out.identities(wrong,e));}
 auto refuses=[&](std::vector<InventoryChannel> bad){check(!buildCheckpointInventory(scope,factory,bad,out,e));InventoryRoot retained;check(out.root(scope,2,retained,e)&&retained.address==&actors[1]);};
 auto bad=channels;bad.pop_back();refuses(bad);bad=channels;bad[1].source=bad[0].source;refuses(bad);bad=channels;++bad[0].scope.generation;refuses(bad);bad=channels;++bad[0].frameAfter;refuses(bad);
 bad=channels;bad[0].initialized=false;refuses(bad);bad=channels;bad[0].complete=false;refuses(bad);bad=channels;bad[0].stopped=false;refuses(bad);
 bad=channels;bad[1].roots[0].address=&actors[0];refuses(bad);bad=channels;bad[0].roots[0].address=nullptr;refuses(bad);bad=channels;bad[0].roots[0].family=Family::Enemy;refuses(bad);bad=channels;bad[0].roots[0].life=SlotLife::Free;refuses(bad);
 auto missing=factory;missing.erase(channels.back().source);check(!buildCheckpointInventory(scope,missing,std::vector<InventoryChannel>(channels.begin(),channels.end()-1),out,e));
 // IDs may equal ordinals in another generation, but consumers cannot mix them.
 auto next=scope;++next.generation;bad=channels;for(auto& c:bad)c.scope=next;CheckpointInventory other;check(buildCheckpointInventory(next,factory,bad,other,e));check(other.root(next,2,root,e));check(!other.root(scope,2,root,e));
 MonoPoolView view;view.capacity=3;view.count=2;view.statuses={0,-2,-1};int freeBacking=0;view.objects={&actors[0],&actors[1],&freeBacking};std::vector<InventoryRoot> roots;
 check(monoInventoryRoots(view,Family::Pikmin,roots,e)&&roots.size()==2&&roots[1].life==SlotLife::Retained);view.count=1;check(!monoInventoryRoots(view,Family::Pikmin,roots,e)&&roots.size()==2);view.count=2;view.statuses[2]=9;check(!monoInventoryRoots(view,Family::Pikmin,roots,e));view.statuses[2]=-1;view.objects[2]=view.objects[0];check(!monoInventoryRoots(view,Family::Pikmin,roots,e));
 std::cout<<checks<<" checkpoint scoped inventory controls PASS\n";
}
