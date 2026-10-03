// Reuse only the explicit producer doubles from the Services component test.
// The REAL composition and Services TUs are linked. Stage/Shape/Animator
// construction below is a double; this does not attest real native factories.
#define main services_control_entry
#include "pc_p2_original_piki_native_services_test.cpp"
#undef main
#include "pc_p2_original_piki_composer.h"
#include "pc_p2_retail_scene.h"
#include <new>
namespace {bool failOneAllocation=false;unsigned failedAllocations=0;}
void* operator new(std::size_t size){if(failOneAllocation){failOneAllocation=false;++failedAllocations;throw std::bad_alloc();}if(auto* value=std::malloc(size?size:1))return value;throw std::bad_alloc();}
void operator delete(void* value)noexcept{std::free(value);}
void operator delete(void* value,std::size_t)noexcept{std::free(value);}
namespace {
bool stageThread=true,bankOwned=false,failAnimation=true;
unsigned bankPreparations=0,animationPreparations=0;
std::function<void(unsigned)> ownerCallback;
p::Services* installed=nullptr;
struct ObservedPlateSource final:p::PlateSource {
 const c::LoadedScene& scene()const override{if(ownerCallback)ownerCallback(2);return ::scene;}
 bool readParameters(const Navi*,p::PlateParameters&,std::string&)const override{return false;}
 bool readPose(const Navi*,p::PlatePose&,std::string&)const override{return false;}
 bool setFormed(p::Handle,Navi*,std::string&)override{return false;}
} observedPlateSource;
struct ObservedEffects final:p::NativeEffects {
 bool sceneRetained=true;mutable unsigned sceneReferenceCalls=0;
 const c::LoadedScene& scene()const override{++sceneReferenceCalls;return ::scene;}
 const c::LoadedScene* retainedScene()const noexcept override{if(ownerCallback)ownerCallback(3);return sceneRetained?&::scene:nullptr;}
 bool ownership(Ownership&,std::string&)const override{return false;}
 bool canRetire(std::string&)const override{return false;}
 bool retire(std::string&)override{return false;}
 bool canRemoveFree(p::Handle,std::string&)const override{return false;}
 bool free(p::Handle,bool,std::string&)override{return false;}
 bool canRemoveThrow(p::Handle,std::string&)const override{return false;}
 bool thrown(p::Handle,bool,std::string&)override{return false;}
 bool hangSound(p::Handle,std::string&)override{return false;}
 bool landSound(p::Handle,std::string&)override{return false;}
 bool calledSound(p::Handle,std::string&)override{return false;}
 bool nudgeRumble(p::Handle,Navi*,std::string&)override{return false;}
 bool boreVoice(p::Handle,bool,std::string&)override{return false;}
} effectReader;
struct ObservedEnvironment final:p::NativeTaskEnvironment {
 const c::LoadedScene& scene()const override{if(ownerCallback)ownerCallback(4);return ::scene;}
 bool gravity(float&,std::string&)const override{return false;}
 bool random(float&,std::string&)override{return false;}
 bool task(p::Handle,p::FreeSearch,bool&,std::string&)const override{return false;}
} environmentReader;
struct ObservedPhysical final:p::PhysicalSource {
 const c::LoadedScene& scene()const override{if(ownerCallback)ownerCallback(5);return ::scene;}
 bool readPhysical(p::Handle,p::PhysicalFacts&,std::string&)const override{return false;}
} physicalReader;
struct ObservedCaptain final:p::NativeCaptainReader {
 const c::LoadedScene& scene()const override{if(ownerCallback)ownerCallback(0);return ::scene;}
 const std::string& naviParameterBytes()const override{return ::scene.catalog;}
 p::PlateSource& plateSource()const override{if(ownerCallback)ownerCallback(1);return observedPlateSource;}
 bool frame(const Navi*,p::CaptainFrame&,std::string&)const override{return false;}
} observedCaptain;
}
// Explicit test-only Stage owner constructs the REAL immutable descriptor type;
// no actual MapMgr, StageInfo, SceneRuntime or source-world grant is exercised.
namespace p2retail {
class SceneRuntime {
public:
 static SceneContext& fixture(){static SceneContext value;value.mStage=reinterpret_cast<StageInfo*>(0x5000);value.mMap=reinterpret_cast<MapMgr*>(0x6000);value.mSnapshot.scene.serial=41;value.mRevision=3;value.mCampaign="campaign";value.mSession="session";return value;}
};
bool SceneContext::ownsCurrentThread()const noexcept{return stageThread;}
}
const p2retail::SceneContext* pc_p2_retail_scene_prepared()noexcept{return &p2retail::SceneRuntime::fixture();}
namespace p2original {namespace piki {
bool NativeBodyBank::prepare(const p2retail::SceneContext&,std::string&){++bankPreparations;bankCurrent=true;bankOwned=true;return true;}
bool NativeBodyBank::owned()const noexcept{return bankOwned;}
Shape* NativeBodyBank::shape(const std::string&)const noexcept{return nullptr;}
bool NativeAnimator::prepare(unsigned,std::string&){++animationPreparations;return !failAnimation;}
bool installServices(Services& service)noexcept{if(installed)return installed==&service;installed=&service;return true;}
}}
int main(){
 reset();world.state=c::Phase::Loading;bankCurrent=false;bankOwned=false;std::string error;
 auto& stage=*pc_p2_retail_scene_prepared();
 check(!p::nativePlate(scene,error),"absent composition exposes no storage");
 stageThread=false;
 check(!p::prepareNativeComposition(stage,observedCaptain,effectReader,environmentReader,physicalReader,error)&&bankPreparations==0,"wrong Stage thread refuses before model preparation");
 stageThread=true;
 failOneAllocation=true;
 check(!p::prepareNativeComposition(stage,observedCaptain,effectReader,environmentReader,physicalReader,error),"real C++ component constructor allocation failure refuses");
 check(failedAllocations==1&&bankOwned&&bankPreparations==1&&animationPreparations==0,"partial bank binding retained before failed animator construction");
 check(!p::nativePlate(scene,error),"failed constructor exposes no fabricated Plate");
 check(!p::prepareNativeComposition(stage,observedCaptain,effectReader,environmentReader,physicalReader,error),"failed animator preparation retains partial actual composition");
 check(bankPreparations==1&&animationPreparations==1&&bankOwned,"model owner remains retained after animation refusal");
 auto* plate=p::nativePlate(scene,error);
 check(plate!=nullptr,"constructed partial Plate borrow does not fabricate twenty bodies");
 check(p::nativeServices(scene,error)==nullptr,"unprepared animation refuses Services borrow");
 failAnimation=false;
 installed=reinterpret_cast<p::Services*>(0x7000);
 check(!p::prepareNativeComposition(stage,observedCaptain,effectReader,environmentReader,physicalReader,error),"conflicting installed Runtime Services owner refuses");
 check(!p::nativeServices(scene,error),"constructed but uninstalled Services are not exposed");
 installed=nullptr;
 check(p::prepareNativeComposition(stage,observedCaptain,effectReader,environmentReader,physicalReader,error),"same producer retry completes retained construction");
 check(bankPreparations==1&&animationPreparations==2,"retry does not reload or lose selected native bank");
 check(p::nativePlate(scene,error)==plate&&p::nativeServices(scene,error)==installed,"stable actual storage and installed Services returned");
 check(p::nativePhysicalSource(scene,error)==&physicalReader,"real mandatory physical producer retained");
 check(!p::nativePlate(foreign,error),"foreign canonical descriptor refuses borrow");
 ownerCallback=[](unsigned){selected=&foreign;};
 check(!p::nativeServices(scene,error),"virtual owner scene replacement refuses before borrow publication");
 ownerCallback={};selected=&scene;
 ownerCallback=[&](unsigned){std::string nested;check(!p::nativePlate(scene,nested),"nested borrow refuses reentrant owner inspection");};
 check(!p::nativeServices(scene,error),"reentrant producer observation invalidates outer borrow");
 ownerCallback={};
 check(p::nativeServices(scene,error)==installed,"refused readonly observation preserves retained storage for retry");
  const std::function<void()> changes[]={[]{selected=&foreign;},[]{selectedWorld=nullptr;},[]{++scene.serial;},[]{world.state=c::Phase::Inactive;},[]{stageThread=false;}};
 for(unsigned callbackIndex=0;callbackIndex<6;++callbackIndex){
  for(const auto& change:changes){
   reset();world.state=c::Phase::Loading;bankCurrent=true;stageThread=true;
   ownerCallback=[&](unsigned index){if(index==callbackIndex)change();};
   check(!p::nativeServices(scene,error),"each callback refuses scene/world/incarnation/phase/thread changes");
  }
 }
 ownerCallback={};reset();world.state=c::Phase::Loading;bankCurrent=true;stageThread=true;
 ObservedCaptain substitute;
 check(!p::prepareNativeComposition(stage,substitute,effectReader,environmentReader,physicalReader,error),"producer substitution cannot replace retained constructor references");
 check(p::nativeServices(scene,error)==installed,"substitution refusal preserves original storage");
 world.state=c::Phase::Inactive;
 check(p::nativePlate(scene,error)==plate,"inactive retained cleanup can borrow actual Plate without action grant");
 effectReader.sceneRetained=false;
 check(!p::nativeServices(scene,error),"retired effect descriptor refuses composition borrow gracefully");
 check(effectReader.sceneReferenceCalls==0,"composition never dereferences retired effect Scene reference");
 effectReader.sceneRetained=true;
 check(p::nativeServices(scene,error)==installed,"real retained effect descriptor restores checked borrow");
 stageThread=false;
 check(!p::nativePhysicalSource(scene,error),"wrong creating thread refuses after construction too");
 std::printf("NativeComposition controls PASS %u (Stage/model/animator doubles; no body factory)\n",checks);
}



