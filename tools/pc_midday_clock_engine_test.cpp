#include "pc_midday_clock.h"
#include "WorldClock.h"
#include <iostream>
using namespace pc_midday;
int main(){
    WorldClock w{};w.mRealSecsPerGameHour=30.25f;w.mHoursInDay=24;w.mRealSecsPerGameDay=726;
    w.mPrevTimeOfDay=12.125f;w.mRealSecsIntoHour=3.75f;w.mTimeOfDay=12.123966f;w.mDeltaTimeOfDay=0.001f;
    w.mCurrentGameHour=12;w.mCurrentDay=8;w.mCurrentGameMinute=7;for(auto& b:w._0C)b=0xa5;
    ClockFields expected{30.25f,24,726,12.125f,3.75f,12.123966f,0.001f,12,8,7};Bytes a,b,after;std::string e;
    if(!encodeClock(expected,a,e)||!captureClockComponent(w,b,e)||a!=b)return 1;
    for(auto byte:w._0C)if(byte!=0xa5)return 2;
    if(!captureClockComponent(w,after,e)||after!=b)return 3;
    w.mDeltaTimeOfDay=-1;after={99};if(captureClockComponent(w,after,e)||after!=Bytes{99})return 4;
    std::cout<<"PASS actual WorldClock header bridge all ten named fields, read-only repeat, unused-byte preservation and invalid-state refusal; fixture object only\n";return 0;
}
