#include "pc_midday_player_core.h"
#include "PlayerState.h"
namespace pc_midday {
namespace {
bool graphMatches(const TimeGraph& g,uint16_t start,uint16_t end){return end>=start&&size_t(end-start)+1<=4096&&g.mStartTime==start&&g.mEndTime==end&&g.mEntries;}
void graphRead(const TimeGraph& g,PlayerGraph& out){out.start=g.mStartTime;out.end=g.mEndTime;out.entries.resize(size_t(out.end-out.start)+1);for(size_t i=0;i<out.entries.size();++i)for(size_t c=0;c<3;++c)out.entries[i][c]=g.mEntries[i].mNum[c];}
}
bool capturePlayerCore(const PlayerState& p,const PlayerCoreTopology& topology,const PlayerCoreReadFence& fence,Bytes& out,std::string& e){
 if(!fence.sceneInitialized||!fence.agreedReadOnlyFence||fence.tickBefore!=fence.tickAfter){e="player capture requires initialized stopped scene fence";return false;}
 static_assert(MAX_DAYS==30&&STAGE_COUNT==5&&PikiColorCount==3,"PlayerState topology changed");
 // Check every known allocated course before any dynamic data read. Never touch
 // zero-count pointers: initGame leaves those pointer slots uninitialized.
 if(!graphMatches(p.mPerHourGraph,topology.hourStart,topology.hourEnd)||!graphMatches(p.mPerDayGraph,topology.dayStart,topology.dayEnd)){e="PlayerState graph factory mismatch";return false;}
 for(size_t i=0;i<5;++i){const auto n=topology.courseEntries[i];if(n>4096){e="course factory count exceeds bound";return false;}if(n&&(!p.mCourseFlags||!p.mCourseFlags[i]||p.mCourseFlags[i]->mEntryCount!=n||p.mCourseFlags[i]->mSize!=n/8+1||!p.mCourseFlags[i]->mFlags)){e="course flags factory mismatch";return false;}}
 PlayerCoreFields v;
 v.sprouted=p.mSproutedNum;v.lostBattle=p.mLostBattlePikis;v.leftBehind=p.mLeftBehindPikis;v.totalPlucked=p.mTotalPluckedPikiCount;
 v.totalRegisteredParts=p.mTotalRegisteredParts;v.totalParts=p.mTotalParts;v.currentParts=p.mCurrParts;v.requiredParts=p.mRequiredUfoPartCount;
 v.totalDead=p.mTotalDeadPikiNum;v.totalBorn=p.mTotalBornPikiNum;v.living=p.mLivingPikiNum;
 v.shipUpgrade=p.mShipUpgradeLevel;v.shipEffect=p.mShipEffectPartFlag;v.container=p.mContainerFlag;v.displayPiki=p.mDisplayPikiFlag;
 v.extinctionPlayed=p.mHasExtinctionDemoPlayed;v.tutorial=p.mIsTutorialMode;v.unused186=p._186;v.naviPilot=p.mIsNaviPilot;v.dayEnd=p.mInDayEnd;v.challenge=p.mIsChallengeMode;v.lastUpdated=p.mLastUpdatedTime;
 for(size_t i=0;i<30;++i){v.collectedByDay[i]=p.mPartsCollectedByDay[i];v.partsToNext[i]=p.mPartsToNextByDay[i];}for(size_t i=0;i<5;++i)v.stageParts[i]=p.mStagePartsCollected[i];
 graphRead(p.mPerHourGraph,v.hour);graphRead(p.mPerDayGraph,v.day);
 for(size_t i=0;i<5;++i){auto& f=v.courses[i];f.entries=topology.courseEntries[i];if(f.entries){const auto& live=*p.mCourseFlags[i];f.bits.assign(live.mFlags,live.mFlags+live.mSize);}}
 return encodePlayerCore(v,out,e);
}
}
