#include "pc_p2_spicy_policy.h"
#include "pc_p2_original_resource_state.h"
#include <cassert>
#include <cstdio>
#include <limits>
using namespace p2originalresource;
int main() {
    p2sprays::SpicyStatus s;
    assert(!s.active() && s.animationRate()==1);
    s.begin(); assert(s.remaining==40 && s.animationRate()==2);
    assert(!s.tick(30,false) && s.remaining==40);
    assert(!s.tick(std::numeric_limits<float>::quiet_NaN(),true) && s.remaining==40);
    assert(!s.tick(-1,true) && s.remaining==40);
    assert(!s.tick(39.5f,true) && s.active());
    s.begin(); assert(s.remaining==40); // re-spray extends; does not stack
    assert(!s.tick(39.5f,true)); assert(s.tick(.5f,true) && !s.active());
    s.begin(); s.clear(); assert(!s.active());
    ResourceState inventory; ResourceSnapshot snapshot; EggContents contents; std::string e;
    assert(!inventory.useSpray(HoneyKind::Spicy,e));
    assert(inventory.restore(snapshot,contents,e));
    assert(inventory.addBerry(HoneyKind::Spicy,9,p2sprays::BerriesPerSpray,e));
    assert(!inventory.useSpray(HoneyKind::Spicy,e));
    assert(inventory.addBerry(HoneyKind::Spicy,1,p2sprays::BerriesPerSpray,e));
    assert(inventory.sprayCount(HoneyKind::Spicy)==1);
    assert(inventory.useSpray(HoneyKind::Spicy,e)); assert(!inventory.useSpray(HoneyKind::Spicy,e));
    assert(inventory.snapshot(snapshot,e) && snapshot.sprayUses[0]==1 && snapshot.sprayUses[1]==0);
    std::puts("P2_SPICY_POLICY_PASS timing/pause/refresh/recovery and real stock production/use; gameplay untested");
}
