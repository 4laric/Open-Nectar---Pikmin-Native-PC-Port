#include "pc_p2_kurage_fx.h"

#include "EffectMgr.h"
#include "zen/particle.h"

#include <cstdio>
#include <cstdlib>

namespace {
// Lifetimes are generator frames (30/s). One burst per 30 Hz source tick, so a
// few frames of overlap read as a continuous upward stream while suction runs
// and nothing is left behind when it stops. One-shots self-terminate, so no
// generator handle is retained (a stored handle outlives its generator in the
// P1 pool, see pc_p2_groink_fx.cpp).
float knob(const char* name, float fallback)
{
    const char* v = std::getenv(name);
    if (!v || !*v) return fallback;
    return float(std::atof(v));
}
} // namespace

void pc_p2_kurage_fx_emit(const p2kuragefx::Command& c)
{
    if (c.kind != p2kuragefx::Kind::Suction || !effectMgr) return;
    static const float jetScale = knob("PIKMIN_P2_KURAGE_FX_SCALE", 1.0f);
    static const short jetLife = short(knob("PIKMIN_P2_KURAGE_FX_LIFE", 12.0f));
    static const float jetCount = knob("PIKMIN_P2_KURAGE_FX_COUNT", 1.0f);
    static const float dustOn = knob("PIKMIN_P2_KURAGE_FX_DUST", 1.0f);
    const Vector3f ground(c.x, c.y, c.z);
    zen::particleGenerator* jet = effectMgr->create(EffectMgr::EFF_Mar_WindJet, ground, nullptr, nullptr);
    if (jet) {
        // TAImar.cpp:206-210: emit along the nozzle axis, ground-up normal. The
        // nozzle here points straight up into the bell.
        jet->setEmitPos(ground);
        jet->setEmitDir(Vector3f(0.0f, 1.0f, 0.0f));
        jet->setOrientedNormalVector(Vector3f(0.0f, 1.0f, 0.0f));
        jet->setScaleSize(jetScale);
        jet->configureOneShotBurst(jetCount, jetLife);
    }
    if (dustOn > 0.0f) {
        zen::particleGenerator* dust = effectMgr->create(EffectMgr::EFF_Mar_WindDust, ground, nullptr, nullptr);
        if (dust) {
            dust->setOrientedNormalVector(Vector3f(0.0f, 1.0f, 0.0f));
            dust->configureOneShotBurst(1.0f, 10);
        }
    }
}
