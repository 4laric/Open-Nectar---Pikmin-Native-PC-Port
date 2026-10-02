#include "pc_midday_demo.h"
#include "Demo.h"
#include <iostream>
#include <stdexcept>
namespace pc_midday {struct DemoStageTag{};}
// Capture-only executable: a call into construction suppression is an error.
// This does not exercise IsolatedDemo::prepare or claim its physical SDL fence.
bool pc_sim_rng_constructor_suppression(bool,std::string&){throw std::logic_error("unexpected construction path");}
using namespace pc_midday;
int n=0;void check(bool v){++n;if(!v)throw std::runtime_error(std::to_string(n));}
int main(){try{
 DemoFlags source(DemoStageTag{});check(!source.mStoredFlags&&!source.mFlagDataList&&!source.mTargetCreature&&source.mCurrentDemoIndex==-1);
 std::array<uint8_t,32>stored{};std::array<DemoFlag*,256>list{};std::array<DemoFlag,62>descriptors{};
 for(unsigned i=0;i<62;++i){auto&d=descriptors[i];d.mName="pinned content";d.mIndex=i;d.mMovieIndex=i==15?-1:i;d._08=i;d._0A=i==16;list[i]=&d;}
 source.mStoredFlags=stored.data();source.mFlagDataList=list.data();source.mCurrentDataIndex=62;source.mWaitTimer=3.75f;
 BirthLedger ledger;PlayerCoreReadFence fence{true,true,19,19};Bytes output;std::string e;DemoFields parsed;
 source.mTargetCreature=reinterpret_cast<Creature*>(uintptr_t(0xdead));
 check(captureDemo(source,ledger,fence,output,e));check(decodeDemo(output,parsed,e));check(parsed.target==0&&parsed.current==-1);
 source.mCurrentDemoIndex=7;check(!captureDemo(source,ledger,fence,output,e));
 uint64_t id=0;check(ledger.birth(source.mTargetCreature,Family::Captain,id,e));check(captureDemo(source,ledger,fence,output,e));check(decodeDemo(output,parsed,e));check(parsed.target==id);
 source.mCurrentDemoIndex=-2;Bytes sentinel={17};check(!captureDemo(source,ledger,fence,sentinel,e));check(sentinel==Bytes{17});
 source.mCurrentDemoIndex=62;check(!captureDemo(source,ledger,fence,sentinel,e));source.mCurrentDemoIndex=7;
 list[63]=&descriptors[0];check(!captureDemo(source,ledger,fence,sentinel,e));list[63]=nullptr;
 auto*d=list[2];list[2]=list[1];check(!captureDemo(source,ledger,fence,sentinel,e));list[2]=d;
 auto before=fence;fence.tickAfter=20;check(!captureDemo(source,ledger,fence,sentinel,e));fence=before;
 check(ledger.retire(source.mTargetCreature,e));check(!captureDemo(source,ledger,fence,sentinel,e));
 source.mTargetCreature=nullptr;check(captureDemo(source,ledger,fence,output,e));check(decodeDemo(output,parsed,e)&&parsed.target==0);
 std::cout<<n<<" actual-header Demo capture controls PASS (no ordinary actor constructors/runtime)\n";return 0;
 }catch(const std::exception&x){std::cerr<<x.what()<<"\n";return 1;}}
