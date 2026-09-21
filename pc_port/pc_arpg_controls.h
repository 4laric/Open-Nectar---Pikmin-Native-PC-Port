#ifndef PC_ARPG_CONTROLS_H
#define PC_ARPG_CONTROLS_H

struct PcArpgVec2 {
    float x;
    float z;
};

struct PcArpgMoveState {
    bool active;
    PcArpgVec2 destination;
    PcArpgVec2 lastPosition;
    float blockedSeconds;
};

// Launcher-only override parser. Unknown values fail closed to the persisted
// mode so a typo cannot silently select a different control scheme.
int pc_arpg_control_mode_override(const char* value, int persistedMode);

enum PcArpgMoveResult {
    PC_ARPG_MOVE_IDLE = 0,
    PC_ARPG_MOVE_ACTIVE,
    PC_ARPG_MOVE_ARRIVED,
    PC_ARPG_MOVE_BLOCKED,
    PC_ARPG_MOVE_CANCELLED,
};

void pc_arpg_move_reset(PcArpgMoveState* state);
void pc_arpg_move_set_destination(PcArpgMoveState* state, PcArpgVec2 current,
                                  PcArpgVec2 destination);
PcArpgMoveResult pc_arpg_move_update(PcArpgMoveState* state, PcArpgVec2 current,
                                     float deltaSeconds, bool allowMovement,
                                     PcArpgVec2* direction);
PcArpgVec2 pc_arpg_swarm_direction(PcArpgVec2 captain, PcArpgVec2 cursor);

#endif
