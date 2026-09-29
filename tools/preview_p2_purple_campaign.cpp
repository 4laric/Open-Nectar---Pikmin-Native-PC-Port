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
#include "BaseInf.h"
#include "ItemMgr.h"
#include "GameStat.h"
#include "RamStream.h"
#include "pc_randomizer.h"
#include "pc_p2_ship.h"
#include "pc_p2_ship_store.h"
#include "pc_p2_purple.h"
#include "pc_window.h"
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
public:
    int idle() override {
        const int result = PlugPikiApp::idle();
        Navi* n = naviMgr ? naviMgr->getNavi() : nullptr;
        if (n) {
            captainSeen = true;
            p2_fixture_require_captain(GameStat::orimaDead,
                n->getCurrState() && n->getCurrState()->getID() == NAVISTATE_Dead, n->mHealth, ticks);
        } else require(!captainSeen, "captain disappeared");
        require(++ticks < 1800, "startup timeout");
        if (gameflow.mMoviePlayer && gameflow.mMoviePlayer->mIsActive) {
            gameflow.mMoviePlayer->requestSkip(); return result;
        }
        if (!n || !pikiMgr || !itemMgr || gameflow.mPauseAll || gameflow.mIsUIOverlayActive
            || !n->getCurrState() || n->getCurrState()->getID() != NAVISTATE_Walk) return result;
        Piki* picked = nullptr;
        Iterator it(pikiMgr);
        CI_LOOP(it) {
            Piki* p = static_cast<Piki*>(*it);
            if (p->isAlive() && p->getState() == PIKISTATE_Normal && p->mMode == PikiMode::FormationMode) { picked = p; break; }
        }
        if (!picked) return result;
        require(pc_randomizer_purple_campaign() && pc_p2_purples_enabled(), "ordinary opt-in not ready");
        picked->setFlower(Flower); pc_p2_make_purple(picked);
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
        PikiHeadItem* sprout = static_cast<PikiHeadItem*>(itemMgr->birth(OBJTYPE_Pikihead));
        require(sprout != nullptr, "sprout allocation");
        sprout->init(n->mSRT.t); sprout->setColor(Red); sprout->mP2Purple = true; sprout->mFlowerStage = Bud;
        BPikiInf saved, loaded; saved.store(sprout);
        unsigned char bytes[32] = {}; RamStream out(bytes, sizeof(bytes)); saved.saveCard(out);
        RamStream in(bytes, sizeof(bytes)); loaded.loadCard(in);
        sprout->setColor(Red); loaded.doRestore(sprout);
        require(sprout->mP2Purple && sprout->mFlowerStage == Bud, "buried save identity/maturity");
        std::puts("P2_PURPLE_CAMPAIGN_STORAGE_PASS injected_identity=1 live_squad=1 population_conserved=1 red_stock_unchanged=1");
        std::fflush(nullptr); std::_Exit(0);
    }
};
int main(int argc, char** argv) {
    if (std::getenv("P2_FIXTURE_FORCE_CAPTAIN_DOWN")) p2_fixture_require_captain(false, false, 0, 0);
    setvbuf(stdout, nullptr, _IONBF, 0);
    if (!pc_randomizer_init(argc, argv)) return 2;
    pc_bbft_init(argc, argv);
    if (!pc_window_init("Purple campaign storage fixture", 960, 540)) return 3;
    pc_settings_init();
    pc_window_set_display_mode(PC_WINDOW_FULLSCREEN_WINDOWED);
    pc_window_set_window_size(960, 540); pc_window_center();
    std::puts("Experimental preview window set to 960x540 windowed and centered");
    gsys->Initialise(); pc_settings_p2d_init(); nodeMgr = new NodeMgr();
    gsys->run(new PurpleCampaignApp()); return 0;
}
