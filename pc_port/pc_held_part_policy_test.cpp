// Engine-free contract test for the generic held ship part (#901).
#include "pc_held_part_policy.h"

#include <cassert>
#include <cstdio>

int main()
{
    using p2heldpart::Drop;
    using p2heldpart::keepAtBirth;
    using p2heldpart::onDeath;

    // Birth guard.
    assert(keepAtBirth(true, false));
    assert(!keepAtBirth(true, true));   // collected / cached / on the ground
    assert(!keepAtBirth(false, false)); // not a part holder

    // Real death drops once.
    assert(onDeath(false, true, 0.0f, false) == Drop::Spawn);
    assert(onDeath(false, true, -3.0f, false) == Drop::Spawn);
    // Latch: spawnItems or an earlier funnel already dropped it.
    assert(onDeath(true, true, 0.0f, false) == Drop::None);
    // Escape / burrow / teardown: health left, no drop, part still held.
    assert(onDeath(false, true, 0.5f, false) == Drop::None);
    // Not a holder.
    assert(onDeath(false, false, 0.0f, false) == Drop::None);
    // Duplicate guard: the part already exists somewhere.
    assert(onDeath(false, true, 0.0f, true) == Drop::AlreadyExists);

    std::puts("pc_held_part_policy_test: ok");
    return 0;
}
