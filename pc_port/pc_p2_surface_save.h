#pragma once
class Controller;
class Graphics;
// F11 opens the ordinary native file-selection UI for an explicitly enabled
// settled surface checkpoint. F1 stays unchanged. Unsupported scenes refuse.
void pc_p2_surface_save_scene_setup();
void pc_p2_surface_save_scene_exit();
bool pc_p2_surface_save_update(Controller*);
void pc_p2_surface_save_draw(Graphics&);
bool pc_p2_surface_save_resume_scene();
bool pc_p2_surface_save_owns_heads();
bool pc_p2_surface_save_living_scene();
void pc_p2_surface_save_before_day_cleanup();
