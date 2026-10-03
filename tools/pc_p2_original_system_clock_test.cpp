#include "pc_p2_original_system_clock.h"
#include "pc_p2_retail_scene.h"
#include "pc_p2_original_captain_damage.h"
#include <iostream>
#include <memory>
#include <thread>
using namespace p2original;
namespace {
unsigned checks=0;void check(bool c,const char* why){++checks;if(!c){std::cerr<<why<<'\n';std::exit(1);}}
const std::string campaign(64,'a'),session(64,'b'),layout(64,'c');
p2retail::SceneContext* stage=nullptr;const p2retail::SceneContext* prepared=nullptr;
std::thread::id creatingThread;
std::string selectedCampaign=campaign,selectedSession=session;std::uint64_t selectedRevision=3;
bool original=true,threadRevoked=false;
const captain::LoadedScene* loaded=nullptr;
enum class Change {None,Pointer,Phase,Serial,Revision,Campaign,Session,Visit,Layout,Map,Stage,Routes,Reentry,Exception};
Change callbackChange=Change::None;SystemClock* callbackClock=nullptr;bool descriptorCallback=false;
void callback();
struct Scene final:captain::LoadedScene {
 std::string c=campaign,s=session,l=layout;std::uint64_t serial=8;
 Navi* actors[2]={reinterpret_cast<Navi*>(0x4000),reinterpret_cast<Navi*>(0x5000)};
 const std::string& selectedCampaign()const override{return c;}
 const std::string& selectedFingerprint()const override{return s;}
 const std::string& sourceCatalog()const override{return l;}
 MoviePlayer* moviePlayer()const override{return nullptr;}
 std::uint64_t incarnation()const override{if(descriptorCallback)callback();return serial;}
 Navi* captainAt(unsigned i)const override{return i<2?actors[i]:nullptr;}
} scene;
}
namespace p2retail {
class SceneRuntime {public:
 static SceneContext* make(){return new SceneContext;}
 static void reset(SceneContext& s){s.mStage=reinterpret_cast<StageInfo*>(0x2000);s.mMap=reinterpret_cast<MapMgr*>(0x3000);
  s.mRoutes=reinterpret_cast<RouteMgr*>(0x6000);s.mPhase=ScenePhase::Prepared;s.mSnapshot.scene={session,"visit",layout,8};
  s.mRevision=3;s.mCampaign=campaign;s.mSession=session;s.mPlan.layoutSha256=layout;}
 static void phase(SceneContext& s,ScenePhase p){s.mPhase=p;}
 static void mutate(SceneContext& s,Change c){switch(c){
  case Change::Phase:s.mPhase=ScenePhase::Installing;break;
  case Change::Serial:++s.mSnapshot.scene.serial;break;
  case Change::Revision:++s.mRevision;break;
  case Change::Campaign:s.mCampaign=std::string(64,'d');break;
  case Change::Session:s.mSession=std::string(64,'d');break;
  case Change::Visit:s.mSnapshot.scene.visit+="changed";break;
  case Change::Layout:s.mSnapshot.scene.layoutSha256=std::string(64,'d');break;
  case Change::Map:s.mMap=nullptr;break;
  case Change::Stage:s.mStage=nullptr;break;
  case Change::Routes:s.mRoutes=nullptr;break;
  default:break;
 }}
};
bool SceneContext::ownsCurrentThread()const noexcept{return !threadRevoked&&std::this_thread::get_id()==creatingThread;}
}
const p2retail::SceneContext* pc_p2_retail_scene_prepared()noexcept{return prepared;}
bool pc_randomizer_original_session(){return original;}
std::string pc_randomizer_original_campaign(){if(!descriptorCallback)callback();return selectedCampaign;}
std::string pc_randomizer_session_fingerprint(){return selectedSession;}
std::uint64_t pc_randomizer_original_selection_revision()noexcept{return selectedRevision;}
const captain::LoadedScene* pc_p2_original_captain_loaded_scene(){return loaded;}
namespace p2original {namespace piki {
class NativeBodyFactory {public:
 static std::unique_ptr<SystemClock> make(){return std::unique_ptr<SystemClock>(new SystemClock);}
 static bool bind(SystemClock& c,std::string& e){return c.sourceStageOwned(*stage,e);}
 static bool init(SystemClock& c,std::string& e){return c.sourceBaseGameSectionInit(e);}
 static bool loaded(SystemClock& c,std::string& e){return c.sourceLoadedScene(e);}
 static bool begin(SystemClock& c,std::string& e){return c.sourceBaseGameSectionUpdateBegin(e);}
 static bool end(SystemClock& c,std::string& e){return c.sourceBaseGameSectionUpdateEnd(e);}
 static bool release(SystemClock& c,std::string& e){return c.sourceStageReleased(e);}
 static float rawDt(const SystemClock& c){return c.mDeltaTime;}
 static float rawFactor(const SystemClock& c){return c.mFrameRate;}
 static bool open(const SystemClock& c){return c.mFrameOpen;}
};
} }
using Owner=piki::NativeBodyFactory;
namespace {
void reset(){p2retail::SceneRuntime::reset(*stage);prepared=stage;creatingThread=std::this_thread::get_id();
 selectedCampaign=campaign;selectedSession=session;selectedRevision=3;original=true;threadRevoked=false;
 loaded=nullptr;scene.c=campaign;scene.s=session;scene.l=layout;scene.serial=8;
 scene.actors[0]=reinterpret_cast<Navi*>(0x4000);scene.actors[1]=reinterpret_cast<Navi*>(0x5000);
 callbackChange=Change::None;callbackClock=nullptr;descriptorCallback=false;}
void callback(){const auto c=callbackChange;callbackChange=Change::None;
 switch(c){
  case Change::None:break;
  case Change::Pointer:prepared=nullptr;break;
  case Change::Exception:throw std::runtime_error("source callback fault control");
  case Change::Reentry:{float out=99;std::string e;
   check(callbackClock&&!callbackClock->readDeltaTime(out,e)&&out==99&&!e.empty(),"nested clock read refuses unchanged");break;}
  default:p2retail::SceneRuntime::mutate(*stage,c);break;
 }
}
void refused(const SystemClock& c){float out=99;std::string e;
 check(!c.readDeltaTime(out,e)&&out==99&&!e.empty(),"clock refusal preserves output and diagnostic");}
std::unique_ptr<SystemClock> ready(){reset();auto c=Owner::make();std::string e;
 check(Owner::bind(*c,e)&&Owner::init(*c,e),"actual test Stage/source section events");return c;}
}
int main(){
 std::unique_ptr<p2retail::SceneContext> owned(p2retail::SceneRuntime::make());stage=owned.get();reset();
 auto clock=Owner::make();std::string e;float dt=99;
 check(Owner::rawFactor(*clock)==1.0f&&Owner::rawDt(*clock)==1.0f/60.0f,"literal Source System constructor scalar values");
 refused(*clock);check(!Owner::init(*clock,e)&&!Owner::begin(*clock,e)&&!Owner::end(*clock,e),"events unavailable without retained Stage");
 check(Owner::bind(*clock,e)&&e.empty(),"genuine prepared Stage owned event");refused(*clock);
 check(!Owner::bind(*clock,e)&&Owner::rawDt(*clock)==1.0f/60.0f,"repeated bind never resets constructor history");
 check(Owner::init(*clock,e)&&e.empty()&&!loaded,"actual BGS init succeeds BEFORE captain roster exists");
 check(clock->readDeltaTime(dt,e)&&dt==2.0f/60.0f&&Owner::rawFactor(*clock)==2.0f&&e.empty(),"literal source setFrameRate2 and readonly dt");
 check(!Owner::init(*clock,e)&&!Owner::loaded(*clock,e)&&clock->readDeltaTime(dt,e),"duplicate init/absent later roster cannot create or revoke earlier dt");
 check(!Owner::end(*clock,e)&&Owner::begin(*clock,e)&&!Owner::begin(*clock,e)&&Owner::open(*clock),"named update event bracket ordering");
 check(!Owner::release(*clock,e)&&clock->readDeltaTime(dt,e)&&dt==2.0f/60.0f,"in-flight update retains ownership but never changes dt");
 check(Owner::end(*clock,e)&&!Owner::open(*clock)&&!Owner::end(*clock,e),"only matching actual end closes update");
 for(auto p:{p2retail::ScenePhase::Prepared,p2retail::ScenePhase::Installing,p2retail::ScenePhase::Committed}){
  p2retail::SceneRuntime::phase(*stage,p);check(clock->readDeltaTime(dt,e)&&dt==2.0f/60.0f,"dt resource observation does not infer active gameplay");}
 p2retail::SceneRuntime::phase(*stage,p2retail::ScenePhase::Prepared);loaded=&scene;
 check(Owner::loaded(*clock,e)&&!Owner::loaded(*clock,e)&&clock->readDeltaTime(dt,e),"later actual canonical descriptor attached once");
 loaded=nullptr;refused(*clock);loaded=&scene;
 for(auto bad:{0,1,2,3,4,5}){scene.c=campaign;scene.s=session;scene.l=layout;scene.serial=8;
  scene.actors[0]=reinterpret_cast<Navi*>(0x4000);scene.actors[1]=reinterpret_cast<Navi*>(0x5000);
  switch(bad){case 0:scene.c=std::string(64,'d');break;case 1:scene.s=std::string(64,'d');break;
   case 2:scene.l=std::string(64,'d');break;case 3:++scene.serial;break;case 4:scene.actors[0]=nullptr;break;
   case 5:scene.actors[1]=scene.actors[0];break;}refused(*clock);
 }
 for(auto c:{Change::Pointer,Change::Phase,Change::Serial,Change::Revision,Change::Campaign,Change::Session,
  Change::Visit,Change::Layout,Change::Map,Change::Stage,Change::Routes,Change::Reentry,Change::Exception}){
  auto sample=ready();callbackClock=sample.get();callbackChange=c;refused(*sample);
  // Each callback can occur at the real later descriptor observation too.
  sample=ready();loaded=&scene;check(Owner::loaded(*sample,e),"attach callback test descriptor");
  descriptorCallback=true;callbackClock=sample.get();callbackChange=c;refused(*sample);
 }
 for(unsigned event=0;event<6;++event){auto sample=ready();callbackClock=sample.get();callbackChange=Change::Reentry;
  bool result=false;
  switch(event){case 0:result=Owner::init(*sample,e);break;case 1:result=Owner::begin(*sample,e);break;
   case 2:result=Owner::end(*sample,e);break;case 3:loaded=&scene;result=Owner::loaded(*sample,e);break;
   case 4:result=Owner::release(*sample,e);break;case 5:result=Owner::bind(*sample,e);break;}
  check(!result&&!Owner::open(*sample)&&Owner::rawDt(*sample)==2.0f/60.0f,"reentry never commits event scalar/ownership changes");
 }
 auto sample=ready();threadRevoked=true;refused(*sample);threadRevoked=false;
 selectedRevision=4;refused(*sample);selectedRevision=3;original=false;refused(*sample);original=true;
 selectedCampaign=std::string(64,'d');refused(*sample);selectedCampaign=campaign;selectedSession=std::string(64,'d');refused(*sample);selectedSession=session;
 bool result=true;float threaded=99;std::string threadError;
 std::thread wrong([&]{result=sample->readDeltaTime(threaded,threadError);});wrong.join();
 check(!result&&threaded==99&&!threadError.empty(),"actual other OS thread refuses retained source dt");
 auto atomicInit=Owner::make();reset();callbackClock=atomicInit.get();callbackChange=Change::Reentry;
 check(!Owner::bind(*atomicInit,e)&&Owner::rawDt(*atomicInit)==1.0f/60.0f,"reentrant fresh Stage bind cannot publish ownership");
 check(Owner::bind(*atomicInit,e),"bind retry after refused reentry");
 callbackChange=Change::Reentry;check(!Owner::init(*atomicInit,e)&&Owner::rawDt(*atomicInit)==1.0f/60.0f,"reentrant first section init cannot publish factor2");refused(*atomicInit);
 check(Owner::init(*atomicInit,e)&&Owner::begin(*atomicInit,e),"actual section/update events after refusal");
 callbackChange=Change::Reentry;check(!Owner::end(*atomicInit,e)&&Owner::open(*atomicInit),"failed matching end retains real in-flight bracket");
 check(!Owner::release(*atomicInit,e)&&Owner::end(*atomicInit,e)&&Owner::release(*atomicInit,e),"retry closes bracket before release");
 sample=ready();loaded=&scene;scene.actors[1]=scene.actors[0];
 check(!Owner::loaded(*sample,e)&&sample->readDeltaTime(dt,e),"invalid later roster refuses attach without erasing earlier source dt");
 scene.actors[1]=reinterpret_cast<Navi*>(0x5000);descriptorCallback=true;callbackChange=Change::Exception;
 check(!Owner::loaded(*sample,e)&&sample->readDeltaTime(dt,e),"loaded callback exception retains unbound descriptor and valid section dt");
 descriptorCallback=false;loaded=nullptr;
 p2retail::SceneRuntime::phase(*stage,p2retail::ScenePhase::Releasing);refused(*sample);
 check(Owner::release(*sample,e)&&Owner::rawFactor(*sample)==2&&Owner::rawDt(*sample)==2.0f/60.0f,"retirement preserves real persistent System scalar history");refused(*sample);
 reset();check(Owner::bind(*sample,e)&&Owner::rawDt(*sample)==2.0f/60.0f,"next Stage binding never invents setFrameRate1");refused(*sample);
 check(Owner::init(*sample,e)&&sample->readDeltaTime(dt,e)&&e.empty()&&dt==2.0f/60.0f,"fresh matching section event re-enables current resource dt");
 reset();auto unbound=Owner::make();prepared=nullptr;check(!Owner::bind(*unbound,e),"expired Stage bind refuses before dereference");
 reset();p2retail::SceneRuntime::phase(*stage,p2retail::ScenePhase::Committed);check(!Owner::bind(*unbound,e),"constructor binding requires actual initial Prepared event");
 std::cout<<checks<<" source SystemClock controls passed; synthetic event authority only, no runtime grant\n";
}
