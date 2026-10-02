#pragma once
#include "pc_midday_codec.h"
struct WorldClock;
namespace pc_midday {
// Component only: scene/fence/first-update state is a separate required envelope.
struct ClockFields {
    float secondsPerHour=0,hoursPerDay=0,secondsPerDay=0;
    float previousTime=0,secondsIntoHour=0,timeOfDay=0,deltaTime=0;
    int32_t hour=0,day=0,minute=0;
};
bool encodeClock(const ClockFields&,Bytes&,std::string&);
bool decodeClock(const Bytes&,ClockFields&,std::string&);
// Reads all named logical fields, without advancing or recomputing the clock.
bool captureClockComponent(const WorldClock&,Bytes&,std::string&);
}
