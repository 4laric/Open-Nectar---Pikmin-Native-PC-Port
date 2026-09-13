#pragma once

#include "pc_p2_bombsarai_bomb.h"
#include "pc_p2_bombsarai_blast.h"
#include "pc_p2_bombsarai_hover.h"
#include "pc_p2_bombsarai_terrain.h"

class Graphics;

// Lane-owned registration/binding seam for the Careening Dirigibug lane
// (#244): one stationary, host-driven BombSarai arena loaded from an opt-in
// install profile (P2_BOMBSARAI_ARENA_1) per the #186 shared arena contract.
// Pinned placement only: the profile fixes the carrier hover position/yaw,
// bomb parameters, and a static receiver list. There is no actor registry,
// AI, FSM, converted visual asset, sound, or effect integration in this
// module; those remain later slices and root-serialized work.

// Parses and installs the profile. Returns false (and installs nothing) on
// any invalid content.
bool pc_p2_bombsarai_arena_setup(const char* profilePath);
void pc_p2_bombsarai_arena_reset();

// Processes exactly one source 30 Hz update: hover vertical control, then
// the held bomb (if any) through the required trace callback. A non-null
// trace is required; there is no no-trace fallback inside the arena. The
// traceContext must be a P2BombSaraiTerrainAdapter whose primitives are
// bound (the arena reuses its getMinY for hover terrain sampling).
// supplyEvent captures a pooled bomb at the pinned carrier joint position;
// throwEvent releases it with the given kind and the pinned carrier yaw.
// Events are explicit harness controls until the real FSM is implemented.
// Returns false if the arena is unavailable or input/trace failed.
bool pc_p2_bombsarai_arena_update(float sourceDelta, bool supplyEvent,
                                  P2BombSaraiThrowKind throwKind, bool throwEvent,
                                  P2BombSaraiTraceFn trace, void* traceContext,
                                  P2BombSaraiCarrierFn carrier, void* carrierContext);

// Latest routed blast hits from the pinned receiver list. blast_fired()
// latches true on the first detonation (even with zero in-volume hits) and
// stays latched until arena reset; the hit array is valid for that blast.
bool pc_p2_bombsarai_arena_blast_fired();
int pc_p2_bombsarai_arena_blast_count();
const P2BombSaraiRoutedHit* pc_p2_bombsarai_arena_blast_hits();

// Debug drawing only: carrier hover marker, held/in-flight bomb sphere, and
// a one-shot blast volume marker. No converted BombSarai assets exist yet
// (#128); nothing here claims visual fidelity.
void pc_p2_bombsarai_arena_draw(Graphics& gfx);
