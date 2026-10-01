#pragma once
// Shared P2 Bomb (bomb-rock) visuals: the bomb's own countdown life-gauge wheel
// and the P1 bomb-rock shape draw with the lit-fuse flash. Used by the Volatile
// Dweevil carrier (pc_p2_otakara.cpp) and meant for any other P2 actor that
// carries or drops a Bomb, e.g. the Careening Dirigibug bombs
// (pc_p2_bombsarai_own_teki.cpp). Output-only: nothing here touches sim state.
//
// Policy (engine-free, unit tested): pc_p2_bomb_telegraph.h. Source truth: the
// Bomb enemy drains mHealth by the frame time while lit (bombState.cpp:113-122),
// its life gauge is the ordinary EnemyBase gauge (enemyBase.cpp:2692-2710,
// ratio mHealth/mMaxHealth at fp27 = 35), the retail Bomb life is 4.5 s.
#include "pc_p2_bomb_telegraph.h"
#include "LifeGauge.h"

class Graphics;
class Matrix4f;
class Vector3f;

// One countdown wheel. Keep one per bomb; call draw() from a 2D pass
// (BTeki::refresh2d for a Teki carrier).
struct P2BombGauge {
    LifeGauge gauge;
    bool ready = false;
    // Draws the P1 life-gauge wheel (Wheel style, snap to target) above
    // bombWorldPos. health counts down from maxHealth (p2bombtelegraph::
    // kBombLife for a stock Bomb). Green at full, red near empty, like every
    // other enemy gauge.
    void draw(Graphics& gfx, const Vector3f& bombWorldPos, float health,
              float maxHealth = p2bombtelegraph::kBombLife);
};

// Draws the P1 bomb-rock shape (objects/bomb/bomb.mod, ItemMgr::mItemShapes[2])
// with view as its view-space matrix (camera look-at * the bomb world matrix).
// While flashing, the bomb materials are tinted by
// p2bombtelegraph::flashTint(flashOn, ratio) for this draw only and restored
// after, because the shape is shared with real P1 bomb rocks. Returns false when
// the shape is not loaded.
bool pc_p2_bomb_draw_shape(Graphics& gfx, const Matrix4f& view, bool flashing, bool flashOn, float ratio);
