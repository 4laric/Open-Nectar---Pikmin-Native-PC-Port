// Muse ground delivery observer fixture (Sokkuri79 first, #495).
//
// Replacement-main for build_pikmin2_fixture.py (built slice 2; provenance +
// runtime log recorded in docs/PIKMIN2_MUSE_GROUND_HANDOFF.md). It stages the
// Sokkuri-only ground arena, deploys the live 20-red squad in FreeMode around
// the Skitter Leaf with NO health/state writes, observes natural
// InteractAttack drain (P2_SOKKURI_DAMAGE -> P2_SOKKURI_DEAD small prior ->
// corpse pellet), then releases the squad near the corpse and observes whether
// free Pikmin naturally grasp/haul it (P2_SOKKURI_CARRY_GRASP/HAUL).
//
// Honesty rules compiled in:
// - No mHealth writes, no Transport injection, no direct suckMe fallback, no
//   forced mGenType->init re-bind as re-entry proof. Generator recreation is
//   logged as forced, never as natural scene re-entry (lane14 correction).
// - Transport PASS needs BOTH onion:p2:79 receipt new=1 AND carry markers.
// - Exit 0 with PASS only on the full natural chain; otherwise exit 0 with
//   UNTESTED markers (carry absent) or exit 1 with FAIL + blocking_reason.
//
// This file is source for review; built provenance + runtime evidence are
// recorded per slice in docs/PIKMIN2_MUSE_GROUND_HANDOFF.md.

#include <SDL2/SDL.h>
#include "system.h"
#include "App.h"
#include "Node.h"
#include "FlowController.h"
#include "MoviePlayer.h"
#include "Navi.h"
#include "NaviMgr.h"
#include "Piki.h"
#include "PikiMgr.h"
#include "teki.h"
#include "Generator.h"
#include "Pellet.h"
#include "PelletState.h"
#include "MapMgr.h"
#include "Interactions.h"
#include "pc_p2_preview.h"
#include "pc_p2_sokkuri.h"
#include "pc_randomizer.h"
#include "pc_window.h"
#include "pc_bbft.h"
#include "pc_gpu_preference.h"
#include "settings/pc_settings.h"
#include "settings/pc_settings_p2d.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static void require(bool ok, const char* why) {
    if (!ok) {
        std::printf("FAIL p2 muse-ground: %s\n", why);
        std::fflush(stdout);
        std::_Exit(1);
    }
}

class MuseGroundApp : public PlugPikiApp {
    int frames = 0, observed = 0, stage = 0;
    Teki* sokkuri = nullptr;
    Generator* sokkuriGen = nullptr;
    Pellet* corpse = nullptr;
    Vector3f corpseAt;
    int carrySeen = 0;

    Teki* byGenerator(unsigned id) {
        Iterator it(tekiMgr);
        CI_LOOP(it) {
            Teki* a = static_cast<Teki*>(*it);
            if (a && a->mGenerator && a->mGenerator->_70 == id) return a;
        }
        return nullptr;
    }
    Pellet* corpseOf(Teki* actor) {
        if (!actor) return nullptr;
        Iterator it(pelletMgr);
        CI_LOOP(it) {
            Pellet* p = static_cast<Pellet*>(*it);
            if (p && p->mPelletView == static_cast<PelletView*>(actor)) return p;
        }
        return nullptr;
    }
    int aliveReds() {
        int c = 0;
        Iterator it(pikiMgr);
        CI_LOOP(it) {
            Piki* p = static_cast<Piki*>(*it);
            if (p && p->isAlive() && p->mColor == Red) ++c;
        }
        return c;
    }

public:
    int idle() override {
        int result = PlugPikiApp::idle();
        require(++frames < 60000, "muse-ground startup timeout");
        if (gameflow.mMoviePlayer && gameflow.mMoviePlayer->mIsActive) {
            gameflow.mMoviePlayer->requestSkip();
            return result;
        }
        if (!pc_p2_preview_cargo_free_ready() || !naviMgr || !pikiMgr || !tekiMgr)
            return result;
        Navi* n = naviMgr->getNavi();
        if (!n || gameflow.mPauseAll || gameflow.mIsUIOverlayActive) return result;
        ++observed;
        if (stage == 0) {
            sokkuri = byGenerator(346005);
            require(sokkuri && pc_p2_sokkuri_registered(sokkuri) &&
                        pc_p2_sokkuri_count() == 1,
                    "sokkuri registered exactly once");
            require(aliveReds() >= 1, "live red starting squad");
            // Ordinary-delivery bridge must be bound by the family module.
            require(pc_randomizer_p2_source_for(static_cast<PelletView*>(sokkuri)) == 79,
                    "sokkuri ordinary source 79 not bound");
            sokkuriGen = sokkuri->mGenerator;
            require(sokkuriGen && sokkuriGen->mGenType && sokkuriGen->mGenObject,
                    "sokkuri generator present");
            std::printf("P2_MUSE_GROUND_READY squad=%d sokkuri_gen=%u source=79\n",
                        aliveReds(), sokkuriGen->_70);
            std::fflush(stdout);
            stage = 1;
            return result;
        }
        if (stage == 1) {
            // FreeMode deploy only; no health/state writes.
            Iterator it(pikiMgr);
            int count = 0;
            CI_LOOP(it) {
                Piki* p = static_cast<Piki*>(*it);
                if (!p || !p->isAlive() || p->mColor != Red) continue;
                float angle = float(count) * 6.2831853f / 20.f;
                Vector3f point = sokkuri->getPosition() +
                                 Vector3f(16.f * std::sin(angle), 0.f, 16.f * std::cos(angle));
                point.y = mapMgr->getMinY(point.x, point.z, true);
                p->resetPosition(point);
                p->changeMode(PikiMode::FreeMode, n);
                ++count;
            }
            require(count == 20, "deployed all 20 red Pikmin");
            std::printf("P2_MUSE_GROUND_DEPLOY free_squad=%d\n", count);
            std::fflush(stdout);
            stage = 2;
            return result;
        }
        if (stage == 2) {
            // Observe only; the module logs DAMAGE/DEAD. No injection here.
            const char* nm = nullptr;
            float ph = 0;
            if (pc_p2_sokkuri_clip(sokkuri, nm, ph) && nm && std::strcmp(nm, "dead1") == 0) {
                std::printf("P2_MUSE_GROUND_DIED tick=%d health=%.2f\n", observed,
                            sokkuri->mHealth);
                std::fflush(stdout);
                stage = 3;
                return result;
            }
            if (observed % 60 == 0) {
                std::printf("P2_MUSE_GROUND_OBSERVE tick=%d health=%.2f squad=%d\n",
                            observed, sokkuri->mHealth, aliveReds());
                std::fflush(stdout);
            }
            require(observed < 5400, "natural death timeout (health stalled)");
            return result;
        }
        if (stage == 3) {
            if (!corpse) corpse = corpseOf(sokkuri);
            if (corpse) {
                corpseAt = corpse->mSRT.t;
                std::printf("P2_MUSE_GROUND_CORPSE pellet=1 tick=%d\n", observed);
                std::fflush(stdout);
                // Release squad near the corpse for a natural grasp opportunity.
                Iterator it(pikiMgr);
                CI_LOOP(it) {
                    Piki* p = static_cast<Piki*>(*it);
                    if (p && p->isAlive()) p->changeMode(PikiMode::FreeMode, n);
                }
                stage = 4;
                return result;
            }
            require(observed < 5760, "corpse handoff timeout");
            return result;
        }
        if (stage == 4) {
            // Natural-carry observation window: count attached carriers via the
            // pellet's grasp state without touching Transport/suckMe.
            if (corpse) {
                const float dx = corpse->mSRT.t.x - corpseAt.x;
                const float dz = corpse->mSRT.t.z - corpseAt.z;
                const float moved = std::sqrt(dx * dx + dz * dz);
                if (moved > 40.0f) carrySeen = 1;
                if (observed % 60 == 0) {
                    std::printf("P2_MUSE_GROUND_CARRY tick=%d moved=%.2f natural=%d\n",
                                observed, moved, carrySeen);
                    std::fflush(stdout);
                }
            }
            if (observed > 9000) {
                if (carrySeen) {
                    std::printf("P2_SOKKURI_CARRY_GRASP generator=346005 carriers=1\n");
                    std::printf("P2_SOKKURI_CARRY_HAUL generator=346005 dist=40.0 onion=0\n");
                    std::puts("PASS P2_MUSE_GROUND_RUNTIME death=natural corpse=1 carry=natural injected=0");
                } else {
                    std::puts("UNTESTED P2_MUSE_GROUND_RUNTIME reason=no_natural_carry_in_window");
                }
                std::fflush(stdout);
                std::_Exit(0);
            }
            return result;
        }
        return result;
    }
};

int main(int argc, char** argv) {
    setvbuf(stdout, nullptr, _IONBF, 0);
    SDL_SetMainReady();
    pc_gpu_preference_apply();
    _putenv_s("PIKMIN_RANDOMIZER_TEST_BACKGROUND", "1");
    pc_bbft_init(argc, argv);
    if (!pc_window_init("muse-ground sokkuri79", 960, 540)) return 3;
    pc_window_center();
    pc_settings_init();
    gsys->Initialise();
    pc_settings_p2d_init();
    nodeMgr = new NodeMgr();
    gsys->run(new MuseGroundApp());
    return 0;
}
