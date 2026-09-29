#pragma once

// Single-player switching runs once after both Navis update. Keep the edge
// latch outside either Kontroller: changing captain while Up is held must not
// manufacture a second press in the newly selected controller.
class P2CaptainSwitchPress {
    bool held = false;
public:
    bool update(bool enabled, bool down) {
        const bool pressed = enabled && down && !held;
        held = enabled && down;
        return pressed;
    }
    void reset() { held = false; }
};

inline bool p2_captain_switch_safe(bool present, bool alive, bool down,
                                  bool captured, bool holding, bool freeState) {
    return present && alive && !down && !captured && !holding && freeState;
}

// Skipping a poll does not neutralize last frame's stick/buttons. Do not call
// Controller::reset: it also changes freeze/device state. No synthetic release
// edges are sent to inactive AI (which could otherwise throw a held object).
template<class ControllerT>
void p2_captain_neutral_input(ControllerT& c) {
    c.mCurrentInput = c.mPrevInput = c.mInputPressed = c.mInputReleased = 0;
    c.mInputDoublePressed = c.mDoublePressMask = c.mInputDelay = 0;
    c.mMainStickX = c.mMainStickY = c.mSubStickX = c.mSubStickY = 0;
    c.mAnalogA = c.mAnalogB = c.mTriggerL = c.mTriggerR = 0;
}
