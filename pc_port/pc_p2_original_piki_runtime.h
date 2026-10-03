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
enum class Motion { Wait, Walk, Run2, Hang, RollJump, Notice, Step, Escape, Yawn, Chat, Search, Irritated, Sit, Sleep };
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
// Services MUST be a process-lifetime stable singleton; no uninstall exists.
// install alone does not initialize actors or activate the original course.
// All operations run on the native game thread. Source mutation/retirement is
// nonreentrant across service callbacks. Read-only queries and animationKey
// notification may nest; a nested mutation refuses and invalidates the outer
// operation's success. Services must propagate such refusal to their callers.
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
 virtual bool canRemoveFreeEffects(Handle,std::string&)const=0;
 virtual bool freeEffects(Handle,bool,std::string&)=0;
 virtual bool canRemoveThrowEffects(Handle,std::string&)const=0;
 virtual bool throwEffects(Handle,bool,std::string&)=0;
 virtual bool hangSound(Handle,std::string&)=0;
 virtual bool landSound(Handle,std::string&)=0;
 virtual bool calledSound(Handle,std::string&)=0;
 virtual bool nudgeRumble(Handle,Navi*,std::string&)=0;
 virtual bool allocateSlot(Handle,Navi*,int&,std::string&)=0;
 virtual bool canReleaseSlot(Handle,Navi*,int,std::string&)const=0;
 virtual bool releaseSlot(Handle,Navi*,int,std::string&)=0;
 virtual bool slotPosition(Handle,Navi*,int,Vector3f&,std::string&)const=0;
 virtual bool formed(Handle,Navi*,std::string&)=0;
 virtual bool sortSlot(Handle,Navi*,int,int happa,std::string&)=0;
 // Actual source entity census/action admission. Absence is an error, never
 // fabricated "no nearby task"; source actor producers own this query.
 // This query is read-only: Execute applies the real update-context gate and
 // checks a target; Probe tests action!=ACT_NULL. Brain refuses unsupported
 // task actions before any foreign Brain/body mutation.
 virtual bool freeTaskAvailable(Handle,FreeSearch,bool& available,std::string&)const=0;
 // Actual source animator primitives; Brain owns Bore/Gather actions.
 virtual bool animationStatus(Handle,Motion&,float& speed,bool& completed,std::string&)const=0;
 virtual bool animationSpeed(Handle,float,std::string&)=0;
 virtual bool finishMotion(Handle,std::string&)=0;
 virtual bool loopStart(Handle,std::string&)=0;
 // Producer applies actual PSM scene akubiOK and sound-object voice routing.
 virtual bool boreVoice(Handle,bool sleep,std::string&)=0;
 virtual bool random(float&,std::string&)=0;
};
struct BoreState {
 std::uint8_t behavior=1,restState=0;
 Motion oneshot=Motion::Yawn;
 float oneshotTimer=0,forceTimer=0,restTimer=0;
 bool finished=false,forced=false,animFinished=false,idle=false,interruptible=false;
};
struct BrainState {
 Action action=Action::Free;
 Navi* navi=nullptr; int slot=-1;
 std::uint16_t delayTimer=0;
 std::uint8_t freeState=0,touchCooldown=0,sortState=0,distanceCounter=0;
 std::uint16_t distanceType=5,oldDistanceType=5;
 float lostTimer=0,tripDistance=0;
 bool releasedSlot=false;
 // Retained attempted effect creation remains cleanup-owned even on refusal.
 bool freeEffectsOwned=false;
 Vector3f gatherGoal;float gatherRadius=0,gatherTimer=0;
 BoreState bore;
 // A newly acquired slot remains owned through a failed old-action cleanup.
 Navi* pendingNavi=nullptr;int pendingSlot=-1;
};
struct RuntimeState {
 State state=State::Walk;
 BrainState brain;
 std::uint16_t flyingFrames=0;
 bool throwEffectsOwned=false;
 bool flowerFalling=false,collisionFlick=true,atari=true,moveVelocity=true,forceActive=false;
 Vector3f velocityDirection;
 float directionalSpeed=0,halfDirectionalSpeed=0,slowFallTimer=0;
 float lookWaitTime=0;std::uint8_t lookSubState=0;
};
bool installServices(Services&)noexcept;
// Canonical owned Loading or Active permits birth initialization only; all
// action/frame/physics queries remain GameWorldActive gated. Startup binds
// selected source resources first, initializes bodies, then grants activation
// through the actual typed Captain bootstrap owner. No ready bool is accepted.
bool initialize(Piki*,std::string&);
struct SlotChange {Handle handle;Navi* captain=nullptr;int oldSlot=-1,newSlot=-1;};
// Genuine CPlate compaction/sort delivers an entire source-listener batch.
// Leaf preflights its no-fail mapping commit before notifying. This validates
// every exact lifetime/owned old slot before writes, works during inactive
// cleanup, and may nest in Services callbacks without erasing runtime owners.
bool slotsChanged(const std::vector<SlotChange>&,std::string&);
bool handle(const Piki*,Handle&);
bool snapshot(Handle,RuntimeState&);
bool frame(Handle,Frame&,std::string&);
bool squad(Navi*,std::vector<Frame>&,std::string&);
// Retail -1 prioritizes the selected body maturity; 0..2 rotates explicit priority.
bool sortFormation(Handle,int happa,std::string&);
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
// Actual ActFreeArg/ActGather dismissal after genuine PartySource selection.
bool gather(Handle,const Vector3f& goal,float radius,std::string&);
bool launch(Handle,Navi*,const Vector3f& cursor,std::string&);
bool position(Handle,const Vector3f&,std::string&);
bool ignoreAtari(Handle,const Creature*,bool&);
bool collision(Handle,const CollEvent&,std::string&);
void forget(Piki*)noexcept;
void sceneExit()noexcept;
// Teardown must use these checked forms before origin retirement or heap reuse.
// They verify the exact native lifetime independently of world action phase.
// Failed resource cleanup retains ownership and refuses retirement.
// Ordered barrier: nativeControl/Throw held-handle forget first; retireScene
// while origin/canonical scene, Navi/CPlate and selected effects remain live;
// observe zero runtime plus genuine provider owners; then retire plates and
// Shape/Bank resources; only then origin sceneExit/native pool or heap reuse.
bool retire(Piki*,std::string&);
bool retireScene(std::string&);
struct Ownership {
 std::uint64_t entries=0,committed=0,pendingInitializations=0,inFlightOwnerOperations=0;
 std::uint64_t formationSlots=0,pendingSlots=0,freeEffectOwners=0,throwEffectOwners=0;
};
// Observes actual retained runtime owners; no source readiness/empty fallback.
// Stage must also observe genuine plate/effect-provider owners independently.
bool readOwnership(Ownership&,std::string&);
// Nonmutating reference preflight; actual resource owners inspect retained
// handles, not world flags. No effect/slot cleanup is performed here.
// Authored key/slot notifications are refused during this readonly inspection;
// ordinary gameplay animation/listener callback delivery remains supported.
bool canRetireScene(std::string&);
bool owned()noexcept;
bool retired(std::string&);
} }

// Strong component retirement ABI for Stage Prepared/Loading/Active barriers.
// These cover this runtime's retained GenPiki FSM owners only. Stage separately
// observes genuine physical factory/partial-body/Shape/Bank owners; ordinary
// starting20 bodies never become a source roster through this empty query.
bool pc_p2_original_piki_runtime_owned()noexcept;
bool pc_p2_original_piki_runtime_can_retire(std::string&);
bool pc_p2_original_piki_runtime_retire(std::string&);
bool pc_p2_original_piki_runtime_retired(std::string&);
