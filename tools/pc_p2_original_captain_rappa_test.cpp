// Actual Rappa policy TU controls, no sound backend or gameplay qualification.
#include "pc_p2_original_captain_rappa.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <cstdlib>
using namespace p2original::captain::rappa;
namespace {unsigned checks=0;}
#define CHECK(c) do{++checks;if(!(c)){std::cerr<<"FAIL "<<__LINE__<<" "<<#c<<"\n";std::abort();}}while(false)
struct PortControl:TrackPort {
 mutable unsigned calls=0;bool refuse=false;std::uint16_t value=0;State* change=nullptr;
 bool readPortAppDirect(std::uint32_t port,std::uint16_t& out,std::string& e)const override{CHECK(port==0xb);++calls;if(refuse){e="missing genuine JASTrack owner";return false;}out=value;if(change)CHECK(setId(*change,137,e));return true;}
};
int main(){State state,partner;std::string e;PlayPlan sound;WaitPlan wait;PortControl port;std::uint16_t table=123;
 CHECK(state.soundId()==UINT32_MAX&&state.delay()==0&&state.tableIndex()==0&&!state.slot());
 CHECK(!planPlay(state,false,0,0,sound,e));CHECK(!planWait(state,port,wait,e)&&port.calls==0);CHECK(!tableNumber(state,table,e)&&table==123);CHECK(!setId(state,137,e));
 CHECK(!initialize(state,static_cast<Slot>(2),e)&&!state.slot()&&state.soundId()==UINT32_MAX);
 CHECK(initialize(state,Slot::Olimar,e)&&state.soundId()==13&&state.slot()==Slot::Olimar);CHECK(initialize(partner,Slot::Partner,e)&&partner.soundId()==14);
 CHECK(tableNumber(state,table,e)&&table==0);
 CHECK(planPlay(state,false,1,1,sound,e)&&!sound.sound()&&!sound.delayAfterSound());CHECK(!afterActualStartSound(state,sound,e)&&state.delay()==0);
 CHECK(planPlay(state,true,std::nextafter(.1f,0.f),-.09f,sound,e)&&!sound.sound());CHECK(planPlay(state,true,-0.f,0,sound,e)&&!sound.sound());
 CHECK(planPlay(state,true,.1f,0,sound,e)&&sound.sound()->sourceId==13&&sound.sound()->priority==0&&sound.delayAfterSound()==16);CHECK(state.delay()==0);CHECK(afterActualStartSound(state,sound,e)&&state.delay()==16); // real backend may return null, policy still commits delay
 CHECK(!afterActualStartSound(state,sound,e)&&state.delay()==16);CHECK(!afterActualStartSound(partner,sound,e)&&partner.delay()==0);
 CHECK(planPlay(state,true,0,-.1f,sound,e)&&sound.delayAfterSound()==16);CHECK(afterActualStartSound(state,sound,e));
 CHECK(planPlay(state,true,.2f,-.8f,sound,e)&&sound.delayAfterSound()==5);CHECK(afterActualStartSound(state,sound,e)&&state.delay()==5); // max absolute, not length or min
 CHECK(planPlay(state,true,.5f,.1f,sound,e)&&sound.delayAfterSound()==10);CHECK(afterActualStartSound(state,sound,e)); // 7.5 truncates toward zero
 CHECK(planPlay(state,true,1,0,sound,e)&&sound.delayAfterSound()==3);CHECK(afterActualStartSound(state,sound,e));
 CHECK(planPlay(state,true,-2,0,sound,e)&&sound.delayAfterSound()==3);CHECK(afterActualStartSound(state,sound,e));
 CHECK(planPlay(state,true,std::numeric_limits<float>::max(),0,sound,e)&&sound.delayAfterSound()==3);
 auto oldSound=sound;CHECK(!planPlay(state,true,NAN,0,sound,e));CHECK(sound.sound()->sourceId==oldSound.sound()->sourceId&&sound.delayAfterSound()==oldSound.delayAfterSound());CHECK(!planPlay(state,false,0,INFINITY,sound,e));
 CHECK(planPlay(state,true,.8f,0,sound,e));CHECK(setId(state,137,e));CHECK(!afterActualStartSound(state,sound,e));CHECK(state.soundId()==137&&state.delay()==3&&state.tableIndex()==0);
 CHECK(planPlay(state,true,.8f,0,sound,e)&&sound.sound()->sourceId==137);CHECK(afterActualStartSound(state,sound,e));
 port.value=65535;CHECK(planWait(state,port,wait,e)&&port.calls==1&&wait.tableAfterRead()==65535&&wait.wait()==state.delay());CHECK(state.tableIndex()==0);CHECK(afterActualReadPort(state,wait,e)&&state.tableIndex()==65535);CHECK(!afterActualReadPort(state,wait,e));
 CHECK(tableNumber(state,table,e)&&table==65535&&port.calls==1);CHECK(state.delay()==5); // no timer tick/decrement API
 auto oldWait=wait;port.refuse=true;CHECK(!planWait(state,port,wait,e));CHECK(wait.tableAfterRead()==oldWait.tableAfterRead()&&wait.wait()==oldWait.wait());port.refuse=false;
 port.change=&state;CHECK(!planWait(state,port,wait,e));CHECK(state.tableIndex()==65535&&wait.tableAfterRead()==oldWait.tableAfterRead());port.change=nullptr;
 CHECK(initialize(state,Slot::Partner,e)&&state.soundId()==14&&state.delay()==5&&state.tableIndex()==65535); // init is not constructor/reset
 CHECK(planPlay(state,true,0,.5f,sound,e)&&sound.sound()->sourceId==14);CHECK(initialize(state,Slot::Partner,e));CHECK(!afterActualStartSound(state,sound,e));
 CHECK(setId(state,UINT32_MAX,e));CHECK(!planPlay(state,true,1,0,sound,e));CHECK(tableNumber(state,table,e)&&table==65535);CHECK(planWait(state,port,wait,e));CHECK(afterActualReadPort(state,wait,e)); // sync functions do not assert ID
 CHECK(!planPlay(state,false,0,0,sound,e)); // source asserts ID even when disabled
 CHECK(setId(state,13,e));CHECK(planPlay(state,true,.1f,0,sound,e));
 State absent;oldSound=sound;CHECK(!planPlay(absent,false,0,0,sound,e));CHECK(sound.sound()->sourceId==oldSound.sound()->sourceId&&sound.delayAfterSound()==oldSound.delayAfterSound());
 unsigned oldCalls=port.calls;oldWait=wait;CHECK(!planWait(absent,port,wait,e));CHECK(port.calls==oldCalls&&wait.tableAfterRead()==oldWait.tableAfterRead()&&wait.wait()==oldWait.wait());
 CHECK(!initialize(state,static_cast<Slot>(65535),e));CHECK(state.soundId()==13&&state.delay()==5&&state.tableIndex()==65535);CHECK(afterActualStartSound(state,sound,e)&&state.delay()==16); // failed init did not invalidate a valid plan
 CHECK(planWait(state,port,wait,e));CHECK(!afterActualReadPort(partner,wait,e)&&partner.tableIndex()==0);
 unsigned unchanged=state.delay();CHECK(planPlay(state,true,0,0,sound,e)&&!sound.sound()&&state.delay()==unchanged);
 std::cout<<"source Rappa engineering controls PASS checks="<<checks<<" backend=ABSENT gameplay=UNTESTED\n";
}
