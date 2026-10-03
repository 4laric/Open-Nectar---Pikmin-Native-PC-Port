// Private engine regression. Stocks and input are explicitly injected;
// this does not qualify natural Honey pickup or campaign save/resume.
#include <SDL2/SDL.h>
#include "App.h"
#include "Node.h"
#include "Navi.h"
#include "NaviMgr.h"
#include "NaviState.h"
#include "Piki.h"
#include "PikiMgr.h"
#include "PikiState.h"
#include "CPlate.h"
#include "Kontroller.h"
#include "GameStat.h"
#include "MoviePlayer.h"
#include "PlayerState.h"
#include "gameflow.h"
#include "system.h"
#include "pc_p2_sprays.h"
#include "pc_p2_cave_campaign_party_engine.h"
#include "pc_p2_cave_campaign_cache_engine.h"
#include "pc_randomizer.h"
#include "MemoryCard.h"
#include "pc_p2_original_resource_state.h"
#include "pc_p2_original_honey_bank.h"
#include "pc_window.h"
#include "pc_bbft.h"
#include "pc_gpu_preference.h"
#include "settings/pc_settings.h"
#include "settings/pc_settings_p2d.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <cstring>
#include <limits>
#include <sstream>
namespace {
void require(bool ok,const char* reason) {
    if (!ok) { std::printf("P2_SPICY_RUNTIME_FAIL %s\n",reason); std::fflush(nullptr); std::_Exit(1); }
}
// Equivalent to canonical scripts/p2_fixture_captain_guard.h; no healing.
void captainGuard(bool dead,bool deadState,float hp,int tick) {
    if (dead || deadState || !std::isfinite(hp) || hp <= 1) {
        std::printf("P2_FIXTURE_CAPTAIN_DOWN tick=%d hp=%.3f orima_dead=%d dead_state=%d outcome=BLOCKED\n",tick,hp,int(dead),int(deadState));
        std::fflush(nullptr); std::_Exit(86);
    }
}
struct Baseline { Piki* p; float attack,speed; int maturity; };
class SpicyApp : public PlugPikiApp {
    int ticks=0,phase=0,pauseTicks=0;
    bool sawCaptain=false,whistled=false;
    bool sawPendingBegin=false;
    std::vector<Baseline> squad;
    p2originalresource::ResourceState stock;
    p2originalresource::EggContents contents;
    p2originalresource::honey::SourceBank bank;
    p2originalresource::honey::Resources resources;
    std::vector<float> pausedRemaining;
    float phaseTime=0,setupTime=0;
    void saveHold(const char* label,const char* expected) {
        std::string e;
        require(!pc_p2_spicy_save_preflight(e)&&e==expected,"wrong living-roster SAVE hold");
        p2originalresource::ResourceSnapshot before,after;
        require(stock.snapshot(before,e),"stock snapshot before SAVE hold");
        const auto generation=pc_randomizer_active_campaign_generation();
        const auto cache=pc_p2_campaign_cache_image();
        const auto banks=pc_randomizer_generated_cave_cache();
        const std::vector<u8> card(cardData,cardData+CARD_DATA_SIZE);
        const auto savedDay=gameflow.mPlayState.mSavedDay;
        const auto saveStatus=gameflow.mPlayState.mSaveStatus;
        for(bool inside:{false,true}) {
            P2CaveCampaignParty party;party.present=true;party.nextKey=37;
            std::ostringstream original;party.write(original);
            require(!pc_p2_cave_campaign_party_capture(party,inside),"spicy Party capture admitted");
            std::ostringstream held;party.write(held);
            require(original.str()==held.str()&&party.nextKey==37,"held Party capture published state");
        }
        // Valid slot prevents the historical invalid-slot guard masking this
        // control. This invokes the actual common card API, not its UI.
        const auto slot=gameflow.mGamePrefs.mSpareMemCardSaveIndex;
        gameflow.mGamePrefs.mSpareMemCardSaveIndex=4;
        gameflow.mMemoryCard.saveCurrentGame();
        gameflow.mGamePrefs.mSpareMemCardSaveIndex=slot;
        require(gameflow.mMemoryCard.didSaveFail(),"spicy native card SAVE admitted");
        require(!std::memcmp(card.data(),cardData,CARD_DATA_SIZE),"held SAVE changed card bytes");
        require(pc_randomizer_active_campaign_generation()==generation,"held SAVE advanced generation");
        require(pc_p2_campaign_cache_image()==cache,"held SAVE changed native cache");
        const auto& heldBanks=pc_randomizer_generated_cave_cache();
        require(banks.inside==heldBanks.inside&&banks.surface==heldBanks.surface&&banks.floor==heldBanks.floor,"held SAVE changed cache banks");
        require(savedDay==gameflow.mPlayState.mSavedDay&&saveStatus==gameflow.mPlayState.mSaveStatus,"held SAVE changed PlayState");
        require(stock.snapshot(after,e)&&before.sprayCounts==after.sprayCounts
            &&before.berryCounts==after.berryCounts&&before.sprayUses==after.sprayUses
            &&before.sprayMade==after.sprayMade&&before.completed.size()==after.completed.size(),"held SAVE changed stock journal");
        std::printf("P2_SPICY_SAVE_HOLD_CONTROL label=%s reason=%s actual_party_api_both_realms=1 actual_card_api=1 card_bytes_generation_cache_stock_unchanged=1 authored_transition_UI=UNTESTED\n",label,expected);
    }
    bool input(Navi* n) {
        auto previous=n->mKontroller->mInputPressed;
        n->mKontroller->mInputPressed=KBBTN_DPAD_UP;
        bool used=pc_p2_sprays_input(n);
        n->mKontroller->mInputPressed=previous;
        return used;
    }
public:
    int idle() override {
        int result=PlugPikiApp::idle();
        Navi* n=naviMgr?naviMgr->getNavi():nullptr;
        if(n&&n->getCurrState()) { sawCaptain=true;captainGuard(GameStat::orimaDead,n->getCurrState()->getID()==NAVISTATE_Dead,n->mHealth,ticks); }
        else require(!sawCaptain,"initialized captain disappeared");
        require(++ticks<7000,"frame timeout");
        auto* movies=gameflow.mMoviePlayer;
        if(movies&&movies->mIsActive) { movies->requestSkip(); return result; }
        if(!n||!n->getCurrState()||!pikiMgr||!playerState||gameflow.mIsUIOverlayActive) return result;
        if(phase==2) {
            for(unsigned i=0;i<squad.size();++i) require(squad[i].p->mP2Spicy.remaining==pausedRemaining[i],"effect timer advanced in pause");
            if(++pauseTicks<30) return result;
            gameflow.mPauseAll=false;
            require(input(n),"repeat input failed");
            for(const auto& b:squad) require(b.p->mP2Spicy.remaining==40,"repeat did not refresh");
            require(!input(n),"zero stock spent");
            phase=3;phaseTime=0;return result;
        }
        if(gameflow.mPauseAll||(phase==0&&n->getCurrState()->getID()!=NAVISTATE_Walk)) return result;
        if(phase==0) {
            setupTime+=gsys->getFrameTime();
            Iterator pikis(pikiMgr);int alive=0;CI_LOOP(pikis) { auto* p=static_cast<Piki*>(*pikis);if(p&&p->isAlive()) ++alive; }
            require(alive==20,"fresh fixture must have exactly20 live Pikmin");
            // Preview releases its authored starting squad to FreeMode. Use
            // the actual whistle receiver with injected aim, never set party
            // slots/modes or move/heal actors to manufacture a formation.
            if(!whistled) {
                Iterator gather(pikiMgr);CI_LOOP(gather) { auto* p=static_cast<Piki*>(*gather);if(p&&p->isAlive()) {
                    Vector3f previous=n->mCursorWorldPos;n->mCursorWorldPos=p->getPosition();
                    n->callPikis(200.0f);n->mCursorWorldPos=previous;break;
                } }
                whistled=true;std::puts("P2_SPICY_SETUP whistle_receiver=actual aim=injected");return result;
            }
            squad.clear();
            Iterator party(n->mPlateMgr);CI_LOOP(party) { auto* p=static_cast<Piki*>(*party);if(p&&p->isAlive()&&p->getState()==PIKISTATE_Normal) squad.push_back({p,p->getAttackPower(),p->getSpeed(.25f),p->mHappa}); }
            if(squad.size()!=20) {
                Iterator gather(pikiMgr);CI_LOOP(gather) { auto* p=static_cast<Piki*>(*gather);if(p&&p->isAlive()&&p->mMode==PikiMode::FreeMode) {
                    Vector3f previous=n->mCursorWorldPos;n->mCursorWorldPos=p->getPosition();
                    n->callPikis(200.0f);n->mCursorWorldPos=previous;
                } }
                if(ticks%30==0) { Iterator pending(pikiMgr);CI_LOOP(pending) { auto* p=static_cast<Piki*>(*pending);if(p&&p->isAlive())std::printf("P2_SPICY_SETUP state=%d mode=%d captain=%d\n",p->getState(),p->mMode,p->mNavi==n); } }
                require(setupTime<8,"incomplete actual formation after initialization");return result;
            }
            std::string e;p2originalresource::ResourceSnapshot injected;
            injected.sprayCounts[0]=2;require(stock.restore(injected,contents,e),"fixture inventory install");
            require(!pc_p2_sprays_bind(&stock,nullptr,e),"bind accepted missing source receiver");
            if(!bank.resources(resources,e))require(false,e.c_str());
            require(pc_p2_sprays_bind(&stock,&resources.receiverClips[1],e),"source-clock/inventory binding");
            float observed=12.5f;bool pending=true;
            require(!pc_p2_spicy_save_observation(nullptr,observed,pending)&&observed==12.5f&&pending,"null observation changed outputs");
            require(pc_p2_spicy_save_preflight(e),"normal zero-effect roster refused");
            // Deliberate invalid scalar controls on an initialized live actor;
            // restore immediately without any simulation tick or health write.
            auto* controlled=squad.front().p;
            for(float invalid:{-1.f,41.f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}) {
                const float saved=controlled->mP2Spicy.remaining;
                controlled->mP2Spicy.remaining=invalid;
                require(!pc_p2_spicy_save_observation(controlled,observed,pending)&&observed==12.5f&&pending,"invalid observation changed outputs");
                saveHold("invalid_timer_injected","invalid_spicy_snapshot");
                controlled->mP2Spicy.remaining=saved;
            }
            require(input(n),"spicy input did not consume");
            require(stock.sprayCount(p2originalresource::HoneyKind::Spicy)==1,"wrong stock decrement");
            for(const auto& b:squad) require(!b.p->mP2Spicy.active()&&b.p->getState()==PIKISTATE_P2Dope,"effect committed before animation callback");
            saveHold("actual_pre_ACTION","spicy_pending_dope");
            std::printf("P2_SPICY_RUNTIME_START alive=%d formation=%zu stocks=injected input=injected receiver=actual_animation captain=unprotected\n",alive,squad.size());
            phase=1;return result;
        }
        phaseTime+=gsys->getFrameTime();
        if(phase==1) {
            if(!sawPendingBegin)for(const auto& b:squad)if(b.p->mP2Spicy.active()&&b.p->getState()==PIKISTATE_P2Dope) {
                saveHold("actual_post_BEGIN_pre_END","spicy_pending_dope");sawPendingBegin=true;break;
            }
            bool ready=true;
            for(const auto& b:squad) ready=ready&&b.p->mP2Spicy.active()&&b.p->getState()==PIKISTATE_Normal;
            require(phaseTime<8,"missing Growup callback or return to normal");
            if(!ready) return result;
            require(sawPendingBegin,"post-BEGIN pending reaction was not observed");
            saveHold("actual_active_Normal","spicy_remaining_requires_graph");
            for(const auto& b:squad) {
                require(b.p->mHappa==b.maturity,"spicy changed maturity");
                require(b.p->getAttackPower()==10&&b.p->getSpeed(.25f)==190,"source effects absent");
                pausedRemaining.push_back(b.p->mP2Spicy.remaining);
            }
            gameflow.mPauseAll=true;phase=2;return result;
        }
        if(phase==3) {
            if(ticks%120==0)std::printf("P2_SPICY_PROGRESS seconds=%.3f remaining=%.3f captain_state=%d\n",phaseTime,squad.front().p->mP2Spicy.remaining,n->getCurrState()->getID());
            bool recovered=true;for(const auto& b:squad) recovered=recovered&&!b.p->mP2Spicy.active();
            require(phaseTime<43,"40sec effect did not expire");
            if(!recovered) return result;
            std::string readyError;
            require(pc_p2_spicy_save_preflight(readyError),"expired Normal roster still held by spicy gate");
            for(const auto& b:squad){float remaining=-1;bool pending=true;
                require(pc_p2_spicy_save_observation(b.p,remaining,pending)&&remaining==0&&!pending,"expired observation invalid");}
            std::puts("P2_SPICY_SAVE_EXPIRED_CONTROL actual_actor_clock=1 spicy_preflight_allowed=1 full_capture_and_card_save=UNTESTED");
            require(phaseTime>=39,"effect expired early");
            for(const auto& b:squad) {
                require(b.p->isAlive()&&b.p->mHappa==b.maturity,"survival or maturity changed");
                require(b.p->getAttackPower()==b.attack&&b.p->getSpeed(.25f)==b.speed,"baseline effects not restored");
            }
            std::string e;require(pc_p2_sprays_bind(nullptr,nullptr,e),"detach");
            std::printf("P2_SPICY_RUNTIME_PASS formation=%zu actual_animation=1 source_stats=1 pause=1 refresh=1 stock_zero=1 recovery_seconds=%.3f injected_stock_and_input=1 natural_pickup=UNTESTED save_resume=UNTESTED\n",squad.size(),phaseTime);
            std::fflush(nullptr);std::_Exit(0);
        }
        return result;
    }
};
}
int main(int argc,char** argv) {
    for(int i=1;i<argc;++i)if(std::string(argv[i])=="--guard-negative")captainGuard(false,false,0,0);
    SDL_SetMainReady();pc_gpu_preference_apply();SDL_setenv("SDL_AUDIODRIVER","dummy",1);
    SDL_setenv("PIKMIN_RANDOMIZER_TEST_BACKGROUND","1",1);pc_bbft_init(argc,argv);
    require(pc_pikipelago_room_preview(),"requires experimental room");
    if(!pc_window_init("P2 spicy engine regression",960,540))return 3;
    pc_settings_init();pc_window_set_window_size(960,540);pc_window_center();
    SDL_Window* w=SDL_GL_GetCurrentWindow();int width,height,x,y;SDL_GetWindowSize(w,&width,&height);SDL_GetWindowPosition(w,&x,&y);
    SDL_Rect bounds{};SDL_GetDisplayBounds(SDL_GetWindowDisplayIndex(w),&bounds);
    require(width==960&&height==540,"window dimensions");
    bool centered=std::abs(x-(bounds.x+(bounds.w-width)/2))<=2&&std::abs(y-(bounds.y+(bounds.h-height)/2))<=2;
    require(centered,"window centering");
    std::printf("P2_SPICY_WINDOW width=%d height=%d centered=%d\n",width,height,int(centered));
    gsys->Initialise();pc_settings_p2d_init();nodeMgr=new NodeMgr();gsys->run(new SpicyApp);return 0;
}
