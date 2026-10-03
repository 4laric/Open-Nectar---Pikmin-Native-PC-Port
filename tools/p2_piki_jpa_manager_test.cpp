#include "pc_p2_piki_jpa_manager.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
#include <type_traits>
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
 auto next=identity;next.session="different-selected-session";assert(bank.load(next,inputs,error));assert(manager.bank().selected().session==identity.session&&handles[0]->bank().selected().session==identity.session);
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
 EmitterHandle retiredAfterManager;
 {Manager temporary(bank);EmitterHandle actual;assert(temporary.create(haloId(1),actual,error));retiredAfterManager=actual;assert(temporary.erase(actual,error));}
 assert(retiredAfterManager->resourceBytes().size()==304&&retiredAfterManager->bank().selected().session==next.session);
 assert(stale->resourceBytes().size()==304&&stale->bank().selected().session==identity.session);
 std::cout<<"Owned JPA selected-bank source pool/admission RNG/lifetime controls PASS (engineering only)\n";
}
