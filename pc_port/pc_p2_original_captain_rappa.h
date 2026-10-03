#pragma once
#include <cstdint>
#include <optional>
#include <string>
namespace p2original {namespace captain {namespace rappa {
enum class Slot:std::uint16_t {Olimar=0,Partner=1};
class PlayPlan;class WaitPlan;
// Literal PSGame::Rappa fields. The concrete PSM/Navi lifetime owner calls
// initialize only at actual PSM::Navi::init(actual mNaviIndex), registers this
// object in the two source Rappa slots, and removes that binding on destruction.
// This policy does not register actors, grant scene readiness or play audio.
class State {
public:
 std::uint32_t soundId()const{return id_;}
 std::uint16_t delay()const{return delay_;}
 std::uint16_t tableIndex()const{return table_;}
 std::optional<Slot> slot()const{return slot_;}
private:
 std::uint32_t id_=UINT32_MAX;
 std::uint16_t delay_=0,table_=0;
 std::optional<Slot> slot_;
 std::uint64_t revision_=0; // guards stale plans, not a retail frame timer
 friend bool initialize(State&,Slot,std::string&);
 friend bool setId(State&,std::uint32_t,std::string&);
 friend bool planPlay(const State&,bool,float,float,PlayPlan&,std::string&);
 friend bool afterActualStartSound(State&,const PlayPlan&,std::string&);
 friend bool planWait(const State&,const class TrackPort&,WaitPlan&,std::string&);
 friend bool afterActualReadPort(State&,const WaitPlan&,std::string&);
};
struct SoundEvent {std::uint32_t sourceId=0,priority=0;};
class PlayPlan {
public:
 const std::optional<SoundEvent>& sound()const{return sound_;}
 std::optional<std::uint16_t> delayAfterSound()const{return delay_;}
private:
 const State* owner_=nullptr;std::uint64_t revision_=0;
 std::optional<SoundEvent> sound_;std::optional<std::uint16_t> delay_;
 friend bool planPlay(const State&,bool,float,float,PlayPlan&,std::string&);
 friend bool afterActualStartSound(State&,const PlayPlan&,std::string&);
};
bool initialize(State&,Slot actualNaviSlot,std::string&);
// Literal source setId event, including PSM::Navi::setShacho source ID 137.
// Initialization must already have occurred through the actual source owner.
bool setId(State&,std::uint32_t sourceId,std::string&);
// No sound backend is needed for a valid no-emission plan. Inputs come from
// source mCStickPosition.x/z after Navi::control's movie/story-active guards.
bool planPlay(const State&,bool enabled,float x,float y,PlayPlan&,std::string&);
// Root executes actual startSound(event.sourceId,0), then rechecks canonical
// scene/actor/PSM object ownership before calling this. A real null JAISound
// result still updates delay; an absent backend is not a completed sound call.
bool afterActualStartSound(State&,const PlayPlan&,std::string&);
class TrackPort {
public:
 virtual ~TrackPort()=default;
 // Actual JASTrack::readPortAppDirect, not a scripted/cached table value.
 virtual bool readPortAppDirect(std::uint32_t,std::uint16_t&,std::string&)const=0;
};
class WaitPlan {
public:
 std::uint16_t wait()const{return wait_;}
 std::uint16_t tableAfterRead()const{return table_;}
private:
 const State* owner_=nullptr;std::uint64_t revision_=0;
 std::uint16_t wait_=0,table_=0;
 friend bool planWait(const State&,const TrackPort&,WaitPlan&,std::string&);
 friend bool afterActualReadPort(State&,const WaitPlan&,std::string&);
};
// seqCpuSync commands 0x3001/0x3003 perform the actual port 0xb read. Caller
// guards the real registered Rappa/track before and after that callback, then
// publishes the table and returns wait. No frame decrement exists in retail.
bool planWait(const State&,const TrackPort&,WaitPlan&,std::string&);
bool afterActualReadPort(State&,const WaitPlan&,std::string&);
// Commands 0x3000/0x3002 read the existing table without a new port callback.
bool tableNumber(const State&,std::uint16_t&,std::string&);
}}}
