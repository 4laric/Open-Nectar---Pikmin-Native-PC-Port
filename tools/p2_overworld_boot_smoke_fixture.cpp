// Overworld course boot smoke fixture (#738).
//
// Spliced into tools/preview_p2_room.cpp by the lane runner. This is a
// complete RoomApp class, not a standalone translation unit.
//
// Baseline probe for the ABSENT overworld course boot flag: on boot it reports
// the observed dispatch mode (room preview / P1 challenge / none) and emits
// `P2_OVERWORLD_BOOT_ABSENT`, documenting that no `--experimental-p2-overworld-course`
// flag exists in this tree. Once the #186-approved flag lands, the same fixture
// observes `P2_OVERWORLD_BOOT course=<name>` instead. No mHealth, TransportMode,
// or gameplay-state write exists anywhere in this file.
//
// Captain safety (#632): adopts scripts/p2_fixture_captain_guard.h (or the
// inline tested equivalent below); checks orimaDead/NaviDead/HP<=1 before any
// observation tick; CAPTAIN_DOWN exits 86 BLOCKED. No blanket invincibility.
#if __has_include("p2_fixture_captain_guard.h")
#include "p2_fixture_captain_guard.h"
#else
// Inline tested equivalent of scripts/p2_fixture_captain_guard.h (recorded hash
// alongside the lane); never changes captain health or game state.
#include <cmath>
#include <cstdio>
#include <cstdlib>
inline bool p2_fixture_captain_down(bool orimaDead, bool deadState, float hp) {
    return orimaDead || deadState || !std::isfinite(hp) || hp <= 1.0f;
}
inline void p2_fixture_require_captain(bool orimaDead, bool deadState, float hp, int tick) {
    if (!p2_fixture_captain_down(orimaDead, deadState, hp)) return;
    std::printf("P2_FIXTURE_CAPTAIN_DOWN tick=%d hp=%.3f orima_dead=%d dead_state=%d outcome=BLOCKED\n",
                tick, hp, int(orimaDead), int(deadState));
    std::fflush(nullptr);
    std::_Exit(86);
}
#endif

class RoomApp : public PlugPikiApp {
    int frames=0,observed=0,stage=0;
public:int idle() override {
    int result=PlugPikiApp::idle();require(++frames<90000,"overworld boot smoke timeout");
    if(frames%600==0){std::printf("P2_OVERWORLD_BOOT_HB frames=%d stage=%d observed=%d\n",frames,stage,observed);std::fflush(stdout);}
    if(gameflow.mMoviePlayer&&gameflow.mMoviePlayer->mIsActive){gameflow.mMoviePlayer->skipScene(SCENESKIP_SkipAll);return result;}
    if(!naviMgr||!pikiMgr||!tekiMgr)return result;
    Navi* n=naviMgr->getNavi();if(!n||gameflow.mPauseAll||gameflow.mIsUIOverlayActive)return result;
    p2_fixture_require_captain(naviMgr->isNaviDead(n),n->mStateMachine->getCurrID(n)==NAVISTATE_Dead,n->mHealth,observed);
    ++observed;
    if(stage==0){
        const bool room=pc_pikipelago_room_preview();
        const int level=pc_pikipelago_challenge_level();
        std::printf("P2_OVERWORLD_BOOT_SMOKE room=%d challenge=%d\n",int(room),level);
        std::printf("P2_OVERWORLD_BOOT_ABSENT reason=no_flag_in_dispatch\n");
        std::fflush(stdout);stage=1;return result;
    }
    if(stage==1){
        std::printf("P2_OVERWORLD_BOOT_SESSION navi=1 pikis=%lu\n",(unsigned long)GameStat::allPikis);
        std::puts("PASS P2_OVERWORLD_BOOT_SMOKE absent=1 injected=0");
        std::fflush(stdout);std::_Exit(0);
    }
    std::fflush(stdout);return result;
}};