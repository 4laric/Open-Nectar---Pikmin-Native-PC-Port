#pragma once
#include "pc_p2_source_body.h"
#include "Vector.h"
#include <cstdint>
#include <string>
#include <vector>
class Navi;
class Creature;
class CollEvent;
namespace p2original { namespace captain { class LoadedScene; } }
namespace p2original { namespace piki {
enum class State { Walk, GoHang, Hanged, Flying, LookAt };
enum class Motion { Wait, Walk, Run2, Hang, RollJump, Notice };
enum class Action { Free, Formation, None };
enum class FreeSearch { Execute, Probe };
struct Handle { Piki* body=nullptr; std::uint64_t lifetime=0; };
struct Frame {
 Handle handle;Vector3f position;State state=State::Walk;
 unsigned species=0,happa=0;Navi* captain=nullptr;int formationSlot=-1;
 bool throwable=false,releasable=false;
};
struct Parameters {
 float walk=0,run=0,budRun=0,flowerRun=0,whiteMultiplier=0,purpleMultiplier=0;
 float flowerGravity=0,whiteDistance=0,grayDistance=0,lostTime=0,acceleration=0;
 float gravity=0,grabRange=0,landingTime=0,heightMin=0,heightMax=0,heightYellow=0,heightWhite=0,heightPurple=0;
 // p003 is reference HP (unused in the retail Piki runtime); do not assign it
 // to native health. P004 is used by retail Piki::getAttackDamage for White.
 float referenceHealth=0,whiteAttackDamage=0;
};
// Values are parsed from the selected source bytes, never retail defaults.
bool parseParameters(const std::string& pikiBytes,const std::string& naviBytes,
                     float sourceGravity,Parameters&,std::string&);
struct CaptainFrame {
 Vector3f position,velocity,rhnd,plateOffset;
 float face=0,sceneAnimationTimer=0;
 bool throwWait=false,throwing=false,controller=false,formationable=false,alive=false;
 bool command=false,cstickNeutral=false,carryingPellet=false,follow=false;
};
// Source startup supplies actual native animation/effect and CPlate consumers.
// There are no default implementations and no "ready"/permission parameters.
// install alone does not initialize actors or activate the original course.
class Services {
public:
 virtual ~Services()=default;
 virtual const captain::LoadedScene& scene()const=0;
 virtual const std::string& pikiParameterBytes()const=0;
 virtual const std::string& naviParameterBytes()const=0;
 virtual bool gravity(float&,std::string&)const=0;
 virtual bool captainFrame(const Navi*,CaptainFrame&,std::string&)const=0;
 virtual bool supports(Handle,Motion,std::string&)const=0;
 virtual bool motion(Handle,Motion,std::string&)=0;
 virtual bool animate(Handle,float delta,std::string&)=0;
 virtual bool currentMotion(Handle,Motion&,std::string&)const=0;
 virtual bool freeEffects(Handle,bool,std::string&)=0;
 virtual bool throwEffects(Handle,bool,std::string&)=0;
 virtual bool hangSound(Handle,std::string&)=0;
 virtual bool landSound(Handle,std::string&)=0;
 virtual bool calledSound(Handle,std::string&)=0;
 virtual bool nudgeRumble(Handle,Navi*,std::string&)=0;
 virtual bool allocateSlot(Handle,Navi*,int&,std::string&)=0;
 virtual bool releaseSlot(Handle,Navi*,int,std::string&)=0;
 virtual bool slotPosition(Handle,Navi*,int,Vector3f&,std::string&)const=0;
 virtual bool formed(Handle,Navi*,std::string&)=0;
 virtual bool sortSlot(Handle,Navi*,int,unsigned happa,std::string&)=0;
 // Actual source invokeAIFree/Bore/Gather remain distinct unfinished actions.
 // An absent action is an error, not "no nearby task" or a P1 delegate.
 virtual bool invokeFree(Handle,FreeSearch,bool& available,std::string&)=0;
 virtual bool startBore(Handle,std::string&)=0;
 virtual bool execBore(Handle,int& result,std::string&)=0;
 virtual bool finishBore(Handle,std::string&)=0;
 virtual bool random(float&,std::string&)=0;
};
struct BrainState {
 Action action=Action::Free;
 Navi* navi=nullptr; int slot=-1;
 std::uint16_t delayTimer=0;
 std::uint8_t freeState=0,touchCooldown=0,sortState=0,distanceCounter=0;
 std::uint16_t distanceType=5,oldDistanceType=5;
 float lostTimer=0,tripDistance=0;
 bool releasedSlot=false;
};
struct RuntimeState {
 State state=State::Walk;
 BrainState brain;
 std::uint16_t flyingFrames=0;
 bool flowerFalling=false,collisionFlick=true,atari=true,moveVelocity=true,forceActive=false;
 Vector3f velocityDirection;
 float directionalSpeed=0,halfDirectionalSpeed=0,slowFallTimer=0;
 float lookWaitTime=0;std::uint8_t lookSubState=0;
};
bool installServices(Services&)noexcept;
bool initialize(Piki*,std::string&);
bool handle(const Piki*,Handle&);
bool snapshot(Handle,RuntimeState&);
bool frame(Handle,Frame&,std::string&);
bool squad(Navi*,std::vector<Frame>&,std::string&);
bool sortFormation(Handle,unsigned happa,std::string&);
bool transition(Handle,State,std::string&);
bool update(Handle,float delta,std::string&);
bool animate(Handle,float delta,std::string&);
bool animationKey(Handle,unsigned keyType,std::string&);
// Called instead of native P1 Creature::moveVelocity and its gravity update.
// The existing native sphere/triangle trace remains the physical collision host.
bool moveVelocity(Handle,float delta,std::string&);
bool applyGravity(Handle,float delta,std::string&);
// Called from real native floor bounce, not a scripted landing assertion.
bool bounce(Handle,std::string&);
bool whistle(Handle,Navi*,std::string&);
bool launch(Handle,Navi*,const Vector3f& cursor,std::string&);
bool position(Handle,const Vector3f&,std::string&);
bool ignoreAtari(Handle,const Creature*,bool&);
bool collision(Handle,const CollEvent&,std::string&);
void forget(Piki*)noexcept;
void sceneExit()noexcept;
} }
