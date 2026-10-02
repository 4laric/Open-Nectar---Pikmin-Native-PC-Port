#include "pc_midday_allocation_owner.h"
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
using pc_midday::AllocationOwner;
static void check(bool v,const char*m){if(!v){std::cerr<<m<<'\n';std::abort();}}
struct Counted {static int live,dead;Counted(){++live;}~Counted(){--live;++dead;}};
int Counted::live=0;int Counted::dead=0;
struct Base {static int dead;~Base(){++dead;}};int Base::dead=0;
struct Derived:Base {static int dead;std::unique_ptr<int[]> data{new int[5]};~Derived(){++dead;}};int Derived::dead=0;
struct ThrowMember {Counted member;ThrowMember(){throw std::runtime_error("member");}};
struct ThrowArray {static int attempts,fail,live;ThrowArray(){if(attempts++==fail)throw std::runtime_error("array element");++live;}~ThrowArray(){--live;}};
int ThrowArray::attempts=0;int ThrowArray::fail=-1;int ThrowArray::live=0;
struct Node;
struct Child {Node* node=nullptr;~Child();};
static int detached=0,expectedHooks=0,nodeLive=0,firstDestroyed=-1,lastDestroyed=-1;
struct Node {Child* children=nullptr;int id;explicit Node(int n):id(n){++nodeLive;}~Node(){check(detached==expectedHooks,"destruction before all detach hooks");if(firstDestroyed<0)firstDestroyed=id;lastDestroyed=id;delete[] children;--nodeLive;}};
Child::~Child(){check(detached==expectedHooks,"array destruction before detach");delete node;}
static void detachNode(void*p,std::size_t)noexcept{static_cast<Node*>(p)->children=nullptr;++detached;}
static void detachArray(void*p,std::size_t n)noexcept{auto*c=static_cast<Child*>(p);for(std::size_t i=0;i<n;++i)c[i].node=nullptr;++detached;}
int main(){int controls=0;
 // One object attempt, two array attempts, then one final object attempt.
 for(std::size_t failure=0;failure<=4;++failure){Counted::dead=0;AllocationOwner o(failure);bool threw=false;try{o.make<Counted>();o.array<Counted>(3);o.make<Counted>();}catch(const std::bad_alloc&){threw=true;}
 check(threw==(failure<4),"wrong allocation injection boundary");check(o.allocationAttempts()==(failure<4?failure+1:4),"allocation attempt accounting");o.reset();check(Counted::live==0&&o.ownedEntries()==0,"injected attempt leaked");o.reset();++controls;}
 {AllocationOwner o;o.make<Derived>();o.reset();check(Derived::dead==1&&Base::dead==1,"nonvirtual base substituted for concrete destructor");++controls;}
 {AllocationOwner o;const int prior=Counted::dead;try{o.make<ThrowMember>();check(false,"member constructor did not throw");}catch(const std::runtime_error&){}check(Counted::live==0&&Counted::dead==prior+1&&o.ownedEntries()==0,"RAII member unwind leaked");++controls;}
 for(int failure=0;failure<4;++failure){AllocationOwner o;ThrowArray::attempts=0;ThrowArray::fail=failure;try{o.array<ThrowArray>(4);check(false,"array constructor did not throw");}catch(const std::runtime_error&){}check(ThrowArray::live==0&&o.ownedEntries()==0,"partial array unwind failed");++controls;}
 for(int failure=0;failure<4;++failure){AllocationOwner o;ThrowArray::attempts=0;ThrowArray::fail=failure;try{o.arrayConstruct<ThrowArray>(4,[](void* p,std::size_t){return new(p)ThrowArray;});check(false,"placement array did not throw");}catch(const std::runtime_error&){}check(ThrowArray::live==0&&o.ownedEntries()==0,"placement array unwind leaked");++controls;}
 for(std::size_t failure=0;failure<2;++failure){AllocationOwner o(failure);ThrowArray::fail=-1;bool threw=false;try{o.arrayConstruct<ThrowArray>(4,[](void* p,std::size_t){return new(p)ThrowArray;});}catch(const std::bad_alloc&){threw=true;}check(threw&&ThrowArray::live==0&&o.ownedEntries()==0,"placement array allocation injection failed");++controls;}
 // Real recursively owning Child[] topology, never fake engine objects.
 for(std::size_t failure=0;failure<=5;++failure){detached=expectedHooks=0;firstDestroyed=lastDestroyed=-1;AllocationOwner o(failure);bool threw=false;
 try{auto*root=o.make<Node>(0);o.setCleanup(root,detachNode);++expectedHooks;
 auto*slots=o.array<Child>(2);o.setCleanup(slots,detachArray);++expectedHooks;root->children=slots;
 for(int i=0;i<2;++i){auto*child=o.make<Node>(i+1);o.setCleanup(child,detachNode);++expectedHooks;slots[i].node=child;}}
 catch(const std::bad_alloc&){threw=true;}
 check(threw==(failure<5),"graph failure boundary");o.reset();check(nodeLive==0&&o.ownedEntries()==0,"graph cleanup leaked/doubled");if(failure>0)check(lastDestroyed==0,"root not destroyed last");if(failure==5)check(firstDestroyed==2,"reverse concrete destruction order");o.reset();++controls;}
 {AllocationOwner o;int foreign=0;bool refused=false;try{o.setCleanup(&foreign,detachNode);}catch(const std::invalid_argument&){refused=true;}check(refused,"foreign cleanup accepted");++controls;}
 std::cout<<controls<<" AllocationOwner real allocation/unwind controls PASS\n";
}
