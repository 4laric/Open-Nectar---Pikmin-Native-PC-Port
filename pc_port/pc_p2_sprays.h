#pragma once
#include "pc_p2_original_honey_policy.h"
class Piki;
class PikiState;
class Navi;
namespace p2originalresource { class ResourceState; }
// The original campaign owner binds its single authoritative inventory only
// after resource/receiver installation; nullptr detaches before destruction.
// growup is receiverClips[1] from the verified actual Honey SourceBank.
bool pc_p2_sprays_bind(p2originalresource::ResourceState*,
    const p2originalresource::honey::ReceiverClip* growup, std::string& error);
bool pc_p2_sprays_input(Navi*);
bool pc_p2_spicy_use(Navi*);
bool pc_p2_spicy_accept(Piki*);
bool pc_p2_spicy_active(const Piki*);
// Read-only Party capture observation. Failure leaves both outputs unchanged.
// SAVE must be held while pendingDope is true; remaining cannot resume Dope.
bool pc_p2_spicy_save_observation(const Piki*, float& remaining, bool& pendingDope) noexcept;
// No save format currently carries spicy continuation. Read the living roster
// before capture/cache/card mutation; this check never grants SAVE admission.
bool pc_p2_spicy_save_preflight(std::string& error);
void pc_p2_spicy_tick(Piki*);
PikiState* pc_p2_spicy_state();
