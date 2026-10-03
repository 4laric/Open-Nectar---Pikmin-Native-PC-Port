#include "pc_p2_piki_halo.h"
#include "pc_p2_piki_jpa_owner_guard.h"
#ifdef _WIN32
#include <windows.h>
#endif
#include <cassert>
#include <cmath>
#include <fstream>
#include <iostream>
#include <iterator>
#include <new>
#include <cstdlib>
// Pure-core allocation control only; never creates Scene/Stage authority.
static bool failNextAllocation=false;
void* operator new(std::size_t n){if(failNextAllocation){failNextAllocation=false;throw std::bad_alloc();}if(void* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete[](void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}
void operator delete[](void* p,std::size_t)noexcept{std::free(p);}
using namespace p2original::pikiJPA;
int main(int argc,char** argv){
 #ifdef _WIN32
 SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
#endif
 if(argc==2&&std::string(argv[1])=="--guard-retired"){detail::requireRetiredOwners(0,0,"selected revoked");std::cout<<"Retired owner destructor guard PASS\n";return 0;}
 if(argc==2&&std::string(argv[1])=="--guard-live"){detail::requireRetiredOwners(1,1,"selected revoked; exact owner retained");return 99;}
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
 assert(halo.prepare(bank,seeds,2,e));
 // Actual immortal shared emitter with maxFrame0 continues dynamics with no
 // contexts: rate draws occur, while StopEmitting prevents ordinary births.
 assert(halo.sourceFrame([](Position,unsigned){assert(false);return false;},e));
 assert(halo.particles().empty());
 for(unsigned species=0;species<6;++species)assert(halo.seed(species)==seeds[species]*0x19660du+0x3c6ef35fu);
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
 // Selected halo-only budget: source manager has2000 particles. This does
 // not admit other effects sharing the retail pool or claim whole-JPA support.
 HaloEffects limit;assert(limit.prepare(bank,seeds,2000,e));
 assert(!limit.prepare(bank,seeds,2001,e));assert(limit.seed(0)==seeds[0]);
 std::array<int,2001> bodies{};
 for(unsigned i=0;i<2000;++i)assert(limit.sharedIdleHalo({&bodies[i],i+1},i%6,haloId(i%6),{float(i),0,0},e));
 assert(!limit.sharedIdleHalo({&bodies[2000],2001},0,haloId(0),{},e));assert(limit.owners()==2000);
 assert(limit.sourceFrame([](Position,unsigned){return false;},e));assert(limit.particles().size()==2000);
 // Force actual vector growth with one lifetime1 particle already retained.
 HaloEffects fault;assert(fault.prepare(bank,seeds,2,e));int first=0,second=0;
 assert(fault.sharedIdleHalo({&first,1},0,haloId(0),{11,12,13},e));
 HaloEffects::Clipped visible=[](Position,unsigned){return false;};
 assert(fault.sourceFrame(visible,e));assert(fault.particles().size()==1);
 const auto particle=fault.particles()[0];std::array<std::uint32_t,6> prior{};
 for(unsigned i=0;i<6;++i)prior[i]=fault.seed(i);
 assert(fault.sharedIdleHalo({&second,2},1,haloId(1),{21,22,23},e));
 failNextAllocation=true;assert(!fault.sourceFrame(visible,e));assert(!failNextAllocation);
 assert(fault.owners()==2&&fault.particles().size()==1&&fault.particles()[0].position.x==particle.position.x&&fault.particles()[0].angle==particle.angle&&fault.particles()[0].scale==particle.scale);
 for(unsigned i=0;i<6;++i)assert(fault.seed(i)==prior[i]);
 assert(fault.sourceFrame(visible,e));assert(fault.particles().size()==2);
 std::cout<<"Piki JPA selected-bank + source-halo ownership policy PASS (engineering only)\n";
}
