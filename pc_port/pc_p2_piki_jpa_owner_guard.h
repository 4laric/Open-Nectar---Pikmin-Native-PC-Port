#pragma once
#include <cstddef>
#include <cstdio>
#include <cstdlib>
namespace p2original { namespace pikiJPA { namespace detail {
// Shared by the actual NativeEffects destructor and the engineering death test.
// No cleanup occurs on refusal; preserve the last checked deletion failure.
inline void requireRetiredOwners(std::size_t nativeOwners,std::size_t coreOwners,const char* lastRefusal){
 if(!nativeOwners&&!coreOwners)return;
 std::fprintf(stderr,"P2_PIKI_JPA_OWNER_DESTROY_REFUSED native=%zu core=%zu last_cleanup=%s\n",nativeOwners,coreOwners,lastRefusal&&*lastRefusal?lastRefusal:"not checked");
 std::fflush(stderr);std::abort();
}
} } }
