#pragma once
#include <cmath>
#include <limits>

// Input planning only. Recruitment remains the native strict XZ radius test.
enum class PcKochappyGatherInput { Refuse, Cursor, Walk };
inline PcKochappyGatherInput pc_kochappy_gather_input(float captainDistance,
    float cursorRadius, float whistleRadius, float neutral, float moveThreshold) {
    if (!std::isfinite(captainDistance) || !std::isfinite(cursorRadius)
        || !std::isfinite(whistleRadius) || !std::isfinite(neutral)
        || !std::isfinite(moveThreshold) || captainDistance < 0
        || cursorRadius <= 20 || whistleRadius <= 0 || neutral < 0
        || !(neutral < 22.f/74.f && 22.f/74.f <= moveThreshold
             && moveThreshold < 65.f/74.f)) return PcKochappyGatherInput::Refuse;
    // Reserve half the loaded whistle radius so cursor aiming has room to
    // recruit after the native expansion, rather than saturating out of reach.
    return captainDistance > cursorRadius + whistleRadius*.5f
        ? PcKochappyGatherInput::Walk : PcKochappyGatherInput::Cursor;
}

// Only previously visited guides may be replayed. Missing or malformed
// observations refuse rather than selecting a future shortcut.
inline int pc_kochappy_route_reentry(const double* spans, int count, int next) {
    if (!spans || count < 1 || count > 128 || next < 1 || next > count) return -1;
    int chosen=-1;double best=512.;
    for(int i=0;i<next;++i) {
        if(!std::isfinite(spans[i]) || spans[i]<0) return -1;
        if(spans[i]<best){best=spans[i];chosen=i;}
    }
    return chosen;
}
struct PcKochappyReentryProgress {
    int count=0;
    int retainedNext=-1;
    bool mayBegin(int next) const {
        return next>0 && count<4 && (retainedNext<0 || next>retainedNext);
    }
    bool begin(int next, int chosen) {
        if(!mayBegin(next) || chosen<0 || chosen>=next) return false;
        ++count;retainedNext=next;return true;
    }
};

enum class PcKochappyCatchupInput { Refuse, Hold, Continue };
// Fixture input pacing only; clocks, roster and positions remain native-owned.
struct PcKochappyRouteCatchup {
    bool active=false;
    int guide=-1, elapsed=0, stable=0, lastProgress=0;
    float best=-1;
    bool begin(int visitedGuide) {
        if(active || visitedGuide<0 || visitedGuide>=128)return false;
        active=true;guide=visitedGuide;elapsed=stable=lastProgress=0;best=-1;
        return true;
    }
    PcKochappyCatchupInput observe(bool originalRoster, float lag, float limit, float captainSpeed, bool settled=true) {
        using I=PcKochappyCatchupInput;
        if(!active || !originalRoster || !std::isfinite(lag) || lag<0
            || !std::isfinite(limit) || limit<=0 || limit>=512
            || !std::isfinite(captainSpeed) || captainSpeed<0)return I::Refuse;
        ++elapsed;
        if(best<0 || lag<best-1.f){best=lag;lastProgress=elapsed;}
        // Ordinary neutral input does not freeze native collision/slip motion.
        // Read the roster against the current pose on every observation.
        if(lag<=limit && settled)++stable;else stable=0;
        if(stable>=3){active=false;return I::Continue;}
        if(elapsed>=180 || elapsed-lastProgress>=90)return I::Refuse;
        return I::Hold;
    }
};

// Read-only observations of the actual modern ActCrowd/CPlate party action.
// Unk0, trips and native routes may settle; Sort and foreign slots refuse.
struct PcKochappyCrowdObservation {
    bool actionOwner=false,plateOwner=false,slotsAvailable=false;
    bool occupantOwner=false,listenerOwner=false,finiteGeometry=false;
    bool neutral=false,tripping=false,route=false;
    int state=-1,slot=-1,used=-1,capacity=-1;
    bool valid() const {
        return actionOwner && plateOwner && slotsAvailable && occupantOwner && listenerOwner
            && finiteGeometry && (state==0 || state==1)
            && capacity>0 && used>0 && used<=capacity && slot>=0 && slot<used;
    }
    bool settled() const { return valid() && state==1 && neutral && !tripping && !route; }
};

// One ordinary right-stick edge can unlock native plate direction; zero input
// alone need not do so. Readiness still requires subsequent native observations.
inline int pc_kochappy_neutral_edge(bool sent,bool locked,bool neutral,bool safeFormed,
    int band,float rightLength,float previousLength,float targetSpeed,int deadZone) {
    if(band<0 || band>2 || !std::isfinite(rightLength) || rightLength<0
        || !std::isfinite(previousLength) || previousLength<0
        || !std::isfinite(targetSpeed) || targetSpeed<0 || deadZone<0 || deadZone>127)return -1;
    if(sent || !locked || neutral || !safeFormed || rightLength>.05f
        || previousLength>.05f || targetSpeed>=50.f)return 0;
    const int axis=deadZone>=22?deadZone+1:22;
    return axis<=74?axis:-1;
}

// Ordinary west-ramp guidance before the unchanged receiver corridor. A guide
// observation is not a prediction of future native follower trajectories.
enum class PcKochappyPrefixInput { Walk, Reached, Done, Refuse };
enum class PcKochappyPrefixContact { Admit, Wait, Refuse };
inline PcKochappyPrefixContact pc_kochappy_prefix_contact(bool finiteBody,bool dry,bool ground,float normalY) {
    using I=PcKochappyPrefixContact;
    if(!finiteBody || !dry || (ground&&!std::isfinite(normalY)))return I::Refuse;
    if(!ground || normalY<=.5f)return I::Wait;
    return I::Admit;
}
struct PcKochappyPrefixContactGate {
    int waits=0;
    PcKochappyPrefixContact observe(bool allContact) {
        if(waits>90)return PcKochappyPrefixContact::Refuse;
        if(allContact)return PcKochappyPrefixContact::Admit;
        return ++waits<=90?PcKochappyPrefixContact::Wait:PcKochappyPrefixContact::Refuse;
    }
};
struct PcKochappyPrefixProgress {
    int guide=0,elapsed=0,lastProgress=0;
    float best=-1.f;
    PcKochappyPrefixInput observe(float distance,bool originalRoster,int count) {
        using I=PcKochappyPrefixInput;
        if(!originalRoster || count<=0 || guide<0 || guide>count
            || !std::isfinite(distance) || distance<0.f)return I::Refuse;
        if(guide==count)return I::Done;
        ++elapsed;
        if(best<0.f || distance<best-1.f){best=distance;lastProgress=elapsed;}
        if(distance<=.5f){++guide;elapsed=lastProgress=0;best=-1.f;return guide==count?I::Done:I::Reached;}
        if(elapsed>=180 || elapsed-lastProgress>=90)return I::Refuse;
        return I::Walk;
    }
};

// Prefix input setup cannot wait indefinitely for the real plate to unlock.
class PcKochappyPrefixNeutralGate {
public:
 int observations=0;
 int observe(bool ready){if(observations>=90)return -1;++observations;return ready?1:0;}
};

// Coordinates/radii originate as native float. Eight relative float ulps
// cover the two quantized endpoints and sphere-centre additions; distance
// evaluation itself uses double. This is only a CURRENT contact tolerance,
// never prospective route/slot padding.
inline double pc_kochappy_current_wall_tolerance(double radius,double coordinateScale){
 if(!std::isfinite(radius)||radius<=0||!std::isfinite(coordinateScale)||coordinateScale<1||coordinateScale<radius)return -1;
 return 8.*std::numeric_limits<float>::epsilon()*coordinateScale;
}
inline bool pc_kochappy_current_wall_contact(double distance,double radius,double coordinateScale){
 const double tolerance=pc_kochappy_current_wall_tolerance(radius,coordinateScale);
 return std::isfinite(distance)&&distance>=0&&tolerance>=0&&distance+tolerance>=radius;
}
