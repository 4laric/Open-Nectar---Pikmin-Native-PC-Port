#include "pc_p2_sfx.h"
#include "SoundID.h"
#include "SoundMgr.h"
#include "jaudio/pikiinter.h"
#include <SDL2/SDL.h>
#include <cstdio>
#include <map>
#include <utility>

// The policy header mirrors SoundID.h so it can stay engine-free; a drift in
// either enum must fail the build rather than play the wrong sound.
static_assert(p2sfx::kChappySwing == SE_CHAPPY_SWING, "SE drift");
static_assert(p2sfx::kChappyFootDamage == SE_CHAPPY_FOOTDAMAGE, "SE drift");
static_assert(p2sfx::kFlogJump == SE_FLOG_JUMP, "SE drift");
static_assert(p2sfx::kFlogLand == SE_FLOG_LAND, "SE drift");
static_assert(p2sfx::kBomb == SE_BOMB, "SE drift");
static_assert(p2sfx::kMinicDie == SE_MINIC_DIE, "SE drift");
static_assert(p2sfx::kMinicAlert == SE_MINIC_ALERT, "SE drift");
static_assert(p2sfx::kSpiderWalk == SE_SPIDER_WALK, "SE drift");
static_assert(p2sfx::kSpiderSwing == SE_SPIDER_SWING, "SE drift");
static_assert(p2sfx::kSpiderDead == SE_SPIDER_DEAD, "SE drift");
static_assert(p2sfx::kTankFire == SE_TANK_FIRE, "SE drift");
static_assert(p2sfx::kTankBreath == SE_TANK_BREATH, "SE drift");
static_assert(p2sfx::kTankWalk == SE_TANK_WALK, "SE drift");
static_assert(p2sfx::kTankSwing == SE_TANK_SWING, "SE drift");
static_assert(p2sfx::kTankDamage == SE_TANK_DAMAGE, "SE drift");
static_assert(p2sfx::kTankDead1 == SE_TANK_DEAD1, "SE drift");
static_assert(p2sfx::kMushSpore == SE_MUSH_SPORE, "SE drift");
static_assert(p2sfx::kKabutoShot == SE_KABUTO_SHOT, "SE drift");
static_assert(p2sfx::kKabutoFlip == SE_KABUTO_FLIP, "SE drift");
static_assert(p2sfx::kKabutoWalk == SE_KABUTO_WALK, "SE drift");
static_assert(p2sfx::kKabutoDead == SE_KABUTO_DEAD, "SE drift");
static_assert(p2sfx::kRockRoll == SE_ROCK_ROLL, "SE drift");
static_assert(p2sfx::kRockBreak == SE_ROCK_BREAK, "SE drift");
static_assert(p2sfx::kCollecPull == SE_COLLEC_PULL, "SE drift");
static_assert(p2sfx::kCollecWalk == SE_COLLEC_WALK, "SE drift");
static_assert(p2sfx::kCollecDead == SE_COLLEC_DEAD, "SE drift");
static_assert(p2sfx::kCollecDown == SE_COLLEC_DOWN, "SE drift");
static_assert(p2sfx::kCollecCry == SE_COLLEC_CRY, "SE drift");
static_assert(p2sfx::kCollecDamage == SE_COLLEC_DAMAGE, "SE drift");
static_assert(p2sfx::kKoganeWalk == SE_KOGANE_WALK, "SE drift");
static_assert(p2sfx::kKoganeDamage == SE_KOGANE_DAMAGE, "SE drift");
static_assert(p2sfx::kSaraiHover == SE_SARAI_HOVER, "SE drift");
static_assert(p2sfx::kSaraiDamage == SE_SARAI_DAMAGE, "SE drift");
static_assert(p2sfx::kSaraiAttack == SE_SARAI_ATTACK, "SE drift");
static_assert(p2sfx::kSaraiDead == SE_SARAI_DEAD, "SE drift");
static_assert(p2sfx::kMarDead1 == SE_MAR_DEAD1, "SE drift");
static_assert(p2sfx::kKurioneWater == SE_KURIONE_WATER, "SE drift");

namespace {
struct Actor {
    p2sfx::ActorGate gate;
    p2sfx::Stride stride;
};
std::map<std::pair<unsigned, unsigned>, Actor> gActors;

Actor& actorFor(unsigned sourceId, unsigned token) { return gActors[std::make_pair(sourceId, token)]; }

// Wall clock in seconds. Output-only: it never feeds the sim.
float nowSeconds() { return float(SDL_GetTicks()) * 0.001f; }
} // namespace

int pc_p2_sfx(unsigned sourceId, unsigned token, p2sfx::Event event, const Vector3f& position)
{
    if (!seSystem) return p2sfx::kNone;
    bool logIt = false;
    const int se = actorFor(sourceId, token).gate.admit(sourceId, event, nowSeconds(), &logIt);
    if (se == p2sfx::kNone) return p2sfx::kNone;
    // playSoundDirect reuses the nearest same-type context within 200 units
    // (or the next system context), so a burst of P2 events never exhausts
    // the 16 Jac event slots on its own.
    seSystem->playSoundDirect(JACEVENT_Battle, se, position);
    if (logIt) {
        char line[128];
        p2sfx::formatMarker(line, sizeof line, sourceId, token, event, se);
        std::printf("%s x=%.1f y=%.1f z=%.1f\n", line, position.x, position.y, position.z);
    }
    return se;
}

void pc_p2_sfx_stride(unsigned sourceId, unsigned token, const Vector3f& position, float strideLength)
{
    Actor& a = actorFor(sourceId, token);
    if (a.stride.advance(position.x, position.z, strideLength)) pc_p2_sfx(sourceId, token, p2sfx::Event::Step, position);
}

void pc_p2_sfx_forget(unsigned sourceId, unsigned token)
{
    for (auto it = gActors.begin(); it != gActors.end();) {
        if (it->first.first == sourceId && (token == 0 || it->first.second == token)) it = gActors.erase(it);
        else ++it;
    }
}
