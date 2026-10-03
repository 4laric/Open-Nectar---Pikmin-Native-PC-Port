#include "pc_p2_original_piki_native_services.h"
#include "pc_p2_original_captain_damage.h"
#include <cstdio>
#include <cstdlib>
#include <functional>

// This links the REAL Services delegate/observation implementation. Resource,
// animator, Plate and physical producers below are explicit test doubles: no
// Shape, native pool, scene activation or gameplay acceptance is established.
namespace c=p2original::captain;
namespace p=p2original::piki;
namespace {
unsigned checks=0;
void check(bool v,const char* text){++checks;if(!v){std::fprintf(stderr,"FAIL %s\n",text);std::exit(1);}}
struct Scene final:c::LoadedScene {
 std::string campaign="campaign",fingerprint="session",catalog="catalog";
 std::uint64_t serial=17;
 Navi* actors[2]={reinterpret_cast<Navi*>(0x1000),reinterpret_cast<Navi*>(0x2000)};
 const std::string& selectedCampaign()const override{return campaign;}
 const std::string& selectedFingerprint()const override{return fingerprint;}
 const std::string& sourceCatalog()const override{return catalog;}
 MoviePlayer* moviePlayer()const override{return nullptr;}
 std::uint64_t incarnation()const override{return serial;}
 Navi* captainAt(unsigned n)const override{return n<2?actors[n]:nullptr;}
} scene,foreign;
struct World final:c::World {
 c::Phase state=c::Phase::GameWorldActive;
 const std::string& selectedCampaign()const override{return scene.campaign;}
 const std::string& selectedFingerprint()const override{return scene.fingerprint;}
 const std::string& sourceCatalog()const override{return scene.catalog;}
 std::uint64_t incarnation()const override{return scene.serial;}
 c::Phase phase()const override{return state;}
 c::Demo demo()const override{return c::Demo::Absent;}
 Navi* captainAt(unsigned n)const override{return scene.captainAt(n);}
} world;
const c::LoadedScene* selected=&scene;
const c::World* selectedWorld=&world;
bool bankCurrent=true;
bool bodyLive=true,retireOnOwnerObservation=false;
std::function<void()> callback;
void invoke(){if(callback)callback();}
struct PlateSource final:p::PlateSource {
 const c::LoadedScene& scene()const override{return ::scene;}
 bool readParameters(const Navi*,p::PlateParameters&,std::string&)const override{return false;}
 bool readPose(const Navi*,p::PlatePose&,std::string&)const override{return false;}
 bool setFormed(p::Handle,Navi*,std::string&)override{return false;}
} plateSource;
struct Captains final:p::NativeCaptainReader {
 const c::LoadedScene& scene()const override{if(retireOnOwnerObservation)bodyLive=false;return ::scene;}
 const std::string& naviParameterBytes()const override{return ::scene.catalog;}
 p::PlateSource& plateSource()const override{return ::plateSource;}
 bool frame(const Navi*,p::CaptainFrame& out,std::string&)const override{out.face=4;invoke();return true;}
} captains;
struct Bank final:p::AnimationBank {
 const c::LoadedScene& scene()const override{return ::scene;}
 bool current(std::string&)const override{return true;}
 Shape* shape(const std::string&)const override{return nullptr;}
} animationBank;
struct Effects final:p::NativeEffects {
 Ownership retained{1,0,0,0};bool cleanupAllowed=false;
 bool sceneRetained=true;mutable unsigned sceneReferenceCalls=0;
 const c::LoadedScene& scene()const override{++sceneReferenceCalls;return ::scene;}
 const c::LoadedScene* retainedScene()const noexcept override{return sceneRetained?&::scene:nullptr;}
 bool ownership(Ownership& out,std::string&)const override{out=retained;return true;}
 bool canRetire(std::string&)const override{return cleanupAllowed;}
 bool retire(std::string&)override{if(!cleanupAllowed)return false;retained={};return true;}
 bool canRemoveFree(p::Handle,std::string&)const override{return false;}
 bool free(p::Handle,bool,std::string&)override{return false;}
 bool canRemoveThrow(p::Handle,std::string&)const override{return false;}
 bool thrown(p::Handle,bool,std::string&)override{return false;}
 bool hangSound(p::Handle,std::string&)override{return false;}
 bool landSound(p::Handle,std::string&)override{return false;}
 bool calledSound(p::Handle,std::string&)override{return false;}
 bool nudgeRumble(p::Handle,Navi*,std::string&)override{return false;}
 bool boreVoice(p::Handle,bool,std::string&)override{return false;}
} effects;
struct Environment final:p::NativeTaskEnvironment {
 bool available=true;
 const c::LoadedScene& scene()const override{return ::scene;}
 bool gravity(float& out,std::string&)const override{out=9;invoke();return true;}
 bool random(float& out,std::string&)override{out=.25f;invoke();return true;}
 bool task(p::Handle,p::FreeSearch,bool& out,std::string&)const override{out=false;invoke();return available;}
} environment;
struct Physical final:p::PhysicalSource {
 bool available=true,alive=true;
 const c::LoadedScene& scene()const override{return ::scene;}
 bool readPhysical(p::Handle,p::PhysicalFacts& out,std::string&)const override{out.alive=alive;invoke();return available;}
} physical;
void reset(){callback={};selected=&scene;selectedWorld=&world;world.state=c::Phase::GameWorldActive;scene.serial=17;scene.catalog="catalog";scene.actors[0]=reinterpret_cast<Navi*>(0x1000);bankCurrent=true;bodyLive=true;retireOnOwnerObservation=false;environment.available=true;physical.available=true;}
}
bool pc_p2_original_piki_body_current(const Piki* body,std::uint64_t life)noexcept{return bodyLive&&body==reinterpret_cast<Piki*>(0x3000)&&life==8;}
const c::LoadedScene* pc_p2_original_captain_loaded_scene(){return selected;}
const c::World* pc_p2_original_captain_world(){return selectedWorld;}
namespace p2original {namespace piki {
struct NativeBodyBank::Impl{};
NativeBodyBank::NativeBodyBank()=default;NativeBodyBank::~NativeBodyBank()=default;
NativeBodyBank& NativeBodyBank::instance(){static NativeBodyBank b;return b;}
bool NativeBodyBank::current()const noexcept{return bankCurrent;}
const std::string* NativeBodyBank::bytes(const std::string&)const noexcept{return nullptr;}
struct NativeAnimator::Impl{};
NativeAnimator::NativeAnimator(AnimationBank&){} NativeAnimator::~NativeAnimator()=default;
bool NativeAnimator::supports(Handle,Motion,std::string&)const{return false;}
bool NativeAnimator::start(Handle,Motion,std::string&){return false;}
bool NativeAnimator::advance(Handle,float,std::string&){return false;}
bool NativeAnimator::status(Handle,Motion& m,float& r,bool& done,std::string&)const{m=Motion::Hang;r=2;done=true;invoke();return true;}
bool NativeAnimator::speed(Handle,float,std::string&){return false;}
bool NativeAnimator::finish(Handle,std::string&){return false;}
bool NativeAnimator::loopStart(Handle,std::string&){return false;}
bool Plate::allocateSlot(Handle,Navi*,int&,std::string&){return false;}
bool Plate::releaseSlot(Handle,Navi*,int,std::string&){return false;}
bool Plate::canReleaseSlot(Handle,Navi*,int,std::string&)const{return false;}
bool Plate::slotPosition(Handle,Navi*,int,Vector3f& out,std::string&)const{out.set(1,2,3);invoke();return true;}
bool Plate::sortSlot(Handle,Navi*,int,int,std::string&){return false;}
bool Plate::formed(Handle,Navi*,std::string&){return false;}
bool nativePhysicalFacts(Handle h,const PhysicalSource* source,PhysicalFacts& out,std::string& e){if(!h.body||!h.lifetime||!source)return false;PhysicalFacts next;if(!source->readPhysical(h,next,e)||!pc_p2_original_piki_body_current(h.body,h.lifetime))return false;out=next;return true;}
}}
int main(){
 p::NativeAnimator animator(animationBank);p::Plate plate(plateSource);
 p::NativeServices services(p::NativeBodyBank::instance(),animator,plate,captains,effects,environment,physical);
 const p::Handle body{reinterpret_cast<Piki*>(0x3000),8};std::string error;
 float value=71;check(services.gravity(value,error)&&value==9,"stable gravity publishes actual producer value");
 const std::function<void()> changes[]={[]{selected=&foreign;},[]{selectedWorld=nullptr;},[]{world.state=c::Phase::Inactive;},[]{++scene.serial;},[]{scene.catalog="changed";},[]{scene.actors[0]=reinterpret_cast<Navi*>(0x4000);},[]{bankCurrent=false;}};
 for(const auto& change:changes){
  reset();callback=change;value=71;check(!services.gravity(value,error)&&value==71,"gravity refuses callback authority change atomically");
  reset();callback=change;value=71;check(!services.random(value,error)&&value==71,"RNG observation does not publish stale value");
  reset();callback=change;p::CaptainFrame frame;frame.face=71;check(!services.captainFrame(scene.actors[0],frame,error)&&frame.face==71,"Captain frame output retained on invalidation");
  reset();callback=change;bool available=true;check(!services.freeTaskAvailable(body,p::FreeSearch::Execute,available,error)&&available,"task output retained on invalidation");
  reset();callback=change;bool alive=false;check(!services.bodyAlive(body,alive,error)&&!alive,"source alive output retained on invalidation");
  reset();callback=change;p::Motion motion=p::Motion::Sleep;float rate=71;bool done=false;check(!services.animationStatus(body,motion,rate,done,error)&&motion==p::Motion::Sleep&&rate==71&&!done,"animator multi-output atomic on invalidation");
  reset();callback=change;Vector3f position(71,72,73);check(!services.slotPosition(body,scene.actors[0],0,position,error)&&position.x==71&&position.y==72&&position.z==73,"Plate position output retained on invalidation");
 }
 // The scene remains byte-for-byte current, but a FINAL owner callback retires
 // the body's native lifetime after the original leaf validates its result.
 const auto lateRetire=[](){retireOnOwnerObservation=true;};
 reset();callback=lateRetire;bool lateAlive=false;check(!services.bodyAlive(body,lateAlive,error)&&!lateAlive&&!bodyLive,"final owner observation cannot publish stale source alive");
 reset();callback=lateRetire;bool lateTask=true;check(!services.freeTaskAvailable(body,p::FreeSearch::Probe,lateTask,error)&&lateTask&&!bodyLive,"final owner observation cannot publish stale task result");
 reset();callback=lateRetire;p::Motion lateMotion=p::Motion::Sleep;check(!services.currentMotion(body,lateMotion,error)&&lateMotion==p::Motion::Sleep&&!bodyLive,"final owner observation cannot publish stale motion");
 reset();callback=lateRetire;float lateRate=71;bool lateDone=false;check(!services.animationStatus(body,lateMotion,lateRate,lateDone,error)&&lateMotion==p::Motion::Sleep&&lateRate==71&&!lateDone&&!bodyLive,"final owner observation cannot publish stale animator fields");
 reset();callback=lateRetire;Vector3f latePosition(71,72,73);check(!services.slotPosition(body,scene.actors[0],0,latePosition,error)&&latePosition.x==71&&latePosition.y==72&&latePosition.z==73&&!bodyLive,"final owner observation cannot publish stale Plate position");
 reset();physical.alive=false;bool alive=true;check(services.bodyAlive(body,alive,error)&&!alive,"authenticated source CF-dead is an actual negative fact");
 physical.available=false;alive=true;check(!services.bodyAlive(body,alive,error)&&alive,"absent physical authority never fabricates dead");
 reset();environment.available=false;bool available=true;check(!services.freeTaskAvailable(body,p::FreeSearch::Probe,available,error)&&available,"incomplete entity census never fabricates no-task");
 p::NativeEffects::Ownership retained;check(effects.ownership(retained,error)&&retained.freeContexts==1,"failed partial effect ownership observed independently of runtime");
 check(!effects.canRetire(error)&&!effects.retire(error),"failed partial cleanup refuses instead of clearing owner");
 check(effects.ownership(retained,error)&&retained.freeContexts==1,"partial effect owner remains retained after failure");
 effects.cleanupAllowed=true;check(effects.retire(error)&&effects.ownership(retained,error)&&retained.freeContexts==0,"actual producer retry clears only after successful disposal");
 reset();effects.sceneRetained=false;float staleGravity=71;
 check(!services.gravity(staleGravity,error)&&staleGravity==71,"unbound effect descriptor refuses without stale outputs");
 bool staleAlive=true;check(!services.bodyAlive(body,staleAlive,error)&&staleAlive,"deleted effect Scene refuses source alive read");
 check(effects.sceneReferenceCalls==0,"Services never dereferences effect Scene through reference getter");
 effects.sceneRetained=true;check(services.gravity(staleGravity,error)&&staleGravity==9,"actual retained descriptor allows checked retry");
 std::printf("NativeServices source observation controls PASS %u (resource/physical/effect doubles; no engine factory)\n",checks);
}
