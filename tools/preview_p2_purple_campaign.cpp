// Isolated actual-engine storage fixture. Purple identity/maturity are injected;
// this does not certify natural conversion, combat, carry, or player controls.
#include <SDL2/SDL.h>
#include "system.h"
#include "App.h"
#include "Node.h"
#include "FlowController.h"
#include "MoviePlayer.h"
#include "Navi.h"
#include "NaviMgr.h"
#include "NaviState.h"
#include "Piki.h"
#include "PikiMgr.h"
#include "PikiState.h"
#include "PikiHeadItem.h"
#include "Pom.h"
#include "Boss.h"
#include "BaseInf.h"
#include "ItemMgr.h"
#include "GameStat.h"
#include "Stream.h"
#include "pc_randomizer.h"
#include "pc_p2_ship.h"
#include "pc_p2_ship_store.h"
#include "pc_p2_purple.h"
#include "pc_window.h"
#include "pc_gpu_preference.h"
#include "pc_bbft.h"
#include "settings/pc_settings.h"
#include "settings/pc_settings_p2d.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>

// Equivalent to the canonical fixture guard; negative mode exits 86 before boot.
static void p2_fixture_require_captain(bool dead, bool deadState, float hp, int tick) {
    if (!dead && !deadState && std::isfinite(hp) && hp > 1.0f) return;
    std::printf("P2_FIXTURE_CAPTAIN_DOWN tick=%d hp=%.3f orima_dead=%d dead_state=%d outcome=BLOCKED\n", tick, hp, int(dead), int(deadState));
    std::fflush(nullptr); std::_Exit(86);
}
static void require(bool ok, const char* why) {
    if (!ok) { std::printf("P2_PURPLE_CAMPAIGN_FAIL %s\n", why); std::fflush(nullptr); std::_Exit(1); }
}
class PurpleCampaignApp : public PlugPikiApp {
    int ticks = 0;
    bool captainSeen = false;
    int phase = 0, phaseTicks = 0, startingField = 0;
    Piki* input = nullptr;
    Piki* naturalStep(Navi* n) {
        ++phaseTicks;
        Pom* violet = nullptr; int count = 0;
        Iterator flowers(bossMgr);
        CI_LOOP(flowers) {
            Boss* b = static_cast<Boss*>(*flowers);
            if (b && b->isAlive() && b->mObjType == OBJTYPE_Pom && pc_p2_violet(static_cast<Pom*>(b))) { violet = static_cast<Pom*>(b); ++count; }
        }
        if (phase == 0) {
            require(count == 1, "exact bound Violet missing");
            Iterator bodies(pikiMgr);
            CI_LOOP(bodies) {
                Piki* p = static_cast<Piki*>(*bodies);
                if (p->isAlive() && p->getState() == PIKISTATE_Normal && p->mMode == PikiMode::FormationMode && !pc_p2_is_purple(p)) { input = p; break; }
            }
            if (!input) return nullptr;
            GameStat::update(); startingField = GameStat::mapPikis;
            phase = 1; phaseTicks = 0;
            std::printf("P2_PURPLE_NATURAL_START field=%d violet=%.1f,%.1f,%.1f\n", startingField, violet->mSRT.t.x, violet->mSRT.t.y, violet->mSRT.t.z);
        }
        if (phase == 1) {
            if (input && input->isAlive() && !pc_p2_is_purple(input) && !input->isStickTo() && violet
                && input->getState() == PIKISTATE_Normal && phaseTicks % 60 == 0) {
                input->changeMode(PikiMode::FreeMode,n); input->mFSM->transit(input,PIKISTATE_Flying);
                // Sweep the scripted reticle through the native arc; its nominal
                // endpoint is not the ground intercept when hold height varies.
                const char* fixedAim = std::getenv("P2_PURPLE_AIM_SCALE");
                const float aimScale = fixedAim ? std::atof(fixedAim) : 0.8f + 0.1f * ((phaseTicks / 60) % 9);
                Vector3f aim = n->mSRT.t + (violet->mSRT.t - n->mSRT.t) * aimScale;
                n->throwPiki(input,aim);
                std::printf("P2_PURPLE_SCRIPTED_THROW real_collision=1 aim_scale=%.2f captain=%.1f,%.1f,%.1f velocity=%.1f,%.1f,%.1f\n",
                    aimScale,n->mSRT.t.x,n->mSRT.t.y,n->mSRT.t.z,input->mVelocity.x,input->mVelocity.y,input->mVelocity.z);
            }
            Iterator heads(itemMgr->getPikiHeadMgr());
            CI_LOOP(heads) {
                PikiHeadItem* h = static_cast<PikiHeadItem*>(*heads);
                if (h && h->isAlive() && h->mP2Purple && h->canPullout()) {
                    n->mSproutToPluck=h; n->mPikiToPluck=nullptr;
                    n->mStateMachine->transit(n,NAVISTATE_NukuAdjust);
                    phase=2; phaseTicks=0; input=nullptr;
                    std::puts("P2_VIOLET_REAL_SPROUT captain_pluck_started=1"); break;
                }
            }
        }
        if (phase == 2) {
            Iterator bodies(pikiMgr);
            CI_LOOP(bodies) {
                Piki* p=static_cast<Piki*>(*bodies);
                if (p->isAlive() && pc_p2_is_purple(p) && p->getState()==PIKISTATE_Normal && p->mMode==PikiMode::FormationMode) {
                    GameStat::update(); require(int(GameStat::mapPikis)==startingField,"conversion/pluck population");
                    require(pc_throw_selection_class(p)==4 && pc_piki_carry_strength(p)==10,"selection/strength");
                    std::puts("P2_PURPLE_ACQUISITION_PASS scripted_throw=1 native_conversion=1 captain_pluck=1 selection=4 strength=10");
                    return p;
                }
            }
        }
        if(phaseTicks%120==0) std::printf("P2_PURPLE_NATURAL_PROGRESS phase=%d ticks=%d violet_state=%d\n",phase,phaseTicks,violet?violet->getCurrentState():-1);
        require(phaseTicks<1800,"natural acquisition timeout"); return nullptr;
    }
public:
    int idle() override {
        const int result = PlugPikiApp::idle();
        Navi* n = naviMgr ? naviMgr->getNavi() : nullptr;
        if (n) {
            captainSeen = true;
            p2_fixture_require_captain(GameStat::orimaDead,
                n->getCurrState() && n->getCurrState()->getID() == NAVISTATE_Dead, n->mHealth, ticks);
        } else require(!captainSeen, "captain disappeared");
        require(++ticks < 6000, "startup timeout");
        if (std::getenv("P2_PURPLE_NATURAL") && ticks % 120 == 0)
            std::printf("P2_PURPLE_GATE tick=%d navi=%d pause=%d ui=%d movie=%d phase=%d input_state=%d\n", ticks,
                n && n->getCurrState() ? n->getCurrState()->getID() : -1,
                int(gameflow.mPauseAll), int(gameflow.mIsUIOverlayActive),
                gameflow.mMoviePlayer ? int(gameflow.mMoviePlayer->mIsActive) : -1, phase,
                input && input->isAlive() ? input->getState() : -1);
        if (gameflow.mMoviePlayer && gameflow.mMoviePlayer->mIsActive) {
            gameflow.mMoviePlayer->requestSkip(); return result;
        }
        const bool natural = std::getenv("P2_PURPLE_NATURAL") != nullptr;
        if (!n || !pikiMgr || !itemMgr || gameflow.mPauseAll || gameflow.mIsUIOverlayActive
            || !n->getCurrState()) return result;
        const int naviState = n->getCurrState()->getID();
        // Native idle is healthy and expected after ten seconds without input.
        // Do not stall sprout observation just because the captain stops walking.
        if (naviState != NAVISTATE_Walk && !(natural && naviState == NAVISTATE_Idle)) return result;
        Piki* picked = nullptr;
        if (natural) picked = naturalStep(n);
        else {
            Iterator it(pikiMgr);
            CI_LOOP(it) {
                Piki* p = static_cast<Piki*>(*it);
                if (p->isAlive() && p->getState() == PIKISTATE_Normal && p->mMode == PikiMode::FormationMode) { picked = p; break; }
            }
        }
        if (!picked) return result;
        require(pc_randomizer_purple_campaign() && pc_p2_purples_enabled(), "ordinary opt-in not ready");
        picked->setFlower(Flower); if (!natural) pc_p2_make_purple(picked);
        GameStat::update();
        const int field = GameStat::mapPikis, ship = p2ship::stock.total();
        const int red = pikiInfMgr.getColorTotal(Red);
        require(pc_piki_carry_strength(picked) == 10, "Purple strength");
        require(pc_p2_ship_deposit(picked), "deposit");
        require(int(GameStat::mapPikis) == field-1 && p2ship::stock.total() == ship+1, "deposit conservation");
        require(pikiInfMgr.getColorTotal(Red) == red, "Red stock altered");
        Piki* restored = pc_p2_ship_withdraw(n, 3);
        require(restored && pc_p2_is_purple(restored) && restored->mHappa == Flower, "withdraw identity/maturity");
        require(int(GameStat::mapPikis) == field && p2ship::stock.total() == ship, "withdraw conservation");
        require(!pc_p2_ship_withdraw(n, 4), "White unexpectedly enabled");
        for (int i = 0; i < 5; ++i) {
            require(pc_p2_ship_deposit(restored), "repeat deposit");
            restored = pc_p2_ship_withdraw(n, 3);
            require(restored && int(GameStat::mapPikis) == field && p2ship::stock.total() == ship, "repeat drift");
        }
        require(pc_p2_ship_deposit(restored), "reserve one sprout slot");
        PikiHeadItem* sprout = static_cast<PikiHeadItem*>(itemMgr->birth(OBJTYPE_Pikihead));
        require(sprout != nullptr, "sprout allocation");
        sprout->init(n->mSRT.t); sprout->setColor(Red); sprout->mP2Purple = true; sprout->mFlowerStage = Bud;
        BPikiInf saved, loaded; saved.store(sprout);
        unsigned char bytes[32] = {}; RamStream out(bytes, sizeof(bytes)); saved.saveCard(out);
        RamStream in(bytes, sizeof(bytes)); loaded.loadCard(in);
        sprout->setColor(Red); loaded.doRestore(sprout);
        require(sprout->mP2Purple && sprout->mFlowerStage == Bud, "buried save identity/maturity");
        std::printf("P2_PURPLE_CAMPAIGN_STORAGE_PASS injected_identity=%d injected_maturity=1 live_squad=1 population_conserved=1 red_stock_unchanged=1\n",int(!natural));
        std::fflush(nullptr); std::_Exit(0);
    }
};
int main(int argc, char** argv) {
    if (std::getenv("P2_FIXTURE_FORCE_CAPTAIN_DOWN")) p2_fixture_require_captain(false, false, 0, 0);
    setvbuf(stdout, nullptr, _IONBF, 0);
    SDL_SetMainReady(); pc_gpu_preference_apply();
    pc_bbft_init(argc, argv);
    if (!pc_randomizer_purple_campaign()) return 2;
    if (!pc_window_init("Purple campaign storage fixture", 960, 540)) return 3;
    pc_settings_init();
    pc_window_set_display_mode(PC_WINDOW_FULLSCREEN_WINDOWED);
    pc_window_set_window_size(960, 540); pc_window_center();
    std::puts("Experimental preview window set to 960x540 windowed and centered");
    int w=0,h=0,x=0,y=0; SDL_Window* window=SDL_GL_GetCurrentWindow();
    SDL_GetWindowSize(window,&w,&h); SDL_GetWindowPosition(window,&x,&y);
    require(w==960 && h==540,"window dimensions");
    std::printf("P2_FIXTURE_WINDOW width=%d height=%d x=%d y=%d\n",w,h,x,y);
    gsys->Initialise(); pc_settings_p2d_init(); nodeMgr = new NodeMgr();
    gsys->run(new PurpleCampaignApp()); return 0;
}

