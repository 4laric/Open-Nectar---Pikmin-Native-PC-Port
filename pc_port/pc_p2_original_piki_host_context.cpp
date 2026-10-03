#include "pc_p2_original_piki_host_context.h"
#include "UpdateMgr.h"
#include <limits>
#include <exception>
namespace p2original { namespace piki {
namespace {
bool fail(std::string& e,const char* message){e=message;return false;}
bool validManager(const UpdateMgr* manager){
 return manager&&manager->mSlotCount>0&&manager->mSlotCount<=1000000
  &&manager->mClientSlotList&&manager->mActiveClientSlotList&&manager->mClientTotal>=0;
}
bool validSlot(const UpdateMgr* manager,int slot,bool active){
 return validManager(manager)&&slot>=0&&slot<manager->mSlotCount
  &&manager->mClientSlotList[slot]>0
  &&manager->mActiveClientSlotList[slot]>=0
  &&manager->mActiveClientSlotList[slot]<=manager->mClientSlotList[slot]
  &&(!active||manager->mActiveClientSlotList[slot]>0)&&manager->mClientTotal>0;
}
}
HostUpdateBinding::~HostUpdateBinding(){
 // Never silently destroy a possibly live registration. The process-retained
 // factory owns this value through successful cleanup, including uncertainty.
 if(mPhase==Phase::Registered||mPhase==Phase::Uncertain)std::terminate();
}
bool HostUpdateBinding::acquire(UpdateContext& context,UpdateMgr* manager,bool searchPiki,std::string& e){
 if(mPhase!=Phase::Empty)return fail(e,"native source context owner already retained");
 if(context.mMgr){
  if(context.mMgr!=manager||!validSlot(manager,context.mMgrSlotIndex,context.mIsPiki))
   return fail(e,"pre-existing native context lacks exact current manager registration");
  // Record the existing native registration unchanged. Its original manager
  // retains it when this Source body leaves the slot; no duplicate addClient.
  mContext=&context;mManager=manager;mSlot=context.mMgrSlotIndex;mPiki=context.mIsPiki;
  mPhase=Phase::Borrowed;e.clear();return true;
 }
 if(!manager){e.clear();return true;}
 if(!validManager(manager)||manager->mClientTotal==std::numeric_limits<int>::max())
  return fail(e,"native source context manager is unavailable or full");
 int slot=-1,smallest=10000;
 for(int i=0;i<manager->mSlotCount;++i){
  const int count=manager->mClientSlotList[i],active=manager->mActiveClientSlotList[i];
  if(count<0||active<0||active>count)return fail(e,"native source context manager census is corrupt");
  if(count<smallest){slot=i;smallest=count;}
 }
 if(slot<0)return fail(e,"native source context manager has no registration capacity");
 const int clients=manager->mClientTotal,active=manager->mActiveClientSlotList[slot];
 // Retain exact ownership BEFORE calling native registration. Any throw or
 // unexpected result leaves Uncertain ownership instead of a lost callback.
 mContext=&context;mManager=manager;mSlot=slot;mPiki=searchPiki;mPhase=Phase::Uncertain;
 context.mMgrSlotIndex=-1;context.mIsPiki=searchPiki;
 try{context.init(manager);}catch(...){return fail(e,"native source context registration threw; ownership retained");}
 if(context.mMgr!=manager||context.mMgrSlotIndex!=slot||context.mIsPiki!=searchPiki
  ||manager->mClientTotal!=clients+1||manager->mClientSlotList[slot]!=smallest+1
  ||manager->mActiveClientSlotList[slot]!=active+(searchPiki?1:0))
  return fail(e,"native source context registration result is uncertain");
 mPhase=Phase::Registered;e.clear();return true;
}
bool HostUpdateBinding::current(std::string& e)const{
 if(mPhase==Phase::Empty){e.clear();return true;}
 if(mPhase==Phase::Uncertain)return fail(e,"uncertain native context requires explicit recovery");
 if(!mContext||mContext->mMgr!=mManager||mContext->mMgrSlotIndex!=mSlot
  ||mContext->mIsPiki!=mPiki||!validSlot(mManager,mSlot,mPiki))
  return fail(e,"native source context registration changed");
 e.clear();return true;
}
bool HostUpdateBinding::canRelease(std::string& e)const{return current(e);}
bool HostUpdateBinding::release(std::string& e){
 if(!current(e))return false;
 if(mPhase==Phase::Registered){
  const int clients=mManager->mClientTotal,count=mManager->mClientSlotList[mSlot],active=mManager->mActiveClientSlotList[mSlot];
  mPhase=Phase::Uncertain;
  try{mContext->exit();}catch(...){return fail(e,"native context disposal threw; ownership retained");}
  if(mContext->mMgr||mContext->mIsPiki||mManager->mClientTotal!=clients-1
   ||mManager->mClientSlotList[mSlot]!=count-1||mManager->mActiveClientSlotList[mSlot]!=active-(mPiki?1:0))
   return fail(e,"native context disposal result is uncertain");
  mContext->mMgrSlotIndex=-1;
 }
 // Borrowed registrations remain physically intact. Releasing this observer
 // does not call native exit or modify another allocation's counter baseline.
 mPhase=Phase::Empty;mContext=nullptr;mManager=nullptr;mSlot=-1;mPiki=false;
 e.clear();return true;
}
} }
