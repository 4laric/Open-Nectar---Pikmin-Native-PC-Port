#pragma once
#include <cmath>
#include <limits>
#include <initializer_list>

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
// Temporary loss of contact may settle through ordinary neutral input only
// while the actual body remains immediately above a safe native floor.
inline bool pc_kochappy_air_contact_wait(bool finiteBody,bool dry,bool ground,
    float normalY,float floorY,float floorNormalY,float bodyY,float groundOffset,float radius) {
    if(!finiteBody||!dry||ground||!std::isfinite(normalY)||!std::isfinite(floorY)
       ||!std::isfinite(floorNormalY)||floorNormalY<=.5f||!std::isfinite(bodyY)
       ||!std::isfinite(groundOffset)||!std::isfinite(radius)||radius<=0)return false;
    const float height=bodyY-groundOffset-floorY;
    return std::isfinite(height)&&height>=0&&height<=radius;
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
 const double uncertainty=8.*std::numeric_limits<float>::epsilon()*coordinateScale;
 // Refuse materially uncertain geometry rather than capping its error and
 // certifying contact. Qualified map2208 yields .002106 versus radius8.5.
 return std::isfinite(uncertainty)&&uncertainty<radius/1024.?uncertainty:-1.;
}
inline bool pc_kochappy_current_wall_contact(double distance,double radius,double coordinateScale){
 const double tolerance=pc_kochappy_current_wall_tolerance(radius,coordinateScale);
 return std::isfinite(distance)&&distance>=0&&tolerance>=0&&distance+tolerance>=radius;
}

inline bool pc_kochappy_enemy_path_clear(double ax,double az,double bx,double bz,double ex,double ez,double sight,double width){
 if(!std::isfinite(ax)||!std::isfinite(az)||!std::isfinite(bx)||!std::isfinite(bz)||!std::isfinite(ex)||!std::isfinite(ez)
  ||!std::isfinite(sight)||sight<=0||!std::isfinite(width)||width<0)return false;
 const double dx=bx-ax,dz=bz-az,square=dx*dx+dz*dz;
 if(!std::isfinite(square))return false;
 const double raw=square>0?((ex-ax)*dx+(ez-az)*dz)/square:0;
 if(!std::isfinite(raw))return false;
 const double t=raw<0?0:raw>1?1:raw;
 const double gap=std::hypot(ax+t*dx-ex,az+t*dz-ez);
 return std::isfinite(gap)&&gap>sight+width;
}

// One finite ordinary swarm burst per visited guide; it changes only input.
struct PcKochappySwarmRecovery {
 int guide=-1,remaining=0;
 bool update(int currentGuide,int elapsed,bool roster,bool formed,float targetError,float speed,float centroidSpan) {
  if(currentGuide<0||currentGuide>=128||elapsed<1||elapsed>180||!roster||!formed
   ||!std::isfinite(targetError)||targetError<0||!std::isfinite(speed)||speed<0
   ||!std::isfinite(centroidSpan)||centroidSpan<0||centroidSpan>=512)return false;
  if(guide!=currentGuide&&elapsed==30&&targetError>=60.f&&speed<1.f&&centroidSpan>1.f){guide=currentGuide;remaining=6;}
  if(remaining>0){--remaining;return true;}
  return false;
 }
};

enum class PcKochappyGuideInput { Walk, Neutral, Refuse };


struct PcKochappyGuidePulse {
 int guide=-1,walk=0,neutral=0,elapsed=0,lastProgress=0,pulses=0;
 float best=-1;
 PcKochappyGuideInput observe(int currentGuide,float remaining){
  using I=PcKochappyGuideInput;
  if(currentGuide<0||currentGuide>=128||!std::isfinite(remaining)||remaining<0||remaining>=512)return I::Refuse;
  if(currentGuide!=guide){guide=currentGuide;walk=neutral=elapsed=lastProgress=pulses=0;best=-1;}
  if(remaining>=12)return I::Walk;
  ++elapsed;
  if(best<0||remaining<best-.1f){best=remaining;lastProgress=elapsed;}
  if(elapsed>180||elapsed-lastProgress>=90)return I::Refuse;
  if(walk>0){--walk;return I::Walk;}
  if(neutral>0){--neutral;return I::Neutral;}
  if(++pulses>64)return I::Refuse;
  walk=1;neutral=1;return I::Walk;
 }
};

// Ordinary near-guide input adapter. Model exact Navi angle bin/boost and
// strict per-axis SDL dead zone; this does not predict collision movement.
struct PcKochappyGuideAxes {bool valid=false;int x=0,y=0;float magnitude=0,bearingError=0;};
struct PcKochappyGuideMotion {
 float speed=0,dt=0,tau=0,nx=0,ny=1,nz=0,vx=0,vz=0,bx=0,bz=0,gravity=0,slipFactor=0;
 bool valid() const {
  for(float v:{speed,dt,tau,nx,ny,nz,vx,vz,bx,bz,gravity,slipFactor})if(!std::isfinite(v))return false;
  return speed>0&&dt>0&&dt<=1.f/30.f+.000001f&&tau>=dt&&ny>.5f
   &&std::fabs(std::sqrt(nx*nx+ny*ny+nz*nz)-1.f)<.001f&&gravity>=0&&slipFactor>=0;
 }
 bool error(float dx,float dz,float tx,float tz,float& out) const {
  const float dot=tx*nx+tz*nz,px=tx-dot*nx,py=-dot*ny,pz=tz-dot*nz;
  const float targetSpeed=std::hypot(tx,tz),len=std::sqrt(px*px+py*py+pz*pz);
  if(!std::isfinite(targetSpeed)||!std::isfinite(len)||len<=0)return false;
  float sx=nx*ny,sy=ny*ny-1.f,sz=nz*ny;const float sl=std::sqrt(sx*sx+sy*sy+sz*sz);
  const float slip=gravity*dt*slipFactor;if(!std::isfinite(slip))return false;
  sx=sl>0?sx/sl*slip:0;sz=sl>0?sz/sl*slip:0;
  const float nextX=vx+(px/len*targetSpeed+bx-vx)*dt/tau+sx;
  const float nextZ=vz+(pz/len*targetSpeed+bz-vz)*dt/tau+sz;
  out=std::hypot(dx-nextX*dt,dz-nextZ*dt);
  return std::isfinite(nextX)&&std::isfinite(nextZ)&&std::isfinite(out);
 }
};
inline PcKochappyGuideAxes pc_kochappy_analog_guide(float dx,float dz,float cameraX,float cameraZ,
    int deadZone,float binDegrees,float clamp,float neutral,float cursor,const PcKochappyGuideMotion* motion=nullptr) {
 PcKochappyGuideAxes out;
 if(motion&&!motion->valid())return out;
 if(!std::isfinite(dx)||!std::isfinite(dz)||!std::isfinite(cameraX)||!std::isfinite(cameraZ)
    ||std::fabs(std::hypot(cameraX,cameraZ)-1.f)>.001f||deadZone<0||deadZone>127
    ||!std::isfinite(binDegrees)||binDegrees<=0||binDegrees>180||!std::isfinite(clamp)||clamp<=0||clamp>1
    ||!std::isfinite(neutral)||neutral<0||!std::isfinite(cursor)||cursor<neutral||cursor>=clamp)return out;
 const float distance=std::hypot(dx,dz);if(!std::isfinite(distance)||distance<=.5f||distance>=12)return out;
 const float pi=3.14159265358979323846f,quarter=pi*.25f,width=pi/180.f*binDegrees;
 float bestError=std::numeric_limits<float>::infinity(),bestMagnitude=10;
 for(int x=-74;x<=74;++x)for(int y=-74;y<=74;++y){
  const float sx=std::abs(x)>deadZone?x/74.f:0.f,sz=std::abs(y)>deadZone?-y/74.f:0.f;
  float magnitude=std::hypot(sx,sz);if(magnitude==0)continue;
  float theta=std::atan2(sx,sz);if(theta<0)theta+=2*pi;
  const float angle=width*int((theta+width*.5f)/width),remainder=angle-int(angle/quarter)*quarter;
  const float length=std::sin(quarter)/(std::sin(remainder)+std::sin(quarter-remainder));
  magnitude*=1.f/length;if(magnitude>=clamp)magnitude=1;
  if(!std::isfinite(magnitude)||magnitude<=cursor||(!motion&&magnitude>=clamp))continue;
  const float lx=std::sin(angle),lz=std::cos(angle);
  const float wx=cameraX*lx-cameraZ*lz,wz=cameraZ*lx+cameraX*lz;
  float error=1.f-(wx*dx+wz*dz)/distance;
  if(motion&&!motion->error(dx,dz,wx*magnitude*motion->speed,wz*magnitude*motion->speed,error))continue;
  if(!std::isfinite(error)||error<-.00001f)continue;
  if(error<bestError-.000001f||(std::fabs(error-bestError)<=.000001f&&magnitude<bestMagnitude)){
   bestError=error;bestMagnitude=magnitude;out={true,x,y,magnitude,error};
  }
 }
 return out;
}

inline PcKochappyGuideAxes pc_kochappy_directed_prefix_edge(float dx,float dz,float cameraX,float cameraZ,int power,int deadZone){
 PcKochappyGuideAxes out;
 const float d=std::hypot(dx,dz);
 if(!std::isfinite(d)||d<=.5f||d>=512||!std::isfinite(cameraX)||!std::isfinite(cameraZ)
   ||std::fabs(std::hypot(cameraX,cameraZ)-1.f)>.001f||power<1||power>74||deadZone<0||deadZone>127)return out;
 const int x=int(std::lround(power*(dx*cameraX+dz*cameraZ)/d));
 const int y=int(std::lround(power*(dx*cameraZ-dz*cameraX)/d));
 const float effectiveX=std::abs(x)>deadZone?float(x):0.f,effectiveY=std::abs(y)>deadZone?float(y):0.f;
 const float magnitude=std::hypot(effectiveX,effectiveY)/74.f;
 if(!std::isfinite(magnitude)||magnitude<=.05f)return out;
 return {true,x,y,magnitude,0};
}
