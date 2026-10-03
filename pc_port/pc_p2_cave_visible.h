#pragma once
class Navi;
class Graphics;
// Returns true only when the ordinary action was consumed by the cave owner.
bool pc_p2_cave_visible_interact(Navi*);
void pc_p2_cave_visible_draw(Graphics&);
void pc_p2_cave_visible_reset();
