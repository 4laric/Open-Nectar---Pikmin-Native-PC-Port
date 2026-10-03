#pragma once
#include "pc_p2_original_piki_native_facts.h"
#include <cstdint>
namespace p2retail {class SceneContext;class SceneRuntime;}
class StageInfo;class MapMgr;
namespace p2original {
namespace piki {class NativeBodyFactory;}
enum class GameSystemMode {Story=0,Versus=1,OnePlayerChallenge=2,TwoPlayerChallenge=3,Piklopedia=4};
struct GameSystemState {
 // Literal GameSystem::init state, not inferred from P1 or Captain World.
 std::uint8_t flags=0,pauseCountdown=0,unused=0;
 GameSystemMode mode=GameSystemMode::Story;
 bool frozen=false,softPause=false,moviePause=false;
 std::uint32_t frameTimer=0;
 bool paused()const noexcept{return !pauseCountdown&&softPause;}
};
// Source-owned scalar GameSystem lifecycle. MoviePlayer/MovieConfig, TimeMgr
// and actor movie flags require independent actual producers; readState does
// not silently supply them. The actual retained Stage event owners alone
// invoke named private mutations. No public permission/flags/phase setter.
class GameSystem final {
public:
 GameSystem(const GameSystem&)=delete;GameSystem& operator=(const GameSystem&)=delete;
 bool readState(GameSystemState&,std::string&)const;
 bool inCave(bool&,std::string&)const;
 bool readTime(std::string&)const;
 bool readMovie(std::string&)const;
 bool readMovieDraw(std::string&)const;
private:
 friend class piki::NativeBodyFactory;friend class p2retail::SceneRuntime;
 GameSystem()=default;
 bool sourceInit(const p2retail::SceneContext&,std::string&);
 bool sourceStartFrame(std::string&);bool sourceEndFrame(std::string&);
 // Actual GameState/CaveState::init after owned physical commit, respectively.
 bool sourceGameStateInit(std::string&);bool sourceCaveStateInit(std::string&);
 bool sourceGameStart(std::string&);
 bool sourceSectionLeft(std::string&);
 // BaseGameSection::waitSyncLoad branches !allowPause; no P1 pause alias.
 bool sourceWaitSyncLoadPause(std::string&);bool sourceWaitSyncLoadComplete(std::string&);
 // Actual source NaviContainer::init only AFTER real screen open succeeds;
 // cleanup before source state/roster retirement. Never infer menu visibility.
 bool sourceContainerScreenOpened(Navi*,std::string&);
 bool sourceContainerCleanup(Navi*,std::string&);
 bool sourceRelease(std::string&);
 const p2retail::SceneContext* mStage=nullptr;StageInfo* mStageInfo=nullptr;MapMgr* mMap=nullptr;
 std::uint64_t mSerial=0,mRevision=0;std::string mCampaign,mSession;
 piki::SceneBinding mBinding;GameSystemState mState;
 bool mSectionKnown=false,mInCave=false;Navi* mContainer=nullptr;
 mutable bool mBusy=false,mReentered=false;
 class Operation;
 bool exact(std::string&)const;
 bool sectionInit(bool,std::string&);
 bool pause(bool,std::string&);
};
}
