#include "pc_p2_campaign_policy.h"
#include "pc_p2_proxy_pack.h"
#include <cassert>
#include <initializer_list>
#include <set>
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
    // inst-chappy (#871): Chappy (2) rides TEKI_Swallow (4). Later family
    // species add their own static case when their identity lands; until
    // then they stay proxy-routed (non-static).
    assert(p2campaign::hasStaticHost(2u));
    for (unsigned source : {33u,35u,43u,53u,67u,76u,17u,0u})
        assert(!p2campaign::hasStaticHost(source));
    for (int original = 0; original < 34; ++original) {
        assert(p2campaign::hostType(2u, original, false) == 4);
        assert(p2campaign::hostType(2u, original, true) == original);
    }
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
    // Pack-generator token policy (finding 5): first sight binds in both
    // modes; a repeat binds for soft proxy and fails otherwise.
    assert(p2proxy::tokenAction(false, false) == p2proxy::TokenAction::Bind);
    assert(p2proxy::tokenAction(true, false) == p2proxy::TokenAction::Bind);
    assert(p2proxy::tokenAction(true, true) == p2proxy::TokenAction::Bind);
    assert(p2proxy::tokenAction(false, true) == p2proxy::TokenAction::Fail);
    // Bind-every-member simulation: members sharing token T each bind, the
    // token counts once (mirrors the bindFamilies loop: insert once, bind
    // on every member when the helper says Bind).
    {
        const unsigned members[] = {7, 7, 7, 9, 9};
        std::set<unsigned> found;
        unsigned binds = 0;
        for (unsigned token : members) {
            const bool isRepeat = !found.insert(token).second;
            assert(p2proxy::tokenAction(true, isRepeat) == p2proxy::TokenAction::Bind);
            ++binds;
        }
        assert(binds == 5);
        assert(found.size() == 2);
    }
    // Strict mode still rejects the repeat (would fail() in bindFamilies).
    {
        std::set<unsigned> found;
        found.insert(7u);
        assert(p2proxy::tokenAction(false, !found.insert(7u).second)
               == p2proxy::TokenAction::Fail);
    }
}
