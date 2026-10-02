#include "pc_midday_clock.h"
#include "WorldClock.h"
#include "pc_midday_restore.h"
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
    RestoreGate gate{true,true,true,true,true,true,true};
    WorldClock target{};for(auto& byte:target._0C)byte=0x6b;
    Bytes baseline;ClockFields start{45,24,1080,9,1,9,0,9,1,0};
    if(!encodeClock(start,baseline,e)||!applyClockComponent(target,baseline,gate,e))return 5;
    for(unsigned f=0;f<7;++f){RestoreGate bad=gate;bool* fields[]={&bad.freshProcess,&bad.paused,&bad.zeroInput,&bad.birthEffectsSuppressed,&bad.rewardsSuppressed,&bad.rngDrawsSuppressed,&bad.audioVoicesSuppressed};*fields[f]=false;
        if(applyClockComponent(target,b,bad,e)||!captureClockComponent(target,after,e)||after!=baseline)return 6;
    }
    for(unsigned f=0;f<3;++f){auto malformed=b;if(f==0)malformed.pop_back();if(f==1)malformed[8]=2;if(f==2)malformed[51]=0xff;
        if(applyClockComponent(target,malformed,gate,e)||!captureClockComponent(target,after,e)||after!=baseline)return 7;
    }
    if(!applyClockComponent(target,b,gate,e)||!captureClockComponent(target,after,e)||after!=b)return 8;
    for(auto byte:target._0C)if(byte!=0x6b)return 9;
    w.mDeltaTimeOfDay=-1;after={99};if(captureClockComponent(w,after,e)||after!=Bytes{99})return 4;
    std::cout<<"PASS actual WorldClock header bridge all ten named fields, read-only repeat, unused-byte preservation and invalid-state refusal; seven apply fences, malformed atomic refusal, bit-exact staged apply; fixture object only\n";return 0;
}
