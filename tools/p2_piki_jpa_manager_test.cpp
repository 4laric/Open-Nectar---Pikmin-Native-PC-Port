#include "pc_p2_piki_jpa_manager.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
#include <type_traits>
#include <new>
#include <cstdlib>
static bool failNextAllocation=false;
static long failAllocationCountdown=-1;
void* operator new(std::size_t n){if(failAllocationCountdown==0){failAllocationCountdown=-1;throw std::bad_alloc();}if(failAllocationCountdown>0)--failAllocationCountdown;if(failNextAllocation){failNextAllocation=false;throw std::bad_alloc();}if(void* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete[](void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}
void operator delete[](void* p,std::size_t)noexcept{std::free(p);}
using namespace p2original::pikiJPA;
static_assert(!std::is_default_constructible<Emitter>::value,"descriptors are manager-only");
static_assert(!std::is_copy_constructible<Emitter>::value,"descriptors cannot be forged by copying");
int main(int argc,char** argv){
 assert(argc==2);std::size_t count=0;const auto* roles=resourceRoles(count);std::vector<SelectedBytes> inputs;
 for(std::size_t i=0;i<count;++i){const auto& r=roles[i];std::ifstream f(std::string(argv[1])+"/"+r.role,std::ios::binary);assert(f);inputs.push_back({r.role,sourceArchive,sourceArchiveSHA,r.offset,r.bytes,{std::istreambuf_iterator<char>(f),{}}});}
 Bank bank;std::string error;SelectedIdentity identity{std::string(64,'c'),std::string(64,'a'),"engineering-actual-selected-bank"};assert(bank.load(identity,inputs,error));
 Bank absent;Manager empty(absent);EmitterHandle out;assert(!empty.create(haloId(0),out,error)&&!out&&empty.frontier()==0&&empty.admissions()==0);
 Manager manager(bank),other(bank);assert(manager.frontier()==0&&manager.freeCount()==300);
 // Six actual successful source admissions, not caller-supplied default seeds.
 std::array<EmitterHandle,300> handles{};std::uint32_t oracle=0;
 for(unsigned i=0;i<6;++i){oracle=oracle*0x19660du+0x3c6ef35fu;assert(manager.create(haloId(i),handles[i],error));assert(handles[i]->seed()==oracle&&handles[i]->poolIndex()==299-i&&handles[i]->admission()==i+1&&handles[i]->group()==2&&manager.owns(handles[i]));assert(handles[i]->resourceBytes().size()==304);}
 const auto before=manager.frontier();const auto admissions=manager.admissions();
 assert(!manager.create(0x177,out,error)&&!out);assert(!manager.create(0x174,out,error)&&!out);assert(!manager.create(0,out,error)&&!out);assert(!manager.create(0x1e,out,error)&&!out);
 assert(manager.frontier()==before&&manager.admissions()==admissions&&manager.live()==6);
 auto retained=handles[0];assert(!manager.create(haloId(1),retained,error)&&retained==handles[0]&&manager.frontier()==before);
 assert(!other.owns(retained)&&!other.erase(retained,error)&&retained==handles[0]&&other.frontier()==0);
 // The selected owner can change its own Bank; owned manager bytes/stamp stay retained.
 auto next=identity;next.packetSHA=std::string(64,'b');next.session="different-selected-session";assert(bank.load(next,inputs,error));assert(manager.bank().selected().session==identity.session&&handles[0]->bank().selected().session==identity.session);
 assert(!manager.rebindSelectedBank(bank,error)&&manager.frontier()==before&&manager.admissions()==admissions&&manager.bank().selected().session==identity.session);
 for(unsigned i=6;i<300;++i){oracle=oracle*0x19660du+0x3c6ef35fu;assert(manager.create(haloId(i%6),handles[i],error)&&handles[i]->poolIndex()==299-i&&handles[i]->seed()==oracle);}
 assert(manager.freeCount()==0&&manager.frontier()==oracle);assert(!manager.create(haloId(0),out,error)&&!out&&manager.frontier()==oracle&&manager.admissions()==300);
 auto stale=handles[49];const auto oldGeneration=stale->admission();assert(manager.erase(handles[49],error)&&!handles[49]&&!manager.owns(stale));
 oracle=oracle*0x19660du+0x3c6ef35fu;assert(manager.create(haloId(0),out,error)&&out->poolIndex()==250&&out->admission()>oldGeneration&&out->seed()==oracle);
 assert(!manager.erase(stale,error)&&stale->admission()==oldGeneration&&manager.owns(out));
 manager.killAll();assert(manager.live()==0&&manager.frontier()==oracle&&manager.admissions()==301&&!manager.owns(out));
 // Tail-first group deletion returns retail initial head299 for this group.
 EmitterHandle resumed;oracle=oracle*0x19660du+0x3c6ef35fu;assert(manager.create(haloId(0),resumed,error)&&resumed->poolIndex()==299&&resumed->seed()==oracle);
 manager.reset();assert(manager.live()==0&&manager.frontier()==oracle&&!manager.owns(resumed));
 resumed.reset();assert(manager.create(haloId(1),resumed,error));assert(resumed->seed()==oracle*0x19660du+0x3c6ef35fu);assert(manager.erase(resumed,error));
 const auto savedFrontier=manager.frontier();const auto savedCount=manager.admissions();
 auto different=next;different.campaignSHA=std::string(64,'d');Bank foreignCampaign;assert(foreignCampaign.load(different,inputs,error));
 Bank partial;assert(partial.load(next,{inputs[1]},error));
 assert(!manager.rebindSelectedBank(foreignCampaign,error)&&!manager.rebindSelectedBank(partial,error));
 assert(manager.bank().selected().session==identity.session&&manager.frontier()==savedFrontier&&manager.admissions()==savedCount);
 failNextAllocation=true;assert(!manager.rebindSelectedBank(bank,error)&&!failNextAllocation);
 assert(manager.bank().selected().session==identity.session&&manager.frontier()==savedFrontier&&manager.admissions()==savedCount);
 bool reachedSuccessfulCopy=false;
 // Enumerate each allocation in validation AND immutable replacement copy.
 for(long allocation=0;allocation<128;++allocation){
  Manager candidate(stale->bank());EmitterHandle admitted;assert(candidate.create(haloId(0),admitted,error));assert(candidate.erase(admitted,error));
  const auto frontier=candidate.frontier();const auto generation=candidate.admissions();const auto* preceding=&candidate.bank();
  failAllocationCountdown=allocation;const bool rebound=candidate.rebindSelectedBank(bank,error);failAllocationCountdown=-1;
  assert(candidate.frontier()==frontier&&candidate.admissions()==generation&&candidate.live()==0);
  if(rebound){reachedSuccessfulCopy=true;assert(candidate.bank().selected().session==next.session);break;}
  assert(&candidate.bank()==preceding&&candidate.bank().selected().session==identity.session);
 }
 assert(reachedSuccessfulCopy);
 assert(manager.rebindSelectedBank(bank,error));assert(manager.bank().selected().session==next.session&&manager.bank().selected().packetSHA==next.packetSHA);
 assert(manager.frontier()==savedFrontier&&manager.admissions()==savedCount&&stale->bank().selected().session==identity.session);
 // Preserve nontrivial free-list history: deletion299 then298 leaves head298.
 Manager history(stale->bank());EmitterHandle h299,h298;assert(history.create(haloId(0),h299,error)&&history.create(haloId(1),h298,error));
 const auto old299=h299;assert(history.erase(h299,error)&&history.erase(h298,error));const auto historyFrontier=history.frontier();
 assert(history.rebindSelectedBank(bank,error));EmitterHandle afterRebind;
 assert(history.create(haloId(2),afterRebind,error)&&afterRebind->poolIndex()==298&&afterRebind->admission()==3&&afterRebind->seed()==historyFrontier*0x19660du+0x3c6ef35fu);
 assert(old299->bank().selected().session==identity.session&&afterRebind->bank().selected().session==next.session);assert(history.erase(afterRebind,error));
 EmitterHandle retiredAfterManager;
 {Manager temporary(bank);EmitterHandle actual;assert(temporary.create(haloId(1),actual,error));retiredAfterManager=actual;assert(temporary.erase(actual,error));}
 assert(retiredAfterManager->resourceBytes().size()==304&&retiredAfterManager->bank().selected().session==next.session);
 assert(stale->resourceBytes().size()==304&&stale->bank().selected().session==identity.session);
 std::cout<<"Owned JPA selected-bank source pool/admission RNG/lifetime controls PASS (engineering only)\n";
}
