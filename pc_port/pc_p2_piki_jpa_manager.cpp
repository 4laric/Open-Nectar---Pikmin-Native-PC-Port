#include "pc_p2_piki_jpa_manager.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <new>
namespace p2original { namespace pikiJPA {
namespace {bool fail(std::string& e,const char* s){e=s;return false;}}
Manager::Manager(const Bank& bank):mBank(std::make_shared<const Bank>(bank)){
 mHead.fill(-1);mTail.fill(-1);mPrev.fill(-1);
 // JPAEmitterManager ctor prepends entries in increasing array order.
 for(unsigned i=0;i<capacity;++i){mNext[i]=mFree;mFree=int(i);}
}
Manager::~Manager(){if(mLive){std::fprintf(stderr,"P2_JPA_MANAGER_RETAINED_EMITTERS_REFUSED live=%u\n",mLive);std::fflush(stderr);std::abort();}}
bool Manager::owns(const EmitterHandle& h)const{return h&&h->poolIndex()<capacity&&mSlots[h->poolIndex()].get()==h.get()&&mSlots[h->poolIndex()]->admission()==h->admission();}
bool Manager::create(unsigned id,EmitterHandle& out,std::string& e){
 if(out)return fail(e,"JPA admission output already retains an emitter");
 std::size_t count=0;const auto* roles=resourceRoles(count);const ResourceRole* role=nullptr;
 for(std::size_t i=0;i<count;++i)if(roles[i].id&&roles[i].id==id){role=&roles[i];break;}
 if(!role)return fail(e,"JPA admission unsupported selected source resource");
 bool halo=false;for(unsigned species=0;species<6;++species)if(haloId(species)==id)halo=true;
 if(!halo)return fail(e,"JPA admission selected resource backend is unsupported");
 const auto* bytes=mBank->bytes(role->role);
 if(!bytes)return fail(e,"JPA admission selected resource is absent");
 if(bytes->size()<24||(*bytes)[8]!='B'||(*bytes)[9]!='E'||(*bytes)[10]!='M'||(*bytes)[11]!='1')return fail(e,"JPA admission selected dynamics block is invalid");
 if(mFree<0)return fail(e,"JPA source emitter pool is exhausted");
 if(mAdmissions==std::numeric_limits<std::uint64_t>::max())return fail(e,"JPA source admission identity overflow");
 const unsigned flags=(unsigned((*bytes)[20])<<24)|(unsigned((*bytes)[21])<<16)|(unsigned((*bytes)[22])<<8)|(*bytes)[23];
 const unsigned group=(flags&4)?0:((flags&1)?1:2);
 const unsigned slot=unsigned(mFree);const auto seed=mFrontier*0x19660du+0x3c6ef35fu;
 EmitterHandle born;
 try{born=EmitterHandle(new Emitter(id,slot,group,seed,mAdmissions+1,mBank,role->role));}
 catch(const std::bad_alloc&){return fail(e,"JPA owned emitter allocation failed");}
 // No failure point after source admission begins; RNG advances only here.
 mFree=mNext[slot];mPrev[slot]=mTail[group];mNext[slot]=-1;
 if(mTail[group]>=0)mNext[unsigned(mTail[group])]=int(slot);else mHead[group]=int(slot);
 mTail[group]=int(slot);mSlots[slot]=born;++mLive;mFrontier=seed;++mAdmissions;
 out=std::move(born);e.clear();return true;
}
void Manager::retire(unsigned slot){
 const unsigned group=mSlots[slot]->group();const int prev=mPrev[slot],next=mNext[slot];
 if(prev>=0)mNext[unsigned(prev)]=next;else mHead[group]=next;
 if(next>=0)mPrev[unsigned(next)]=prev;else mTail[group]=prev;
 mSlots[slot].reset();mPrev[slot]=-1;mNext[slot]=mFree;mFree=int(slot);--mLive;
}
bool Manager::erase(EmitterHandle& h,std::string& e){
 if(!owns(h))return fail(e,"JPA deletion requires exact live owned emitter");
 retire(h->poolIndex());h.reset();e.clear();return true;
}
void Manager::killAll(){for(unsigned group=0;group<9;++group)while(mTail[group]>=0)retire(unsigned(mTail[group]));}
} }
