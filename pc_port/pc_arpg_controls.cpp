#include "pc_arpg_controls.h"

#include <cmath>

namespace {
constexpr float kArrivalRadius = 18.0f;
constexpr float kProgressEpsilon = 1.0f;
constexpr float kBlockedTimeout = 1.25f;

float length(PcArpgVec2 value)
{
    return std::sqrt(value.x * value.x + value.z * value.z);
}

PcArpgVec2 normalise(PcArpgVec2 value)
{
    const float magnitude = length(value);
    if (magnitude <= 0.0001f) return { 0.0f, 0.0f };
    return { value.x / magnitude, value.z / magnitude };
}
}

void pc_arpg_move_reset(PcArpgMoveState* state)
{
    if (!state) return;
    *state = { false, { 0.0f, 0.0f }, { 0.0f, 0.0f }, 0.0f };
}

void pc_arpg_move_set_destination(PcArpgMoveState* state, PcArpgVec2 current,
                                  PcArpgVec2 destination)
{
    if (!state) return;
    state->active = true;
    state->destination = destination;
    state->lastPosition = current;
    state->blockedSeconds = 0.0f;
}

PcArpgMoveResult pc_arpg_move_update(PcArpgMoveState* state, PcArpgVec2 current,
                                     float deltaSeconds, bool allowMovement,
                                     PcArpgVec2* direction)
{
    if (direction) *direction = { 0.0f, 0.0f };
    if (!state || !state->active) return PC_ARPG_MOVE_IDLE;
    if (!allowMovement) {
        pc_arpg_move_reset(state);
        return PC_ARPG_MOVE_CANCELLED;
    }

    const PcArpgVec2 remaining {
        state->destination.x - current.x,
        state->destination.z - current.z,
    };
    if (length(remaining) <= kArrivalRadius) {
        pc_arpg_move_reset(state);
        return PC_ARPG_MOVE_ARRIVED;
    }

    const PcArpgVec2 progress {
        current.x - state->lastPosition.x,
        current.z - state->lastPosition.z,
    };
    if (length(progress) < kProgressEpsilon) {
        state->blockedSeconds += deltaSeconds > 0.0f ? deltaSeconds : 0.0f;
    } else {
        state->blockedSeconds = 0.0f;
        state->lastPosition = current;
    }
    if (state->blockedSeconds >= kBlockedTimeout) {
        pc_arpg_move_reset(state);
        return PC_ARPG_MOVE_BLOCKED;
    }

    if (direction) *direction = normalise(remaining);
    return PC_ARPG_MOVE_ACTIVE;
}

PcArpgVec2 pc_arpg_swarm_direction(PcArpgVec2 captain, PcArpgVec2 cursor)
{
    return normalise({ cursor.x - captain.x, cursor.z - captain.z });
}
