#include "pc_p2_campaign_policy.h"
#include <cassert>
#include <initializer_list>
int main() {
    const unsigned sources[] = {9,23,44,54,57,59,60,61,62,78,79};
    const int hosts[] = {3,3,3,24,0,3,3,3,3,0,3};
    for (unsigned i=0;i<11;++i) {
        for (int original=0;original<34;++original) {
            assert(p2campaign::hostType(sources[i],original,false)==hosts[i]);
            assert(p2campaign::hostType(sources[i],original,true)==original);
        }
    }
    for (unsigned source: {0u,1u,41u,45u,58u,99u,999u})
        assert(p2campaign::hostType(source,17,false)==17);
    for (unsigned source : {9u,23u,44u,54u,57u,59u,60u,61u,62u,78u,79u})
        assert(p2campaign::hasStaticHost(source));
    for (unsigned source : {2u,17u,0u}) assert(!p2campaign::hasStaticHost(source));
    // Exhaustive agreement (#871 D5): hasStaticHost(s) matches the sentinel
    // probe definition hostType(s,-1,false) != -1 for every s in 0..200.
    // Static hosts return constants (3/24/0, never -1); default sources echo
    // the sentinel, so with original=-1 they return -1 exactly when there is
    // no static host.
    for (unsigned s = 0; s <= 200; ++s)
        assert(p2campaign::hasStaticHost(s) == (p2campaign::hostType(s, -1, false) != -1));
    // Sentinel-collision probe: TEKI_NULL is -1, but hostType never returns a
    // negative today, so the -1/-2 probe cannot collide with a real host.
    for (unsigned s = 0; s <= 200; ++s) {
        assert(p2campaign::hostType(s, -1, false) >= -1);
        assert(p2campaign::hostType(s, -2, false) >= -2);
    }
}
