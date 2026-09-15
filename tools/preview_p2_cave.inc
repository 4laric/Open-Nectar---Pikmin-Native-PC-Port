// Lifecycle fixture only: synthetic casualty/maturity/conversion and real
// native restoration, collection hook and transfer writer. Not transport AI.
#include "pc_p2_cave.h"
#include "pc_p2_cave_anchor.h"
#include "pc_p2_cave_rooms_engine.h"
#include "pc_p2_cave_items_engine.h"
#include "pc_p2_species.h"
#include "GoalItem.h"
#include <fstream>

// Lane 44 (#482): live proxy-room loop. The proxy geometry is labelled; the
// reachability answers come from the real live squad keys, not a scripted table.
static void caveRoomsLogTimeline(const P2CaveRoomLayout* layout, const char* tag) {
    std::printf("P2_CAVE_ROOMS_TIMELINE tag=%s hole=%d treasure_elec=%d treasure_water=%d untagged=%d\n",
        tag, int(pc_p2_cave_rooms_reachable_now(layout->hole)),
        int(pc_p2_cave_rooms_reachable_now("leaf_elec_0")),
        int(pc_p2_cave_rooms_reachable_now("leaf_water_0")),
        int(pc_p2_cave_rooms_reachable_now(layout->entrance)));
}
static void caveRoomsGrantSpecies(int species) {
    // Only convert a still-Red squad member so a previously granted key is kept.
    Iterator it(pikiMgr);
    CI_LOOP(it) {
        Piki* p=static_cast<Piki*>(*it);
        if(p && p->isAlive() && pc_p2_species(p)==P2SpeciesRed) { pc_p2_set_species(p,species); return; }
    }
}
// Lane 46 (#484): physical cave-item loop. Places the captain at the tagged
// elec treasure's leaf, releases the squad so free-mode Pikmin can acquire it,
// falls back to the real Transport AI if they do not, and waits for the live
// receipt seam to credit it exactly once. The acquisition method is reported;
// the carry state machine and receipt are the production path.
static int caveItemTicks=0;
static Pellet* caveCarryTarget=nullptr;
static void caveItemsFixture(Navi* n) {
    const P2CaveItemPlacement* items=pc_p2_cave_items_placement();
    require(items!=nullptr,"cave items placement missing");
    if(caveItemTicks==0){
        caveCarryTarget=pc_p2_cave_items_pellet_for("treasure_elec");
        require(caveCarryTarget!=nullptr,"tagged elec treasure not spawned");
        const float x=caveCarryTarget->mSRT.t.x,z=caveCarryTarget->mSRT.t.z,y=mapMgr->getMinY(x,z,true);
        n->resetPosition(Vector3f(x,y,z));
        n->mStateMachine->transit(n,NAVISTATE_Walk);
        std::printf("P2_CAVE_ITEM_STAGE target=treasure_elec x=%.3f y=%.3f z=%.3f spawned=%d\n",
            x,y,z,pc_p2_cave_items_spawned());
        ++caveItemTicks;return;
    }
    ++caveItemTicks;
    if(caveItemTicks<120)return;
    if(caveItemTicks==120){
        Iterator it(pikiMgr);int count=0;CI_LOOP(it){Piki* p=static_cast<Piki*>(*it);if(!p->isAlive())continue;
            const float px=caveCarryTarget->mSRT.t.x+((count%5)-2)*6.f,pz=caveCarryTarget->mSRT.t.z+((count/5)-1)*6.f;
            p->mSRT.t=Vector3f(px,mapMgr->getMinY(px,pz,true),pz);
            p->changeMode(PikiMode::FreeMode,n);++count;}
        std::printf("P2_CAVE_ITEM_FREE_RELEASE target=treasure_elec count=%d\n",count);
        return;
    }
    if(caveItemTicks<150)return;
    if(caveItemTicks==150){
        int transport=0;Iterator it(pikiMgr);CI_LOOP(it){Piki* p=static_cast<Piki*>(*it);if(p->mMode==PikiMode::TransportMode)++transport;}
        if(transport==0){
            int assigned=0;Iterator a(pikiMgr);CI_LOOP(a){Piki* p=static_cast<Piki*>(*a);if(!p->isAlive())continue;
                p->mActiveAction->abandon(nullptr);p->mActiveAction->mCurrActionIdx=PikiAction::Transport;
                p->mActiveAction->mChildActions[PikiAction::Transport].initialise(caveCarryTarget);
                p->mMode=PikiMode::TransportMode;++assigned;}
            std::printf("P2_CAVE_ITEM_TRANSPORT_ASSIGNED target=treasure_elec count=%d injected=1\n",assigned);
        } else {
            std::printf("P2_CAVE_ITEM_NATURAL_CARRY transport=%d\n",transport);
        }
        return;
    }
    if(pc_p2_cave_items_delivery_events()>0){
        capture("p2-cave-items.ppm");
        const bool again=pc_p2_cave_items_deliver(caveCarryTarget);
        std::printf("P2_CAVE_ITEM_REDELIVER duplicate_call=%d events=%d new=%d\n",
            int(again),pc_p2_cave_items_delivery_events(),pc_p2_cave_items_delivered());
        std::puts("PASS cave items: physical placement, real carry, exactly-once receipt (proxy geometry/model)");
        std::fflush(stdout);std::_Exit(0);
    }
    if(caveItemTicks%150==0){
        int transport=0,free=0;Iterator it(pikiMgr);CI_LOOP(it){Piki* p=static_cast<Piki*>(*it);if(p->mMode==PikiMode::TransportMode)++transport;if(p->mMode==PikiMode::FreeMode)++free;}
        std::printf("P2_CAVE_ITEM_PROGRESS tick=%d alive=%d state=%d transport=%d free=%d x=%.2f z=%.2f goal=%p events=%d new=%d\n",
            caveItemTicks,int(caveCarryTarget->isAlive()),caveCarryTarget->getState(),transport,free,
            caveCarryTarget->mSRT.t.x,caveCarryTarget->mSRT.t.z,(void*)caveCarryTarget->mTargetGoal,
            pc_p2_cave_items_delivery_events(),pc_p2_cave_items_delivered());
        std::fflush(stdout);
        capture("p2-cave-items-progress.ppm");
    }
    require(caveItemTicks<2400,"cave item carry did not deliver");
}
static void caveRoomsFixture(Navi* n) {
    if(pc_p2_cave_items_active()){caveItemsFixture(n);return;}
    static int ticks=0;
    if(++ticks<120)return;
    const P2CaveRoomLayout* layout=pc_p2_cave_rooms_layout();
    require(layout!=nullptr,"rooms layout missing");
    const P2CaveRoomUnit* entrance=p2CaveRoomsFind(*layout,layout->entrance);
    require(entrance!=nullptr,"rooms entrance missing");
    const float ex=p2CaveRoomsWorldX(*layout,*entrance),ez=p2CaveRoomsWorldZ(*layout,*entrance);
    n->resetPosition(Vector3f(ex,mapMgr->getMinY(ex,ez,true),ez));
    n->mStateMachine->transit(n,NAVISTATE_Walk);
    P2CaveAbilities keys=pc_p2_cave_rooms_squad_abilities();
    std::printf("P2_CAVE_ROOMS_ENTER id=%s unit_at=%s geometry=%s proxy=1\n",
        layout->entrance.c_str(),pc_p2_cave_rooms_unit_at(n->mSRT.t.x,n->mSRT.t.z).c_str(),
        layout->geometry.c_str());
    std::printf("P2_CAVE_ROOMS_PROJECTION salt=%d projection=%s\n",layout->salt,
        p2CaveRoomsLogicalProjection(*layout).c_str());
    std::printf("P2_CAVE_ROOMS_SQUAD blue=%d yellow=%d red=%d white=%d\n",
        int(keys.blue),int(keys.yellow),int(keys.red),int(keys.white));
    caveRoomsLogTimeline(layout,"fresh_floor_no_abilities");
    caveRoomsGrantSpecies(P2SpeciesYellow);
    std::printf("P2_CAVE_ROOMS_GRANT species=yellow natural_acquire=0 staged=1\n");
    caveRoomsLogTimeline(layout,"come_back_with_yellow");
    caveRoomsGrantSpecies(P2SpeciesBlue);
    std::printf("P2_CAVE_ROOMS_GRANT species=blue natural_acquire=0 staged=1\n");
    caveRoomsLogTimeline(layout,"return_with_yellow_and_blue");
    capture("p2-cave-rooms.ppm");
    std::puts("PASS cave rooms: proxy floor, live squad keys, hazard timeline (proxy geometry)");
    std::fflush(stdout);std::_Exit(0);
}
static void caveFixture(Navi* n) {
    static int ticks=0;
    if(pc_p2_cave_rooms_active()){caveRoomsFixture(n);return;}

    if(ticks==0 && pc_p2_preview_cargo_count()){
        // This fixture verifies checkpoint restoration, not random live combat.
        Iterator enemies(tekiMgr);CI_LOOP(enemies){Teki* e=static_cast<Teki*>(*enemies);if(e && e->isAlive())e->kill(false);}
        std::puts("P2_CAVE_FIXTURE_ENEMIES_REMOVED no_receipt_credit");
    }
    static bool renderOnly=[](){FILE* f=std::fopen("p2-cave-marker-render.txt","r");if(!f)return false;std::fclose(f);return true;}();
    if(renderOnly && ticks==0){
        P2CaveAnchor marker;std::ifstream input("p2-cave-transition.txt");
        require(p2_cave_read_anchor(input,pc_p2_cave_floor(),marker),"marker render config");
        n->resetPosition(Vector3f(marker.x-60,marker.y,marker.z+60));
        n->mStateMachine->transit(n,NAVISTATE_Walk);
    }
    // Allow the inherited landing effects to clear before inspecting markers.
    if(++ticks<(pc_p2_preview_cargo_count() && !renderOnly?90:330))return;
    if(renderOnly){capture("p2-cave-marker.ppm");std::puts("PASS marker render fixture (synthetic captain position)");std::fflush(stdout);std::_Exit(0);}
    for(int f=0;f<DEMOFLAG_COUNT;++f)playerState->mDemoFlags.setFlagOnly(f);
    std::ifstream entry("p2-cave-entry.txt");std::string version,token;int floor,count;float health;
    require(bool(entry>>version>>token>>floor>>health>>count),"cave fixture input");
    int expected[4][3]={},actual[4][3]={};
    for(int i=0;i<count;++i){int color,maturity;require(bool(entry>>color>>maturity),"cave fixture squad");++expected[color][maturity];}
    std::vector<Piki*> pikis;Iterator it(pikiMgr);CI_LOOP(it){Piki* p=static_cast<Piki*>(*it);if(!p->isAlive())continue;pikis.push_back(p);++actual[pc_p2_is_purple(p)?3:p->mColor][p->mHappa];}
    require(pikis.size()==size_t(count),"cave restored count");
    for(int c=0;c<4;++c)for(int h=0;h<3;++h)require(actual[c][h]==expected[c][h],"cave restored species/maturity");
    require(std::fabs(n->mHealth/C_NAVI_PARM(n,mHealth)-health)<0.001f,"cave restored captain health");
    std::puts("P2_CAVE_RESTORE_PASS species maturity health exact");
    capture("p2-cave-entry.ppm");
    if(FILE* extinct=std::fopen("p2-cave-extinction.txt","r")){
        std::fclose(extinct);for(Piki* p:pikis)p->kill(false);
        std::puts("P2_CAVE_EXTINCTION_INJECTED awaiting production tick");std::fflush(stdout);return;
    }
    if(FILE* restore=std::fopen("p2-cave-restore-only.txt","r")){
        std::fclose(restore);std::puts("PASS cave repeated restore; no checkpoint written");std::fflush(stdout);std::_Exit(0);
    }
    gameflow.mPauseAll=true;require(!pc_p2_cave_checkpoint(false),"paused cave transitioned");gameflow.mPauseAll=false;
    Suckable* pod=pc_p2_preview_goal();require(pod!=nullptr,"cave Pod missing");
    P2CaveAnchor anchor;std::ifstream anchorFile("p2-cave-transition.txt");
    if(anchorFile)require(p2_cave_read_anchor(anchorFile,floor,anchor),"fixture transition anchor");
    Vector3f destination=anchor.enabled?Vector3f(anchor.x,anchor.y,anchor.z):pod->mSRT.t;
    Vector3f position=n->mSRT.t;
    n->resetPosition(Vector3f(destination.x+300,destination.y,destination.z));
    require(!pc_p2_cave_checkpoint(false),"distant cave transitioned");n->resetPosition(position);
    if(anchor.enabled){
        n->resetPosition(Vector3f(destination.x,destination.y+60,destination.z));
        require(!pc_p2_cave_interact(destination.x,destination.y,destination.z),"anchor interacted across vertical layer");
        require(!pc_p2_cave_checkpoint(false),"anchor checkpoint across vertical layer");
        n->resetPosition(destination);n->mStateMachine->transit(n,NAVISTATE_Walk);
        require(!pc_p2_cave_interact(destination.x+300,destination.y,destination.z),"foreign actor requested anchor");
        require(pc_p2_cave_interact(destination.x,destination.y,destination.z),"anchor interaction rejected");
        capture("p2-cave-anchor.ppm");
    }
    if(floor==1){
        require(pikis.size()==20,"first fixture squad");pikis.back()->kill(false);
        pikis[0]->mHappa=Flower;pikis[1]->mHappa=Bud;n->mHealth=C_NAVI_PARM(n,mHealth)*.625f;
    } else {
        require(pikis.size()==19,"casualty was restored for free");
        // Conversion gameplay is tested separately by preview_p2_purple.inc.
        for(size_t i=0;i<10;++i)pc_p2_make_purple(pikis[i]);
    }
    if(pc_p2_preview_cargo_count())verifyCargoReceipts();
    else require(pc_p2_preview_deliver(pc_p2_preview_treasure()),"cave collection hook failed");
    n->resetPosition(destination);
    n->mStateMachine->transit(n,NAVISTATE_Walk);
    require(pc_p2_cave_checkpoint(false),"cave boundary checkpoint failed");
    require(!pc_p2_cave_checkpoint(false),"duplicate cave transition accepted");
    std::puts("PASS cave: restoration, pause/distance guards, synthetic survivors, collection hook, single transfer");
    std::fflush(nullptr);std::_Exit(42);
}
