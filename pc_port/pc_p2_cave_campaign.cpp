#include "pc_p2_cave_campaign.h"
#include "pc_p2_cave_campaign_cache.h"
#include "pc_p2_cave_campaign_cache_engine.h"
#include "pc_p2_cave_campaign_party_engine.h"
#include "pc_p2_cave_party_landing.h"
#include "MapCode.h"
#include "pc_p2_cave_survivor_permit.h"
#if defined(PIKMIN_P2_SURFACE_SAVE_PROVIDER)
#include "pc_p2_surface_save.h"
#endif
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

namespace {
struct Config {std::uint64_t seed=0;std::string token,surface,floor;float entryX=0,entryZ=0,exitX=0,exitZ=0,spawnX=0,spawnZ=0;};
Config config;
StageInfo* surfaceStage=nullptr;
StageInfo* floorStage=nullptr;
bool prepared=false,sceneReady=false,restored=false,requested=false,pending=false;
bool detaching=false,restoringParty=false;
zen::ogScrFileChkSelMgr* saveChoice=nullptr;
bool choosingSave=false;
unsigned saveChoiceTicks=0;
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
bool safe(){return sceneReady&&pc_randomizer_ready()&&naviMgr&&naviMgr->getActiveNavi()
    &&naviMgr->getActiveNavi()->getCurrState()&&naviMgr->getActiveNavi()->getCurrState()->getID()==NAVISTATE_Walk
    &&!gameflow.mPauseAll&&!gameflow.mIsUIOverlayActive&&!gameflow.mIsDayEndActive
    &&gameflow.mMoviePlayer&&!gameflow.mMoviePlayer->mIsActive;}
bool atBoundary(){auto* n=naviMgr?naviMgr->getActiveNavi():nullptr;if(!n)return false;
    const float x=inside()?config.exitX:config.entryX,z=inside()?config.exitZ:config.entryZ;
    return std::hypot(n->mSRT.t.x-x,n->mSRT.t.z-z)<=80.f;}
void flush(){
    if(!flowCont.mCurrentStage||!generatorCache||!generatorList||!generatorList->mGenListHead||!pelletMgr)
        invalid("missing live generator flush context");
    const unsigned stage=flowCont.mCurrentStage->mStageIndex;
    if(stage>=STAGE_COUNT)invalid("foreign native cache directory key");
    generatorCache->beginSave(stage);
    Generator* gen;
    FOREACH_NODE_REUSE(Generator,generatorList->mGenListHead->mChild,gen){
        if(gen->mCarryOverFlags&GENCARRY_SaveGenerator)generatorCache->saveGenerator(gen);}
    FOREACH_NODE_REUSE(Generator,generatorList->mGenListHead->mChild,gen){
        if((gen->mCarryOverFlags&GENCARRY_SaveGenerator)&&(gen->mCarryOverFlags&GENCARRY_SaveCreature))
            generatorCache->saveGeneratorCreature(gen);}
    Iterator pellets(pelletMgr);CI_LOOP(pellets){auto* p=static_cast<Pellet*>(*pellets);
        if(p->mConfig&&p->mConfig->mPelletType()==PELTYPE_UfoPart)generatorCache->saveUfoParts(p);}
    generatorCache->endSave();
    std::printf("P2_CAMPAIGN_LIVE_CACHE_FLUSH stage_key=%u floor=%d\n",stage,int(inside()));
}
void moveParty(P2CaveCampaignParty& party,bool entering){
    std::array<P2CavePartyPoint,2> to{};
    for(auto& c:party.captains){
        if(entering){party.surfaceHomes[c.slot]=c.position;to[c.slot]={config.spawnX+float(c.slot*30),c.position.y,config.spawnZ};}
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
    if(!pc_randomizer_generated_cave())return;
    std::ifstream in("p2-cave-campaign.txt");std::string magic,version,extra;
    if(!(in>>magic>>version>>config.seed>>config.token>>config.surface>>config.floor
        >>config.entryX>>config.entryZ>>config.exitX>>config.exitZ>>config.spawnX>>config.spawnZ)
        ||magic!="P2_CAVE_CAMPAIGN"||version!="1"||(in>>extra)
        ||config.floor!="stages/generated-forest.ini"||config.surface.rfind("stages/",0)!=0
        ||config.surface.find("..")!=std::string::npos
        ||!pc_randomizer_generated_cave_matches(config.seed,"forest_1",1,"treasure_water",
            "forest_1:f1:leaf:0","item:forest_1:f1:leaf:0:0",config.token.c_str()))invalid("seed-bound map provider config");
    for(float value:{config.entryX,config.entryZ,config.exitX,config.exitZ,config.spawnX,config.spawnZ})
        if(!std::isfinite(value)||std::fabs(value)>=1e7f)invalid("map boundary position");
    FOREACH_NODE(StageInfo,flowCont.mStageList.mChild,stage){if(stage->mFileName
        &&config.surface==stage->mFileName&&stage->mStageID==STAGE_Forest)surfaceStage=stage;}
    if(!surfaceStage||surfaceStage->mStageIndex>=STAGE_COUNT)invalid("missing ordinary source surface");
    // Allocated with the game setup heap; excluded from the fixed five-stage
    // story list/card serialization. Its heads are in typed floor state.
    floorStage=new StageInfo();floorStage->mStageInf.init();
    floorStage->mStageName="Generated Forest Cave";floorStage->mFileName="stages/generated-forest.ini";
    floorStage->mStageID=STAGE_Forest;floorStage->mStageIndex=surfaceStage->mStageIndex;
    floorStage->mIsVisible=FALSE;
    prepared=true;
}
bool pc_p2_cave_campaign_resume_scene(){
    if(!pc_randomizer_generated_cave()||!pc_randomizer_resumed())return false;
    const auto& party=pc_randomizer_generated_cave_party();
    if(!party.present||!party.resumeLiving)return false;
    if(!prepared||!party.valid()||party.inside!=inside()||!adoptPermit())
        invalid("invalid authenticated living scene resume");
    flowCont.mCurrentStage=inside()?floorStage:surfaceStage;
    if(!flowCont.mCurrentStage)invalid("missing owned resume stage");
    gameflow.mCurrentStageID=flowCont.mCurrentStage->mStageID;
    std::printf("P2_CAMPAIGN_RESUME_SCENE floor=%d generation=%llu ordinary_section=1\n",
        int(inside()),static_cast<unsigned long long>(permitGeneration));
    return true;
}
void pc_p2_cave_campaign_select_stage(){
    if(!pc_randomizer_generated_cave())return;
    clearPermit();
    if(!prepared)invalid("provider not prepared");
    if(inside()){
        const auto& party=pc_randomizer_generated_cave_party();
        if(!party.present||!party.inside)invalid("floor cache without typed party");
        flowCont.mCurrentStage=floorStage;
    }else if(pending)flowCont.mCurrentStage=surfaceStage;
    auto* stage=flowCont.mCurrentStage;if(!stage||!stage->mFileName)invalid("missing stage selection");
    std::snprintf(flowCont.mCurrStageFilePath,sizeof(flowCont.mCurrStageFilePath),"%s",stage->mFileName);
    std::snprintf(flowCont.mDoorStageFilePath,sizeof(flowCont.mDoorStageFilePath),"%s",stage->mFileName);
}
void pc_p2_cave_campaign_before_preload(){
    if(pc_randomizer_generated_cave()&&inside())pc_p2_cave_campaign_cache_restore_floor();
}
void pc_p2_cave_campaign_scene_setup(){
    sceneReady=false;restored=false;requested=false;
    if(!pc_randomizer_generated_cave()||!prepared)return;
    saveChoice=new zen::ogScrFileChkSelMgr();
    choosingSave=false;
    auto party=pc_randomizer_generated_cave_party();
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
                auto* triangle=mapMgr->getCurrTri(point.x,point.z,true);
                if(!triangle)invalid("destination landing has no collision footing");
                const auto attribute=MapCode::getAttribute(triangle);
                if(attribute==ATTR_Water||attribute==ATTR_Hole)invalid("destination landing has unsafe footing");
                point.y=mapMgr->getMinY(point.x,point.z,true);
                if(!point.valid())invalid("destination landing height invalid");
            };
            for(auto& c:party.captains)land(c.position);
            for(auto& b:party.bodies)land(b.position);
            party.landing=false;
            if(!party.valid())invalid("invalid destination landing geometry");
            pc_randomizer_generated_cave_party_set(party);
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
    if(!pc_randomizer_generated_cave())return;
    if(choosingSave)gameflow.mIsUIOverlayActive=FALSE;
    choosingSave=false;saveChoice=nullptr;
    detaching=pending&&permitGeneration!=0;
    sceneReady=false;pc_p2_cave_items_shutdown();pc_p2_cave_geometry_shutdown();pc_p2_cave_rooms_shutdown();
    pc_p2_cave_campaign_party_scene_exit();
}
P2CaveBoundarySnapshot pc_p2_cave_campaign_boundary(){
    P2CaveBoundarySnapshot result;
    if(!pc_randomizer_generated_cave()||!prepared||!sceneReady
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
    if(!pc_randomizer_generated_cave()||!choosingSave||!saveChoice)return result;
    result.active=true;result.state=saveChoice->mState;
    if(saveChoice->mMemChkMgr)result.defaultFile=saveChoice->mMemChkMgr->pcDefaultFileSnapshot();
    if(saveChoice->mIsScreenVisible&&saveChoice->mFileSelectMgr)
        result.slot=saveChoice->mFileSelectMgr->pcSaveInputSlot();
    return result;
}
bool pc_p2_cave_campaign_update_save_choice(Controller* input){
    if(!pc_randomizer_generated_cave()||!choosingSave||!saveChoice)return false;
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
        std::puts("P2_CAMPAIGN_SAVE_CHOICE_CANCELLED realm_unchanged=1");
    }
    return true;
}
void pc_p2_cave_campaign_draw_save_choice(Graphics& gfx){
    if(pc_randomizer_generated_cave()&&choosingSave&&saveChoice)saveChoice->draw(gfx);
}
void pc_p2_cave_campaign_tick(){
    if(!pc_randomizer_generated_cave()||!sceneReady)return;
    if(inside()){pc_p2_cave_carry_tick();pc_p2_cave_geometry_tick();pc_p2_cave_bud_tick();}
}
bool pc_p2_cave_campaign_commit_transition(){
    if(!pc_randomizer_generated_cave()||!requested)return false;
    requested=false;if(!safe()||!atBoundary())return false;
    const bool entering=!inside();auto party=pc_randomizer_generated_cave_party();
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
    flush();
    const auto oldImage=pc_p2_cave_campaign_cache_image();
    const auto oldBanks=pc_randomizer_generated_cave_cache();
    const auto oldParty=pc_randomizer_generated_cave_party();
    const auto oldPlayState=gameflow.mPlayState;
    std::uint64_t oldGeneration=0;
    pc_randomizer_checkpoint_info(&oldGeneration,nullptr);
    if(entering)pc_p2_cave_campaign_cache_enter(STAGE_Forest);
    else pc_p2_cave_campaign_cache_return();
    moveParty(party,entering);pc_randomizer_generated_cave_party_set(party);
    // Persist the destination realm and its party with the ordinary native
    // card, without the sunset deposit/actor teardown. Only a completed native
    // SAVE authorizes the section transition.
    gameflow.mMemoryCard.saveCurrentGame();
    std::uint64_t savedGeneration=0;
    pc_randomizer_checkpoint_info(&savedGeneration,nullptr);
    if(savedGeneration<=oldGeneration){
        clearPermit();
        pc_p2_cave_campaign_cache_restore_image(oldImage);
        pc_randomizer_generated_cave_cache_set(oldBanks);
        pc_randomizer_generated_cave_party_set(oldParty);
        gameflow.mPlayState=oldPlayState;
        std::puts("P2_CAMPAIGN_BOUNDARY_HELD native_save_failed=1 rollback=1");return false;}
    // saveOptions runs after the native campaign checkpoint. Its I/O failure
    // cannot undo a committed destination card or authorize replaying the old
    // realm; preserve the actual checkpoint as the boundary authority.
    std::printf("P2_CAMPAIGN_BOUNDARY_SAVE generation=%llu options_failed=%d\n",
        static_cast<unsigned long long>(savedGeneration),int(gameflow.mMemoryCard.didSaveFail()));
    if(!adoptPermit())invalid("committed boundary checkpoint proof unavailable");
    pending=true;sceneReady=false;
    std::printf("P2_CAMPAIGN_BOUNDARY_COMMITTED floor=%d bodies=%zu surface_heads=%zu floor_heads=%zu\n",
        int(entering),party.bodies.size(),party.surfaceHeads.size(),party.floorHeads.size());
    return true;
}
bool pc_p2_cave_campaign_restored_party(){return pc_randomizer_generated_cave()&&restored;}
bool pc_p2_cave_campaign_survivor_permit(const std::string& sourceKey,std::uint32_t recordUid,
    std::uint32_t attempt,std::uint64_t activation,const std::string& catalogFingerprint,
    std::uint64_t* generation,std::uint8_t sha[32]){
    if(!pc_randomizer_generated_cave()){
#if defined(PIKMIN_P2_SURFACE_SAVE_PROVIDER)
        return pc_p2_surface_save_survivor_permit(sourceKey,recordUid,attempt,activation,catalogFingerprint,generation,sha);
#else
        return false;
#endif
    }
    const auto& party=pc_randomizer_generated_cave_party();
    return p2CaveSurvivorPermit(party,detaching||restoringParty,permitGeneration,
        pc_randomizer_active_campaign_generation(),permitSha,sourceKey,recordUid,attempt,
        activation,catalogFingerprint,generation,sha);
}
bool pc_p2_cave_campaign_owns_heads(){
    if(!pc_randomizer_generated_cave()||!prepared)return false;
    const auto& party=pc_randomizer_generated_cave_party();
    return party.present&&party.resumeLiving&&party.inside==inside()
        &&flowCont.mCurrentStage==(inside()?floorStage:surfaceStage);
}
int pc_p2_cave_campaign_floor(){return pc_randomizer_generated_cave()&&prepared&&inside()?1:0;}
std::string pc_p2_cave_campaign_token(){return pc_randomizer_generated_cave()&&prepared?config.token:std::string();}
void pc_p2_cave_campaign_before_day_cleanup(){
    if(!pc_randomizer_generated_cave()||!prepared)return;
    if(!inside()){
        auto party=pc_randomizer_generated_cave_party();
        if(party.present){party.resumeLiving=false;party.bodies.clear();
            pc_randomizer_generated_cave_party_set(party);}
        return;
    }
    // A live floor must never enter P1's sunset survivor deposit. Its SAVE
    // authority is the settled native boundary transaction above.
    invalid("floor reached destructive surface day-end path");
}

bool pc_p2_cave_campaign_survivor_body(const std::string& sourceKey,std::uint32_t recordUid,
    std::uint32_t attempt,std::uint64_t activation,const std::string& catalogFingerprint,
    OriginalPikiBodyState& state,std::uint64_t* generation,std::uint8_t sha[32]){
    if(!pc_randomizer_generated_cave()){
#if defined(PIKMIN_P2_SURFACE_SAVE_PROVIDER)
        return pc_p2_surface_save_survivor_body(sourceKey,recordUid,attempt,activation,catalogFingerprint,state,generation,sha);
#else
        return false;
#endif
    }
    return p2CaveSurvivorBody(pc_randomizer_generated_cave_party(),detaching||restoringParty,
        permitGeneration,pc_randomizer_active_campaign_generation(),permitSha,
        sourceKey,recordUid,attempt,activation,catalogFingerprint,state,generation,sha);
}
