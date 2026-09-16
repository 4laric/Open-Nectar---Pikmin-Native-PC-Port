// Private sustained-flight fixture (Muse l65, Honeywisp EnemyID 16).
//
// Gate-2 closer attempt: the port re-appears only while a Pikmin/Navi is
// inside sight (SIGHT 200, pc_p2_qurione.cpp) but drops on contact inside
// HIT_RADIUS 30. This fixture parks every red in the sight ring around the
// wisp's current XZ (distXZ 60..100: visible, untouchable) and keeps the
// captain near, so Stay re-triggers across cycles without tripping Drop.
// It counts move->still transitions as flight cycles and emits
// P2_QURIONE_MUSE_* markers for experimental/pikmin2_muse_honeywisp.py.
// No health is written; all P2_QURIONE_* markers come from production.
// Window: 960x540 centred (fixture baseline); silent audio for unattended runs.
#include <SDL2/SDL.h>
#include "App.h"
#include "Node.h"
#include "Generator.h"
#include "MapMgr.h"
#include "NaviMgr.h"
#include "Navi.h"
#include "NaviState.h"
#include "Piki.h"
#include "PikiMgr.h"
#include "PikiState.h"
#include "GlobalGameOptions.h"
#include "MoviePlayer.h"
#include "pc_bbft.h"
#include "pc_gpu_preference.h"
#include "pc_p2_preview.h"
#include "teki.h"
#include "pc_window.h"
#include "settings/pc_settings.h"
#include "settings/pc_settings_p2d.h"
#include "system.h"
#include "gameflow.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace {
void require(bool value, const char* message)
{
    if (!value) { std::printf("FAIL QURIONE_MUSE %s\n", message); std::fflush(stdout); std::_Exit(1); }
}

// Sight ring: inside SIGHT=200, outside HIT_RADIUS=30, with margin.
constexpr float RING_INNER = 60.0f;
constexpr float RING_STEP = 40.0f;
constexpr int WANT_CYCLES = 2;

class QurioneMuseApp final : public PlugPikiApp {
    int frames = 0;
    bool settled = false;
    bool sawMove = false;
    int cycles = 0;
public:
    Teki* findWisp() {
        Iterator tit(tekiMgr); CI_LOOP(tit) {
            Teki* t = static_cast<Teki*>(*tit);
            if (t && t->mTekiType == TEKI_Qurione && t->mGenerator && t->mGenerator->_70 == 203001u) return t;
        }
        return nullptr;
    }
    void parkRing(const Vector3f& center) {
        int i = 0;
        Iterator pit(pikiMgr); CI_LOOP(pit) {
            Piki* p = static_cast<Piki*>(*pit);
            if (p && p->isAlive()) {
                const float angle = float(i) * 2.39996f; // golden angle spread
                const float radius = RING_INNER + float(i % 2) * RING_STEP;
                const float x = center.x + radius * std::sin(angle);
                const float z = center.z + radius * std::cos(angle);
                p->resetPosition(Vector3f(x, mapMgr->getMinY(x, z, true), z));
                ++i;
            }
        }
    }
    int idle() override {
        int result = PlugPikiApp::idle();
        require(++frames < 3600, "timeout");
        if (gameflow.mMoviePlayer && gameflow.mMoviePlayer->mIsActive) {
            gameflow.mMoviePlayer->requestSkip();
            return result;
        }
        if (!pc_pikipelago_room_preview() || !naviMgr || !naviMgr->getNavi() || gameflow.mPauseAll || gameflow.mIsUIOverlayActive) return result;
        Navi* n = naviMgr->getNavi();

        Teki* wisp = findWisp();
        if (!wisp) {
            std::printf("P2_QURIONE_MUSE_DONE cycles=%d reason=wisp-gone\n", cycles);
            std::fflush(stdout);
            std::_Exit(cycles >= WANT_CYCLES ? 0 : 1);
        }
        const Vector3f p = wisp->mSRT.t;
        const float speed = wisp->mVelocity.length();

        if (!settled) {
            parkRing(p);
            n->resetPosition(Vector3f(p.x + RING_INNER, mapMgr->getMinY(p.x + RING_INNER, p.z, true), p.z));
            std::printf("P2_QURIONE_MUSE_SETTLE ring_parked=1 captain_near=1\n");
            std::fflush(stdout);
            settled = true;
            return result;
        }
        // Re-park every 300 frames: the squad mingles, and mingling out of
        // sight is exactly what stalled lane-15's re-appear.
        if (frames % 300 == 0) parkRing(p);
        if (speed > 20.0f) {
            if (!sawMove) {
                std::printf("P2_QURIONE_MUSE_CYCLE index=%d pos=(%.2f,%.2f,%.2f)\n",
                            cycles, p.x, p.y, p.z);
                std::fflush(stdout);
            }
            sawMove = true;
        } else if (sawMove && speed < 1.0f) {
            sawMove = false;
            ++cycles;
            std::printf("P2_QURIONE_MUSE_STILL cycles=%d pos=(%.2f,%.2f,%.2f)\n",
                        cycles, p.x, p.y, p.z);
            std::fflush(stdout);
            if (cycles >= WANT_CYCLES) {
                std::printf("P2_QURIONE_MUSE_DONE cycles=%d reason=target-met\n", cycles);
                std::fflush(stdout);
                std::_Exit(0);
            }
        }
        return result;
    }
};
}

int main(int argc, char** argv)
{
    SDL_setenv("SDL_AUDIODRIVER", "dummy", 1); SDL_SetMainReady();
    pc_gpu_preference_apply(); _putenv_s("PIKMIN_RANDOMIZER_TEST_BACKGROUND", "1"); pc_bbft_init(argc, argv);
    require(pc_pikipelago_room_preview(), "requires --experimental-pikmin2-room");
    if (!pc_window_init("P2 Qurione muse fixture", 960, 540)) return 3;
    pc_window_center();
    pc_settings_init(); gsys->Initialise(); pc_settings_p2d_init(); nodeMgr = new NodeMgr();
    gsys->run(new QurioneMuseApp());
    return 0;
}
