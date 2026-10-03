#include "pc_p2_original_piki_host_context.h"
#include "UpdateMgr.h"
#include "system.h"
#include "Stream.h"
#include <cstdio>
#include <cstdlib>
// Real unchanged native UpdateContext/UpdateMgr methods are linked. Rendering,
// console and fatal diagnostics below are unused harness dependencies only.
Stream* sysCon=nullptr;
System* gsys=nullptr;
void System::halt(const char*,int,const char*){std::abort();}
void Stream::print(const char*,...){std::abort();}
namespace {
unsigned checks=0;
void require(bool v,const char* text){++checks;if(!v){std::fprintf(stderr,"FAIL %s\n",text);std::exit(1);}}
struct Manager {
 UpdateMgr value;int clients[2]={0,0},active[2]={0,0};
 Manager(){value.mSlotCount=2;value.mClientSlotList=clients;value.mActiveClientSlotList=active;}
};
}
int main(){
 using Binding=p2original::piki::HostUpdateBinding;std::string error;
 Manager manager;UpdateContext context;context.mIsPiki=false;
 {
  Binding owner;require(owner.acquire(context,&manager.value,true,error),"actual native search context registers");
  require(owner.phase()==Binding::Phase::Registered&&manager.value.mClientTotal==1&&manager.clients[0]==1&&manager.active[0]==1,"exact one native registration");
  require(!owner.acquire(context,&manager.value,true,error)&&manager.value.mClientTotal==1,"same owner cannot register twice");
  require(owner.canRelease(error)&&owner.release(error),"owned native registration disposes");
  require(manager.value.mClientTotal==0&&manager.clients[0]==0&&manager.active[0]==0&&context.mMgr==nullptr,"native count baseline restored");
  require(!owner.owned()&&owner.release(error),"successful disposal is idempotent");
 }
 context.mIsPiki=false;context.init(&manager.value);
 const int before=manager.value.mClientTotal,slot=context.mMgrSlotIndex;
 {
  Binding owner;require(owner.acquire(context,&manager.value,true,error),"existing pool registration is borrowed");
  require(owner.phase()==Binding::Phase::Borrowed&&manager.value.mClientTotal==before&&!context.mIsPiki,"borrow does not rewrite active flag or duplicate registration");
  require(owner.release(error)&&context.mMgr==&manager.value&&manager.value.mClientTotal==before&&context.mMgrSlotIndex==slot,"borrow release preserves actual preallocated pool baseline");
 }
 {
  Binding owner;require(owner.acquire(context,&manager.value,false,error),"preallocated context recaptured");
  context.mMgrSlotIndex=1;
  require(!owner.canRelease(error)&&!owner.release(error)&&owner.owned()&&manager.value.mClientTotal==before,"changed context refuses cleanup without counter mutation");
  context.mMgrSlotIndex=slot;
  require(owner.release(error)&&manager.value.mClientTotal==before,"corrected borrowed observation can release safely");
 }
 Manager foreign;
 {Binding owner;require(!owner.acquire(context,&foreign.value,false,error)&&!owner.owned(),"foreign existing native manager rejected");}
 context.exit();context.mMgrSlotIndex=-1;
 manager.clients[0]=manager.clients[1]=10000;manager.value.mClientTotal=20000;
 {Binding owner;require(!owner.acquire(context,&manager.value,false,error)&&!owner.owned()&&!context.mMgr,"native addClient full capacity refused before mutation");}
 manager.clients[0]=manager.clients[1]=0;manager.value.mClientTotal=0;manager.active[1]=1;
 {Binding owner;require(!owner.acquire(context,&manager.value,false,error)&&!owner.owned()&&!context.mMgr,"corrupt native census refused before registration");}
 manager.active[1]=0;
 {Binding owner;require(owner.acquire(context,nullptr,false,error)&&!owner.owned()&&!context.mMgr,"absent native manager creates no fictitious registration");}
 {
  Binding owner;require(owner.acquire(context,&manager.value,false,error),"new nonsearch registration starts");
  const int registeredSlot=context.mMgrSlotIndex;
  context.mMgr=&foreign.value;
  require(!owner.release(error)&&owner.owned()&&manager.value.mClientTotal==1&&foreign.value.mClientTotal==0,"changed manager retains real new registration");
  context.mMgr=&manager.value;context.mMgrSlotIndex=registeredSlot;
  require(owner.release(error)&&manager.value.mClientTotal==0,"actual registration cleanup retries after ownership restoration");
 }
 std::printf("Source bootstrap real native context controls PASS %u (no Piki factory/Stage activation)\n",checks);
}
