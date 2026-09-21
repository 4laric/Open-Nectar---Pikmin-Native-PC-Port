#include "pc_arpg_controls.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

static bool near(float a, float b) { return std::fabs(a - b) < 0.001f; }
static void require(bool condition, const char* message)
{
    if (condition) return;
    std::fprintf(stderr, "FAIL: %s\n", message);
    std::exit(1);
}

int main()
{
    require(pc_arpg_control_mode_override(nullptr, 0) == 0, "missing launcher override preserves settings");
    require(pc_arpg_control_mode_override("bogus", 1) == 1, "invalid launcher override preserves settings");
    require(pc_arpg_control_mode_override("arpg", 0) == 2, "named launcher override selects ARPG");
    require(pc_arpg_control_mode_override("2", 0) == 2, "numeric launcher override selects ARPG");

    PcArpgMoveState state {};
    PcArpgVec2 direction {};
    pc_arpg_move_reset(&state);
    require(pc_arpg_move_update(&state, { 0, 0 }, 1.0f / 30.0f, true, &direction)
            == PC_ARPG_MOVE_IDLE, "reset state is idle");

    pc_arpg_move_set_destination(&state, { 0, 0 }, { 30, 40 });
    require(pc_arpg_move_update(&state, { 0, 0 }, 0.1f, true, &direction)
            == PC_ARPG_MOVE_ACTIVE, "destination activates movement");
    require(near(direction.x, 0.6f) && near(direction.z, 0.8f), "movement direction normalises");

    // A replacement click resets progress tracking and changes direction.
    pc_arpg_move_set_destination(&state, { 2, 0 }, { -48, 0 });
    require(pc_arpg_move_update(&state, { 2, 0 }, 0.1f, true, &direction)
            == PC_ARPG_MOVE_ACTIVE, "replacement destination stays active");
    require(near(direction.x, -1.0f) && near(direction.z, 0.0f), "replacement changes direction");

    // Arrival and state cancellation never leave a movement vector behind.
    require(pc_arpg_move_update(&state, { -40, 0 }, 0.1f, true, &direction)
            == PC_ARPG_MOVE_ARRIVED, "arrival is detected");
    require(!state.active && near(direction.x, 0.0f), "arrival clears motion");
    pc_arpg_move_set_destination(&state, { 0, 0 }, { 100, 0 });
    require(pc_arpg_move_update(&state, { 0, 0 }, 0.1f, false, &direction)
            == PC_ARPG_MOVE_CANCELLED, "disallowed state cancels movement");

    // A captain held against collision eventually relinquishes the command.
    pc_arpg_move_set_destination(&state, { 0, 0 }, { 100, 0 });
    for (int i = 0; i < 12; ++i) {
        require(pc_arpg_move_update(&state, { 0, 0 }, 0.1f, true, &direction)
                == PC_ARPG_MOVE_ACTIVE, "blocked grace period remains active");
    }
    require(pc_arpg_move_update(&state, { 0, 0 }, 0.1f, true, &direction)
            == PC_ARPG_MOVE_BLOCKED, "blocked movement times out");

    const PcArpgVec2 swarm = pc_arpg_swarm_direction({ 10, 10 }, { 13, 14 });
    require(near(swarm.x, 0.6f) && near(swarm.z, 0.8f), "swarm points toward cursor");
    const PcArpgVec2 zero = pc_arpg_swarm_direction({ 1, 1 }, { 1, 1 });
    require(near(zero.x, 0.0f) && near(zero.z, 0.0f), "coincident swarm is neutral");
    return 0;
}
