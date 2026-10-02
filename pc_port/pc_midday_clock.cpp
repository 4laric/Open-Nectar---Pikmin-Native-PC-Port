#include "pc_midday_clock.h"
#include <cmath>
#include <cstring>
#include <limits>
#include <algorithm>
namespace pc_midday {
namespace {
bool valid(const ClockFields& c,std::string& e){
    if(sizeof(float)!=4||!std::numeric_limits<float>::is_iec559){e="unsupported clock float representation";return false;}
    for(float f:{c.secondsPerHour,c.hoursPerDay,c.secondsPerDay,c.previousTime,c.secondsIntoHour,c.timeOfDay,c.deltaTime})
        if(!std::isfinite(f)){e="nonfinite clock component";return false;}
    // Only ordinary logical day clocks are admitted. Scene/movie save eligibility
    // remains separate; this does not infer it from a valid clock.
    if(c.secondsPerHour<=0||c.hoursPerDay!=24||c.secondsPerDay<=0||c.secondsIntoHour<0||c.secondsIntoHour>=c.secondsPerHour||
       c.previousTime<0||c.previousTime>=24||c.timeOfDay<0||c.timeOfDay>=24||c.deltaTime<0||c.deltaTime>=24||
       c.hour<0||c.hour>=24||c.day<0||c.minute<0||c.minute>=60){e="clock component outside ordinary day bounds";return false;}
    return true;
}
void put(Bytes& b,uint32_t n){for(unsigned i=0;i<4;++i)b.push_back(uint8_t(n>>(8*i)));}
uint32_t get(const Bytes& b,size_t at){uint32_t n=0;for(unsigned i=0;i<4;++i)n|=uint32_t(b[at+i])<<(8*i);return n;}
}
bool encodeClock(const ClockFields& c,Bytes& out,std::string& e){
    if(!valid(c,e))return false;
    Bytes b={'P','C','C','L','O','C','K','1'};put(b,1);
    for(float f:{c.secondsPerHour,c.hoursPerDay,c.secondsPerDay,c.previousTime,c.secondsIntoHour,c.timeOfDay,c.deltaTime}){uint32_t n;std::memcpy(&n,&f,4);put(b,n);}
    for(int32_t n:{c.hour,c.day,c.minute})put(b,uint32_t(n));
    out=std::move(b);e.clear();return true;
}
bool decodeClock(const Bytes& b,ClockFields& out,std::string& e){
    const Bytes magic={'P','C','C','L','O','C','K','1'};
    if(b.size()!=52||!std::equal(magic.begin(),magic.end(),b.begin())||get(b,8)!=1){e="clock framing/version mismatch";return false;}
    ClockFields c;size_t offset=12;
    for(float* f:{&c.secondsPerHour,&c.hoursPerDay,&c.secondsPerDay,&c.previousTime,&c.secondsIntoHour,&c.timeOfDay,&c.deltaTime}){uint32_t n=get(b,offset);std::memcpy(f,&n,4);offset+=4;}
    for(int32_t* n:{&c.hour,&c.day,&c.minute}){const uint32_t value=get(b,offset);if(value>uint32_t(INT32_MAX)){e="negative/overflow clock counter";return false;}*n=int32_t(value);offset+=4;}
    if(!valid(c,e))return false;
    out=c;e.clear();return true;
}
}
