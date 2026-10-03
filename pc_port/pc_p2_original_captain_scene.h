#pragma once
#include <string>
namespace p2retail { class SceneContext; }

// Actual selected retail stage resource owner supplies the context. Reset owns
// the final two native body resets and genuine bank binding; it publishes only
// Loading. Physical floor/body admission must precede activate.
bool pc_p2_original_captain_scene_reset(std::string& error);
bool pc_p2_original_captain_scene_activate(std::string& error);
// These queries concern captain bank/control ownership only. Source Party,
// held Pikmin and path consumers must retire through their own actual owners
// before this bank is detached or the native roster is destroyed.
bool pc_p2_original_captain_scene_owned(const p2retail::SceneContext&) noexcept;
bool pc_p2_original_captain_scene_can_retire(const p2retail::SceneContext&,std::string& error);
bool pc_p2_original_captain_scene_retire(const p2retail::SceneContext&,std::string& error);
