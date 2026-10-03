#include "pc_p2_catfish_attachments.h"
#include "pc_p2_captor_mouth.h"
#include <cassert>
#include <fstream>
#include <sstream>
#include <iostream>
std::string synthetic(){
 std::ostringstream out;out<<"P2_ATTACHMENTS_1 10 7\n";
 const char* names[]={"kosi","ago","kamu1","kamu2","body","head","regL_momo","regL_sune","regR_momo","regR_sune"};
 const int parents[]={-1,0,1,1,0,0,0,6,0,8};for(int i=0;i<10;++i)out<<names[i]<<' '<<parents[i]<<'\n';
 const std::map<std::string,int> clips={{"attack",85},{"dead",95},{"flick",70},{"move1",25},{"type5",40},{"wait1",30},{"waitact2",16}};
 for(auto& c:clips){out<<c.first<<' '<<c.second<<' '<<c.second<<'\n';for(int f=0;f<c.second;++f)out<<f<<' ';out<<'\n';
  for(int f=0;f<c.second;++f){for(int j=0;j<10;++j)out<<"0 0 0 0 0 0 1 1 1 1 ";out<<'\n';}}
 return out.str();
}
int main(int argc,char** argv){
 std::string error,bytes=synthetic();std::istringstream valid(bytes);assert(p2original::catfish::readAttachments(valid,error));
 for(auto pair:{std::pair<const char*,const char*>{"kamu1 1","kamu1 0"},{"kamu2 1","mouth 1"},{"attack 85 85","attack 86 85"}}){
  auto mutated=bytes;mutated.replace(mutated.find(pair.first),std::string(pair.first).size(),pair.second);std::istringstream bad(mutated);assert(!p2original::catfish::readAttachments(bad,error));}
 std::istringstream truncated(bytes.substr(0,bytes.size()-50));assert(!p2original::catfish::readAttachments(truncated,error));
 if(argc==2){std::ifstream actual(argv[1]);assert(p2original::catfish::readAttachments(actual,error));}
 p2captor::Vec3 slots[2]={{0,0,0},{0,0,0}};p2captor::Prey prey[4]={};
 for(auto& p:prey){p.alive=p.visible=true;}
 prey[0].pos={0,20,0}; // Strict 3D boundary refuses despite zero XZ distance.
 prey[1].pos={19,0,0};prey[2].pos={1,0,0};prey[3].pos={0,0,0};
 bool occupied[2]={};int selected[2]={-1,-1};
 int eaten=p2captor::eatAt(slots,2,20,prey,4,occupied,p2captor::defaultEligible,[&](int n,int i){selected[i]=n;return true;});
 assert(eaten==2&&selected[0]==1&&selected[1]==2); // Manager order, not nearest first.
 occupied[0]=occupied[1]=false;selected[0]=selected[1]=-1;
 eaten=p2captor::eatAt(slots,2,20,prey,4,occupied,p2captor::defaultEligible,[&](int n,int i){if(n==1)return false;selected[i]=n;return true;});
 assert(eaten==2&&selected[0]==2&&selected[1]==3); // Refused receivers do not occupy slots.
 prey[1].stuckToSelf=true;prey[2].stuckToAnyMouth=true;occupied[0]=occupied[1]=false;
 eaten=p2captor::eatAt(slots,2,20,prey,4,occupied,p2captor::defaultEligible,[](int,int){return true;});assert(eaten==1);
 std::cout<<"PASS original Catfish authored joints and strict physical mouth selection\n";
}
