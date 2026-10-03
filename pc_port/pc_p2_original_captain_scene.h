#pragma once
#include <string>
namespace p2retail { class SceneContext; }
class Navi;

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
// Revoke action/activation and control borrows first; retain exact canonical
// descriptor and inactive World through Body/Party/Plate cleanup. Keep geometry/collision
// bank owned until actual held Pikmin/Party/plate cleanup has completed.
bool pc_p2_original_captain_scene_revoke(const p2retail::SceneContext&,std::string& error);
bool pc_p2_original_captain_scene_retire(const p2retail::SceneContext&,std::string& error);

// Exact physical owner tag through selection expiry and inactive retirement;
// refusal classification only, never action/lifetime/alive authorization.
bool pc_p2_original_captain_scene_body_owned(const Navi*) noexcept;
