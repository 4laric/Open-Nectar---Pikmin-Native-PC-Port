// Acceptance additionally requires a production P2_PURPLE_DIRECT stage=hipdrop
// marker for the printed target/source pointers, family=adult_bulborb and
// damage_applied=1 and queued_after-queued_before=50, together with the
// regeneration-compensated 50 HP delta below. Adult accepted=0
// is expected (that flag describes dwarf press). Health alone is NOT acceptance.
// Natural Violet conversion/native pluck precede the test. Captain positioning,
// a single descending-source placement, and post-contact source isolation are
// fixture setup, not player controls, throw accuracy, or pathfinding coverage.
#include <SDL2/SDL.h>
#include "system.h"
#include "App.h"
#include "Node.h"
#include "MoviePlayer.h"
#include "Navi.h"
#include "NaviMgr.h"
#include "NaviState.h"
#include "Piki.h"
#include "PikiMgr.h"
#include "PikiState.h"
#include "PikiHeadItem.h"
#include "Pellet.h"
#include "PelletState.h"
#include "PikiAI.h"
#include "GoalItem.h"
#include "Route.h"
#include "Pom.h"
#include "Boss.h"
#include "ItemMgr.h"
#include "GameStat.h"
#include "MapMgr.h"
#include "Collision.h"
#include "Camera.h"
#include "Controller.h"
#include "Kontroller.h"
#include "nlib/System.h"
#include "gameflow.h"
#include "pc_randomizer.h"
#include "pc_p2_purple.h"
#include "pc_p2_ship.h"
#include "pc_p2_ship_store.h"
#include "pc_p2_input_script.h"
#include "FlowController.h"
#include "WorldClock.h"
#include "pc_p2_purple_direct.h"
#include "pc_p2_purple_flight.h"
#include "pc_p2_campaign_actor.h"
#include "pc_p2_chappy.h"
#include "pc_p2_purple_impact.h"
#include "pc_window.h"
#include "pc_gpu_preference.h"
#include "pc_bbft.h"
#include "settings/pc_settings.h"
#include "settings/pc_settings_p2d.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <climits>
#include <chrono>
#include <set>
#include <fstream>

static const auto fixtureStarted=std::chrono::steady_clock::now();
static void milestone(const char* name,int tick) {
    const double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-fixtureStarted).count();
    std::printf("P2_PURPLE_TIMING milestone=%s tick=%d wall_seconds=%.3f\n",name,tick,seconds);
}
static void p2_fixture_require_captain(bool present,bool dead,bool managerDead,bool deadState,float hp,int tick,bool missingAllowed) {
    if (!present && missingAllowed && !dead && !managerDead) return;
    if (present && !dead && !managerDead && !deadState && std::isfinite(hp) && hp > 1.0f) return;
    std::printf("P2_FIXTURE_CAPTAIN_DOWN tick=%d hp=%.3f present=%d orima_dead=%d manager_dead=%d dead_state=%d outcome=BLOCKED\n",tick,hp,int(present),int(dead),int(managerDead),int(deadState));
    std::fflush(nullptr); std::_Exit(86);
}
static void require(bool ok, const char* why) {
    if (!ok) { std::printf("P2_PURPLE_COMBAT_FAIL reason=%s\n",why); std::fflush(nullptr); std::_Exit(1); }
}
static void auditTerrain(const char* owner,const Vector3f& centre,float radius) {
    if(!mapMgr || !std::isfinite(radius) || radius<0) return;
    // Observations only: ring samples cannot certify all intervening terrain.
    for(int i=-1;i<16;++i) {
        const float angle=i*6.283185307f/16.f;
        const float x=centre.x+(i<0?0:radius*std::cos(angle));
        const float z=centre.z+(i<0?0:radius*std::sin(angle));
        CollTriInfo* tri=mapMgr->getCurrTri(x,z,true);
        std::printf("P2_PURPLE_TERRAIN owner=%s sample=%d x=%.3f z=%.3f y=%.3f triangle=%d map_code=%u radius=%.3f\n",
            owner,i,x,z,mapMgr->getMinY(x,z,true),int(tri!=nullptr),tri?unsigned(tri->mMapCode):0,radius);
    }
}
static void auditPart(const char* owner,CollPart* part,int depth=0) {
    if(!part) return;
    require(depth<32,"collision audit tree depth");
    std::printf("P2_PURPLE_COLLISION_PART owner=%s depth=%d id=%u type=%u active=%d xyz=%.3f,%.3f,%.3f radius=%.3f\n",
        owner,depth,unsigned(part->getID().mId),unsigned(part->mPartType),int(part->mIsUpdateActive),
        part->mCentre.x,part->mCentre.y,part->mCentre.z,part->mRadius);
    for(int i=0;i<part->getChildCount();++i) auditPart(owner,part->getChildAt(i),depth+1);
}
static void auditBody(const char* owner,Creature* creature) {
    if(!creature || !creature->isAlive()) return;
    const Vector3f centre=creature->getBoundingSphereCentre();
    const float radius=creature->getBoundingSphereRadius();
    std::printf("P2_PURPLE_BODY owner=%s xyz=%.3f,%.3f,%.3f bound=%.3f,%.3f,%.3f radius=%.3f collision_radius=%.3f\n",
        owner,creature->mSRT.t.x,creature->mSRT.t.y,creature->mSRT.t.z,centre.x,centre.y,centre.z,radius,creature->mCollisionRadius);
    if(creature->mCollInfo && creature->mCollInfo->hasInfo()) auditPart(owner,creature->mCollInfo->getBoundingSphere());
    auditTerrain(owner,centre,radius);
}
// Read-only fixture telemetry. Form inherited member pointers in a derived
// scope, then apply them to the actual ActTransport; no layout casts/mutations.
struct PurpleTransportTrace : ActTransport {
    static void emit(ActTransport* action, Piki* piki) {
        const int state=action->*(&PurpleTransportTrace::mState);
        if(state!=STATE_Move && state!=STATE_Guru && state!=STATE_Goal) {
            std::printf("P2_PURPLE_HAUL_ACTION state=%d route_not_started=1\n",state);return;
        }
        const int count=action->*(&PurpleTransportTrace::mNumRoutePoints);
        const int index=action->*(&PurpleTransportTrace::mPathIndex);
        const int next=action->*(&PurpleTransportTrace::mNextPathIndex);
        static bool routeAudited=false;
        if(!routeAudited && routeMgr && count>0) {
            routeAudited=true;
            for(int i=0;i<count;++i) {
                const int id=piki->mPathBuffers[i].mWayPointIdx;
                WayPoint* wp=routeMgr->getWayPoint('test',id);
                if(wp) std::printf("P2_PURPLE_FULL_ROUTE index=%d id=%d xyz=%.3f,%.3f,%.3f open=%d water=%d\n",
                    i,id,wp->mPosition.x,wp->mPosition.y,wp->mPosition.z,int(wp->mIsOpen),int(wp->inWater()));
            }
        }
        std::printf("P2_PURPLE_HAUL_ACTION state=%d route_count=%d path_index=%d next_index=%d path_type=%d slot=%d can_carry=%d stall_timer=%.3f better_pathfinding=%d\n",
            state,count,index,next,
            int(action->*(&PurpleTransportTrace::mPathType)),action->*(&PurpleTransportTrace::mSlotIndex),
            int(action->*(&PurpleTransportTrace::mCanCarry)),action->*(&PurpleTransportTrace::mPcStallTimer),int(pc_settings_get_better_pathfinding()));
        if(routeMgr && index>=0 && index<count) {
            const int id=piki->mPathBuffers[index].mWayPointIdx;
            WayPoint* wp=routeMgr->getWayPoint('test',id);
            if(wp) std::printf("P2_PURPLE_HAUL_WAYPOINT id=%d xyz=%.2f,%.2f,%.2f open=%d flags=%u\n",
                id,wp->mPosition.x,wp->mPosition.y,wp->mPosition.z,int(wp->mIsOpen),unsigned(wp->mFlags));
        }
    }
};
class PurpleCombatApp : public PlugPikiApp {
    int ticks=0, phase=0, phaseTicks=0, startingField=0, pluckAttempts=0;
    int combatTicks=0, observedTicks=0, throwAttempts=0, throwTick=0;
    bool captainSeen=false, thrown=false, descentStaged=false, isolated=false;
    bool activeSeen=false,guardInjected=false,haulAttached=false;
    Piki* input=nullptr;
    Piki* acquired=nullptr;
    bool manualWithdrawRequested=false;
    BTeki* target=nullptr;
    unsigned targetUid=0;
    float initialHealth=0, maxQueued=0, regeneration=0;
    int regenerationFrames=0;
    Vector3f parkPosition;
    bool sunsetRequested=false, sunsetSeen=false;
    int sunsetTicks=0, sunsetDay=-1, expectedDay=-1, savedMaturity=-1, resumeReady=0;
    unsigned saveIndexBefore=0;
    bool mode(const char* name) const {
        const char* value=std::getenv("P2_PURPLE_COMBAT_MODE");
        return value && std::strcmp(value,name)==0;
    }
    void injectInitializedGuard(Navi* n) {
        const char* test=std::getenv("P2_PURPLE_GUARD_CASE");
        if(!test && std::getenv("P2_FIXTURE_FORCE_CAPTAIN_DOWN")) test="health";
        if(!test || guardInjected || !n || !n->getCurrState() || !mapMgr || !pikiMgr) return;
        const int state=n->getCurrState()->getID();
        if(state!=NAVISTATE_Walk && state!=NAVISTATE_Idle) return;
        // Startup briefly exposes a Walk captain before the landing movie and
        // squad exist. Inject only after the required live fixture baseline;
        // the unconditional real guard below still protects every earlier frame.
        if(gameflow.mPauseAll || gameflow.mIsUIOverlayActive
            || (gameflow.mMoviePlayer && gameflow.mMoviePlayer->mIsActive)) return;
        GameStat::update();
        if(int(GameStat::mapPikis)!=20) return;
        guardInjected=true;captainSeen=true;
        std::printf("P2_PURPLE_GUARD_ARMED case=%s initialized=1 tick=%d health_before=%.3f field=%d state=%d\n",test,ticks,n->mHealth,int(GameStat::mapPikis),state);
        milestone("initialized_guard_injection",ticks);
        if(!std::strcmp(test,"health_pause")) gameflow.mPauseAll=true;
        if(!std::strcmp(test,"missing_movie")) {
            require(gameflow.mMoviePlayer!=nullptr,"initialized movie guard requires movie player");
            gameflow.mMoviePlayer->mIsActive=true;
        }
        if(!std::strcmp(test,"missing") || !std::strcmp(test,"missing_movie")) naviMgr=nullptr;
        else if(!std::strcmp(test,"manager")) naviMgr->informOrimaDead(n);
        else if(!std::strcmp(test,"global")) GameStat::orimaDead=true;
        else if(!std::strcmp(test,"dead_state")) n->mStateMachine->transit(n,NAVISTATE_Dead);
        else n->mHealth=0;
        std::printf("P2_PURPLE_GUARD_INJECTED case=%s pause=%d movie=%d manager_present=%d\n",test,int(gameflow.mPauseAll),int(gameflow.mMoviePlayer && gameflow.mMoviePlayer->mIsActive),int(naviMgr!=nullptr));
    }
    bool stockOne() const {
        return savedMaturity>=0 && savedMaturity<3 && p2ship::stock.total()==1
            && p2ship::stock.counts[0][savedMaturity]==1;
    }
    void boundAdult(bool liveRequired=true) {
        require(pc_p2_purple_direct_enabled(),"combat profile missing after save/restart");
        // Restart confirms the map's default Impact Site, where this Hope
        // generator is absent. The saved seed mapping must still be exact;
        // never manufacture an actor merely to validate an absent-stage row.
        require(pc_randomizer_p2_source_for_id(3640055869u)==2,"persisted seed combat mapping mismatch");
        bool found=false;
        Iterator it(tekiMgr);
        CI_LOOP(it) {
            BTeki* actor=static_cast<BTeki*>(*it);
            if (actor && actor->isAlive() && pc_p2_campaign_source(actor)==2
                && pc_p2_purple_direct_adult_registered(actor)) {
                std::printf("P2_PURPLE_PERSIST_BINDING uid=%u source=2 registered=1\n",pc_p2_campaign_token(actor));
                found=true; break;
            }
        }
        std::printf("P2_PURPLE_PERSIST_CATALOG uid=3640055869 source=2 profile_enabled=1 live_registered=%d live_required=%d stage=%d\n",
            int(found),int(liveRequired),gameflow.mCurrentStageID);
        require(!liveRequired || found,"no exact live adult combat binding in persistence fixture");
    }
    void sunsetStep() {
        require(++sunsetTicks<9000,"ordinary day-save timeout");
        require(gameflow.mCurrGameSectionID==SECTION_OnePlayer && flowCont.mGameEndFlag==GAMEEND_None,
            "sunset left ordinary healthy campaign");
        if(gameflow.mIsDayEndActive) sunsetSeen=true;
        require(gameflow.mWorldClock.mCurrentDay<=expectedDay,"unexpected extra day advance");
        if(sunsetSeen && gameflow.mGamePrefs.mHasSaveGame
            && gameflow.mGamePrefs.mMostRecentSaveIndex!=saveIndexBefore) {
            require(gameflow.mWorldClock.mCurrentDay==expectedDay && stockOne(),"day-save identity/maturity/day mismatch");
            pc_p2_input_script_clear(1);
            std::printf("P2_PURPLE_PERSIST_SAVED day_before=%d day=%d maturity=%d stock=1 native_save_index_before=%u native_save_index_after=%u external_CAMPAIGN_SAVED_required=1 identity_injected=0 maturity_injected=0\n",
                sunsetDay,expectedDay,savedMaturity,saveIndexBefore,unsigned(gameflow.mGamePrefs.mMostRecentSaveIndex));
            std::fflush(nullptr); std::_Exit(0);
        }
        const bool confirming=sunsetSeen && gameflow.mWorldClock.mCurrentDay==expectedDay;
        pc_p2_input_script_set(1,confirming && sunsetTicks%20<4?KBBTN_A:0,0,0);
        if(sunsetTicks%120==0) std::printf("P2_PURPLE_PERSIST_PROGRESS ticks=%d day=%d active=%d stock=%d\n",
            sunsetTicks,gameflow.mWorldClock.mCurrentDay,int(gameflow.mIsDayEndActive),p2ship::stock.total());
    }
    void beginPersistence(Navi* n) {
        require(acquired && acquired->isAlive() && pc_p2_is_purple(acquired),"persistence requires naturally acquired Purple");
        require(!pc_randomizer_resumed() && p2ship::stock.total()==0,"day-save requires fresh empty baseline");
        boundAdult(); savedMaturity=acquired->mHappa;
        require(savedMaturity>=0 && savedMaturity<3,"invalid acquired maturity");
        GameStat::update(); const int field=GameStat::mapPikis;
        require(field==20,"fresh conversion must conserve starting20");
        require(pc_p2_ship_deposit(acquired) && stockOne() && int(GameStat::mapPikis)==field-1,"deposit identity/population");
        Piki* restored=pc_p2_ship_withdraw(n,3);
        require(restored && pc_p2_is_purple(restored) && !restored->mP2White
            && restored->mHappa==savedMaturity && pc_piki_carry_strength(restored)==10
            && pc_throw_selection_class(restored)==4 && p2ship::stock.total()==0
            && int(GameStat::mapPikis)==field && restored->mMode==PikiMode::FormationMode,"withdraw identity/population");
        sunsetDay=gameflow.mWorldClock.mCurrentDay; expectedDay=pc_randomizer_next_day(sunsetDay);
        require(expectedDay==sunsetDay+1 && flowCont.mGameEndFlag==GAMEEND_None,"ordinary next day required");
        saveIndexBefore=gameflow.mGamePrefs.mMostRecentSaveIndex;
        sunsetRequested=true; acquired=nullptr; input=nullptr;
        pc_p2_input_script_set(1,0,0,0);
        std::printf("P2_PURPLE_PERSIST_BEGIN day=%d expected_day=%d maturity=%d field=%d stock=0 identity_injected=0 maturity_injected=0 clock_advanced=1 menu_input_scripted=1\n",
            sunsetDay,expectedDay,savedMaturity,field);
        gameflow.mWorldClock.setTime(gameflow.mParameters->mEndHour());
    }
    static int expectedNumber(const char* name,int minimum,int maximum) {
        const char* text=std::getenv(name); require(text && *text,"missing restart expectation");
        char* end=nullptr; errno=0; const long n=std::strtol(text,&end,10);
        require(!errno && end && !*end && n>=minimum && n<=maximum,"invalid restart expectation"); return int(n);
    }
    void resumePersistence(Navi* n) {
        savedMaturity=expectedNumber("P2_PURPLE_EXPECT_MATURITY",0,2);
        expectedDay=expectedNumber("P2_PURPLE_EXPECT_DAY",1,99999);
        require(pc_randomizer_resumed() && gameflow.mWorldClock.mCurrentDay==expectedDay && stockOne(),
            "native restart checkpoint/day/stock/maturity mismatch");
        if(++resumeReady<60) return;
        boundAdult(false); GameStat::update(); const int field=GameStat::mapPikis;
        Iterator bodies(pikiMgr); CI_LOOP(bodies) {
            Piki* p=static_cast<Piki*>(*bodies);
            require(!p || !p->isAlive() || !pc_p2_is_purple(p),"duplicate field Purple before withdrawal");
        }
        Piki* restored=pc_p2_ship_withdraw(n,3);
        require(restored && pc_p2_is_purple(restored) && !restored->mP2White
            && restored->mHappa==savedMaturity && pc_piki_carry_strength(restored)==10
            && pc_throw_selection_class(restored)==4 && p2ship::stock.total()==0
            && int(GameStat::mapPikis)==field+1,"restart withdrawal identity/population");
        require(!pc_p2_ship_withdraw(n,3),"restart duplicated stock");
        require(pc_p2_ship_deposit(restored) && stockOne() && int(GameStat::mapPikis)==field,"restart redeposit conservation");
        pc_p2_input_script_clear(1);
        std::printf("P2_PURPLE_PERSIST_RESUME_PASS day=%d maturity=%d stock=1 field_before=%d field_after=%d checkpoint_resumed=1 identity_injected=0 maturity_injected=0 strength=10 selection=4\n",
            expectedDay,savedMaturity,field,int(GameStat::mapPikis));
        std::fflush(nullptr); std::_Exit(0);
    }

    Pellet* haul=nullptr;
    GoalItem* haulGoal=nullptr;
    Piki* haulRed=nullptr;
    Vector3f haulStart, approachStart;
    int haulPhase=0, haulTicks=0, haulStable=0, rewardBefore=0, expectedReward=0, haulMaturity=-1, haulPopulationBefore=0;
    bool haulMoved=false, haulGoalSeen=false, haulGone=false;
    int haulRecalls=0;
    void keepHaulSquad(Navi* n) {
        // Formation can itself auto-assign transport on pellet contact. Keep
        // non-test squad members gathered throughout the run, including the
        // retired Red control. Never detach an actual helper to hide a failure.
        Iterator squad(pikiMgr); CI_LOOP(squad) {
            Piki* p=static_cast<Piki*>(*squad);
            if(!p || !p->isAlive() || p==acquired || (p==haulRed && haulPhase<=2)) continue;
            if(p->getStickObject()==haul) {
                std::printf("P2_PURPLE_HAUL_EXTRA_CARRIER purple=%d mode=%d state=%d strength=%d\n",
                    int(pc_p2_is_purple(p)),p->mMode,p->getState(),int(haul->mCarrierCounter));
                require(false,"non-test Pikmin attached before fixture recall");
            }
            if(p->getState()==PIKISTATE_Normal && p->mMode!=PikiMode::FormationMode) {
                require(p->mNavi==n,"non-test squad captain changed");
                p->changeMode(PikiMode::FormationMode,n);
                ++haulRecalls;
            }
        }
    }
    void assignHaul(Piki* p) {
        require(p && p->isAlive() && p->getState()==PIKISTATE_Normal && !p->isStickTo(),"carrier not ready for native approach");
        p->mActiveAction->abandon(nullptr);
        p->mActiveAction->mCurrActionIdx=PikiAction::Transport;
        p->mActiveAction->mChildActions[PikiAction::Transport].initialise(haul);
        p->mMode=PikiMode::TransportMode;
        approachStart=p->mSRT.t;
        haulAttached=false;milestone("carrier_assignment",ticks);
        std::printf("P2_PURPLE_HAUL_ASSIGN purple=%d native_approach=1 slot_teleport=0 forced_attachment=0 x=%.2f z=%.2f\n",
            int(pc_p2_is_purple(p)),p->mSRT.t.x,p->mSRT.t.z);
    }
    void transportStep(Navi* n) {
        const bool redOnly=mode("transport_red_control");
        const bool manual=mode("transport_manual");
        const bool staged=mode("transport_staged") || manual;
        const bool positiveOnly=mode("transport_positive") || staged;
        require(++haulTicks<7200,"native transport/delivery timeout");
        require(acquired && acquired->isAlive() && !acquired->mP2White
            && (redOnly ? !pc_p2_is_purple(acquired) && acquired->mColor==Red : pc_p2_is_purple(acquired)),"test carrier identity lost");
        if(!haulPhase) {
            require(std::fabs(pc_settings_get_carry_speed_scale()-1.f)<0.001f,"default production carry speed required");
            haulGoal=itemMgr->getContainer(Red);
            require(haulGoal && haulGoal->mOnionColour==Red,"active Red Onion required");
            if(redOnly) haulRed=acquired;
            else if(!positiveOnly) { Iterator squad(pikiMgr); CI_LOOP(squad) {
                Piki* p=static_cast<Piki*>(*squad);
                if(p && p->isAlive() && !pc_p2_is_purple(p) && !p->mP2White && p->mColor==Red
                    && p->getState()==PIKISTATE_Normal && p->mMode==PikiMode::FormationMode) { haulRed=p; break; }
            } }
            require(positiveOnly || haulRed,"ordinary Red control missing");
            // Plucking may leave nearby non-test Pikmin idle/free. Explicitly
            // gather them before introducing cargo so they cannot invalidate
            // the single-carrier control. No actor position is changed.
            Iterator nonTest(pikiMgr); CI_LOOP(nonTest) {
                Piki* p=static_cast<Piki*>(*nonTest);
                if(p && p->isAlive() && p!=haulRed && p!=acquired) {
                    p->mActiveAction->abandon(nullptr);
                    p->changeMode(PikiMode::FormationMode,n);
                }
            }
            std::puts("P2_PURPLE_HAUL_ISOLATION non_test_squad_formation=1 actor_position_injected=0");
            haul=pelletMgr->newNumberPellet(Red,NUMPEL_TenPellet);
            require(haul && haul->mConfig->mCarryMinPikis()==10,"standard weight10 pellet missing");
            expectedReward=haul->mConfig->mMatchingOnyonSeeds();require(expectedReward>0,"positive matching Onion yield required");
            const char* route=std::getenv("P2_PURPLE_HAUL_ROUTE");
            require(!route || !std::strcmp(route,"east197") || !std::strcmp(route,"west197"),"unknown fixture cargo route");
            const bool west=route && !std::strcmp(route,"west197");
            Vector3f pos=haulGoal->mSRT.t+Vector3f(west?-180:180,0,80);pos.y=mapMgr->getMinY(pos.x,pos.z,true)+5;
            require(std::isfinite(pos.y),"cargo placement terrain invalid");
            std::printf("P2_PURPLE_HAUL_ORIGIN route=%s xyz=%.2f,%.2f,%.2f distance_xz=196.98 violet_unchanged=1\n",west?"west197":"east197",pos.x,pos.y,pos.z);
            haul->init(pos);haul->startAI(TRUE);haulStart=pos;
            milestone("cargo_spawn",ticks);
            GameStat::update();haulPopulationBefore=GameStat::allPikis[Red];
            rewardBefore=GameStat::bornPikis[Red];haulMaturity=acquired->mHappa;haulPhase=1;haulTicks=0;
            std::printf("P2_PURPLE_HAUL_BEGIN injected_cargo=1 weight=10 expected_reward=%d maturity=%d source_identity_injected=%d cargo_position_staged_once=1\n",expectedReward,haulMaturity,int(staged));
            return;
        }
        require(acquired->mHappa==haulMaturity,"carrier maturity changed");
        keepHaulSquad(n);
        bool present=false;Iterator pellets(pelletMgr);CI_LOOP(pellets) if(static_cast<Pellet*>(*pellets)==haul){present=true;break;}
        const bool alive=present && haul->isAlive();
        const int reward=GameStat::bornPikis[Red]-rewardBefore;
        require(reward>=0 && reward<=expectedReward,"unexpected/duplicate Red reward");
        if(haulPhase==1) {
            require(alive,"cargo disappeared before assignment");
            const Vector3f d=haul->mSRT.t-haulStart;const float drift=d.x*d.x+d.z*d.z;
            const bool settling=(!redOnly && !positiveOnly && haulTicks<90)
                || drift>=0.25f || !haul->onGround() || std::fabs(d.y)>0.25f;
            if(settling){haulStart=haul->mSRT.t;haulStable=0;}else ++haulStable;
            if(haulStable>=30 && haul->isVisible() && haul->getState()==PELSTATE_Normal){
                haulStart=haul->mSRT.t;
                if(manual) {
                    approachStart=acquired->mSRT.t;pc_p2_input_script_clear(1);
                    std::puts("P2_PURPLE_MANUAL_READY field=20 purple=1 reds=19 starting_species_injected=1 native_acquisition=0 scripted_transport_assignment=0 actor_position_injected=0 cargo_spawned_once=1 reset=F7_or_Reset_cmd");
                    if(std::getenv("P2_PURPLE_MANUAL_BOOT_ONLY")) { std::fflush(nullptr);std::_Exit(0); }
                } else assignHaul(positiveOnly?acquired:haulRed);
                haulPhase=positiveOnly?3:2;haulTicks=0;haulStable=0;
            }
        } else if(haulPhase==2) {
            require(alive && reward==0,"control consumed cargo or generated reward");
            const Vector3f d=haul->mSRT.t-haulStart;require(d.x*d.x+d.z*d.z<4,"single Red moved weight10 cargo");
            require(haul->mCarrierCounter<=1,"unexpected helper in control");
            if(haulRed->getStickObject()==haul && haul->mCarrierCounter==1) ++haulStable;
            if(haulStable>=90) {
                haulRed->mActiveAction->abandon(nullptr);haulRed->changeMode(PikiMode::FormationMode,n);
                require(haulRed->getStickObject()!=haul,"Red control did not detach");
                std::puts("P2_PURPLE_HAUL_RED_CONTROL_PASS strength=1 attached_observations=90 displacement_under_2=1 reward=0");
                if(redOnly) {
                    GameStat::update();
                    require(GameStat::allPikis[Red]==haulPopulationBefore,"Red control population changed");
                    std::puts("P2_PURPLE_HAUL_RED_ONLY_PASS population_unchanged=1 released=1 source_identity_injected=0");
                    std::fflush(nullptr);std::_Exit(0);
                }
                haulStart=haul->mSRT.t;assignHaul(acquired);haulPhase=3;haulTicks=0;haulStable=0;
            }
        } else {
            if(alive) {
                int attached=0;Iterator bodies(pikiMgr);CI_LOOP(bodies) if(static_cast<Piki*>(*bodies)->getStickObject()==haul) ++attached;
                require(attached<=1 && haul->mCarrierCounter<=10,"extra carrier invalidates single-Purple haul");
                if(!haulAttached && attached==1 && haul->mCarrierCounter==10) {haulAttached=true;milestone("carrier_attached",ticks);}
                const Vector3f d=haul->mSRT.t-haulStart;
                if(!haulMoved && attached==1 && haul->mCarrierCounter==10 && d.x*d.x+d.z*d.z>100) {
                    const Vector3f approach=acquired->mSRT.t-approachStart;
                    require(approach.x*approach.x+approach.z*approach.z>25,"native approach not observed");
                    haulMoved=true;
                    milestone("cargo_moved_ten",ticks);
                    std::printf("P2_PURPLE_HAUL_MOVEMENT_PASS strength=10 attached=1 distance=%.2f native_approach=1 forced_attachment=0 cargo_teleport_after_spawn=0\n",std::sqrt(d.x*d.x+d.z*d.z));
                }
                if(haul->getState()==PELSTATE_Goal) {
                    require(haulMoved && haul->mTargetGoal==static_cast<Suckable*>(haulGoal),"wrong destination or missing native transport");
                    if(!haulGoalSeen) {milestone("onion_uptake",ticks);std::puts("P2_PURPLE_HAUL_ONION_UPTAKE target=red_onion native_goal_state=1");}
                    haulGoalSeen=true;
                }
            } else {
                require(haulGoalSeen && haulMoved,"cargo vanished without observed Onion uptake");haulGone=true;
            }
            if(haulGone && reward==expectedReward && acquired->getStickObject()!=haul && acquired->getState()==PIKISTATE_Normal) {
                if(++haulStable>=90) {
                    GameStat::update();
                    require(GameStat::allPikis[Red]-haulPopulationBefore==expectedReward,"reward counter/population mismatch");
                    milestone("delivery_verified",ticks);
                    std::printf("P2_PURPLE_HAUL_POPULATION before=%d after=%d expected_delta=%d\n",haulPopulationBefore,GameStat::allPikis[Red],expectedReward);
                    std::printf("P2_PURPLE_HAUL_DELIVERY_PASS reward=%d expected_reward=%d purple_alive=1 maturity=%d released=1 stable_ticks=%d duplicate_reward=0 injected_cargo=1 scripted_action_assignment=%d controls_validated=0 scripted_non_test_recalls=%d starting_species_injected=%d\n",reward,expectedReward,haulMaturity,haulStable,int(!manual),haulRecalls,int(staged));
                    std::fflush(nullptr);std::_Exit(0);
                }
            } else haulStable=0;
        }
        if(haulTicks%60==0) {
            std::printf("P2_PURPLE_HAUL_PROGRESS phase=%d ticks=%d cargo_alive=%d cargo_state=%d strength=%d purple_state=%d attached=%d reward=%d expected=%d recalls=%d grounded=%d stable_ticks=%d\n",
                haulPhase,haulTicks,int(alive),alive?haul->getState():-1,alive?int(haul->mCarrierCounter):0,acquired->getState(),int(acquired->getStickObject()==haul),reward,expectedReward,haulRecalls,int(alive && haul->onGround()),haulStable);
            if(alive) {
                const Vector3f goal=haulGoal->getGoalPos();
                std::printf("P2_PURPLE_HAUL_ROUTE cargo=%.2f,%.2f,%.2f velocity=%.2f,%.2f,%.2f goal=%.2f,%.2f,%.2f target_red=%d target_present=%d goal_waypoint=%d computed_speed=%.3f\n",
                    haul->mSRT.t.x,haul->mSRT.t.y,haul->mSRT.t.z,haul->mVelocity.x,haul->mVelocity.y,haul->mVelocity.z,
                    goal.x,goal.y,goal.z,int(haul->mTargetGoal==static_cast<Suckable*>(haulGoal)),
                    int(haul->mTargetGoal!=nullptr),haulGoal->getRouteIndex(),pc_p2_transport_speed(haul,0));
                Piki* carrier=redOnly?haulRed:acquired;
                if(carrier->mMode==PikiMode::TransportMode && carrier->mActiveAction->getCurrAction())
                    PurpleTransportTrace::emit(static_cast<ActTransport*>(carrier->mActiveAction->getCurrAction()),carrier);
            }
        }
    }

    bool approachAndPluck(Navi* n,PikiHeadItem* head) {
        // Ordinary controller movement/pluck. Never relocate the captain or
        // sprout, or force a plucking state to satisfy natural acceptance.
        const float dx=head->mSRT.t.x-n->mSRT.t.x,dz=head->mSRT.t.z-n->mSRT.t.z;
        const float distance=std::sqrt(dx*dx+dz*dz);
        if(ticks%60==0) std::printf("P2_PURPLE_PLUCK_APPROACH distance=%.3f captain=%.3f,%.3f,%.3f sprout=%.3f,%.3f,%.3f state=%d port=%u observed_stick=%d,%d frozen=%d\n",
            distance,n->mSRT.t.x,n->mSRT.t.y,n->mSRT.t.z,head->mSRT.t.x,head->mSRT.t.y,head->mSRT.t.z,
            n->getCurrState()->getID(),n->mKontroller?n->mKontroller->mPlayerNum:0,
            n->mKontroller?int(n->mKontroller->mMainStickX):0,n->mKontroller?int(n->mKontroller->mMainStickY):0,
            int(n->mKontroller && n->mKontroller->mIsControllerFrozen));
        if(distance>20.f) {
            require(n->controlCamera()!=nullptr,"natural approach camera missing");
            const Vector3f& axis=n->controlCamera()->mViewXAxis;
            const float power=distance>45.f?65.f:35.f;
            pc_p2_input_script_set(1,0,int(std::lround(power*(dx*axis.x+dz*axis.z)/distance)),
                int(std::lround(power*(dx*axis.z-dz*axis.x)/distance)));
            return false;
        }
        pc_p2_input_script_set(1,KBBTN_A);
        ++pluckAttempts;
        milestone("native_sprout_pluck_requested",ticks);
        std::printf("P2_PURPLE_PLUCK_ATTEMPT attempt=%d captain_position_staged=0 native_input=1 forced_pluck_state=0 distance=%.3f player_controls_validated=0\n",pluckAttempts,distance);
        return true;
    }
    Piki* naturalStep(Navi* n) {
        ++phaseTicks;
        if(phase==2) pc_p2_input_script_set(1,0);
        Pom* violet = nullptr; int count = 0;
        Iterator flowers(bossMgr);
        CI_LOOP(flowers) {
            Boss* b = static_cast<Boss*>(*flowers);
            if (b && b->isAlive() && b->mObjType == OBJTYPE_Pom && pc_p2_violet(static_cast<Pom*>(b))) { violet = static_cast<Pom*>(b); ++count; }
        }
        if (phase == 0) {
            Iterator existing(pikiMgr);
            CI_LOOP(existing) {
                Piki* p=static_cast<Piki*>(*existing);
                require(!p || !p->isAlive() || !pc_p2_is_purple(p), "pre-existing Purple invalidates natural acquisition");
            }
            require(count == 1, "exact bound Violet missing");
            Iterator bodies(pikiMgr);
            CI_LOOP(bodies) {
                Piki* p = static_cast<Piki*>(*bodies);
                if (p->isAlive() && p->getState() == PIKISTATE_Normal && p->mMode == PikiMode::FormationMode && !pc_p2_is_purple(p)) { input = p; break; }
            }
            if (!input) return nullptr;
            GameStat::update(); startingField = GameStat::mapPikis;
            phase = 1; phaseTicks = 0;
            milestone("natural_input_selected",ticks);
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
                milestone("native_throw",ticks);
                std::printf("P2_PURPLE_SCRIPTED_THROW real_collision=1 aim_scale=%.2f captain=%.1f,%.1f,%.1f velocity=%.1f,%.1f,%.1f\n",
                    aimScale,n->mSRT.t.x,n->mSRT.t.y,n->mSRT.t.z,input->mVelocity.x,input->mVelocity.y,input->mVelocity.z);
            }
            Iterator heads(itemMgr->getPikiHeadMgr());
            CI_LOOP(heads) {
                PikiHeadItem* h = static_cast<PikiHeadItem*>(*heads);
                if (h && h->isAlive() && h->mP2Purple && h->canPullout()) {
                    if(!approachAndPluck(n,h)) break;
                    phase=2; phaseTicks=0; input=nullptr;
                    std::puts("P2_VIOLET_REAL_SPROUT captain_pluck_requested=1 actor_position_injected=0"); break;
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
                    milestone("native_acquisition_verified",ticks);
                    std::puts("P2_PURPLE_ACQUISITION_PASS scripted_throw=1 native_conversion=1 captain_pluck=1 selection=4 strength=10");
                    return p;
                }
            }
        }
        if (phase == 2 && phaseTicks > 0 && phaseTicks % 180 == 0) {
            Iterator retryHeads(itemMgr->getPikiHeadMgr());
            CI_LOOP(retryHeads) {
                PikiHeadItem* h = static_cast<PikiHeadItem*>(*retryHeads);
                if (!h || !h->isAlive() || !h->mP2Purple || !h->canPullout()) continue;
                require(pluckAttempts < 3, "native pluck failed after three staged attempts");
                phase=1;phaseTicks=0;
                approachAndPluck(n,h);
                break;
            }
        }
        if(phaseTicks%120==0) std::printf("P2_PURPLE_NATURAL_PROGRESS phase=%d ticks=%d violet_state=%d\n",phase,phaseTicks,violet?violet->getCurrentState():-1);
        require(phaseTicks<1800,"natural acquisition timeout"); return nullptr;
    }

    bool targetPresent() const {
        if (!tekiMgr) return false;
        Iterator it(tekiMgr);
        CI_LOOP(it) {
            BTeki* actor=static_cast<BTeki*>(*it);
            if (actor==target && pc_p2_campaign_source(actor)==2
                && pc_randomizer_generator_id(actor->mGenerator)==targetUid) return true;
        }
        return false;
    }
    void diagnostics(Navi* n) {
        if(itemMgr && itemMgr->getPikiHeadMgr()) { Iterator heads(itemMgr->getPikiHeadMgr()); CI_LOOP(heads) {
            PikiHeadItem* head=static_cast<PikiHeadItem*>(*heads);
            if(head && head->isAlive() && head->mP2Purple) std::printf("P2_PURPLE_SPROUT_OBSERVATION xyz=%.3f,%.3f,%.3f pullable=%d\n",
                head->mSRT.t.x,head->mSRT.t.y,head->mSRT.t.z,int(head->canPullout()));
        } }
        if(bossMgr) { Iterator flowers(bossMgr); CI_LOOP(flowers) {
            Boss* b=static_cast<Boss*>(*flowers);
            if(b && b->isAlive() && b->mObjType==OBJTYPE_Pom && pc_p2_violet(static_cast<Pom*>(b))) auditBody("violet",b);
        } }
        if(haul && pelletMgr) { Iterator bodies(pelletMgr); CI_LOOP(bodies) {
            if(static_cast<Pellet*>(*bodies)==haul) {auditBody("cargo",haul);break;}
        } }
        const char* state="unavailable"; const char* clip="unavailable"; float clipPhase=0;
        const bool present=target && targetPresent();
        if (present) pc_p2_chappy_probe(target,&state,&clip,&clipPhase);
        if (present && acquired) std::printf("P2_PURPLE_COMBAT_GEOMETRY uid=%u target=%.3f,%.3f,%.3f source=%.3f,%.3f,%.3f source_velocity=%.3f,%.3f,%.3f\n",
            targetUid,target->mSRT.t.x,target->mSRT.t.y,target->mSRT.t.z,
            acquired->mSRT.t.x,acquired->mSRT.t.y,acquired->mSRT.t.z,
            acquired->mVelocity.x,acquired->mVelocity.y,acquired->mVelocity.z);
        const auto flight=pc_p2_purple_flight_sample(acquired);
        std::printf("P2_PURPLE_COMBAT_PROGRESS tick=%d acquisition_phase=%d acquisition_ticks=%d combat_ticks=%d navi=%d pause=%d ui=%d target_present=%d uid=%u source=2 health=%.3f queued=%.3f enemy_state=%s clip=%s clip_phase=%.3f piki_state=%d flight=%d staged=%d isolated=%d\n",
            ticks,phase,phaseTicks,combatTicks,n&&n->getCurrState()?n->getCurrState()->getID():-1,
            int(gameflow.mPauseAll),int(gameflow.mIsUIOverlayActive),int(present),targetUid,
            present?target->mHealth:-1.f,present?target->mStoredDamage:-1.f,state?state:"null",clip?clip:"null",clipPhase,
            acquired?acquired->getState():-1,int(flight.phase),int(descentStaged),int(isolated));
    }
    void combatStep(Navi* n) {
        require(++combatTicks<900,"adult direct contact/damage timeout (see progress)");
        require(acquired->isAlive() && pc_p2_is_purple(acquired),"acquired Purple lost");
        if (!target) {
            require(pc_p2_purple_direct_enabled() && pc_p2_purple_flight_enabled(),"direct/flight profiles disabled");
            unsigned requested=0;
            const char* uidText=std::getenv("P2_PURPLE_COMBAT_UID");
            if (uidText) {
                char* end=nullptr; errno=0;
                const unsigned long value=std::strtoul(uidText,&end,10);
                require(*uidText && *uidText!='-' && end && !*end && !errno && value>0 && value<=UINT_MAX,"invalid decimal target UID");
                requested=static_cast<unsigned>(value);
            }
            Iterator enemies(tekiMgr);
            CI_LOOP(enemies) {
                BTeki* candidate=static_cast<BTeki*>(*enemies);
                if (!candidate || !candidate->isAlive() || pc_p2_campaign_source(candidate)!=2
                    || !pc_p2_chappy_registered(candidate) || !pc_p2_purple_direct_adult_registered(candidate)) continue;
                const unsigned uid=pc_randomizer_generator_id(candidate->mGenerator);
                if (requested && uid!=requested) continue;
                if (!target || uid<targetUid) { target=candidate; targetUid=uid; }
            }
            if (!target) return; // Later generated actors may register after stage entry.
            require(target->mHealth>50.f && std::isfinite(target->mHealth) && target->mStoredDamage==0.f,
                "adult must be alive above 50 HP with no pending damage");
            parkPosition=n->mSRT.t;
            // Park other squad members at the acquisition site and detach them
            // from formation before moving the captain. No enemy is relocated.
            Iterator squad(pikiMgr);
            CI_LOOP(squad) {
                Piki* p=static_cast<Piki*>(*squad);
                if (p && p!=acquired && p->isAlive()) {
                    require(!p->isStickTo(),"other squad member already attached");
                    p->changeMode(PikiMode::FreeMode,n); p->resetPosition(parkPosition);
                }
            }
            initialHealth=target->mHealth;
            std::printf("P2_PURPLE_COMBAT_BASELINE uid=%u target=%p view=%p health=%.3f native_max=%.3f family_max=%.3f regen_rate=%.6f\n",
                targetUid,static_cast<void*>(target),static_cast<void*>(static_cast<PelletView*>(target)),
                initialHealth,target->getMaxLife(),pc_p2_chappy_max_health(target,-1.f),target->getParameterF(TPF_LifeRecoverRate));
            require(std::isfinite(target->getMaxLife()) && std::fabs(initialHealth-target->getMaxLife())<0.01f,
                "adult must start at native maximum health for regeneration accounting");
            std::printf("P2_PURPLE_COMBAT_TARGET uid=%u source=2 target=%p piki=%p health_before=%.3f queued_before=%.3f generated_actor=1 adapter_registered=1 other_squad_parked=1\n",
                targetUid,static_cast<void*>(target),static_cast<void*>(acquired),initialHealth,target->mStoredDamage);
        }
        require(targetPresent() && target->isAlive(),"configured generated adult disappeared/died");
        require(pc_p2_chappy_registered(target) && pc_p2_purple_direct_adult_registered(target),"adult adapter registration lost");
        if (!thrown) {
            const Vector3f pos=target->mSRT.t;
            n->resetPosition(Vector3f(pos.x-90.f,mapMgr->getMinY(pos.x-90.f,pos.z,true),pos.z));
            acquired->changeMode(PikiMode::FreeMode,n);
            acquired->mFSM->transit(acquired,PIKISTATE_Flying);
            n->throwPiki(acquired,Vector3f(pos.x+42.f,pos.y,pos.z));
            require(pc_p2_purple_flight_active(acquired),"native throw did not arm Purple flight");
            thrown=true; throwTick=combatTicks; ++throwAttempts;
            std::printf("P2_PURPLE_COMBAT_THROW attempt=%d uid=%u source=2 target=%p piki=%p native_throw=1 captain_position_staged=1 enemy_modified=0 controls_validated=0\n",
                throwAttempts,targetUid,static_cast<void*>(target),static_cast<void*>(acquired));
            return;
        }
        maxQueued=target->mStoredDamage>maxQueued?target->mStoredDamage:maxQueued;
        const float delta=initialHealth-target->mHealth;
        require(std::isfinite(delta) && std::isfinite(target->mStoredDamage),"nonfinite target damage");
        // BTeki::update applies queued damage through chappy_update, then
        // regenerates dt * (getMaxLife() * LifeRecoverRate). Include the first
        // observed damaged frame. Earlier full-health frames clamp to maximum
        // and contribute nothing. Each subsequent still-damaged frame counts.
        const float dt=NSystem::getFrameTime();
        const float maximum=target->getMaxLife();
        const float rate=target->getParameterF(TPF_LifeRecoverRate);
        const float frameRecovery=dt*(maximum*rate);
        require(std::isfinite(dt) && dt>0.f && dt<=0.5f && std::isfinite(maximum)
            && maximum>0.f && std::fabs(maximum-initialHealth)<0.01f
            && std::isfinite(rate) && rate>=0.f && std::isfinite(frameRecovery),
            "invalid/changing native regeneration parameters");
        if (delta>0.f) {
            regeneration+=frameRecovery; ++regenerationFrames;
            require(regeneration<5.f,"native regeneration exceeds bounded 5 HP observation budget");
            std::printf("P2_PURPLE_COMBAT_REGEN uid=%u frame=%d dt=%.6f max_health=%.3f rate=%.8f frame_recovery=%.6f total_recovery=%.6f raw_delta=%.6f compensated_delta=%.6f\n",
                targetUid,regenerationFrames,dt,maximum,rate,frameRecovery,regeneration,delta,delta+regeneration);
        }
        const auto flight=pc_p2_purple_flight_sample(acquired);
        // Let the native throw reach its descent phase. Stage only the source
        // once; collision traversal and attack dispatch remain engine-owned.
        // This bounded collision setup deliberately does not certify aiming.
        if (!descentStaged && !isolated && delta==0.f && maxQueued==0.f
            && flight.phase==PcP2PurpleFlightPhase::Descent) {
            acquired->resetPosition(target->mSRT.t+Vector3f(0,80,0));
            acquired->mVelocity=Vector3f(0,-100,0);
            acquired->mTargetVelocity=acquired->mVelocity;
            descentStaged=true;
            std::printf("P2_PURPLE_COMBAT_DESCENT_SETUP uid=%u source=2 source_position_staged=1 source_velocity_staged=1 flight_phase_injected=0 enemy_modified=0 collision_injected=0\n",targetUid);
        }
        if (!isolated && (delta>0.f || maxQueued>0.f)) {
            // Stop follow-up ordinary attacks after native collision evidence.
            // Do not touch enemy damage queues, health, or FSM.
            if (acquired->isStickTo()) acquired->endStickObject();
            acquired->changeMode(PikiMode::FreeMode,n);
            acquired->resetPosition(parkPosition);
            n->resetPosition(parkPosition);
            isolated=true;
            std::printf("P2_PURPLE_COMBAT_CONTACT_OBSERVATION uid=%u source=2 target=%p piki=%p health_before=%.3f health_after=%.3f delta=%.3f queued=%.3f flight=%d post_contact_source_isolated=1 production_collision_marker_required=1\n",
                targetUid,static_cast<void*>(target),static_cast<void*>(acquired),initialHealth,target->mHealth,delta,target->mStoredDamage,int(flight.phase));
        }
        // Retry only an undamaging completed throw after native recovery; do
        // not force the Pikmin state/flight to finish or replay a claimed hit.
        if (!isolated && combatTicks-throwTick>=180 && delta==0.f && maxQueued==0.f
            && acquired->getState()==PIKISTATE_Normal && !acquired->isStickTo()
            && !pc_p2_purple_flight_active(acquired)) {
            require(throwAttempts<3,"three completed native throws missed adult direct damage");
            std::printf("P2_PURPLE_COMBAT_RETRY uid=%u source=2 completed_attempt=%d native_recovery=1 health=%.3f queue=%.3f\n",
                targetUid,throwAttempts,target->mHealth,target->mStoredDamage);
            thrown=false; descentStaged=false;
        }
        if (isolated && ++observedTicks>=30 && target->mStoredDamage==0.f) {
            require(delta>45.f && delta<=50.f && std::fabs(delta+regeneration-50.f)<0.05f,
                "adult native health delta plus measured regeneration is not 50");
            require(maxQueued==0.f || std::fabs(maxQueued-50.f)<0.01f,"unexpected observed queued damage");
            std::printf("P2_PURPLE_COMBAT_HEALTH_EVIDENCE mode=adult_direct uid=%u source=2 target=%p piki=%p health_before=%.3f health_after=%.3f delta=%.3f max_observed_queue=%.3f regeneration=%.6f compensated_delta=%.6f regen_frames=%d exact_production_queue_delta_required=50 native_throw=1 injected_damage=0 forced_enemy_state=0 production_collision_marker_required=1 dwarf_quake_pending=1 dwarf_crush_pending=1\n",
                targetUid,static_cast<void*>(target),static_cast<void*>(acquired),initialHealth,target->mHealth,delta,maxQueued,regeneration,delta+regeneration,regenerationFrames);
            std::fflush(nullptr); std::_Exit(0);
        }
    }


public:
    int idle() override {
        const int result=PlugPikiApp::idle();
        Navi* n=naviMgr?naviMgr->getNavi():nullptr;
        // Injection modifies the initialized runtime, never an engine-free stand-in.
        // The real guard then executes before diagnostics, pause/movie or any other return.
        injectInitializedGuard(n);
        n=naviMgr?naviMgr->getNavi():nullptr;
        const bool expectedTeardown=sunsetRequested && sunsetSeen
            && gameflow.mCurrGameSectionID==SECTION_OnePlayer && flowCont.mGameEndFlag==GAMEEND_None
            && gameflow.mWorldClock.mCurrentDay==expectedDay && stockOne();
        const bool missingAllowed=!captainSeen || expectedTeardown;
        p2_fixture_require_captain(n!=nullptr,GameStat::orimaDead,naviMgr && n && naviMgr->isNaviDead(n),
            n && n->getCurrState() && n->getCurrState()->getID()==NAVISTATE_Dead,n?n->mHealth:-1.f,ticks,missingAllowed);
        if(n) {
            if(!captainSeen) milestone("initialized_captain_seen",ticks);
            captainSeen=true;
        }
        if (++ticks%120==0) diagnostics(n);
        if(mode("transport_manual") && (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_F7] || std::ifstream("manual-reset.request").good())) {
            std::puts("P2_PURPLE_MANUAL_RESET_REQUEST fresh_session_required=1");std::fflush(nullptr);std::_Exit(90);
        }
        require(ticks<(sunsetRequested?15000:6000),"global fixture timeout");
        if(mode("persistence_resume")) pc_p2_input_script_set(1,(!n || gameflow.mIsUIOverlayActive) && ticks%20<4?KBBTN_A:0,0,0);
        if(sunsetRequested) {
            sunsetStep();
            if(gameflow.mMoviePlayer&&gameflow.mMoviePlayer->mIsActive) gameflow.mMoviePlayer->requestSkip();
            return result;
        }
        if(gameflow.mMoviePlayer&&gameflow.mMoviePlayer->mIsActive) { gameflow.mMoviePlayer->requestSkip(); return result; }
        if(!n||!pikiMgr||!itemMgr||!bossMgr||!tekiMgr||!mapMgr||!n->getCurrState()
            ||gameflow.mPauseAll||gameflow.mIsUIOverlayActive) return result;
        if(!activeSeen && (n->getCurrState()->getID()==NAVISTATE_Walk || n->getCurrState()->getID()==NAVISTATE_Idle)) {
            activeSeen=true;milestone("active_gameplay",ticks);
        }
        if(mode("persistence_resume")) {
            // The restored captain exists during ship/map entry before the
            // playable stage actors are ready. Match the ordinary fixture's
            // active walk/idle gate before checking live combat bindings.
            const int state=n->getCurrState()->getID();
            if(state==NAVISTATE_Walk || state==NAVISTATE_Idle) resumePersistence(n);
            return result;
        }
        if (!acquired) {
            const int state=n->getCurrState()->getID();
            if(state==NAVISTATE_Walk||state==NAVISTATE_Idle) {
                // Ordinary visible play leaves the starting squad in its Onion.
                // This ready-scene companion explicitly uses native withdrawal.
                if(mode("transport_manual") && !manualWithdrawRequested) {
                    GameStat::update();
                    GoalItem* onion=itemMgr->getContainer(Red);
                    if(int(GameStat::mapPikis)==0 && onion && onion->getTotalStorePikis()>=20) {
                        onion->exitPikis(20);manualWithdrawRequested=true;
                        std::puts("P2_PURPLE_MANUAL_WITHDRAW count=20 native_onion_exit=1 scripted_setup=1");
                    }
                }
                if(mode("transport_red_control") || mode("transport_staged") || mode("transport_manual")) {
                    const bool stagedStart=mode("transport_staged") || mode("transport_manual");
                    if(stagedStart) {
                        GameStat::update();
                        // Native withdrawal creates actors over successive ticks.
                        // Do not stage the first exit while the rest are stored.
                        if(int(GameStat::mapPikis)!=20) return result;
                    }
                    int aliveCount=0,normalCount=0,redCount=0,formationCount=0;
                    Iterator squad(pikiMgr);CI_LOOP(squad) {
                        Piki* p=static_cast<Piki*>(*squad);
                        if(p && p->isAlive()) {
                            ++aliveCount;
                            if(p->getState()==PIKISTATE_Normal) ++normalCount;
                            if(p->mColor==Red && !pc_p2_is_purple(p) && !p->mP2White) ++redCount;
                            if(p->mMode==PikiMode::FormationMode) ++formationCount;
                        }
                        if(p && p->isAlive() && p->mColor==Red && !pc_p2_is_purple(p) && !p->mP2White
                            && p->getState()==PIKISTATE_Normal && !p->isStickTo()
                            && (p->mMode==PikiMode::FormationMode || stagedStart)) { acquired=p;break; }
                    }
                    if(!acquired && ticks%120==0) std::printf("P2_PURPLE_START_WAIT alive=%d normal=%d ordinary_red=%d formation=%d\n",aliveCount,normalCount,redCount,formationCount);
                    if(acquired) {
                        GameStat::update();require(int(GameStat::mapPikis)==20,"ordinary Red control starting squad");
                        if(mode("transport_red_control")) std::puts("P2_PURPLE_HAUL_RED_START field=20 native_red=1 source_identity_injected=0");
                        else {
                            const int previousMode=acquired->mMode;
                            acquired->changeMode(PikiMode::FormationMode,n);
                            std::printf("P2_PURPLE_STAGED_GATHER previous_mode=%d scripted_gather=1 actor_position_injected=0\n",previousMode);
                            pc_p2_make_purple(acquired);
                            require(pc_p2_is_purple(acquired) && pc_piki_carry_strength(acquired)==10,"staged Purple identity/strength");
                            int purpleCount=0,redCount=0;Iterator counted(pikiMgr);CI_LOOP(counted) {
                                Piki* p=static_cast<Piki*>(*counted);if(!p || !p->isAlive()) continue;
                                if(pc_p2_is_purple(p)) ++purpleCount;else if(p->mColor==Red && !p->mP2White) ++redCount;
                            }
                            require(purpleCount==1 && redCount==19,"staged squad identity counts");
                            std::puts("P2_PURPLE_HAUL_STAGED_START field=20 starting_species_injected=1 native_acquisition=0 actor_position_injected=0");
                        }
                    }
                } else acquired=naturalStep(n);
            }
            return result;
        }
        if(mode("persistence_dayend")) beginPersistence(n);
        else if(mode("transport_delivery") || mode("transport_positive") || mode("transport_red_control") || mode("transport_staged") || mode("transport_manual")) transportStep(n);
        else combatStep(n);
        return result;
    }
};
int main(int argc,char** argv) {
    setvbuf(stdout,nullptr,_IONBF,0);
    const char* guardCase=std::getenv("P2_PURPLE_GUARD_CASE");
    require(!guardCase || !std::strcmp(guardCase,"health") || !std::strcmp(guardCase,"manager")
        || !std::strcmp(guardCase,"global") || !std::strcmp(guardCase,"dead_state") || !std::strcmp(guardCase,"missing")
        || !std::strcmp(guardCase,"health_pause") || !std::strcmp(guardCase,"missing_movie"),"unknown initialized guard case");
    const char* mode=std::getenv("P2_PURPLE_COMBAT_MODE");
    if(mode && std::strcmp(mode,"adult_direct") && std::strcmp(mode,"persistence_dayend") && std::strcmp(mode,"persistence_resume") && std::strcmp(mode,"transport_delivery") && std::strcmp(mode,"transport_positive") && std::strcmp(mode,"transport_red_control") && std::strcmp(mode,"transport_staged") && std::strcmp(mode,"transport_manual")) {
        std::printf("P2_PURPLE_COMBAT_UNIMPLEMENTED mode=%s implemented=adult_direct,persistence_dayend,persistence_resume,transport_delivery,transport_positive,transport_red_control,transport_staged,transport_manual\n",mode); return 2;
    }
    SDL_SetMainReady(); pc_gpu_preference_apply(); pc_bbft_init(argc,argv);
    require(pc_randomizer_purple_campaign() && pc_randomizer_p2_bridge(),"ordinary Purple seed campaign required");
    if(!pc_window_init(mode && !std::strcmp(mode,"transport_manual")?"Purple carry smoke - staged Purple - F7 resets":"Purple campaign combat fixture",960,540)) return 3;
    pc_settings_init(); pc_window_set_display_mode(PC_WINDOW_FULLSCREEN_WINDOWED);
    pc_window_set_window_size(960,540); pc_window_center();
    std::puts("Experimental preview window set to 960x540 windowed and centered");
    int w=0,h=0,x=0,y=0; SDL_Window* window=SDL_GL_GetCurrentWindow();
    SDL_GetWindowSize(window,&w,&h); SDL_GetWindowPosition(window,&x,&y);
    require(w==960&&h==540,"window dimensions");
    std::printf("P2_FIXTURE_WINDOW width=%d height=%d x=%d y=%d\n",w,h,x,y);
    milestone("window_ready",0);
    std::printf("P2_PURPLE_COMBAT_SCOPE mode=%s natural_acquisition=%d player_controls_validated=0 production_collision_marker_required=1\n",
        mode?mode:"adult_direct",int(!mode || (std::strcmp(mode,"transport_red_control") && std::strcmp(mode,"transport_staged") && std::strcmp(mode,"transport_manual"))));
    gsys->Initialise(); pc_settings_p2d_init(); nodeMgr=new NodeMgr();
    gsys->run(new PurpleCombatApp()); return 0;
}
