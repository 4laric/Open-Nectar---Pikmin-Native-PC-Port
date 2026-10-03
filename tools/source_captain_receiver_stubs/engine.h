#pragma once
#include <map>
#include <type_traits>
#include <cmath>
#define immut const
constexpr float PI=3.14159265358979323846f;
inline float roundAng(float x){return x;}
struct Vector3f {float x=0,y=0,z=0;Vector3f()=default;Vector3f(float a,float b,float c):x(a),y(b),z(c){}void set(float a,float b,float c){x=a;y=b;z=c;}};
struct FakeSystem {int draws=0;float random=.5f;float dt=.1f;float getRand(float scale){++draws;return random*scale;}float getFrameTime(){return dt;}};
extern FakeSystem* gsys;
class Creature {public: virtual ~Creature()=default;};
class BTeki:public Creature{};
class Piki;class Navi;class Interaction;
constexpr int PIKISTATE_Normal=0,PIKISTATE_Dying=6,PIKISTATE_Dead=7,PIKISTATE_Swallowed=8,PIKISTATE_Flick=22,PIKISTATE_Pressed=33,PIKISTATE_Panic=36,PIKISTATE_HanachirashiBlow=37;
constexpr int NAVISTATE_Walk=0,NAVISTATE_Dead=29,NAVISTATE_HanachirashiFlick=38;
constexpr int Leaf=0,Bud=1,Flower=2,KEY_Finished=0;
constexpr int PIKIANIM_JHit=0,PIKIANIM_JKoke=1,PIKIANIM_GetUp=2;
struct MsgBounce{};struct KeyEvent{int mEventType=KEY_Finished;};struct MsgAnim{KeyEvent* mKeyEvent;};
struct PaniMotionInfo{int motion;PaniMotionInfo(int i,void* =nullptr):motion(i){}};
struct Action {int resumed=0,restarted=0;void resume(){++resumed;}bool resumable(){return true;}void restart(){++restarted;}};
struct Gauge {float value=0;int updates=0;void updValue(float h,float){value=h;++updates;}};
namespace PikiMode {constexpr int FreeMode=0,FormationMode=1;}
template<class T>class AState {public:int id;explicit AState(int i):id(i){}virtual ~AState()=default;int getID(){return id;}virtual void init(T*){}virtual void exec(T*){}virtual void cleanup(T*){}virtual void procBounceMsg(T*,MsgBounce*){}virtual void procAnimMsg(T*,MsgAnim*){}void transit(T*,int);};
class PikiState:public AState<Piki>{public:PikiState(int i,const char* =nullptr):AState(i){}};
class NaviState:public AState<Navi>{public:NaviState(int i):AState(i){}virtual bool invincible(Navi*){return false;}};
template<class T>struct Machine{std::map<int,AState<T>*> states;void transit(T* target,int id){if(target->current)target->current->cleanup(target);target->current=states.at(id);target->current->init(target);}};
class Piki:public Creature {public:AState<Piki>* current=nullptr;Machine<Piki>* mFSM=nullptr;Action action;Action* mActiveAction=&action;Navi* mNavi=nullptr;Vector3f mVelocity,mTargetVelocity;float mHealth=10,mFaceDirection=0;int mHappa=Flower,mode=-1,motion=-1;bool mIsBeingDamaged=false,mIsWhistlePending=false,mP2Purple=false,alive=true,mouth=false;Creature* sticker=nullptr;
 bool isAlive(){return alive&&mHealth>0;}bool isStickToMouth(){return mouth;}int getState(){return current?current->id:0;}AState<Piki>* getCurrState(){return current;}void endStickObject(){sticker=nullptr;mouth=false;}Creature* getStickObject(){return sticker;}void setFlower(int h){mHappa=h;}void startMotion(PaniMotionInfo a,PaniMotionInfo){motion=a.motion;}void changeMode(int m,Navi*){mode=m;}bool stimulate(const Interaction&);
};
class Navi:public Creature {public:AState<Navi>* current=nullptr;Machine<Navi>* mStateMachine=nullptr;Vector3f mVelocity,mTargetVelocity;float mHealth=100,mFaceDirection=0;Gauge mLifeGauge;int motion=-1;bool isAlive(){return mHealth>0;}AState<Navi>* getCurrState(){return current;}void startMotion(PaniMotionInfo a,PaniMotionInfo){motion=a.motion;}bool stimulate(const Interaction&);};
template<class T>void AState<T>::transit(T* t,int i){if constexpr(std::is_same<T,Piki>::value)t->mFSM->transit(t,i);else t->mStateMachine->transit(t,i);}
class Interaction {public:Creature* mOwner;Interaction(Creature* p):mOwner(p){}virtual ~Interaction()=default;virtual bool actPiki(Piki*)const{return true;}virtual bool actNavi(Navi*)const{return true;}};
inline bool Piki::stimulate(const Interaction& i){return i.actPiki(this);}inline bool Navi::stimulate(const Interaction& i){return i.actNavi(this);}
#define FLICK_BACKWARDS_ANGLE (-1000.f)
#define C_NAVI_PARM(n,p) (100.f)

using NaviStateMachine=Machine<Navi>;
