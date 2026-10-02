#pragma once
#include <cstdint>
// Test observation only: copied values, no game pointers or mutation API escape.
struct PcTransportObservation {
    uintptr_t actor=0, target=0; bool member=false, alive=false, pellet=false;
    unsigned generator=0; int state=-1; float x=0,y=0,z=0;
    bool valid() const { return actor && target && member && alive && pellet; }
};
struct PcWorkerRecallEvent {
    uintptr_t actor=0; int captain=-1, mode=-1, action=-1, state=-1, afterMode=-1, afterState=-1;
    PcTransportObservation target;
    bool held=false, recall=false, vs=false, alive=false, callable=false, buried=false, kinoko=false, fired=false, damaged=false, rope=false;
    bool instant=false; float heldSeconds=0,radius=0,distance=0,x=0,y=0,z=0,cursorX=0,cursorY=0,cursorZ=0;
    bool eligible() const {
        return actor && target.valid() && target.actor==actor && captain==0 && mode==9 && action==21 && state==0
            && held && recall && heldSeconds>=0.6f && radius>0 && distance>=0 && distance<radius
            && !vs && alive && callable && !buried && !kinoko && !fired && !damaged && !rope;
    }
    bool nativeResult() const { return instant ? afterMode==1 && afterState==0 : afterState==26; }
};
inline bool pc_worker_observer_enabled=false;
inline unsigned pc_worker_observer_count=0;
inline bool pc_worker_observer_overflow=false;
inline PcWorkerRecallEvent pc_worker_observer_events[64];
inline void pc_worker_observer_begin() { pc_worker_observer_count=0;pc_worker_observer_overflow=false;pc_worker_observer_enabled=true; }
inline void pc_worker_observer_end() { pc_worker_observer_enabled=false; }
inline void pc_worker_observer_record(const PcWorkerRecallEvent& event) {
    if(!pc_worker_observer_enabled || !event.actor)return;
    if(pc_worker_observer_count==64){pc_worker_observer_overflow=true;return;}
    pc_worker_observer_events[pc_worker_observer_count++]=event;
}
