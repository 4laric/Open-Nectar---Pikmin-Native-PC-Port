#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
namespace p2catfishsource {
constexpr float Pi=3.14159265358979323846f;
struct Vec {float x=0,y=0,z=0;};
inline float rad(float deg){return deg*Pi/180.0f;}
inline float angle(float a){while(a>Pi)a-=2*Pi;while(a<-Pi)a+=2*Pi;return a;}
inline float separation(Vec a,Vec b){const float x=a.x-b.x,y=a.y-b.y,z=a.z-b.z;return x*x+y*y+z*z;}
inline float flatSeparation(Vec a,Vec b){const float x=a.x-b.x,z=a.z-b.z;return x*x+z*z;}
inline float facing(Vec a,Vec b,float face){return angle(std::atan2(b.x-a.x,b.z-a.z)-face);}
// Literal retail catfish/enemyparm.txt, member SHA c251933cb91034fca63e90dbcc3f75819bb8ee9601b5c7918d5ff772deb69182.
struct Params {
 float health=200,speed=60,turn=.1f,maxTurn=10,territory=280,home=80,privateRadius=70,sight=200;
 float view=180,search=200,searchAngle=180,fov=50,searchHeight=50,attack=50,attackAngle=25;
 float hitRadius=50,hitAngle=25,damage=10,alert=15,alertHealth=30,rotationEnd=90;
 float knockback=80,flickDamage=1,flickRange=25,poison=300;
};
inline float turnStep(float difference){return std::clamp(difference*.1f,-rad(10),rad(10));}
inline bool attackable(Vec actor,Vec prey,float difference){return separation(actor,prey)<50*50&&std::fabs(difference)<=rad(25);}
// Preserve the research helper's exact boolean expression, including its height clause.
inline bool outOfRange(Vec actor,Vec prey,float difference,float view=180){
 const float d=flatSeparation(actor,prey);
 return (d>70*70&&(d>200*200&&std::fabs(prey.y-actor.y)<50))||std::fabs(difference)>rad(view);
}
inline bool startFlick(float acceptedDamage,int stuck){
 const int rounded=int(acceptedDamage+(acceptedDamage>=0?.5f:-.5f));
 const unsigned count=static_cast<std::uint8_t>(rounded);
 const int threshold=stuck<1?1:stuck<2?1:stuck<3?2:2;
 return count>unsigned(threshold);
}
enum State {Wait=0,Dead=1,Turn=2,Walk=3,Attack=4,Flick=5,TurnToHome=6,GoHome=7};
enum Anim {AttackAnim=0,DeadAnim=1,FlickAnim=2,MoveAnim=3,PressAnim=4,CarryAnim=5,WaitAnim=6,TurnAnim=7,EatAnim=8};
struct Key {int frame=0,type=0;};
struct Registration {const char* clip;int duration;std::vector<Key> keys;};
inline const Registration& registration(int id){
 // Indexed enemyanimmgr registrations: three wait1 rows are deliberately distinct.
 static const Registration rows[]={{"attack",85,{{17,2},{75,3}}},{"dead",95,{}},{"flick",70,{{25,2},{47,3}}},
 {"move1",25,{{0,0},{24,1}}},{"wait1",30,{}},{"type5",40,{{10,0},{29,1}}},
 {"wait1",30,{{0,0},{29,1}}},{"wait1",30,{{0,0},{29,1}}},{"waitact2",16,{}}};
 return rows[id];
}
// SysShape::Animator::animate: keys trigger only when key.frame < int(timer).
// An unfinished loop resets to its start and discards overshoot. END fires once
// at BCA duration and clamps visible pose to duration-1.
class Motion {
public:
 void start(int id){mId=id;mFrame=0;mCursor=0;mFinish=mCompleted=false;}
 void finish(){mFinish=true;}
 int id()const{return mId;}
 const char* clip()const{return registration(mId).clip;}
 float frame()const{return std::min(mFrame,float(registration(mId).duration-1));}
 bool finishing()const{return mFinish;}
 std::vector<Key> advance(float delta){
  std::vector<Key> emitted;if(!std::isfinite(delta)||delta<=0||delta>1024||mCompleted)return emitted;
  mFrame+=delta;const auto& row=registration(mId);
  while(mCursor<row.keys.size()&&row.keys[mCursor].frame<int(mFrame)){
   Key k=row.keys[mCursor++];emitted.push_back(k);
   if(k.type==1&&!mFinish){int start=0;for(size_t n=0;n<mCursor;++n)if(row.keys[n].type==0)start=row.keys[n].frame;
    mFrame=float(start);mCursor=0;return emitted;}
  }
  if(mFrame>=row.duration){mFrame=float(row.duration-1);mCompleted=true;emitted.push_back({row.duration,1000});}
  return emitted;
 }
private:int mId=WaitAnim;float mFrame=0;size_t mCursor=0;bool mFinish=false,mCompleted=false;
};
class CorpseMotion {
public:
 void prepare(){mMotion.start(CarryAnim);mRunning=false;}
 void start(bool restart=false){if(restart)mMotion.start(CarryAnim);mRunning=true;}
 void stop(){mRunning=false;}
 void finish(){mMotion.finish();}
 void advance(float delta){if(mRunning)mMotion.advance(delta);}
 const Motion& motion()const{return mMotion;}
private:Motion mMotion;bool mRunning=false;
};
// Catfish setEnemyNonStone/resetEnemyNonStone: reset requests the down
// effect only when the flag was set. Bitter queue ownership is separate.
class NonStoneGate {
public:
 void set(){mNoInterrupt=true;}
 void reset(){if(mNoInterrupt){mNoInterrupt=false;++mClearSerial;}}
 bool noInterrupt()const{return mNoInterrupt;}
 std::uint64_t clearSerial()const{return mClearSerial;}
private:bool mNoInterrupt=false;std::uint64_t mClearSerial=0;
};
inline State attackEnd(bool target,bool inAttackRange){return target?(inAttackRange?Attack:Turn):TurnToHome;}
inline State flickReturn(State previous,int requested=-1){return requested>=0?State(requested):previous;}
} // namespace p2catfishsource
