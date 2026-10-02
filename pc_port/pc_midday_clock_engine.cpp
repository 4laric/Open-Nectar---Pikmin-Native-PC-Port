#include "pc_midday_clock.h"
#include "WorldClock.h"
namespace pc_midday {
bool captureClockComponent(const WorldClock& w,Bytes& out,std::string& e){
    const ClockFields c={w.mRealSecsPerGameHour,w.mHoursInDay,w.mRealSecsPerGameDay,
        w.mPrevTimeOfDay,w.mRealSecsIntoHour,w.mTimeOfDay,w.mDeltaTimeOfDay,
        w.mCurrentGameHour,w.mCurrentDay,w.mCurrentGameMinute};
    return encodeClock(c,out,e);
}
}
