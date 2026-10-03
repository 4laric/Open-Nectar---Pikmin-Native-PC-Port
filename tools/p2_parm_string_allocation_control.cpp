#include "Parameters.h"
#include "Common/String.h"
#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <new>
static std::size_t allocations=0;
void* operator new(std::size_t size){++allocations;if(auto* p=std::malloc(size))return p;throw std::bad_alloc();}
void* operator new[](std::size_t size){return ::operator new(size);}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete[](void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}
void operator delete[](void* p,std::size_t)noexcept{std::free(p);}
// Constructor/allocation control only. Serialization/editor methods are unused
// test link seams; this executable has no native actor/session implementation.
BaseParm::BaseParm(Parameters* owner,ayuID id):mID(id),mNext(owner->mFirstParm){owner->mFirstParm=this;}
template<> void Parm<String>::write(RandomAccessStream&){}
template<> void Parm<String>::read(RandomAccessStream&){}
#ifdef WIN32
template<> void Parm<String>::genAge(AgeServer&){}
#endif
int main(int argc,char** argv){
    assert(argc==1||argc==2);const auto expected=argc==2?std::strtoul(argv[1],nullptr,10):0;
    auto before=allocations;String defaultString;assert(allocations==before+1);delete[] defaultString.mString;
    char value[]="new",empty[]="";
    before=allocations;
    for(unsigned retry=0;retry<1000;++retry){
        Parameters owner("control");Parm<String> name(&owner,String(value,0),String(empty,0),String(empty,0),ayuID("x99 "),"name");
        assert(name.mValue.mString==value&&owner.mFirstParm==&name);
#ifdef WIN32
        assert(name.mDefaultValue.mString==value&&name.mMinValue.mString==empty&&name.mMaxValue.mString==empty);
#endif
    }
    std::printf("1000 real Parm<String> constructions: default-allocation delta=%zu expected=%lu\n",allocations-before,expected);
    assert(allocations-before==expected);
    std::puts("PASS measured real Parm<String> allocation delta matches the expected control; no gameplay authority");
}
