#include "pc_p2_retail_start.h"
#include <cassert>
#include <fstream>
#include <iostream>

static std::string file(const std::string& name){std::ifstream input(name,std::ios::binary);assert(input);return {std::istreambuf_iterator<char>(input),{}};}
static std::string digest(const std::string& data){unsigned char hash[32];pc_netplay_sha::sha256(data.data(),data.size(),hash);return pc_netplay_sha::hex(hash,32);}
int main(int argc,char** argv){
 assert(argc==2);using namespace p2retail;
 for(unsigned floor=1;floor<=2;++floor){
  const auto directory=std::string(argv[1])+"/floor"+std::to_string(floor)+"/";
  SelectedSceneInputs inputs;inputs.selection.cave="tutorial_1";inputs.selection.floor=floor;
  inputs.bytes[0]=file(directory+"p2-retail-floor.txt");inputs.bytes[3]=file(directory+"p2-retail-start.json");
  inputs.bytes[4]=file(directory+"p2-retail-unit-pool.txt");inputs.bytes[5]=file(directory+"p2-retail-start-layout.txt");
  std::string error;assert(parseFloorPlan(inputs.bytes[0],digest(inputs.bytes[0]),inputs.plan,error));
  SourceStart start;assert(parseSourceStart(inputs,start,error));
  assert(start.mapStart[1]==(floor==1?50:75.5));
  assert(start.slotPosition[1]==(floor==1?0:25.5));
  unsigned refusals=0;
  auto refuses=[&](const SelectedSceneInputs& altered){
   SourceStart retained=start;assert(!parseSourceStart(altered,retained,error));assert(!error.empty());
   assert(retained.mapStart==start.mapStart&&retained.slotPosition==start.slotPosition&&retained.unit==start.unit);++refusals;
  };
  auto replace=[&](const std::string& from,const std::string& to,unsigned input=3){
   auto altered=inputs;const auto position=altered.bytes[input].find(from);assert(position!=std::string::npos);
   altered.bytes[input].replace(position,from.size(),to);refuses(altered);
  };
  replace("\"native_ready\":false","\"native_ready\":true");
  replace("\"ground_y_offset\":8.5","\"ground_y_offset\":9.0");
  replace("\"x_offset\":-4.526","\"x_offset\":40.0");
  replace("\"schema\":1","\"schema\":1,\"schema\":1");
  replace("\"type\":7","\"type\":4");
  replace("\"kind\":1","\"kind\":2");
  replace("e2fdaccdb0e411270d85370779a068dd81eb4136c2e6c1273585a0ee8711cddb",std::string(64,'0'));
  replace("8d7b76ef45810f1093dfb8ee19e89d8b6dc123ae3885609afa9f01476033aa4f",std::string(64,'0'));
  replace("actual MapMgr getMinY at map_start, before horizontal offsets","reproject after horizontal offsets");
  replace("7 \t# type [Start]","4 \t# type [Start]",5);
  auto altered=inputs;altered.bytes[3]+="extra";refuses(altered);
  altered=inputs;altered.plan.pod.x+=1;refuses(altered);
  altered=inputs;altered.plan.layoutSha256=std::string(64,'0');refuses(altered);
  altered=inputs;altered.bytes[4]+="extra";refuses(altered);
  altered=inputs;altered.bytes[5]+="extra";refuses(altered);
  if(floor==1)replace("\"offset\":2","\"offset\":3");
  std::cout<<"floor "<<floor<<": actual source parsing PASS, "<<refusals<<" negative controls PASS; no native ground or World grant\n";
 }
}
