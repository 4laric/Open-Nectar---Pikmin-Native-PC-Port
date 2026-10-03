#include "pc_p2_fire_flick.h"
#include <cassert>
#include <cstdio>
int main() {
    using namespace p2fireflick;
    // Regression: old caller used an 80-unit XZ attack radius. Retail flick
    // has a strict 40-unit sphere, including the vertical separation.
    assert(nearby(39,0,0));
    assert(!nearby(40,0,0));
    assert(!nearby(0,40,0));
    assert(!nearby(30,30,0));
    assert(nearby(20,20,20));
    assert(!nearby(60,0,0));
    static_assert(Chance==1 && Knockback==120 && Damage==1, "retail FireChappy parameters");
    std::puts("PASS P2_FIRE_FLICK_RANGE");
}
