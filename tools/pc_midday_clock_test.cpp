#include "pc_midday_clock.h"
#include <iostream>
#include <stdexcept>
#include <limits>
using namespace pc_midday;
int checks=0;
void check(bool yes,const char* why){++checks;if(!yes)throw std::runtime_error(why);}
int main(){try{
    ClockFields c{30.25f,24,726,12.125f,3.75f,12.123966f,0.001f,12,8,7},out;
    Bytes bytes;std::string e;check(encodeClock(c,bytes,e)&&bytes.size()==52,"all ten logical fields encoded");
    check(decodeClock(bytes,out,e),"decode valid clock component");Bytes again;check(encodeClock(out,again,e)&&again==bytes,"all clock bits preserved without phase recomputation");
    for(size_t offset=12;offset<52;offset+=4){auto altered=bytes;altered[offset]^=1;ClockFields decoded;
        if(offset==16){check(!decodeClock(altered,decoded,e),"hours-per-day profile is fixed at24");continue;}
        check(decodeClock(altered,decoded,e)&&encodeClock(decoded,again,e)&&again==altered,"each named field retains independent encoded bits");}
    for(float* field:{&c.secondsPerHour,&c.hoursPerDay,&c.secondsPerDay,&c.previousTime,&c.secondsIntoHour,&c.timeOfDay,&c.deltaTime}){
        const float saved=*field;*field=std::numeric_limits<float>::quiet_NaN();again={99};check(!encodeClock(c,again,e)&&again==Bytes{99},"nonfinite field refuses without output mutation");*field=saved;
    }
    for(size_t count:{size_t(0),size_t(8),size_t(51),size_t(53)}){auto bad=bytes;bad.resize(count);out.day=777;check(!decodeClock(bad,out,e)&&out.day==777,"invalid frame length leaves decoder output unchanged");}
    auto bad=bytes;bad[8]=2;out.day=777;check(!decodeClock(bad,out,e)&&out.day==777,"future clock version refused");
    bad=bytes;bad[0]^=1;check(!decodeClock(bad,out,e),"foreign component rejected");
    bad=bytes;bad[44]=255;bad[45]=255;bad[46]=255;bad[47]=255;check(!decodeClock(bad,out,e),"negative serialized day refused");
    c.secondsIntoHour=c.secondsPerHour;check(!encodeClock(c,again,e),"unadvanced overflow phase refused");c.secondsIntoHour=3.75f;
    c.hoursPerDay=25;check(!encodeClock(c,again,e),"unsupported hours-per-day refused");c.hoursPerDay=24;
    c.timeOfDay=25;check(!encodeClock(c,again,e),"movie time cannot masquerade as ordinary day clock");
    std::cout<<"PASS "<<checks<<" typed clock-component controls; no scene capture or world restore\n";return 0;
}catch(const std::exception& x){std::cerr<<"FAIL "<<checks<<": "<<x.what()<<"\n";return 1;}}
