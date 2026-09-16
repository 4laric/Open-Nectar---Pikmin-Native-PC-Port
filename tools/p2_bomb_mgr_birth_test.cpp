// Bomb::Mgr birth-seam fixture + standalone unit test (issue #616).
//
// Build modes (documented, selected by define):
//   (1) Standalone unit test (no engine, no lease):
//         g++ -std=c++17 -Wall -Wextra -Ipc_port -DP2_BOMB_MGR_BIRTH_UNITTEST
//             tools/p2_bomb_mgr_birth_test.cpp pc_port/pc_p2_bombsarai_blast.cpp
//             -o p2_bomb_mgr_birth_test && ./p2_bomb_mgr_birth_test
//       Unity-includes the engine-free manager core + #577 payload TU and runs
//       behavior checks through the REAL p2_bombsarai_route_blast.
//   (2) Replacement-main live fixture (leased build + provenance):
//         built by scripts/build_pikmin2_fixture.py (define unset); App drives
//         the manager against LIVE host carrier actors with captain-guard
//         adoption. Sidecar p2-bomb-mgr-birth.txt registers carriers; absent
//         sidecar keeps the manager inert. Staged squad/captain positions are
//         labeled STAGED; birth/follow/reset/rebirth markers are production.
#ifdef P2_BOMB_MGR_BIRTH_UNITTEST
#define P2_BOMB_MGR_BIRTH_NO_HOST
#include "pc_p2_bomb_mgr_birth.cpp"
// Unity-include the #577 payload TU: it is not in the main build object
// list, so no duplicate symbols arise (blast TU stays linked from the
// main build). The standalone unit-test build links all three TUs.
#include "pc_p2_bomb_payload_actor.cpp"

#include <cmath>
#include <cstdio>

namespace {

int failures = 0;
int checks = 0;

void check(bool condition, const char* name)
{
    ++checks;
    if (!condition) {
        ++failures;
        std::printf("FAIL %s\n", name);
    }
}

P2BombSaraiVec3 joint(float x, float y, float z)
{
    P2BombSaraiVec3 v;
    v.x = x;
    v.y = y;
    v.z = z;
    return v;
}

bool carrierLive(void*, std::uint64_t token) { return token != 0; }
bool carrierDead(void*, std::uint64_t) { return false; }

} // namespace

int main()
{
    // 1. Registration + unregistered-ID rejection + bad-input refusal.
    {
        P2BombMgr mgr(2);
        check(mgr.registeredCount() == 0, "start-unregistered");
        P2BombPayloadConfig config;
        P2BombMgrHandle bad = mgr.birth(7, joint(0, 0, 0), config);
        check(!p2_bomb_mgr_handle_valid(bad), "unregistered-rejected");
        check(!p2_bomb_mgr_handle_valid(mgr.findLive(7)), "findLive-empty");
        mgr.registerCarrier(7);
        mgr.registerCarrier(7);
        check(mgr.registeredCount() == 1, "register-idempotent");
        check(mgr.isRegistered(7), "is-registered");
        P2BombSaraiVec3 nan = joint(0, 0, 0);
        nan.y = std::nanf("");
        bad = mgr.birth(7, nan, config);
        check(!p2_bomb_mgr_handle_valid(bad), "nonfinite-joint-refused");
        bad = mgr.birth(0, joint(0, 0, 0), config);
        check(!p2_bomb_mgr_handle_valid(bad), "zero-token-refused");
        P2BombPayloadConfig badConfig;
        badConfig.blastRadius = 0.0f;
        bad = mgr.birth(7, joint(0, 0, 0), badConfig);
        check(!p2_bomb_mgr_handle_valid(bad), "bad-config-refused");
    }

    // 2. Birth, live handle, duplicate guard, exhaustion, follow.
    {
        P2BombMgr mgr(2);
        mgr.registerCarrier(7);
        mgr.registerCarrier(8);
        P2BombPayloadConfig config;
        P2BombMgrHandle h = mgr.birth(7, joint(1, 2, 3), config);
        check(p2_bomb_mgr_handle_valid(h), "birth-valid");
        check(mgr.isLive(h), "birth-live");
        check(mgr.phase(h) == P2BombMgrPhase::Born, "birth-born");
        check(mgr.carrierToken(h) == 7, "birth-carrier");
        check(mgr.activeCount() == 1, "birth-active-count");
        check(p2_bomb_payload_handle_valid(mgr.payloadHandle(h)), "payload-handoff-valid");
        P2BombMgrHandle found = mgr.findLive(7);
        check(p2_bomb_mgr_handle_valid(found) && found.slot == h.slot
                  && found.generation == h.generation,
              "findLive-match");
        P2BombMgrHandle dup = mgr.birth(7, joint(4, 5, 6), config);
        check(!p2_bomb_mgr_handle_valid(dup), "duplicate-carrier-refused");
        P2BombMgrHandle h2 = mgr.birth(8, joint(4, 5, 6), config);
        check(p2_bomb_mgr_handle_valid(h2), "second-birth-valid");
        P2BombMgrHandle full = mgr.birth(9, joint(0, 0, 0), config);
        check(!p2_bomb_mgr_handle_valid(full), "unregistered-or-exhausted-refused");
        check(mgr.followCarrier(h, joint(5, 6, 7)), "follow-ok");
        P2BombSaraiVec3 at = mgr.position(h);
        check(at.x == 5.0f && at.y == 6.0f && at.z == 7.0f, "follow-moved");
        P2BombSaraiVec3 nan = joint(0, 0, 0);
        nan.z = std::nanf("");
        check(!mgr.followCarrier(h, nan), "follow-nonfinite-rejected");
        at = mgr.position(h);
        check(at.x == 5.0f, "follow-kept-position");
    }

    // 3. Carrier-gone releases without blast; stale handle stays dead.
    {
        P2BombMgr mgr(1);
        mgr.registerCarrier(7);
        P2BombPayloadConfig config;
        P2BombMgrHandle h = mgr.birth(7, joint(1, 2, 3), config);
        check(mgr.onCarrierGone(h), "carrier-gone-released");
        check(!mgr.isLive(h), "gone-not-live");
        check(mgr.phase(h) == P2BombMgrPhase::Lost, "gone-lost");
        check(mgr.blastCount() == 0, "gone-no-blast");
        check(!mgr.onCarrierGone(h), "gone-again-rejected");
        // No re-arm of the lost record: like the owned pool, slots free only
        // on reset(). A fresh lifecycle needs reset() (or a new carrier).
        P2BombMgrHandle h2 = mgr.birth(7, joint(1, 2, 3), config);
        check(!p2_bomb_mgr_handle_valid(h2), "lost-slot-not-reused");
        mgr.reset();
        mgr.registerCarrier(7);
        P2BombMgrHandle h3 = mgr.birth(7, joint(1, 2, 3), config);
        check(p2_bomb_mgr_handle_valid(h3) && mgr.isLive(h3), "rearm-after-reset");
    }

    // 4. Detonation exactly-once through the REAL routing fn + receiver routing.
    {
        P2BombMgr mgr(1);
        mgr.registerCarrier(7);
        P2BombPayloadConfig config;
        P2BombMgrHandle h = mgr.birth(7, joint(0, 0, 0), config);
        P2BombSaraiReceiver receivers[2];
        receivers[0].id = 101;
        receivers[0].position = joint(10, 0, 0);
        receivers[0].kind = P2BombSaraiReceiverKind::Teki;
        receivers[1].id = 202;
        receivers[1].position = joint(1000, 0, 0);
        receivers[1].kind = P2BombSaraiReceiverKind::Piki;
        P2BombSaraiRoutedHit hits[2];
        const int n = mgr.detonate(h, P2BombPayloadTrigger::Contact, carrierLive, nullptr,
                                   receivers, 2, hits, 2);
        check(n == 1, "detonate-one-hit");
        check(n > 0 && hits[0].receiverId == 101, "detonate-near-receiver");
        check(mgr.blastCount() == 1, "detonate-blast-count");
        check(!mgr.isLive(h), "detonated-not-live");
        check(mgr.phase(h) == P2BombMgrPhase::Detonated, "detonated-phase");
        const int dup = mgr.detonate(h, P2BombPayloadTrigger::Contact, carrierLive, nullptr,
                                     receivers, 2, hits, 2);
        check(dup == 0, "duplicate-detonate-suppressed");
        check(mgr.suppressedCount() >= 1, "suppressed-counted");
        check(mgr.blastCount() == 1, "no-double-blast");
        check(mgr.detonate(P2BombMgrHandle{}, P2BombPayloadTrigger::Contact, carrierLive,
                           nullptr, receivers, 2, hits, 2)
                  == -1,
              "detonate-invalid-handle");
    }

    // 5. Reset retires handles (stale/recycled rejection) and re-arms.
    {
        P2BombMgr mgr(1);
        mgr.registerCarrier(7);
        P2BombPayloadConfig config;
        P2BombMgrHandle h = mgr.birth(7, joint(1, 2, 3), config);
        check(mgr.isLive(h), "pre-reset-live");
        mgr.reset();
        check(!mgr.isLive(h), "stale-handle-dead-after-reset");
        check(mgr.phase(h) == P2BombMgrPhase::Free, "stale-handle-free");
        check(mgr.registeredCount() == 0, "reset-clears-registration");
        check(!mgr.followCarrier(h, joint(0, 0, 0)), "stale-follow-rejected");
        check(mgr.detonate(h, P2BombPayloadTrigger::Death, carrierDead, nullptr, nullptr, 0,
                           nullptr, 0)
                  == -1,
              "stale-detonate-rejected");
        mgr.registerCarrier(7);
        P2BombMgrHandle h2 = mgr.birth(7, joint(1, 2, 3), config);
        check(p2_bomb_mgr_handle_valid(h2) && mgr.isLive(h2), "rebirth-after-reset");
    }

    // 6. Death trigger through the owned pool's carrier-death seam (the same
    // pool instance family the manager owns; attribution via live carrier).
    {
        P2BombPayloadPool pool(1);
        P2BombPayloadConfig config;
        P2BombPayloadHandle payload = pool.birth(7, joint(0, 0, 0), config);
        check(p2_bomb_payload_handle_valid(payload), "death-seam-born");
        check(pool.onCarrierDeath(payload, carrierLive, nullptr), "carrier-death-detonates");
        check(pool.blastCount() == 1, "carrier-death-blast");
        const P2BombSaraiBlastEvent& blast = pool.lastBlast(payload);
        check(blast.carrierValid, "carrier-death-attributed");
    }

    std::printf("P2_BOMB_MGR_UNITTEST checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}

#else
// ---- Replacement-main live fixture (full engine; leased build) ----
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
#include "GameStat.h"
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
#include "pc_p2_bomb_mgr_birth.cpp"
// Unity-include the #577 payload TU: it is not in the main build object
// list, so no duplicate symbols arise (blast TU stays linked from the
// main build). The standalone unit-test build links all three TUs.
#include "pc_p2_bomb_payload_actor.cpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>

// Equivalent of scripts/p2_fixture_captain_guard.h (canonical sha recorded in
// the provider doc). scripts/ is not on the native include path, so the
// fixture embeds the identical 3-signal check verbatim instead of including
// it; behavior parity is asserted by the standalone negative-guard log line.
inline bool p2_bomb_mgr_guard_down(bool orimaDead, bool deadState, float hp)
{
    return orimaDead || deadState || !std::isfinite(hp) || hp <= 1.0f;
}

namespace {
void require(bool value, const char* message)
{
    if (!value) {
        std::printf("FAIL BOMB_MGR_BIRTH %s\n", message);
        std::fflush(stdout);
        std::_Exit(1);
    }
}
void requireFinite(float v, const char* message)
{
    require(std::isfinite(v), message);
}

class BombMgrBirthApp final : public PlugPikiApp {
    enum Phase { SETTLE, BIRTH, FOLLOW, RESET, REBIRTH, DONE };
    int frames = 0;
    int observed = 0;
    Phase phase = SETTLE;
    unsigned carrier = 0;
    bool stagedFallback = false;
    int posMarks = 0;
    int rebirthPosMarks = 0;
    bool rejectLogged = false;

    Teki* findCarrierActor()
    {
        // Prefer the sidecar-registered carrier; else the first live host
        // teki (STAGED fallback: carrier selection staged, birth path real).
        P2BombMgr& mgr = pc_p2_bomb_mgr_birth_manager();
        Iterator it(tekiMgr);
        CI_LOOP(it) {
            Teki* t = static_cast<Teki*>(*it);
            if (t && t->isAlive() && t->mGenerator && mgr.isRegistered(t->mGenerator->_70)) {
                stagedFallback = false;
                return t;
            }
        }
        CI_LOOP(it) {
            Teki* t = static_cast<Teki*>(*it);
            if (t && t->isAlive() && t->mGenerator) {
                stagedFallback = true;
                return t;
            }
        }
        return nullptr;
    }

    int idle() override
    {
        int result = PlugPikiApp::idle();
        require(++frames < 5400, "timeout");
        // Captain guard FIRST: before pause/movie/UI early returns, readiness
        // gates, observation counters or PASS markers (fanout #632).
        Navi* guardNavi = (naviMgr) ? naviMgr->getNavi() : nullptr;
        if (guardNavi) {
            const bool deadState = guardNavi->getCurrState()
                && guardNavi->getCurrState()->getID() == NAVISTATE_Dead;
            if (p2_bomb_mgr_guard_down(GameStat::orimaDead, deadState, guardNavi->mHealth)) {
                std::printf("P2_FIXTURE_CAPTAIN_DOWN tick=%d outcome=BLOCKED\n", observed);
                std::fflush(stdout);
                std::_Exit(86);
            }
        }
        if (gameflow.mMoviePlayer && gameflow.mMoviePlayer->mIsActive) {
            gameflow.mMoviePlayer->requestSkip();
            return result;
        }
        if (!pc_pikipelago_room_preview() || !naviMgr || !naviMgr->getNavi() || gameflow.mPauseAll
            || gameflow.mIsUIOverlayActive) {
            return result;
        }
        Navi* n = naviMgr->getNavi();
        P2BombMgr& mgr = pc_p2_bomb_mgr_birth_manager();

        if (phase == SETTLE) {
            // STAGED: park the squad far from any carrier and the captain out
            // of blast reach (radius 90); captain damage is not the subject.
            Iterator pit(pikiMgr);
            CI_LOOP(pit) {
                Piki* p = static_cast<Piki*>(*pit);
                if (p && p->isAlive()) {
                    p->resetPosition(Vector3f(206.0f, mapMgr->getMinY(206.0f, 1858.0f, true), 1858.0f));
                }
            }
            n->resetPosition(Vector3f(-420.0f, mapMgr->getMinY(-420.0f, 1500.0f, true), 1500.0f));
            pc_p2_bomb_mgr_birth_setup();
            std::printf("P2_BOMB_MGR_SETTLE reds_parked=1 captain_staged=1 ready=%d\n",
                        int(pc_p2_bomb_mgr_birth_ready()));
            std::fflush(stdout);
            phase = BIRTH;
            return result;
        }
        if (phase == DONE) return result;

        Teki* actor = findCarrierActor();
        if (!actor) return result;
        const unsigned gen = actor->mGenerator->_70;
        const Vector3f p = actor->mSRT.t;
        requireFinite(p.x, "carrier-nan-x");
        requireFinite(p.y, "carrier-nan-y");
        requireFinite(p.z, "carrier-nan-z");

        if (phase == BIRTH) {
            if (!mgr.isRegistered(gen)) {
                // Negative test (labelled): unregistered-ID rejection first.
                const P2BombMgrHandle rejected = mgr.birth(gen, {p.x, p.y, p.z}, P2BombPayloadConfig{});
                require(!p2_bomb_mgr_handle_valid(rejected), "unregistered-not-rejected-live");
                if (!rejectLogged) {
                    rejectLogged = true;
                    std::printf("P2_BOMB_MGR_NEGATIVE backed_by=manager reason=unregistered\n");
                    std::fflush(stdout);
                }
                mgr.registerCarrier(gen);
            }
            carrier = gen;
            const P2BombMgrHandle h = pc_p2_bomb_mgr_birth_carrier(gen);
            require(p2_bomb_mgr_handle_valid(h), "live-birth-failed");
            require(mgr.isLive(h), "live-birth-not-live");
            std::printf("P2_BOMB_MGR_STAGED carrier=%u fallback=%d\n", gen, int(stagedFallback));
            std::fflush(stdout);
            phase = FOLLOW;
            return result;
        }
        if (phase == FOLLOW) {
            pc_p2_bomb_mgr_birth_update(actor);
            const P2BombMgrHandle h = mgr.findLive(gen);
            require(p2_bomb_mgr_handle_valid(h) && mgr.isLive(h), "follow-lost-live");
            ++observed;
            // POS markers come from the production update path (1 Hz).
            if (observed % 60 == 0) ++posMarks;
            if (posMarks >= 3) {
                pc_p2_bomb_mgr_birth_reset();
                std::printf("P2_BOMB_MGR_REENTRY reset=1\n");
                std::fflush(stdout);
                phase = RESET;
            }
            return result;
        }
        if (phase == RESET) {
            require(!p2_bomb_mgr_handle_valid(mgr.findLive(gen)), "reset-did-not-retire");
            mgr.registerCarrier(gen);
            const P2BombMgrHandle h = pc_p2_bomb_mgr_birth_carrier(gen);
            require(p2_bomb_mgr_handle_valid(h) && mgr.isLive(h), "rebirth-failed");
            std::printf("P2_BOMB_MGR_REBIRTH carrier=%u\n", gen);
            std::fflush(stdout);
            phase = REBIRTH;
            return result;
        }
        if (phase == REBIRTH) {
            pc_p2_bomb_mgr_birth_update(actor);
            const P2BombMgrHandle h = mgr.findLive(gen);
            require(p2_bomb_mgr_handle_valid(h) && mgr.isLive(h), "rebirth-lost-live");
            ++observed;
            if (observed % 60 == 0) ++rebirthPosMarks;
            if (rebirthPosMarks >= 3) {
                std::printf("P2_BOMB_MGR_DONE carrier=%u staged_fallback=%d\n", gen,
                            int(stagedFallback));
                std::fflush(stdout);
                phase = DONE;
                std::_Exit(0);
            }
            return result;
        }
        return result;
    }
};
} // namespace

int main(int argc, char** argv)
{
    SDL_setenv("SDL_AUDIODRIVER", "dummy", 1);
    SDL_SetMainReady();
    pc_gpu_preference_apply();
    _putenv_s("PIKMIN_RANDOMIZER_TEST_BACKGROUND", "1");
    pc_bbft_init(argc, argv);
    require(pc_pikipelago_room_preview(), "requires --experimental-pikmin2-room");
    if (!pc_window_init("P2 BombMgr birth fixture", 960, 540)) return 3;
    pc_window_center();
    pc_settings_init();
    gsys->Initialise();
    pc_settings_p2d_init();
    nodeMgr = new NodeMgr();
    gsys->run(new BombMgrBirthApp());
    return 0;
}
#endif // P2_BOMB_MGR_BIRTH_UNITTEST
