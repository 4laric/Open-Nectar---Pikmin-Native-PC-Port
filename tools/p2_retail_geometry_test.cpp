#include "pc_p2_retail_geometry.h"
#include <cassert>
#include <fstream>
#include <iostream>
static std::string file(const std::string& path){std::ifstream input(path,std::ios::binary);assert(input);return {std::istreambuf_iterator<char>(input),{}};}
static std::string digest(const std::string& bytes){unsigned char hash[32];pc_netplay_sha::sha256(bytes.data(),bytes.size(),hash);return pc_netplay_sha::hex(hash,32);}
static unsigned u32(const std::string& bytes,std::size_t offset){unsigned out=0;for(unsigned i=0;i<4;++i)out=(out<<8)|static_cast<unsigned char>(bytes.at(offset+i));return out;}
int main(int argc,char** argv){
 assert(argc==5);using namespace p2retail;
 for(unsigned floor=1;floor<=2;++floor){SelectedSceneInputs inputs;inputs.selection.cave="tutorial_1";inputs.selection.floor=floor;
  inputs.bytes[1]=file(argv[floor*2-1]);inputs.bytes[2]=file(argv[floor*2]);
  inputs.plan.geometrySha256=digest(inputs.bytes[1]);inputs.plan.routesSha256=digest(inputs.bytes[2]);
  GeometryFacts facts;std::string error;assert(parseRetailGeometry(inputs,facts,error));
  assert(facts.vertices>0&&facts.triangles>0&&facts.routePoints>0);unsigned failures=0;
  auto refuses=[&](const SelectedSceneInputs& altered){GeometryFacts retained=facts;
   assert(!parseRetailGeometry(altered,retained,error));assert(!error.empty());
   assert(retained.vertices==facts.vertices&&retained.triangles==facts.triangles&&retained.routePoints==facts.routePoints);++failures;};
  auto altered=inputs;altered.bytes[1].resize(16);refuses(altered);
  altered=inputs;altered.bytes[2]+="extra";refuses(altered);
  altered=inputs;altered.plan.geometrySha256=std::string(64,'0');refuses(altered);
  std::size_t cursor=0,vertex=0,collision=0,grid=0;
  while(cursor<inputs.bytes[1].size()){const auto tag=u32(inputs.bytes[1],cursor);if(tag==0x10)vertex=cursor;if(tag==0x100)collision=cursor;if(tag==0x110)grid=cursor;
   cursor+=8+u32(inputs.bytes[1],cursor+4);if(tag==65535)break;}
  assert(vertex&&collision&&grid);
  altered=inputs;altered.bytes[1][vertex+32]=char(0x7f);altered.bytes[1][vertex+33]=char(0xc0);altered.bytes[1][vertex+34]=0;altered.bytes[1][vertex+35]=0;refuses(altered);
  altered=inputs;for(unsigned i=0;i<4;++i)altered.bytes[1][collision+68+i]=char(0xff);refuses(altered);
  altered=inputs;altered.bytes[1][grid+79]=1;refuses(altered);
  altered=inputs;altered.bytes[1].replace(cursor,5,"other");refuses(altered);
  std::cout<<"floor "<<floor<<": native MOD/INI semantic admission PASS, vertices="<<facts.vertices<<", triangles="<<facts.triangles<<", routes="<<facts.routePoints<<", negative controls="<<failures<<" PASS; no native stage or World grant\n";
 }
}
