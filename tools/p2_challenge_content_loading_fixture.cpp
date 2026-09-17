// P2 challenge stage content-loading boot fixture (#694).
//
// Spliced into tools/preview_p2_room.cpp by scripts/build_p2_challenge_content_loading.py.
// This is a complete RoomApp class, not a standalone translation unit.
//
// Scenario: a worker-staged stage-content sidecar (p2-challenge-content.txt)
// plus a caller-staged p2-cave-generate.txt manifest select one P2 challenge
// stage floor. The fixture validates the selection through
// p2_challenge_content::select(), verifies staged spawn intents cover the
// stage roster, then observes LIVE squad/actors with finite positions in the
// running engine. Generation and births stay with the integrated engine paths
// (#129 generator, actor-manager birth); this fixture never provisions arenas,
// births actors, writes saves, or touches the economy. A stall is an honest
// FAIL, never a fallback credit. Gate 6 stays UNTESTED (no re-entry).
//
// Captain safety (#632): the canonical guard runs FIRST after engine idle and
// BEFORE movie/pause/UI early returns, readiness gates, observation counters
// or PASS markers; CAPTAIN_DOWN exits 86 BLOCKED. The captain is parked outside
// attack reach (captain damage is not this test). No blanket invincibility.
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
#include "pc_p2_challenge_content_loading.h"

class RoomApp : public PlugPikiApp {
    int frames=0,observed=0,stage=0;
    bool selected=false;
    p2_challenge_content::Expectation expect;
    int liveSquadPeak=0,liveActorsPeak=0;
    int liveSquad() {
        int count=0;Iterator it(pikiMgr);CI_LOOP(it){Piki* p=static_cast<Piki*>(*it);if(p&&p->isAlive())++count;}return count;
    }
    int liveActors() {
        int count=0;Iterator it(tekiMgr);CI_LOOP(it){Teki* a=static_cast<Teki*>(*it);if(a&&a->isAlive())++count;}return count;
    }
public:int idle() override {
    int result=PlugPikiApp::idle();require(++frames<90000,"challenge content timeout");
    // Captain guard FIRST, before movie/pause/UI returns and any observation.
    if(naviMgr&&pikiMgr&&tekiMgr){
        Navi* guardN=naviMgr->getNavi();
        if(guardN){p2_fixture_require_captain(GameStat::orimaDead,guardN->mStateMachine->getCurrID(guardN)==NAVISTATE_Dead,guardN->mHealth,observed);}
    }
    if(gameflow.mMoviePlayer&&gameflow.mMoviePlayer->mIsActive){gameflow.mMoviePlayer->requestSkip();return result;}
    if(!pc_p2_preview_ready()||!naviMgr||!pikiMgr||!tekiMgr)return result;
    Navi* n=naviMgr->getNavi();if(!n||gameflow.mPauseAll||gameflow.mIsUIOverlayActive)return result;
    ++observed;
    if(stage==0){
        for(int f=0;f<DEMOFLAG_COUNT;++f)playerState->mDemoFlags.setFlagOnly(f);
        n->mKontroller=new FixtureController();
        require(p2_challenge_content::select("p2-challenge-content.txt",expect),"stage content selection");
        require(expect.valid,"selection valid");
        require(p2_challenge_content::verifyCoverage(expect),"spawn coverage");
        int squad=liveSquad();
        require(squad>=1,"live starting squad");
        // Park the captain well outside attack reach (not under test).
        Vector3f park=n->mSRT.t+Vector3f(150.0f,0.0f,0.0f);
        park.y=mapMgr->getMinY(park.x,park.z,true);n->resetPosition(park);
        std::printf("P2_CHALLENGE_CONTENT_READY cave=%s floor=%d squad=%d captain_parked=1\n",
                    expect.caveId.c_str(),expect.floor,squad);
        std::fflush(stdout);stage=1;return result;
    }
    if(stage==1){
        int squad=liveSquad(),actors=liveActors();
        if(squad>liveSquadPeak)liveSquadPeak=squad;
        if(actors>liveActorsPeak)liveActorsPeak=actors;
        require(p2_challenge_content::positionsFinite(),"nonfinite actor position");
        if(observed%90==0){std::printf("P2_CHALLENGE_CONTENT_OBSERVE squad=%d actors=%d tick=%d\n",squad,actors,observed);std::fflush(stdout);}
        if(liveSquadPeak>=1&&liveActorsPeak>=1){
            std::printf("P2_CHALLENGE_CONTENT_LIVE squad=%d actors=%d tick=%d\n",liveSquadPeak,liveActorsPeak,observed);
            std::puts("PASS P2_CHALLENGE_CONTENT_RUN content=1");
            std::fflush(stdout);std::_Exit(0);
        }
        if(observed>=6000){std::puts("FAIL P2_CHALLENGE_CONTENT no_live_content");std::fflush(stdout);std::_Exit(1);}
        return result;
    }
    std::fflush(stdout);return result;
}};