#include "pc_p2_retail_cave_context.h"
#include <cassert>
#include <iostream>
using namespace p2retail;
// Policy test double only; never a gameplay provider or native birth evidence.
struct Provider final:FloorProvider {
 bool ready=true,missing=false,cleanup=true;unsigned installs=0,releases=0;
 int actors[100]{};
 bool preflight(const CaveDescriptor&,const FloorDefinition&,unsigned,const SceneIdentity&,std::string& e)override{
  if(!ready)e="unsupported_literal";
  return ready;
 }
 bool install(const CaveDescriptor& c,const FloorDefinition& f,unsigned floor,const SceneIdentity&,
              std::vector<LiveBinding>& out,std::string&)override{
  ++installs;unsigned a=0;
  for(unsigned r=0;r<f.rows.size();++r)for(unsigned o=0;o<f.rows[r].minimum();++o){
   assert(a<100);out.push_back({&actors[a++],{r,o,1,instanceKey(c,floor,f.rows[r],o)}});
  }
  if(missing&&!out.empty())out.pop_back();
  return true;
 }
 bool release(std::string& e)override{++releases;if(!cleanup)e="test_cleanup_failure";return cleanup;}
};
int main(){
 unsigned floors=0;for(const auto& c:retailCatalog())floors+=c.maxFloor;
 assert(retailCatalog().size()==14&&floors==105);
 const auto* cave=descriptor("tutorial_1");assert(cave&&cave->maxFloor==2);
 assert(!descriptor("engineeredforest_1")&&!definition(*cave,0)&&!definition(*cave,3));
 const auto* f2=definition(*cave,2);assert(f2&&f2->rows.size()==6);
 assert(f2->rows[1].catalogId=="YellowKochappy"&&f2->rows[1].sourceId==45);
 assert(f2->rows.back().catalogId=="map01"&&!f2->rows.back().boss&&f2->rows.back().heldTreasure.empty());
 SceneIdentity scene{"seed","visit",std::string(64,'a'),3};Snapshot s;std::string error;
 FloorSession session;Provider provider;assert(!session.snapshot(scene,s));
 provider.ready=false;assert(!session.activate("tutorial_1",1,scene,true,provider,error)&&provider.installs==0);
 provider.ready=true;provider.missing=true;provider.cleanup=false;
 assert(!session.activate("tutorial_1",1,scene,true,provider,error));assert(!session.snapshot(scene,s));
 assert(!session.activate("tutorial_1",1,scene,true,provider,error));
 provider.cleanup=true;assert(session.unload(error));provider.missing=false;
 assert(session.activate("tutorial_1",2,scene,true,provider,error));assert(session.snapshot(scene,s)&&s.lastFloor());
 auto stale=scene;stale.serial++;assert(!session.snapshot(stale,s));
 bool boss=true;assert(!session.heldDrop(scene,&provider.actors[0],"tutorial_1:floor2:enemy:0:0","map01",1,s,boss));
 assert(session.unload(error)&&!session.snapshot(scene,s));
 std::cout<<"PASS retail descriptor/source45/loose-map01/preflight/partial-cleanup/stale-scene policy\n";
}
