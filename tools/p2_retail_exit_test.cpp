#include "pc_p2_retail_exit.h"
#include <cassert>
#include <fstream>
#include <iostream>
namespace p2retail {
#include "pc_p2_retail_cave_catalog.inc"
}
int main(int argc,char** argv){
 using namespace p2retail;
 const auto* cave=descriptor("tutorial_1");assert(cave);
 for(unsigned number=1;number<=2;++number){
  std::string bytes;
  if(argc==3){std::ifstream file(argv[number],std::ios::binary);assert(file);bytes.assign(std::istreambuf_iterator<char>(file),{});}
  else {
   // Pure contract fixture only. Actual source plans may be supplied as args;
   // this generated policy input never allocates a scene or grants gameplay.
   const auto* floor=definition(*cave,number);std::ostringstream text;
   unsigned count=0;for(const auto& r:floor->rows)count+=r.minimum();
   text<<"P2_RETAIL_FLOOR_1 tutorial_1 "<<number<<' '<<cave->sourceSha256<<' '<<cave->catalogSha256
    <<' '<<std::string(64,'a')<<' '<<std::string(64,'b')<<" actors "<<count<<'\n';
   for(unsigned row=0;row<floor->rows.size();++row)for(unsigned i=0;i<floor->rows[row].minimum();++i)
    text<<"actor "<<row<<' '<<i<<' '<<instanceKey(*cave,number,floor->rows[row],i)<<" 0 0 0 0 0 0\n";
   text<<"pod 0 0 0 0 0 0\nexit 2 1 0 0 1190 210\ntransition "<<(number==cave->maxFloor?"geyser":"hole")<<'\n';bytes=text.str();
  }
  unsigned char hash[32];pc_netplay_sha::sha256(bytes.data(),bytes.size(),hash);
  FloorPlan plan;std::string error;assert(parseFloorPlan(bytes,pc_netplay_sha::hex(hash,32),plan,error));
  Snapshot floor;floor.cave=plan.cave;floor.floor=number;floor.maxFloor=cave->maxFloor;floor.source=cave->source;
  floor.sourceSha256=plan.sourceSha256;floor.catalogSha256=plan.catalogSha256;
  floor.story=floor.inCave=true;floor.scene={"policy-seed","policy-visit",plan.layoutSha256,1};
  ExitSpec spec;assert(exitSpec(plan,floor,std::string(64,'c'),std::string(64,'d'),1,spec,error));
  assert(spec.anchor.x==plan.exit.x&&spec.anchor.y==plan.exit.y&&spec.anchor.z==plan.exit.z
   &&spec.anchor.unit==plan.exit.unit&&spec.anchor.slot==plan.exit.slot&&spec.anchor.yawDegrees==plan.exit.yawDegrees);
  const ExitSpec valid=spec;
  auto refuses=[&](const FloorPlan& p,const Snapshot& s,const std::string& campaign,const std::string& session,std::uint64_t revision){
   assert(!exitSpec(p,s,campaign,session,revision,spec,error));assert(!error.empty());
   assert(spec.planBytes==valid.planBytes&&spec.anchor.x==valid.anchor.x&&spec.revision==valid.revision);
  };
  for(unsigned field=0;field<7;++field){auto bad=plan;
   switch(field){case 0:++bad.exit.unit;break;case 1:++bad.exit.slot;break;case 2:++bad.exit.x;break;
    case 3:++bad.exit.y;break;case 4:++bad.exit.z;break;case 5:++bad.exit.yawDegrees;break;case 6:bad.transition="forest_1";break;}
   refuses(bad,floor,valid.campaign,valid.session,1);
  }
  auto bad=plan;bad.authenticatedBytes+=' ';refuses(bad,floor,valid.campaign,valid.session,1);
  auto wrong=floor;wrong.scene.serial=0;refuses(plan,wrong,valid.campaign,valid.session,1);
  wrong=floor;wrong.inCave=false;refuses(plan,wrong,valid.campaign,valid.session,1);
  wrong=floor;wrong.cave="forest_1";refuses(plan,wrong,valid.campaign,valid.session,1);
  wrong=floor;wrong.sourceSha256[0]^=1;refuses(plan,wrong,valid.campaign,valid.session,1);
  refuses(plan,floor,"",valid.session,1);refuses(plan,floor,valid.campaign,"",1);refuses(plan,floor,valid.campaign,valid.session,0);
 }
 std::cout<<"retail exit exact-plan policy controls PASS; no native runtime authority\n";
}
