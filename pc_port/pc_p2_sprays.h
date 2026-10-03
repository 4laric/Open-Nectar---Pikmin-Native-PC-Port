#pragma once
class Piki;
class PikiState;
class Navi;
namespace p2originalresource { class ResourceState; }
// The original campaign owner binds its single authoritative inventory only
// after resource/receiver installation; nullptr detaches before destruction.
void pc_p2_sprays_bind(p2originalresource::ResourceState*);
bool pc_p2_sprays_input(Navi*);
bool pc_p2_spicy_use(Navi*);
bool pc_p2_spicy_accept(Piki*);
bool pc_p2_spicy_active(const Piki*);
void pc_p2_spicy_tick(Piki*);
PikiState* pc_p2_spicy_state();
