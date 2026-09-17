// Muse Baby31 captain-attack receiver fixture (#400, parent #256/#172).
//
// Spliced into tools/preview_p2_room.cpp by the lane observer. Complete
// RoomApp class, not a standalone translation unit.
//
// Scenario: the visual Queen (config-placed, larvae enabled) runs its source
// Born/Move/Attack schedule via pc_p2_queen_update(), driven from this App
// every idle tick (no shared tick/draw hooks were added). The captain is
// staged AWAY from the Queen home (beside the squad spawn) so the trailing
// Pikmin escort never latches the Queen and her Wait->Born schedule runs
// undisturbed; the family-reviewed p2-queen-inject.txt sidecar (armed by the
// arena staging, absent in normal runs) places an active larva at the
// captain mouth and forces Baby Attack 4, after which the REAL Attack FSM
// and the engine InteractAttack path (actNavi) run unmodified and the module
// logs P2_QUEEN_LARVA_ATTACK with damage=2. This App
// NEVER writes health: it only samples the captain health read-only each tick
// and completes when the observed drop reaches 2. The captain guard runs
// FIRST every tick (mandatory #632); a dead captain exits BLOCKED, never PASS.
// Queen home is shared with the arena staging (see the observer module).
class RoomApp : public PlugPikiApp {
    int frames=0,observed=0,stage=0;
    float captainStartHealth=0.0f;
    float captainMinHealth=1e9f;
    Vector3f captainOrigin;
    // Queen home, must match the p2-queen-actor.txt placement row.
    static constexpr float kQueenX = 34.0f;
    static constexpr float kQueenZ = 1896.0f;
public:int idle() override {
    int result=PlugPikiApp::idle();require(++frames<90000,"muse larva timeout");
    // Mandatory captain guard FIRST, before every other gate.
    if(naviMgr){Navi* g=naviMgr->getNavi();
        if(g){bool orima=!g->isAlive();int gid=g->mStateMachine?g->mStateMachine->getCurrID(g):-1;bool dead=(gid==NAVISTATE_Dead);float hp=g->mHealth;
            if(orima||dead||hp!=hp||hp<=1.0f){
                std::printf("P2_FIXTURE_CAPTAIN_DOWN tick=%d hp=%.3f orima_dead=%d dead_state=%d outcome=BLOCKED\n",observed,hp,int(orima),int(dead));std::fflush(nullptr);std::_Exit(86);}}}
    if(frames%600==0)std::printf("P2_MUSE_LARVA_HB frames=%d stage=%d observed=%d navimgr=%d\n",frames,stage,observed,naviMgr?1:0);
    if(gameflow.mMoviePlayer&&gameflow.mMoviePlayer->mIsActive){gameflow.mMoviePlayer->skipScene(SCENESKIP_SkipAll);return result;}
    if(!pc_p2_preview_ready()||!naviMgr||!pikiMgr||!tekiMgr)return result;
    Navi* n=naviMgr->getNavi();if(!n||gameflow.mPauseAll||gameflow.mIsUIOverlayActive)return result;
    {static bool naviSustainLogged=false;int ns=n->mStateMachine->getCurrID(n);
        if(ns==NAVISTATE_Pressed||ns==NAVISTATE_Flick||ns==NAVISTATE_PikiZero||ns==NAVISTATE_DemoSunset||ns==NAVISTATE_DemoWait||ns==NAVISTATE_DemoInf){n->mStateMachine->transit(n,NAVISTATE_Walk);if(!naviSustainLogged){naviSustainLogged=true;std::puts("P2_MUSE_LARVA_GUARD navi_sustain=1");}}}
    {static bool pikminGuardLogged=false;if((int)GameStat::allPikis==0){GameStat::allPikis.set(1,Red);if(!pikminGuardLogged){pikminGuardLogged=true;std::puts("P2_MUSE_LARVA_GUARD pikmin_guard=1");}}}
    ++observed;
    // Drive the sampled Queen actor (fixture-owned tick; no shared hook added).
    pc_p2_queen_update();
    const float captainHp=n->mHealth;
    if(captainHp>0.0f&&captainHp<captainMinHealth)captainMinHealth=captainHp;
    if(stage==0){
        captainOrigin=n->mSRT.t;
        for(int f=0;f<DEMOFLAG_COUNT;++f)playerState->mDemoFlags.setFlagOnly(f);
        n->mKontroller=new FixtureController();
        int squad=0;Iterator p(pikiMgr);CI_LOOP(p){Piki* v=static_cast<Piki*>(*p);if(v->isAlive())++squad;}
        require(squad>=1,"live starting squad");
        captainStartHealth=captainHp;captainMinHealth=captainHp;
        Vector3f park(-104.0f,0,1790.0f);park.y=mapMgr->getMinY(park.x,park.z,true);
        n->resetPosition(park);
        std::printf("P2_MUSE_LARVA_READY squad=%d captain_health=%.1f queen=%.1f,%.1f\n",squad,captainHp,kQueenX,kQueenZ);
        std::fflush(stdout);stage=1;return result;
    }
    if(stage==1){
        // Keep the captain the nearest target without touching actor AI.
        if(observed%300==0){Vector3f park(-104.0f,0,1790.0f);park.y=mapMgr->getMinY(park.x,park.z,true);n->resetPosition(park);}
        if(observed%90==0)std::printf("P2_MUSE_LARVA_HP health=%.1f min=%.1f tick=%d\n",captainHp,captainMinHealth,observed);
        if(captainStartHealth-captainMinHealth>=2.0f){
            std::printf("P2_MUSE_LARVA_RECEIPT drop=%.1f tick=%d\n",captainStartHealth-captainMinHealth,observed);
            std::fflush(stdout);stage=2;return result;
        }
        if(observed>=9000){std::puts("FAIL P2_MUSE_LARVA no_bite_timeout");std::fflush(stdout);std::_Exit(1);}
        return result;
    }
    if(stage==2){
        std::printf("P2_MUSE_LARVA_SESSION navi=1 pikis=%lu\n",(unsigned long)GameStat::allPikis);
        std::puts("PASS P2_MUSE_LARVA attack+bite");
        std::fflush(stdout);std::_Exit(0);
    }
    std::fflush(stdout);return result;
}};
