// Lane 12 (#130) live slot-0 captain/squad adapter runtime fixture.
// Private real-GL display fixture; no production registration or gameplay claims.
//
// Drives the LIVE pc_p2_captain adapter (p2_captain::setup_from_navi_mgr is now
// auto-bound by the GameCoreSection constructor) through the four semantics a
// captor family needs: target identity, claim/release, interrupted capture and
// cleanup.
// A separate --knockout-roster scenario exercises the survivor-gated game-over /
// NaviMgr::informOrimaDead hook added to NaviDeadState::init.
// The --survivor-path scenario (with PIKMIN_P2_SECOND_CAPTAIN=1 and the
// fixture-only PIKMIN_P2_SECOND_CAPTAIN_LIVE=1) flips the live gate, drives a
// real second captain, knocks the active captain down through the integrated
// InteractAttack receiver, and verifies the survivor rebind and final stage end.
#include <SDL2/SDL.h>
#include <GL/gl.h>
#include "App.h"
#include "GameCoreSection.h"
#include "GameStat.h"
#include "Graphics.h"
#include "Interactions.h"
#include "Navi.h"
#include "NaviMgr.h"
#include "NaviState.h"
#include "Node.h"
#include "Piki.h"
#include "PikiMgr.h"
#include "MoviePlayer.h"
#include "pc_bbft.h"
#include "pc_gfx.h"
#include "pc_gpu_preference.h"
#include "pc_p2_captain.h"
#include "pc_p2_preview.h"
#include "pc_window.h"
#include "settings/pc_settings.h"
#include "settings/pc_settings_p2d.h"
#include "system.h"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {
bool sKnockoutScenario = false;
bool sSurvivorScenario = false;

void require(bool value, const char* message)
{
    if (!value) { std::printf("FAIL P2_CAPTAIN_RUNTIME %s\n", message); std::fflush(stdout); std::_Exit(1); }
}

// Reusable P6 PPM capture (mirrors the other room fixtures). Reads the GL back
// buffer after a real draw so a two-captain frame can be saved as evidence.
void capture(const char* path)
{
    pc_gfx_flush_batch();
    auto bind = reinterpret_cast<PFNGLBINDFRAMEBUFFERPROC>(SDL_GL_GetProcAddress("glBindFramebuffer"));
    require(bind != nullptr, "framebuffer entry point unavailable");
    GLint previous = 0; glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previous); bind(GL_FRAMEBUFFER, 0);
    int width = 0, height = 0; SDL_GL_GetDrawableSize(SDL_GL_GetCurrentWindow(), &width, &height);
    std::vector<unsigned char> pixels(size_t(width) * size_t(height) * 3);
    glReadBuffer(GL_BACK); glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    bind(GL_FRAMEBUFFER, previous);
    require(glGetError() == GL_NO_ERROR, "capture GL error");
    bool visible = false; for (unsigned char value : pixels) visible |= value > 8;
    require(visible, "empty capture");
    FILE* file = std::fopen(path, "wb"); require(file != nullptr, "capture file");
    std::fprintf(file, "P6\n%d %d\n255\n", width, height);
    for (int y = height - 1; y >= 0; --y) std::fwrite(pixels.data() + size_t(y) * width * 3, 1, size_t(width) * 3, file);
    std::fclose(file);
}

class CaptainApp final : public PlugPikiApp {
    int frames = 0;
    bool setup = false;
    int stage = 0;
    Piki* squadPiki = nullptr;
    std::uint64_t epoch = 1;
    // Survivor scenario multi-frame state: arm on the first ready frame, then
    // act once the plate has been refreshed (so releasePikis() can iterate).
    bool survivorArmed = false;
    int survivorFrames = 0;
    Piki* survivorPiki = nullptr;
    int survivorPreMode = 0;
    Navi* survivorNavi0 = nullptr;
    Navi* survivorNavi1 = nullptr;
    bool captured = false;
public:
    void draw(Graphics& gfx) override {
        PlugPikiApp::draw(gfx);
        // Capture the survivor scene once, early, while both captains are
        // present in the roster. NOTE: the second captain's model render is
        // still deferred (Navi::refresh early-return), so the PPM shows the
        // first captain + squad; it is scene evidence, not two-captain render
        // evidence (that remains blocked).
        if (survivorArmed && !captured && frames >= 3 && frames < 5) {
            capture("survivor-scene.ppm");
            captured = true;
        }
    }
    int idle() override {
        int result = PlugPikiApp::idle();
        require(++frames < 900, "timeout");
        if (gameflow.mMoviePlayer && gameflow.mMoviePlayer->mIsActive) {
            gameflow.mMoviePlayer->requestSkip();
            return result;
        }
        if (!pc_p2_preview_ready() || !naviMgr || !naviMgr->getNavi() || gameflow.mPauseAll
            || gameflow.mIsUIOverlayActive)
            return result;
        if (!setup) {
            setup = true;
            int liveSquad = 0;
            Iterator squad(pikiMgr);
            CI_LOOP(squad) { if (static_cast<Piki*>(*squad)) ++liveSquad; }
            std::printf("P2_CAPTAIN_SQUAD live=%d\n", liveSquad);
            std::fflush(stdout);

            // The scene setup hook should already have bound the live adapter.
            require(pc_p2_captain::adapter() != nullptr,
                "live adapter auto-bound by GameCoreSection constructor");
            require(pc_p2_captain::setup_from_navi_mgr(), "setup_from_navi_mgr idempotent");
            require(pc_p2_captain::adapter() != nullptr, "adapter remains bound");

            if (sSurvivorScenario) {
                // --- Survivor path end-to-end (#130): natural knockdown + rebind ---
                // Stage 0: arm. Record a real starting squad Piki and the two
                // captains, then return so the game update/draw runs and the
                // plate's trailing slot count (CPlate::refresh) is populated before
                // the knockdown (releasePikis() iterates mTotalSlotCount).
                require(naviMgr->hasSecondNavi(), "second captain present in roster");
                survivorNavi0 = naviMgr->getNavi(0);
                survivorNavi1 = naviMgr->getNavi(1);
                require(survivorNavi0 != nullptr && survivorNavi1 != nullptr, "both captain slots live");
                require(pc_p2_captain::health(0) > 0.0f && pc_p2_captain::health(1) > 0.0f,
                    "both captains adopted into the adapter");
                require(naviMgr->getActiveNavi() == survivorNavi0, "slot 0 active at scene start");
                Iterator sit(pikiMgr);
                survivorPiki = nullptr;
                CI_LOOP(sit) {
                    Piki* p = static_cast<Piki*>(*sit);
                    if (p && p->isAlive() && p->mNavi == survivorNavi0) { survivorPiki = p; break; }
                }
                require(survivorPiki != nullptr, "a live starting squad Piki bound to captain 0 exists");
                survivorPreMode = survivorPiki->mMode;
                std::printf("P2_CAPTAIN_SURVIVOR_ARMED plate=%d piki_mode_before=%d\n",
                    survivorNavi0->getPlatePikis(), survivorPreMode);
                std::fflush(stdout);
                return result;
            }

            // --- Gate 1: target identity ---
            Navi* navi = naviMgr->getNavi();
            require(pc_p2_captain::captain_handle(0) == static_cast<void*>(navi),
                "captain_handle(0) resolves the live slot-0 Navi");
            require(!pc_p2_captain::navi_dead(0), "slot 0 not dead");
            require(pc_p2_captain::health(0) > 0.0f, "slot 0 health adopted");
            std::printf("P2_CAPTAIN_TARGET_IDENTITY slot=0 handle_matches=1 health=%.1f\n",
                pc_p2_captain::health(0));
            std::fflush(stdout);

            // --- Gate 2: claim/release (captain) ---
            // Single-captain port: the zero-control guard refuses ingesting the
            // only controllable captain, exactly as the source never strands the
            // player with no one left.
            require(!pc_p2_captain::capture_captain(0, epoch),
                "only-captain capture refused (zero-control guard)");
            require(pc_p2_captain::health(0) > 0.0f, "captain unaffected by refusal");
            std::printf("P2_CAPTAIN_CLAIM_REFUSED captain=0 guard=zero_control\n");
            std::fflush(stdout);

            // --- Gate 3/4/5: live Piki claim, release, interrupted capture ---
            Piki* piki = static_cast<Piki*>(pikiMgr->birth());
            require(piki != nullptr, "piki birth");
            piki->init(navi);
            piki->initColor(Red);
            piki->setFlower(Leaf);
            piki->resetPosition(navi->mSRT.t);
            piki->mMode = PikiMode::AttackMode;
            squadPiki = piki;
            require(piki->mNavi == navi, "piki owned by slot-0 captain");

            require(pc_p2_captain::capture_actor(epoch, piki), "capture_actor claims live piki");
            require(piki->mNavi == nullptr, "captor-held piki freed from captain");
            require(pc_p2_captain::captive_count() == 1, "captive count incremented");
            std::printf("P2_CAPTAIN_ACTOR_CAPTURED captive_count=%d\n",
                pc_p2_captain::captive_count());
            std::fflush(stdout);

            require(pc_p2_captain::release_actor(epoch, piki, 0), "release_actor returns to squad");
            require(piki->mNavi == navi, "released piki re-owned by slot 0");
            require(pc_p2_captain::captive_count() == 0, "captive count cleared");
            std::printf("P2_CAPTAIN_ACTOR_RELEASED owner=0 captive_count=%d\n",
                pc_p2_captain::captive_count());
            std::fflush(stdout);

            // Interrupted capture (captor death): held actors are freed to the
            // ground (whistle-reclaimable), never deleted.
            ++epoch;
            require(pc_p2_captain::capture_actor(epoch, piki), "re-capture actor");
            const std::vector<std::uint32_t> dropped = pc_p2_captain::drop_captured(epoch);
            require(dropped.size() == 1, "interrupted capture drops the held actor");
            require(piki->mNavi == nullptr, "dropped actor free (whistle-reclaimable)");
            require(pc_p2_captain::captive_count() == 0, "captive table empty after drop");
            std::printf("P2_CAPTAIN_INTERRUPT_DROP released=%d state=free_reclaimable\n",
                (int)dropped.size());
            std::fflush(stdout);

            // Reclaim for cleanup, then reload: a captive is restored to its
            // previous owner on reload — never lost or duplicated.
            piki->mNavi = navi;
            require(pc_p2_captain::adopt_squad() >= 1, "freed piki re-adopted into squad");
            require(pc_p2_captain::capture_actor(++epoch, piki), "capture for reload conservation");
            require(piki->mNavi == nullptr, "captive removed from squad");
            require(pc_p2_captain::reload(), "reload succeeds");
            require(piki->isAlive(), "captive survived reload (not lost)");
            require(piki->mNavi == navi, "reload restored captive to previous owner");
            require(pc_p2_captain::captive_count() == 0, "no duplicated captive after reload");
            std::printf("P2_CAPTAIN_CLEANUP_RELOAD conserved=1 captive_count=%d\n",
                pc_p2_captain::captive_count());
            std::fflush(stdout);

            if (sKnockoutScenario) {
                // --- Gate: survivor-gated game over + knockout roster sync ---
                // Injected death: force health to zero and run Navi::finishDamage,
                // which transits to NAVISTATE_Dead and (via the lane-12 hook)
                // marks the roster via informOrimaDead and gates GameStat::orimaDead
                // on the alive-captain set. Labelled injected, not natural combat.
                if (!gameflow.mGameInterface) {
                    std::printf("P2_CAPTAIN_KNOCKOUT untested (no game interface)\n");
                    std::fflush(stdout);
                    std::puts("PASS P2_CAPTAIN_RUNTIME"); std::fflush(stdout); std::_Exit(0);
                }
                navi->mHealth = 0.0f;
                navi->finishDamage();
                require(naviMgr->isNaviDead(navi), "roster recorded the knockout");
                require(naviMgr->getAliveOrima() == nullptr, "no surviving captain remains");
                require(GameStat::orimaDead, "game over still signalled with zero survivors");
                std::printf("P2_CAPTAIN_KNOCKOUT_SYNC dead=1 alive_orima=none orima_dead=1\n");
                std::fflush(stdout);
                std::puts("PASS P2_CAPTAIN_RUNTIME"); std::fflush(stdout); std::_Exit(0);
            }

            std::puts("P2_CAPTAIN_LIVE_SEAM_PASS target_identity=1 claim_release=1 interrupt=1 cleanup=1");
            std::puts("PASS P2_CAPTAIN_RUNTIME"); std::fflush(stdout); std::_Exit(0);
            return result;
        }
        if (sSurvivorScenario && survivorArmed) {
            // Stage 1+: let the frame loop (update + draw) run so CPlate::refresh
            // has populated the plate's traversable slot count, then knock slot 0
            // down through the integrated receiver and observe the squad release.
            if (++survivorFrames < 4) return result;

            InteractAttack attack(nullptr, nullptr, 500.0f, false);
            require(attack.actNavi(survivorNavi0), "InteractAttack::actNavi landed on active captain");
            require(survivorNavi0->mHealth <= 1.0f, "attack receiver reduced captain to down");
            survivorNavi0->finishDamage();

            require(survivorNavi0->getCurrState()->getID() == NAVISTATE_Dead,
                "(a) downed captain entered Dead (ODead)");
            require(!GameStat::orimaDead, "(b) game not ended on first knockout");
            require(!GameCoreSection::inPause(), "(b) core not paused on first knockout");
            require(naviMgr->getAliveOrima() == survivorNavi1, "(b) survivor remains alive");
            require(naviMgr->getActiveNavi() == survivorNavi1, "(c) control rebound to survivor (active index)");
            // Observed squad release: the real starting Piki the downed captain
            // held is now FreeMode (releasePikis on the survivor branch).
            require(survivorPiki->mMode == PikiMode::FreeMode,
                "(squad) survivor-down released the starting squad to FreeMode");
            std::printf("P2_CAPTAIN_SURVIVOR_DOWN dead=0 survivor=1 plate=%d piki_mode_before=%d piki_mode_after=%d orima_dead=0 paused=0 active=1\n",
                survivorNavi0->getPlatePikis(), survivorPreMode, (int)survivorPiki->mMode);
            std::fflush(stdout);

            // Final stage end (injected second-captain knockout).
            survivorNavi1->mHealth = 0.0f;
            survivorNavi1->finishDamage();
            require(naviMgr->isNaviDead(survivorNavi1), "(d) second captain recorded dead");
            require(naviMgr->getAliveOrima() == nullptr, "(d) no survivor remains");
            require(GameStat::orimaDead, "(d) game over signalled with zero survivors");
            std::printf("P2_CAPTAIN_SURVIVOR_STAGE_END dead=2 alive_orima=none orima_dead=1\n");
            std::fflush(stdout);
            std::puts("PASS P2_CAPTAIN_RUNTIME"); std::fflush(stdout); std::_Exit(0);
        }
        return result;
    }
};
} // namespace

int main(int argc, char** argv)
{
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--knockout-roster") sKnockoutScenario = true;
        if (std::string(argv[i]) == "--survivor-path") sSurvivorScenario = true;
    }
    SDL_setenv("SDL_AUDIODRIVER", "dummy", 1);
    SDL_SetMainReady();
    pc_gpu_preference_apply();
    _putenv_s("PIKMIN_RANDOMIZER_TEST_BACKGROUND", "1");
    pc_bbft_init(argc, argv);
    require(pc_pikipelago_room_preview(), "requires --experimental-pikmin2-room");
    if (!pc_window_init("P2 captain live adapter fixture", 960, 540)) return 3;
    pc_window_center();
    {
        SDL_Window* window = SDL_GL_GetCurrentWindow();
        int width = 0, height = 0, x = 0, y = 0;
        SDL_GetWindowSize(window, &width, &height);
        SDL_GetWindowPosition(window, &x, &y);
        SDL_Rect bounds{0, 0, 0, 0};
        SDL_GetDisplayBounds(SDL_GetWindowDisplayIndex(window), &bounds);
        const bool centered = std::abs(x - (bounds.x + (bounds.w - width) / 2)) <= 2
            && std::abs(y - (bounds.y + (bounds.h - height) / 2)) <= 2;
        std::printf("P2_CAPTAIN_WINDOW size=%dx%d pos=%d,%d display=%dx%d centered=%d\n",
            width, height, x, y, bounds.w, bounds.h, int(centered));
        std::fflush(stdout);
    }
    pc_settings_init();
    gsys->Initialise();
    pc_settings_p2d_init();
    nodeMgr = new NodeMgr();
    gsys->run(new CaptainApp());
    return 0;
}
