#include "pc_p2_authored_cave_campaign.h"
#include "pc_p2_authored_cave_route.h"
#include "pc_p2_campaign_flush.h"
#include "pc_p2_cave_campaign_cache.h"
#include "pc_p2_cave_campaign_cache_engine.h"
#include "pc_p2_cave_campaign_party_engine.h"
#include "pc_p2_cave_party_landing.h"
#include "MapCode.h"
#include "pc_p2_cave_survivor_permit.h"
#include "pc_p2_surface_save.h"
#include "pc_p2_cave_rooms_engine.h"
#include "pc_p2_cave_geometry_engine.h"
#include "pc_p2_cave_items_engine.h"
#include "pc_p2_cave_bud_actor.h"
#include "pc_p2_cave_carry_engine.h"
#include "pc_p2_cave.h"
#include "pc_p2_teki_lifetime.h"
#include "pc_randomizer.h"
#include "FlowController.h"
#include "OnePlayerSection.h"
#include "Generator.h"
#include "Pellet.h"
#include "Navi.h"
#include "NaviMgr.h"
#include "NaviState.h"
#include "Piki.h"
#include "PikiMgr.h"
#include "PlayerState.h"
#include "MoviePlayer.h"
#include "MapMgr.h"
#include "gameflow.h"
#include "zen/ogFileChkSel.h"
#include "zen/ogFileSelect.h"
#include "zen/ogMemChk.h"
#include "Controller.h"
#include <fstream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <optional>

// Match the historical cave route's optional netplay seam in both profiles.
__attribute__((weak)) bool pc_netplay_session_active(void);
namespace {
struct Config {std::uint64_t seed=0;std::string token,surface,floor;float entryX=0,entryZ=0,exitX=0,exitZ=0,spawnX=0,spawnZ=0;};
Config config;
P2AuthoredCaveRoute selectedRoute;
std::string pendingDestinationImage;
bool authored(){return pc_randomizer_authored_cave_route().present;}
P2CaveCampaignParty savedParty(){return pc_randomizer_authored_cave_session().party;}
void setParty(const P2CaveCampaignParty& party){
    auto session=pc_randomizer_authored_cave_session();session.party=party;
    if(!pc_randomizer_authored_cave_session_set(session))std::abort();
}
StageInfo* surfaceStage=nullptr;
StageInfo* floorStage=nullptr;
bool prepared=false,sceneReady=false,restored=false,requested=false,pending=false;
bool detaching=false,restoringParty=false;
zen::ogScrFileChkSelMgr* saveChoice=nullptr;
bool choosingSave=false;
unsigned saveChoiceTicks=0;
std::optional<decltype(gameflow.mPlayState)> choicePlayState;
std::optional<decltype(gameflow.mGamePrefs)> choicePrefs;
void restoreChoice(){
    if(choicePlayState)gameflow.mPlayState=*choicePlayState;
    if(choicePrefs)gameflow.mGamePrefs=*choicePrefs;
    choicePlayState.reset();choicePrefs.reset();
}
std::uint64_t permitGeneration=0;
std::array<std::uint8_t,32> permitSha{};
void clearPermit(){detaching=false;restoringParty=false;permitGeneration=0;permitSha.fill(0);}
bool adoptPermit(){
    std::uint64_t generation=0;std::array<std::uint8_t,32> digest{};
    if(!pc_randomizer_checkpoint_info(&generation,digest.data())||!generation
        ||generation!=pc_randomizer_active_campaign_generation())return false;
    bool nonzero=false;for(auto byte:digest)nonzero|=byte!=0;
    if(!nonzero)return false;
    permitGeneration=generation;permitSha=digest;return true;
}
[[noreturn]] void invalid(const char* why){std::fprintf(stderr,"Invalid ordinary generated cave: %s\n",why);std::abort();}
bool inside(){return pc_randomizer_generated_cave_cache().inside;}
bool safe(){return sceneReady&&pc_randomizer_ready()
    &&!(pc_netplay_session_active&&pc_netplay_session_active())
    &&naviMgr&&naviMgr->getActiveNavi()
    &&naviMgr->getActiveNavi()->isAlive()
    &&std::isfinite(naviMgr->getActiveNavi()->mHealth)&&naviMgr->getActiveNavi()->mHealth>1.f
    &&naviMgr->getActiveNavi()->getCurrState()&&naviMgr->getActiveNavi()->getCurrState()->getID()==NAVISTATE_Walk
    &&!gameflow.mPauseAll&&!gameflow.mIsUIOverlayActive&&!gameflow.mIsDayEndActive
    &&gameflow.mMoviePlayer&&!gameflow.mMoviePlayer->mIsActive;}
bool atBoundary(){auto* n=naviMgr?naviMgr->getActiveNavi():nullptr;if(!n)return false;
    const float x=inside()?config.exitX:config.entryX,z=inside()?config.exitZ:config.entryZ;
    return std::hypot(n->mSRT.t.x-x,n->mSRT.t.z-z)<=80.f;}
void moveParty(P2CaveCampaignParty& party,bool entering){
    std::array<P2CavePartyPoint,2> to{};
    for(auto& c:party.captains){
        if(entering){party.surfaceHomes[c.slot]=c.position;to[c.slot]={config.spawnX+float(c.slot*30),selectedRoute.floor.landing.y,config.spawnZ};}
        else to[c.slot]=party.surfaceHomes[c.slot];}
    if(!p2CavePlaceLandingParty(party,to))invalid("invalid transported party landing");
    for(auto& b:party.bodies){
        // Keep provenance/key while retiring the old map's pointer binding.
        b.generator=b.originRealm==int(entering)?b.originGenerator:0;}
    party.inside=entering;
    party.landing=true;
}
}
void pc_p2_cave_campaign_prepare(){
    prepared=false;surfaceStage=nullptr;floorStage=nullptr;
    saveChoice=nullptr;choosingSave=false;
    clearPermit();
    if(!authored())return;
    selectedRoute=pc_randomizer_authored_cave_route();
    if(!selectedRoute.valid()||selectedRoute.surface.stage!=STAGE_Forest)invalid("selected native forest route");
    config.seed=selectedRoute.seed;config.token=selectedRoute.token;
    config.surface=selectedRoute.surface.file;config.floor=selectedRoute.floor.file;
    config.entryX=selectedRoute.surface.landing.x;config.entryZ=selectedRoute.surface.landing.z;
    config.spawnX=selectedRoute.floor.landing.x;config.spawnZ=selectedRoute.floor.landing.z;
    config.exitX=selectedRoute.exit.x;config.exitZ=selectedRoute.exit.z;
    FOREACH_NODE(StageInfo,flowCont.mStageList.mChild,stage){if(stage->mFileName
        &&config.surface==stage->mFileName&&stage->mStageID==STAGE_Forest)surfaceStage=stage;}
    if(!surfaceStage||surfaceStage->mStageIndex!=selectedRoute.surface.index||surfaceStage->mStageID!=selectedRoute.surface.stage)invalid("missing ordinary source surface");
    // Allocated with the game setup heap; excluded from the fixed five-stage
    // story list/card serialization. Its heads are in typed floor state.
    floorStage=new StageInfo();floorStage->mStageInf.init();
    floorStage->mStageName="Generated Forest Cave";floorStage->mFileName="stages/generated-forest.ini";
    floorStage->mStageID=STAGE_Forest;floorStage->mStageIndex=surfaceStage->mStageIndex;
    floorStage->mIsVisible=FALSE;
    prepared=true;
}
bool pc_p2_cave_campaign_resume_scene(){
    if(!authored()||!pc_randomizer_resumed())return false;
    const auto& party=savedParty();
    if(!party.present||!party.resumeLiving)return false;
    if(!prepared||!party.valid()||party.inside!=inside()
        ||!pc_randomizer_authored_cave_session().activeCacheMatches(pc_p2_campaign_cache_image())
        ||!adoptPermit())
        invalid("invalid authenticated living scene resume");
    flowCont.mCurrentStage=inside()?floorStage:surfaceStage;
    if(!flowCont.mCurrentStage)invalid("missing owned resume stage");
    gameflow.mCurrentStageID=flowCont.mCurrentStage->mStageID;
    std::printf("P2_CAMPAIGN_RESUME_SCENE floor=%d generation=%llu ordinary_section=1\n",
        int(inside()),static_cast<unsigned long long>(permitGeneration));
    return true;
}
void pc_p2_cave_campaign_select_stage(){
    if(!authored())return;
    clearPermit();
    if(!prepared)invalid("provider not prepared");
    if(inside()){
        const auto& party=savedParty();
        if(!party.present||!party.inside)invalid("floor cache without typed party");
        flowCont.mCurrentStage=floorStage;
    }else if(pending)flowCont.mCurrentStage=surfaceStage;
    auto* stage=flowCont.mCurrentStage;if(!stage||!stage->mFileName)invalid("missing stage selection");
    std::snprintf(flowCont.mCurrStageFilePath,sizeof(flowCont.mCurrStageFilePath),"%s",stage->mFileName);
    std::snprintf(flowCont.mDoorStageFilePath,sizeof(flowCont.mDoorStageFilePath),"%s",stage->mFileName);
}
void pc_p2_cave_campaign_before_preload(){
    if(!authored())return;
    if(pending&&!pendingDestinationImage.empty())pc_p2_campaign_cache_restore_image(pendingDestinationImage);
    else if(inside())pc_p2_cave_campaign_cache_restore_floor();
}
void pc_p2_cave_campaign_scene_setup(){
    sceneReady=false;restored=false;requested=false;
    if(!authored()||!prepared)return;
    saveChoice=new zen::ogScrFileChkSelMgr();
    choosingSave=false;
    auto party=savedParty();
    if(inside()){
        pc_p2_cave_rooms_setup();const auto* layout=pc_p2_cave_rooms_layout();
        if(!layout||layout->seed!=config.seed||layout->cave!="forest_1"||layout->floor!=1)invalid("foreign floor layout");
        pc_p2_cave_geometry_setup();
    }
    if(inside()||pending||(party.present&&party.resumeLiving&&flowCont.mCurrentStage==surfaceStage)){
        // P2 CaveState stops the day timer; loadMainMapSituation and the
        // geyser return restore CaveSaveData.mTime. Preserve that exact time
        // through the native card instead of resetting to the day's start.
        gameflow.mWorldClock.setTime(party.surfaceTime);
        // Boundary SAVE records a landing descriptor before the destination
        // map exists. Resolve only that descriptor against actual destination
        // collision; an already landed SAVE retains exact captured heights.
        if(party.landing){
            if(!mapMgr)invalid("landing without destination map");
            auto land=[](P2CavePartyPoint& point){
                float height=0.f;
                // One upward static collision query supplies BOTH the footing
                // identity and its height. An underside cannot hide wet ground,
                // and absence cannot turn into getMinY's synthetic zero.
                auto* triangle=mapMgr->getStaticGroundBelow(point.x,point.z,point.y+1.f,height);
                if(!triangle)invalid("destination landing has no collision footing");
                const auto attribute=MapCode::getAttribute(triangle);
                if(attribute==ATTR_Water||attribute==ATTR_Hole)invalid("destination landing has unsafe footing");
                if(!std::isfinite(height)||std::fabs(height-point.y)>10.f)invalid("destination landing differs from selected pose");
                point.y=height;
                if(!point.valid())invalid("destination landing height invalid");
            };
            for(auto& c:party.captains)land(c.position);
            for(auto& b:party.bodies)land(b.position);
            party.landing=false;
            if(!party.valid())invalid("invalid destination landing geometry");
            setParty(party);
        }
        if(!adoptPermit())invalid("party restore without authenticated selected checkpoint");
        restoringParty=true;
        pc_p2_cave_campaign_party_restore(party);
        restoringParty=false;restored=true;}
    pending=false;
    if(inside()){
        pc_p2_cave_bud_setup();pc_p2_cave_items_setup();
    }
    sceneReady=true;
    std::printf("P2_CAMPAIGN_SCENE_READY floor=%d restored_party=%d\n",int(inside()),int(restored));
}
void pc_p2_cave_campaign_scene_exit(){
    if(!authored())return;
    if(choosingSave)gameflow.mIsUIOverlayActive=FALSE;
    choosingSave=false;saveChoice=nullptr;
    detaching=pending&&permitGeneration!=0;
    sceneReady=false;pc_p2_cave_items_shutdown();pc_p2_cave_geometry_shutdown();pc_p2_cave_rooms_shutdown();
    pc_p2_cave_campaign_party_scene_exit();
}
P2CaveBoundarySnapshot pc_p2_cave_campaign_boundary(){
    P2CaveBoundarySnapshot result;
    if(!authored()||!prepared||!sceneReady
        ||flowCont.mCurrentStage!=(inside()?floorStage:surfaceStage))return result;
    result.action=inside()?P2CaveBoundaryAction::Return:P2CaveBoundaryAction::Enter;
    result.seed=config.seed;result.cave="forest_1";result.floor=inside()?1:0;
    result.token=config.token;result.sceneGeneration=pc_p2_scene_generation();
    result.checkpointGeneration=pc_randomizer_active_campaign_generation();
    result.x=inside()?config.exitX:config.entryX;
    result.z=inside()?config.exitZ:config.entryZ;
    result.ready=safe()&&saveChoice&&!choosingSave&&!requested&&!pending;
    return result;
}
bool pc_p2_cave_campaign_request_boundary(const P2CaveBoundarySnapshot& selected){
    if(!p2CaveBoundaryMatches(selected,pc_p2_cave_campaign_boundary())||!atBoundary())return false;
    // Direct randomizer startup bypasses the title/file menu. Reuse its real
    // preparation and controller-driven file choice at each cave boundary;
    // physical backup slots are derived by the native card inventory.
    choicePlayState=gameflow.mPlayState;choicePrefs=gameflow.mGamePrefs;
    requested=false;saveChoice->startSave();choosingSave=true;saveChoiceTicks=0;
    gameflow.mIsUIOverlayActive=TRUE;
    std::puts("P2_CAMPAIGN_SAVE_CHOICE_STARTED native_file_menu=1");
    return true;
}
void pc_p2_cave_campaign_request(){
    pc_p2_cave_campaign_request_boundary(pc_p2_cave_campaign_boundary());
}
P2CaveSaveChoiceSnapshot pc_p2_cave_campaign_save_choice(){
    P2CaveSaveChoiceSnapshot result;
    if(!authored()||!choosingSave||!saveChoice)return result;
    result.active=true;result.state=saveChoice->mState;
    if(saveChoice->mMemChkMgr)result.defaultFile=saveChoice->mMemChkMgr->pcDefaultFileSnapshot();
    if(saveChoice->mIsScreenVisible&&saveChoice->mFileSelectMgr)
        result.slot=saveChoice->mFileSelectMgr->pcSaveInputSlot();
    return result;
}
bool pc_p2_cave_campaign_update_save_choice(Controller* input){
    if(!authored()||!choosingSave||!saveChoice)return false;
    if(!input)invalid("card selection without native controller");
    if(++saveChoiceTicks%30==0)
        std::printf("P2_CAMPAIGN_SAVE_INPUT tick=%u player=%u down=%u pressed=%u previous=%u\n",
            saveChoiceTicks,unsigned(input->mPlayerNum),unsigned(input->mCurrentInput),unsigned(input->mInputPressed),unsigned(input->mPrevInput));
    CardQuickInfo selection;
    const auto state=saveChoice->update(input,selection);
    if(state==zen::ogScrFileChkSelMgr::SelectionA||state==zen::ogScrFileChkSelMgr::SelectionB
        ||state==zen::ogScrFileChkSelMgr::SelectionC){
        // These are the actual controller-selected logical A/B/C slots. The
        // native inventory selects an unused/older physical backup itself.
        CardQuickInfo infos[4];gameflow.mMemoryCard.getQuickInfos(infos);
        gameflow.mPlayState.mSaveSlot=static_cast<u8>(state-zen::ogScrFileChkSelMgr::SelectionA);
        requested=true;choosingSave=false;gameflow.mIsUIOverlayActive=FALSE;
        std::printf("P2_CAMPAIGN_SAVE_CHOICE_SELECTED slot=%u backup=%u native_inventory=1\n",
            unsigned(gameflow.mPlayState.mSaveSlot),unsigned(gameflow.mGamePrefs.mSpareMemCardSaveIndex));
    }else if(state==zen::ogScrFileChkSelMgr::ErrorOrCompleted||state==zen::ogScrFileChkSelMgr::ForceExit){
        requested=false;choosingSave=false;gameflow.mIsUIOverlayActive=FALSE;
        restoreChoice();
        std::puts("P2_CAMPAIGN_SAVE_CHOICE_CANCELLED realm_unchanged=1");
    }
    return true;
}
void pc_p2_cave_campaign_draw_save_choice(Graphics& gfx){
    if(authored()&&choosingSave&&saveChoice)saveChoice->draw(gfx);
}
void pc_p2_cave_campaign_tick(){
    if(!authored()||!sceneReady)return;
    if(inside()){pc_p2_cave_carry_tick();pc_p2_cave_geometry_tick();pc_p2_cave_bud_tick();}
}
bool pc_p2_cave_campaign_commit_transition(){
    if(!authored()||!requested)return false;
    struct ChoiceRollback {bool committed=false;~ChoiceRollback(){if(!committed)restoreChoice();}} choiceRollback;
    requested=false;if(!safe()||!atBoundary())return false;
    const bool entering=!inside();auto party=savedParty();
    if(!pc_p2_cave_campaign_party_capture(party,!entering)){
        std::puts("P2_CAMPAIGN_BOUNDARY_HELD unsettled_party_or_heads=1");return false;}
    if(entering){int reds=0;for(const auto& body:party.bodies)reds+=body.species==1;
        if(reds<2){std::puts("P2_CAMPAIGN_BOUNDARY_HELD living_red_supply=1");return false;}
        party.surfaceTime=gameflow.mWorldClock.mTimeOfDay;
        if(!party.valid()){std::puts("P2_CAMPAIGN_BOUNDARY_HELD invalid_surface_clock=1");return false;}}
    // Use the native card slot the player selected. A direct boot without a
    // usable backup slot cannot silently select/create a different save file.
    if(gameflow.mGamePrefs.mSpareMemCardSaveIndex<1||gameflow.mGamePrefs.mSpareMemCardSaveIndex>4
        ||gameflow.mGamePrefs.mMemCardSaveIndex>4
        ||gameflow.mPlayState.mSaveSlot>3){
        std::puts("P2_CAMPAIGN_BOUNDARY_HELD selected_native_save_slot=0");return false;}
    const PcP2CampaignLiveCache oldLive;
    const auto oldBanks=pc_randomizer_generated_cave_cache();
    const auto oldSession=pc_randomizer_authored_cave_session();
    const auto oldPlayState=gameflow.mPlayState;
    const auto oldPrefs=gameflow.mGamePrefs;
    const auto oldGeneration=pc_randomizer_active_campaign_generation();
    auto rollback=[&](){
        oldLive.restore();
        if(!pc_randomizer_authored_cave_checkpoint_set(oldSession,oldBanks))invalid("rollback contract rejected");
        gameflow.mPlayState=oldPlayState;gameflow.mGamePrefs=oldPrefs;clearPermit();
    };
    std::string flushReason;
    if(!pc_p2_campaign_flush(flushReason)){oldLive.restore();return false;}
    const auto sourceImage=pc_p2_campaign_cache_image();
    auto banks=oldBanks;
    if(entering){
        if(!banks.enter(sourceImage)){rollback();return false;}
        generatorCache->initGame();
        if(!banks.floor.empty())pc_p2_campaign_cache_restore_image(banks.floor);
        if(!banks.captureFloor(pc_p2_campaign_cache_image())){rollback();return false;}
    }else{
        const auto surface=banks.surface;
        if(!banks.leave(sourceImage)){rollback();return false;}
        pc_p2_campaign_cache_restore_image(surface);
    }
    moveParty(party,entering);
    const auto destinationImage=pc_p2_campaign_cache_image();
    P2AuthoredCaveSession next;
    next.present=true;next.route=selectedRoute;next.day=gameflow.mWorldClock.mCurrentDay;next.party=party;
    next.surfaceCacheSha=P2AuthoredCaveSession::bankHash(selectedRoute.routeSha,"surface",banks.surface);
    next.floorCacheSha=P2AuthoredCaveSession::bankHash(selectedRoute.routeSha,"floor",banks.floor);
    next.activeCacheSha=P2AuthoredCaveSession::bankHash(selectedRoute.routeSha,entering?"floor":"surface",destinationImage);
    if(!next.activeCacheMatches(destinationImage)||!pc_randomizer_authored_cave_checkpoint_set(next,banks)){
        rollback();return false;
    }
    gameflow.mMemoryCard.saveCurrentGame();
    std::uint64_t savedGeneration=0;std::array<std::uint8_t,32> savedSha{};
    const bool proof=pc_randomizer_checkpoint_info(&savedGeneration,savedSha.data());
    if(savedGeneration<=oldGeneration){
        rollback();std::puts("P2_CAMPAIGN_BOUNDARY_HELD native_save_failed=1 rollback=1");return false;
    }
    if(!proof||savedGeneration!=pc_randomizer_active_campaign_generation()||!adoptPermit())
        invalid("advanced native SAVE lacks full selected checkpoint proof");
    pendingDestinationImage=destinationImage;
    oldLive.restore();
    // saveOptions runs after the native campaign checkpoint. Its I/O failure
    // cannot undo a committed destination card or authorize replaying the old
    // realm; preserve the actual checkpoint as the boundary authority.
    std::printf("P2_CAMPAIGN_BOUNDARY_SAVE generation=%llu options_failed=%d\n",
        static_cast<unsigned long long>(savedGeneration),int(gameflow.mMemoryCard.didSaveFail()));
    if(!adoptPermit())invalid("committed boundary checkpoint proof unavailable");
    pending=true;sceneReady=false;
    choiceRollback.committed=true;choicePlayState.reset();choicePrefs.reset();
    std::printf("P2_CAMPAIGN_BOUNDARY_COMMITTED floor=%d bodies=%zu surface_heads=%zu floor_heads=%zu\n",
        int(entering),party.bodies.size(),party.surfaceHeads.size(),party.floorHeads.size());
    return true;
}
bool pc_p2_cave_campaign_restored_party(){return authored()&&restored;}
bool pc_p2_cave_campaign_survivor_permit(const std::string& sourceKey,std::uint32_t recordUid,
    std::uint32_t attempt,std::uint64_t activation,const std::string& catalogFingerprint,
    std::uint64_t* generation,std::uint8_t sha[32]){
    if(!authored()){
        return pc_p2_surface_save_survivor_permit(sourceKey,recordUid,attempt,activation,catalogFingerprint,generation,sha);
    }
    const auto& party=savedParty();
    return p2CaveSurvivorPermit(party,detaching||restoringParty,permitGeneration,
        pc_randomizer_active_campaign_generation(),permitSha,sourceKey,recordUid,attempt,
        activation,catalogFingerprint,generation,sha);
}
bool pc_p2_cave_campaign_owns_heads(){
    if(!authored()||!prepared)return false;
    const auto& party=savedParty();
    return party.present&&party.resumeLiving&&party.inside==inside()
        &&flowCont.mCurrentStage==(inside()?floorStage:surfaceStage);
}
int pc_p2_cave_campaign_floor(){return authored()&&prepared&&inside()?1:0;}
std::string pc_p2_cave_campaign_token(){return authored()&&prepared?config.token:std::string();}
void pc_p2_cave_campaign_before_day_cleanup(){
    if(!authored()||!prepared)return;
    if(!inside()){
        auto party=savedParty();
        if(party.present){
            if(!pc_randomizer_authored_cave_session_set(P2AuthoredCaveSession{}))invalid("day boundary session retirement");}
        return;
    }
    // A live floor must never enter P1's sunset survivor deposit. Its SAVE
    // authority is the settled native boundary transaction above.
    invalid("floor reached destructive surface day-end path");
}

bool pc_p2_cave_campaign_survivor_body(const std::string& sourceKey,std::uint32_t recordUid,
    std::uint32_t attempt,std::uint64_t activation,const std::string& catalogFingerprint,
    OriginalPikiBodyState& state,std::uint64_t* generation,std::uint8_t sha[32]){
    if(!authored()){
        return pc_p2_surface_save_survivor_body(sourceKey,recordUid,attempt,activation,catalogFingerprint,state,generation,sha);
    }
    return p2CaveSurvivorBody(savedParty(),detaching||restoringParty,
        permitGeneration,pc_randomizer_active_campaign_generation(),permitSha,
        sourceKey,recordUid,attempt,activation,catalogFingerprint,state,generation,sha);
}
