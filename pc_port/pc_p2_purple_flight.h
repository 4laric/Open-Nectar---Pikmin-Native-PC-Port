#pragma once

class Piki;

enum class PcP2PurpleFlightPhase { None, Ascent, EntryPause, Descent, Recovery };

struct PcP2PurpleFlightSample {
    PcP2PurpleFlightPhase phase = PcP2PurpleFlightPhase::None;
    float phaseElapsed = 0.0f;
    float motionElapsed = 0.0f;
};

void pc_p2_purple_flight_reset();
void pc_p2_purple_flight_setup();
bool pc_p2_purple_flight_enabled();
void pc_p2_purple_flight_arm(Piki*);
bool pc_p2_purple_flight_update(Piki*, float deltaTime, float gravity);
bool pc_p2_purple_flight_land(Piki*, bool enemyContact);
void pc_p2_purple_flight_contact(Piki*, bool enemyContact);
void pc_p2_purple_flight_cancel(Piki*);
bool pc_p2_purple_flight_active(const Piki*);
PcP2PurpleFlightSample pc_p2_purple_flight_sample(const Piki*);

// Read-only checkpoint component. Pointers are local identity-resolver inputs,
// never serialized. Caller must hold the authoritative post-update capture fence.
#include <vector>
#include <string>
struct PcP2PurpleFlightCheckpointEntry {
    const Piki* actor = nullptr;
    PcP2PurpleFlightPhase phase = PcP2PurpleFlightPhase::None;
    float phaseElapsed = 0, motionElapsed = 0;
    bool hadIgnoreGravity = false, hadPriorityFaceDirection = false;
};
struct PcP2PurpleFlightCheckpoint {
    bool enabled = false;
    std::vector<PcP2PurpleFlightCheckpointEntry> entries;
};
bool pc_p2_purple_flight_capture(PcP2PurpleFlightCheckpoint&, std::string& error);
