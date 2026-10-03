#include "pc_p2_original_captain_rappa.h"
#include <cmath>
#include <limits>
namespace p2original {namespace captain {namespace rappa {namespace {
bool fail(std::string& e,const char* s){e=s;return false;}
bool initialized(const State& s,std::string& e){return s.slot().has_value()||fail(e,"missing actual PSM Navi Rappa initialization event");}
}
bool initialize(State& s,Slot slot,std::string& e){
 e.clear();if(slot!=Slot::Olimar&&slot!=Slot::Partner)return fail(e,"invalid actual source Navi roster slot");
 if(s.revision_==UINT64_MAX)return fail(e,"source Rappa plan revision exhausted");
 // Source init only replaces the ID and registration; it does not reset the
 // constructor's delay/table fields on later initialization of this object.
 s.id_=slot==Slot::Olimar?13u:14u;s.slot_=slot;++s.revision_;return true;
}
bool setId(State& s,std::uint32_t id,std::string& e){
 e.clear();if(!initialized(s,e))return false;if(s.revision_==UINT64_MAX)return fail(e,"source Rappa plan revision exhausted");s.id_=id;++s.revision_;return true;
}
bool planPlay(const State& s,bool enabled,float x,float y,PlayPlan& out,std::string& e){
 e.clear();if(!initialized(s,e))return false;if(s.id_==UINT32_MAX)return fail(e,"source Rappa sound ID remains invalid");
 if(!std::isfinite(x)||!std::isfinite(y))return fail(e,"nonfinite actual source C-stick Rappa input");
 PlayPlan next;next.owner_=&s;next.revision_=s.revision_;
 if(enabled){float ax=x>=0?x:-x,ay=y>=0?y:-y,magnitude=ax>ay?ax:ay;
  if(magnitude>=.1f){if(s.revision_==UINT64_MAX)return fail(e,"source Rappa plan revision exhausted");
   float value=magnitude-1;value*=-1;float compensated=value<0?0:value>1?1:value;
   int extra=static_cast<int>(compensated*15.f);
   next.sound_=SoundEvent{s.id_,0};next.delay_=static_cast<std::uint16_t>(extra+3);
  }
 }
 out=next;return true;
}
bool afterActualStartSound(State& s,const PlayPlan& p,std::string& e){
 e.clear();if(!initialized(s,e))return false;
 if(p.owner_!=&s||p.revision_!=s.revision_||!p.sound_||!p.delay_||p.sound_->sourceId!=s.id_||p.sound_->priority!=0||s.revision_==UINT64_MAX)return fail(e,"missing or stale actual source Rappa sound event");
 s.delay_=*p.delay_;++s.revision_;return true;
}
bool planWait(const State& s,const TrackPort& track,WaitPlan& out,std::string& e){
 e.clear();if(!initialized(s,e))return false;if(s.revision_==UINT64_MAX)return fail(e,"source Rappa plan revision exhausted");
 WaitPlan next;next.owner_=&s;next.revision_=s.revision_;
 if(!track.readPortAppDirect(0xb,next.table_,e))return false;
 if(s.revision_!=next.revision_)return fail(e,"actual track callback changed source Rappa state");
 next.wait_=s.delay_;out=next;return true;
}
bool afterActualReadPort(State& s,const WaitPlan& p,std::string& e){
 e.clear();if(!initialized(s,e))return false;
 if(p.owner_!=&s||p.revision_!=s.revision_||s.revision_==UINT64_MAX)return fail(e,"missing or stale actual source Rappa port event");
 s.table_=p.table_;++s.revision_;return true;
}
bool tableNumber(const State& s,std::uint16_t& out,std::string& e){e.clear();if(!initialized(s,e))return false;out=s.tableIndex();return true;}
}}}
