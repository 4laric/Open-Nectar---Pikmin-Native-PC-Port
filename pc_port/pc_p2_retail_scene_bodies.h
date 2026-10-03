#pragma once
#include "pc_p2_retail_scene.h"
// Strong actual GameCore source-body lifecycle owner. No accepting fallback.
// Retirement includes the source party, captains, borrowed rig/model resources
// and path consumers; a paused/inactive World alone is not retirement.
// Admission is separate from partial resource ownership: it verifies the actual
// source two-captain and 20-Pikmin roster/party/bank, never a native count flag.
// Startup invokes this after real Captain reset/bank binding in Loading. The
// sole Body owner consumes selectedStartingPiki, owns all partial allocations
// and installs real species/FSM/Services; refusal retains retirement ownership.
bool pc_p2_retail_scene_bodies_create(const p2retail::SceneContext&,std::string&);
bool pc_p2_retail_scene_bodies_admitted(const p2retail::SceneContext&,std::string&);
bool pc_p2_retail_scene_bodies_owned(const p2retail::SceneContext&) noexcept;
bool pc_p2_retail_scene_bodies_can_retire(const p2retail::SceneContext&,std::string&);
bool pc_p2_retail_scene_bodies_retire(const p2retail::SceneContext&,std::string&);
bool pc_p2_retail_scene_bodies_retired(const p2retail::SceneContext&) noexcept;
