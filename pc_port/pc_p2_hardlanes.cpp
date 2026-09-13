// Hard-lane shared registration seam (#244, #245, #246).
//
// Batch 2 step 1: wire the isolated hard-lane policy/arena modules into the
// shared game target with live host adapters. BombSarai is registered first: its
// carrier arena is bound to the live P1 static map through the lane's
// P2BombSaraiMapBinding terrain adapter and advanced on the authoritative frame
// delta via the lane's source clock. It only runs inside the private room
// preview and only when the opt-in `p2-bombsarai-arena.txt` profile is present,
// so ordinary P1 play is unaffected. Fuefuki/BigTreasure host adapters land in
// later slices.
//
// No shared semantics are changed: this module owns no saves, rewards, damage
// or actor lifetime. It is a debug/arena registration for runtime evidence.
#include "pc_p2_hardlanes.h"
#include "pc_p2_bombsarai_arena.h"
#include "pc_p2_bombsarai_bomb.h"
#include "pc_p2_bombsarai_clock.h"
#include "pc_p2_bombsarai_map_trace.h"
#include "pc_p2_bombsarai_terrain.h"
#include "pc_bbft.h"
#include "gameflow.h"
#include "MoviePlayer.h"
#include "Graphics.h"
#include "MapMgr.h"
#include "system.h"
#include <cstdint>
#include <cstdio>

namespace {
P2BombSaraiMapBinding* sBinding = nullptr;
P2BombSaraiTerrainAdapter* sAdapter = nullptr;
P2BombSaraiSourceClock* sClock = nullptr;
bool sReady = false;

bool sCarrierAlive(void*, std::uint64_t) { return true; }
}

void pc_p2_hardlanes_reset()
{
    pc_p2_bombsarai_arena_reset();
    if (sClock) sClock->reset();
    sReady = false;
}

void pc_p2_hardlanes_setup()
{
    pc_p2_hardlanes_reset();
    if (!pc_pikipelago_room_preview() || !mapMgr) return;
    if (!sBinding) sBinding = new P2BombSaraiMapBinding();
    if (!sAdapter) sAdapter = new P2BombSaraiTerrainAdapter();
    if (!sClock) sClock = new P2BombSaraiSourceClock();
    if (!pc_p2_bombsarai_arena_setup("p2-bombsarai-arena.txt")) return;
    sBinding->reset(mapMgr);
    sAdapter->reset(P2BombSaraiMapBinding::traceMove, sBinding,
                    P2BombSaraiMapBinding::getMinY, sBinding);
    sReady = true;
    std::printf("P2_HARDLANES_READY family=BombSarai arena=1\n");
}

void pc_p2_hardlanes_update()
{
    if (!sReady || !gsys) return;
    const bool active = !gameflow.mPauseAll && !gameflow.mIsUIOverlayActive
        && !(gameflow.mMoviePlayer && gameflow.mMoviePlayer->mIsActive);
    const int ticks = sClock->step(gsys->getFrameTime(), active);
    for (int i = 0; i < ticks; ++i)
        pc_p2_bombsarai_arena_update(P2BombSaraiBomb::kSourceDelta,
                                     P2BombSaraiTerrainAdapter::trace, sAdapter,
                                     sCarrierAlive, nullptr);
}

void pc_p2_hardlanes_draw(Graphics& gfx)
{
    if (!sReady) return;
    pc_p2_bombsarai_arena_draw(gfx);
}
