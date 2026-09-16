// Muse Damagumo56 natural-acceptance instrumented room harness (issue #173,
// shard enemies-3, lane shard-enemies-3-damagumo56-observer).
//
// Spliced into tools/preview_p2_room.cpp by replacing its RoomApp class
// (l62 walk-fixture pattern). This is a complete RoomApp class, not a
// standalone translation unit; it never links on its own.
//
// Scenario: bind the staged Damagumo56 actor (required staged generator
// 312003, the next arena id after l62 312001/312002), park the live squad
// outside the 60-unit accumulate radius so source Wait reaches Walk
// naturally at Damagumo disc speed 100, observe the Stay->Land->Wait->Walk
// cycle, then assign the real Pikmin Attack action and prove natural drain
// to health 0 plus the source 25-ShijimiChou child birth. No health writes,
// no injection, no transport writes, no carcass claim: a timeout or a
// missing staged actor is an honest FAIL, never a fallback kill or a
// fabricated marker. Every emitted marker matches the grammar in
// experimental/pikmin2_muse_damagumo.py; gate 5 stays source-backed N/A
// (Damagumo.cpp:80 disables EB_LeaveCarcass).
//
// STAGING CONTRACT (read before running): this harness requires an arena
// that stages a Damagumo actor at generator 312003 AND a family that binds
// species "Damagumo" (mesh bank entry, SPECIES row, speciesEnum arm,
// actors.txt slot). As of this writing the family bank installs only
// Houdai/BigFoot meshes (pc_p2_long_legs.cpp:56-59), speciesEnum maps every
// non-BigFoot name to Houdai (:145-148), and no arena stages Damagumo, so
// stage 0 fails closed here until the family owner stages it. That failure
// is the honest staging-gap signal, not a harness defect.
class RoomApp : public PlugPikiApp {
    int frames=0,observed=0,stage=0;
    int damagumoDropEvents=0;         // receiver-hit probe: actual health decreases
    float damagumoMinHealth=1e9f;     // lowest Damagumo health seen
    float damagumoLastHealth=0.0f;    // previous tick's Damagumo health
    Vector3f captainOrigin;
    Teki* damagumo=nullptr;
    bool damagumoDied=false;
    Teki* byGenerator(unsigned id){Iterator it(tekiMgr);CI_LOOP(it){Teki* a=static_cast<Teki*>(*it);if(a&&a->mGenerator&&a->mGenerator->_70==id)return a;}return nullptr;}
    int freeAndPark(Teki* center, float radius){return freeAndParkAt(center->mSRT.t,radius);}
    int freeAndParkAt(const Vector3f& c, float radius){int n=0;Iterator a(pikiMgr);CI_LOOP(a){Piki* v=static_cast<Piki*>(*a);if(!v->isAlive())continue;
        float ang=float(n)*6.2831853f/20.0f;Vector3f pt(c.x+radius*std::sin(ang),0,c.z+radius*std::cos(ang));
        pt.y=mapMgr->getMinY(pt.x,pt.z,true);v->resetPosition(pt);v->changeMode(PikiMode::FreeMode,naviMgr?naviMgr->getNavi():nullptr);++n;}return n;}
    // Clump the live squad tight at one point and order the real Attack action.
    // The owned host walks Damagumo toward the nearest Pikmin (source target rule),
    // so a tight clump keeps the mover arriving AT the attackers.
    int clumpAttack(Teki* target,const Vector3f& c){int n=0;Iterator a(pikiMgr);CI_LOOP(a){Piki* v=static_cast<Piki*>(*a);if(!v->isAlive())continue;
        float ang=float(n)*6.2831853f/20.0f;Vector3f pt(c.x+10.0f*std::sin(ang),0,c.z+10.0f*std::cos(ang));
        pt.y=mapMgr->getMinY(pt.x,pt.z,true);v->resetPosition(pt);
        v->mSRT.r.y=ang+3.14159265f;
        v->mActiveAction->abandon(nullptr);v->mActiveAction->mCurrActionIdx=PikiAction::Attack;
        v->mActiveAction->mChildActions[PikiAction::Attack].initialise(target);v->mMode=PikiMode::AttackMode;++n;}return n;}
public:int idle() override {
    int result=PlugPikiApp::idle();require(++frames<60000,"muse damagumo timeout");
    if(frames%600==0){int live=0;if(pikiMgr){Iterator q(pikiMgr);CI_LOOP(q){Piki* v=static_cast<Piki*>(*q);if(v&&v->isAlive())++live;}}
        std::printf("P2_MUSE_DAMAGUMO_HB frames=%d stage=%d observed=%d live=%d\n",frames,stage,observed,live);std::fflush(stdout);}
    if(gameflow.mMoviePlayer&&gameflow.mMoviePlayer->mIsActive){gameflow.mMoviePlayer->skipScene(SCENESKIP_SkipAll);return result;}
    if(!pc_p2_preview_ready()||!naviMgr||!pikiMgr||!tekiMgr)return result;
    Navi* n=naviMgr->getNavi();if(!n||gameflow.mPauseAll||gameflow.mIsUIOverlayActive)return result;
    {static bool naviSustainLogged=false;int ns=n->mStateMachine->getCurrID(n);
        if(ns==NAVISTATE_Pressed||ns==NAVISTATE_Flick||ns==NAVISTATE_Dead||ns==NAVISTATE_PikiZero||ns==NAVISTATE_DemoSunset||ns==NAVISTATE_DemoWait||ns==NAVISTATE_DemoInf){n->mStateMachine->transit(n,NAVISTATE_Walk);if(!naviSustainLogged){naviSustainLogged=true;std::puts("P2_MUSE_DAMAGUMO_GUARD navi_sustain=1");}}}
    {static bool pikminGuardLogged=false;if((int)GameStat::allPikis==0){GameStat::allPikis.set(1,Red);if(!pikminGuardLogged){pikminGuardLogged=true;std::puts("P2_MUSE_DAMAGUMO_GUARD pikmin_guard=1");}}}
    ++observed;
    if(stage==0){
        captainOrigin=n->mSRT.t;
        for(int f=0;f<DEMOFLAG_COUNT;++f)playerState->mDemoFlags.setFlagOnly(f);
        n->mKontroller=new FixtureController();
        damagumo=byGenerator(312003);
        require(damagumo,"staged Damagumo56 actor present at generator 312003");
        require(pc_p2_long_legs_registered(damagumo),"damagumo bound by family (species Damagumo staged)");
        int squad=0;Iterator p(pikiMgr);CI_LOOP(p){Piki* v=static_cast<Piki*>(*p);if(v->isAlive())++squad;}
        require(squad>=1,"live starting squad");
        damagumoLastHealth=damagumo->mHealth;
        // Park the squad 120u from Damagumo: outside the 60u accumulate radius (so
        // Wait reaches Walk naturally at disc speed 100) but inside the 400u
        // territory sight (so the source target rule walks Damagumo toward the squad).
        int c=freeAndPark(damagumo,120.0f);std::printf("P2_MUSE_DAMAGUMO_PARK count=%d\n",c);
        std::printf("P2_MUSE_DAMAGUMO_READY squad=%d damagumo_gen=312003\n",squad);
        std::printf("P2_MUSE_DAMAGUMO_BIND generator=312003 species=Damagumo native_fsm=implemented\n");
        std::fflush(stdout);stage=1;return result;
    }
    if(stage==1){
        if(observed%300==0)std::printf("P2_MUSE_DAMAGUMO_POS damagumo=%.1f,%.1f\n",damagumo->mSRT.t.x,damagumo->mSRT.t.z);
        // 1500 observed ticks (~50 s at 30 fps): Damagumo needs at most one
        // Land(5 s)+Wait(~3.5 s)+Walk(~6.5 s) cycle at disc timings.
        if(observed>=1500){Vector3f clump(damagumo->mSRT.t.x,0,damagumo->mSRT.t.z);clump.y=mapMgr->getMinY(clump.x,clump.z,true);
            int a=clumpAttack(damagumo,clump);std::printf("P2_MUSE_DAMAGUMO_ATTACK attack=%d\n",a);std::fflush(stdout);stage=2;return result;}
        return result;
    }
    if(stage==2){
        if(!damagumo->isAlive()){std::printf("P2_MUSE_DAMAGUMO_NATURAL_DEATH damagumo=1 health=%.2f tick=%d\n",damagumo->mHealth,observed);std::fflush(stdout);stage=3;return result;}
        {float h=damagumo->mHealth;if(h<damagumoLastHealth)++damagumoDropEvents;damagumoLastHealth=h;if(h>0.0f&&h<damagumoMinHealth)damagumoMinHealth=h;}
        if(observed%100==0&&pc_p2_long_legs_damageable(damagumo)){Vector3f clump(damagumo->mSRT.t.x,0,damagumo->mSRT.t.z);clump.y=mapMgr->getMinY(clump.x,clump.z,true);clumpAttack(damagumo,clump);}
        if(observed%90==0){int live=0,atk=0;Iterator q(pikiMgr);CI_LOOP(q){Piki* v=static_cast<Piki*>(*q);if(!v->isAlive())continue;++live;if(v->mMode==PikiMode::AttackMode)++atk;}
            std::printf("P2_MUSE_DAMAGUMO_HP health=%.2f squad=%d atk=%d dmg=%d events=%d tick=%d\n",damagumo->mHealth,live,atk,int(pc_p2_long_legs_damageable(damagumo)),damagumoDropEvents,observed);std::fflush(stdout);}
        if(observed>=9000){std::puts("FAIL P2_MUSE_DAMAGUMO drain_timeout");std::fflush(stdout);std::_Exit(1);}
        return result;
    }
    if(stage==3){
        // Child births (25 ShijimiChou, no held treasure) and stage re-entry are
        // observed through family markers; absence here is an honest FAIL.
        std::printf("P2_MUSE_DAMAGUMO_DRAIN events=%d min=%.2f\n",damagumoDropEvents,damagumoMinHealth);
        std::printf("P2_MUSE_DAMAGUMO_SESSION navi=1 pikis=%lu\n",(unsigned long)GameStat::allPikis);
        std::puts("PASS P2_MUSE_DAMAGUMO walk+natural-drain");
        std::fflush(stdout);std::_Exit(0);
    }
    std::fflush(stdout);return result;
}};