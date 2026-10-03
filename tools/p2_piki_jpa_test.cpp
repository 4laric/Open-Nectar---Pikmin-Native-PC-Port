#include "pc_p2_piki_halo.h"
#include <cassert>
#include <cmath>
#include <fstream>
#include <iostream>
#include <iterator>
using namespace p2original::pikiJPA;
int main(int argc,char** argv){
 assert(argc==2);std::size_t count;auto roles=resourceRoles(count);assert(count==16);
 std::vector<SelectedBytes> inputs;
 for(std::size_t i=0;i<count;++i){const auto& r=roles[i];std::ifstream in(std::string(argv[1])+"/"+r.role,std::ios::binary);assert(in);
  inputs.push_back({r.role,sourceArchive,sourceArchiveSHA,r.offset,r.bytes,{std::istreambuf_iterator<char>(in),{}}});
 }
 SelectedIdentity selected{std::string(64,'c'),std::string(64,'a'),"engineering-selected-session"};Bank bank;std::string e;
 assert(bank.load(selected,inputs,e));assert(bank.bytes("piki-016b.jpa"));
 auto changed=inputs;changed[0].bytes[9]^=1;assert(!bank.load(selected,changed,e));assert(bank.selected().campaignSHA==selected.campaignSHA);assert(*bank.bytes(inputs[0].role)==inputs[0].bytes);
 changed=inputs;changed[0].memberOffset++;assert(!bank.load(selected,changed,e));
 changed=inputs;changed[0].archiveSHA=std::string(64,'b');assert(!bank.load(selected,changed,e));
 changed=inputs;changed.push_back(inputs[0]);assert(!bank.load(selected,changed,e));
 changed={inputs[4]};assert(!bank.load(selected,changed,e)); // genuine red effect, absent texture
 auto wrong=selected;wrong.campaignSHA="short";assert(!bank.load(wrong,inputs,e));
 assert(haloId(1)==0x16b&&blurId(1)==0x174&&haloId(9)==0&&blurId(5)==0);
 std::array<std::uint32_t,6> seeds{{10,20,30,40,50,60}};HaloEffects halo;
 assert(halo.prepare(bank,seeds,2,e));int a=0,b=0,c=0;ContextId A{&a,1},B{&b,2};
 assert(halo.sharedIdleHalo(A,1,0x16b,{1,2,3},e));
 assert(!halo.sharedIdleHalo({&a,7},1,0x16b,{1,2,3},e));assert(halo.owners()==1);
 assert(!halo.sharedIdleHalo(A,1,0x16a,{1,2,3},e));
 assert(halo.sharedIdleHalo(B,1,0x16b,{4,5,6},e));
 assert(!halo.sharedIdleHalo({&c,3},1,0x16b,{1,2,3},e));assert(halo.owners()==2);
 assert(!halo.prepare(bank,seeds,2,e));assert(!halo.removeIdleHalo({&a,7},e));assert(halo.owners()==2);
 assert(!halo.sourceFrame({},e));assert(halo.seed(1)==20);
 assert(halo.sourceFrame([](Position,unsigned){return false;},e));assert(halo.particles().size()==2);
 // Retail CNode append order A then B; JPA pool push_front draws B then A.
 assert(halo.particles()[0].position.x==4&&halo.particles()[1].position.x==1);
 assert(halo.sharedIdleHalo(A,1,0x16b,{1,2,3},e));
 assert(halo.sourceFrame([](Position,unsigned){return false;},e));
 assert(halo.particles()[0].position.x==1&&halo.particles()[1].position.x==4);
 std::uint32_t oracle=20;for(unsigned i=0;i<46;++i)oracle=oracle*0x19660du+0x3c6ef35fu;
 assert(halo.seed(1)==oracle); // one shared rate draw + eleven per point birth
 assert(halo.removeIdleHalo(A,e));assert(halo.particles().size()==2);assert(halo.owners()==1);
 assert(halo.sourceFrame([](Position,unsigned){return false;},e));assert(halo.particles().size()==1&&halo.particles()[0].position.x==4);
 auto before=halo.seed(1);assert(halo.sourceFrame([](Position,unsigned pid){assert(pid==0x16b);return true;},e));assert(halo.particles().empty());assert(halo.seed(1)==before*0x19660du+0x3c6ef35fu);
 assert(!halo.follow(B,{NAN,0,0},e));assert(halo.removeIdleHalo(B,e));assert(halo.sourceFrame([](Position,unsigned){return false;},e));assert(halo.prepare(bank,seeds,2,e));
 std::cout<<"Piki JPA selected-bank + source-halo ownership policy PASS (engineering only)\n";
}
