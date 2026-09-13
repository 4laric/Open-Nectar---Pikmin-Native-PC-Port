// Private real-GL BombSarai arena fixture. This file is compiled by the
// isolated fixture build only; it is not part of the game target.
// Requires user-supplied GPVE01 rev 0 assets; it never ships or shares them.
#include <SDL2/SDL.h>
#include <GL/gl.h>
#include "gl/pc_opengl.h"
#include "App.h"
#include "Node.h"
#include "Graphics.h"
#include "MapMgr.h"
#include "Navi.h"
#include "NaviMgr.h"
#include "MoviePlayer.h"
#include "Shape.h"
#include "system.h"
#include "pc_bbft.h"
#include "pc_gpu_preference.h"
#include "pc_window.h"
#include "pc_p2_preview.h"
#include "pc_p2_bombsarai_arena.h"
#include "pc_p2_bombsarai_clock.h"
#include "pc_p2_bombsarai_map_trace.h"
#include "pc_p2_bombsarai_terrain.h"
#include "settings/pc_settings.h"
#include "settings/pc_settings_p2d.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {
void require(bool value, const char* message)
{
    if (!value) { std::printf("FAIL BOMBSARAI_RUNTIME %s\n", message); std::fflush(stdout); std::_Exit(1); }
}

struct WallProbe { bool valid = false; P2BombSaraiVec3 center{}, velocity{}; };

bool carrierAlive(void*, std::uint64_t) { return true; }

class BombSaraiApp final : public PlugPikiApp {
    int frames = 0, sourceTicks = 0;
    bool setup = false, probes = false, supplied = false, thrown = false;
    bool blastSeen = false;
    P2BombSaraiMapBinding binding;
    P2BombSaraiTerrainAdapter adapter;
    P2BombSaraiSourceClock clock;
    WallProbe wall;
public:
    int idle() override {
        int result = PlugPikiApp::idle();
        require(++frames < 3600, "timeout");
        if (gameflow.mMoviePlayer && gameflow.mMoviePlayer->mIsActive) {
            clock.reset(); gameflow.mMoviePlayer->requestSkip(); return result;
        }
        if (!pc_p2_preview_ready() || !naviMgr || !naviMgr->getNavi() || gameflow.mPauseAll
            || gameflow.mIsUIOverlayActive) { clock.reset(); return result; }
        Navi* n = naviMgr->getNavi();
        if (!setup) {
            n->resetPosition(Vector3f(0, 0, -250)); n->mFaceDirection = 0; n->mSRT.r.set(0, 0, 0);
            binding.reset(mapMgr);
            adapter.reset(P2BombSaraiMapBinding::traceMove, &binding,
                          P2BombSaraiMapBinding::getMinY, &binding);
            require(pc_p2_bombsarai_arena_setup("p2-bombsarai-arena.txt"), "arena setup");
            setup = true;
        }
        if (!probes) { runProbes(); probes = true; std::puts("P2_BOMBSARAI_MAP_PROBES_PASS");
                       binding.reset(mapMgr); }
        const int ticks = clock.step(gsys->getFrameTime(), true);
        for (int i = 0; i < ticks; ++i) {
            ++sourceTicks;
            const bool supply = !supplied && sourceTicks >= 30; supplied |= supply;
            const bool throwNow = !thrown && sourceTicks >= 60; thrown |= throwNow;
            require(pc_p2_bombsarai_arena_update(P2BombSaraiBomb::kSourceDelta, supply,
                                                 P2BombSaraiThrowKind::Release, throwNow,
                                                 P2BombSaraiTerrainAdapter::trace, &adapter,
                                                 carrierAlive, nullptr), "arena update");
            if (supply) std::printf("P2_BOMBSARAI_SUPPLY source_tick=%d\n", sourceTicks);
            if (throwNow) std::printf("P2_BOMBSARAI_THROW source_tick=%d\n", sourceTicks);
            if (pc_p2_bombsarai_arena_blast_fired()) blastSeen = true;
        }
        if (blastSeen) {
            const P2BombSaraiRoutedHit* hits = pc_p2_bombsarai_arena_blast_hits();
            const int count = pc_p2_bombsarai_arena_blast_count();
            std::printf("P2_BOMBSARAI_BLAST ticks=%d traces=%llu floors=%llu walls=%llu hits=%d\n",
                        sourceTicks, (unsigned long long)binding.calls(),
                        (unsigned long long)binding.floors(), (unsigned long long)binding.walls(),
                        count);
            for (int i = 0; i < count; ++i) {
                std::printf("P2_BOMBSARAI_HIT id=%llu kind=%d damage=%.3f self=%d token=%llu\n",
                            (unsigned long long)hits[i].receiverId, (int)hits[i].kind,
                            hits[i].damage, hits[i].attributeToSelf ? 1 : 0,
                            (unsigned long long)hits[i].attackerToken);
            }
            std::puts("PASS BOMBSARAI_RUNTIME"); std::fflush(stdout); std::_Exit(0);
        }
        return result;
    }
    void draw(Graphics& gfx) override {
        PlugPikiApp::draw(gfx);
        if (setup) pc_p2_bombsarai_arena_draw(gfx);
    }
private:
    void runProbes() {
        require(mapMgr && mapMgr->mMapModel, "map unavailable");
        float ground = 0.0f;
        require(P2BombSaraiTerrainAdapter::getMinY(&adapter, 0, 0, ground), "center ground unavailable");
        P2BombSaraiTraceResult result{};
        // Flat-floor probe: downward trace lands, center rests at ground+radius.
        const float radius = 5.0f;
        require(P2BombSaraiTerrainAdapter::trace(&adapter, { 0, ground + 15, 0 }, { 0, -300, 0 },
                                                 P2BombSaraiBomb::kSourceDelta, radius, result),
                "center trace");
        std::printf("P2_BOMBSARAI_FLOOR_PROBE ground=%.6f center=%.6f floor=%d\n",
                    ground, result.position.y, result.floor ? 1 : 0);
        require(result.floor && result.hasGroundY, "center floor classification");
        require(std::fabs(result.position.y - ground) <= radius + 0.25f,
                "center floor conversion");
        // Free trace: no contact, no groundY sample needed.
        require(P2BombSaraiTerrainAdapter::trace(&adapter, { 0, ground + 100, 0 }, { 0, 0, 0 },
                                                 P2BombSaraiBomb::kSourceDelta, radius, result),
                "free trace");
        require(!result.floor && !result.wall, "free center collision");
        // Wall probe: find a vertical triangle away from the floor and hit it.
        Shape* model = mapMgr->mMapModel;
        for (int i = 0; i < model->mTriCount && !wall.valid; ++i) {
            const CollTriInfo& tri = model->mTriList[i];
            const Vector3f& a = model->mVertexList[tri.mVertexIndices[0]];
            const Vector3f& b = model->mVertexList[tri.mVertexIndices[1]];
            const Vector3f& c = model->mVertexList[tri.mVertexIndices[2]];
            Vector3f center((a.x + b.x + c.x) / 3.0f, (a.y + b.y + c.y) / 3.0f,
                            (a.z + b.z + c.z) / 3.0f);
            const Vector3f normal = tri.mTriangle.mNormal;
            float mapGround = 0.0f;
            if (!P2BombSaraiTerrainAdapter::getMinY(&adapter, center.x, center.z, mapGround)) continue;
            if (std::fabs(normal.y) < 0.05f && center.y > mapGround + 15) {
                wall = { true, { center.x + normal.x * 15, center.y + normal.y * 15,
                                 center.z + normal.z * 15 },
                         { -normal.x * 300, -normal.y * 300, -normal.z * 300 } };
            }
        }
        require(wall.valid, "no wall probe candidate");
        require(P2BombSaraiTerrainAdapter::trace(&adapter, wall.center, wall.velocity,
                                                 P2BombSaraiBomb::kSourceDelta, radius, result),
                "wall trace");
        require(result.wall, "wall probe did not hit");
        require(result.hasGroundY, "wall probe missing ground sample");
        std::puts("P2_BOMBSARAI_WALL_PROBE_PASS");
    }
};
}

int main(int argc, char** argv) {
    SDL_setenv("SDL_AUDIODRIVER", "dummy", 1); SDL_SetMainReady(); pc_gpu_preference_apply();
    _putenv_s("PIKMIN_RANDOMIZER_TEST_BACKGROUND", "1"); pc_bbft_init(argc, argv);
    require(pc_pikipelago_room_preview(), "requires --experimental-pikmin2-room");
    require(pc_window_init("BombSarai arena runtime fixture", 960, 720), "window init");
    pc_settings_init(); gsys->Initialise(); pc_settings_p2d_init(); nodeMgr = new NodeMgr();
    gsys->run(new BombSaraiApp()); return 0;
}
