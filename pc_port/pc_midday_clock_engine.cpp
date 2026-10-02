#include "pc_midday_clock.h"
#include "WorldClock.h"
#include "pc_midday_restore.h"
namespace pc_midday {
bool captureClockComponent(const WorldClock& w,Bytes& out,std::string& e){
    const ClockFields c={w.mRealSecsPerGameHour,w.mHoursInDay,w.mRealSecsPerGameDay,
        w.mPrevTimeOfDay,w.mRealSecsIntoHour,w.mTimeOfDay,w.mDeltaTimeOfDay,
        w.mCurrentGameHour,w.mCurrentDay,w.mCurrentGameMinute};
    return encodeClock(c,out,e);
}
bool applyClockComponent(WorldClock& w,const Bytes& state,const RestoreGate& g,std::string& e){
    if(!g.freshProcess||!g.paused||!g.zeroInput||!g.birthEffectsSuppressed||!g.rewardsSuppressed||!g.rngDrawsSuppressed||!g.audioVoicesSuppressed){
        e="clock apply requires full fresh paused constructor fence";return false;
    }
    ClockFields c;if(!decodeClock(state,c,e))return false;
    // No method calls, allocations or callbacks after validation; do not touch
    // unknown _0C padding or infer/recompute a supposedly equivalent phase.
    w.mRealSecsPerGameHour=c.secondsPerHour;w.mHoursInDay=c.hoursPerDay;w.mRealSecsPerGameDay=c.secondsPerDay;
    w.mPrevTimeOfDay=c.previousTime;w.mRealSecsIntoHour=c.secondsIntoHour;w.mTimeOfDay=c.timeOfDay;w.mDeltaTimeOfDay=c.deltaTime;
    w.mCurrentGameHour=c.hour;w.mCurrentDay=c.day;w.mCurrentGameMinute=c.minute;
    e.clear();return true;
}
}
