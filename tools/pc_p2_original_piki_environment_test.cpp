#include "pc_p2_original_piki_factory.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <thread>
#include <tuple>
using namespace p2original;
using Factory=piki::NativeBodyFactory;
namespace {
unsigned checks=0;void check(bool c,const char* why){++checks;if(!c){std::cerr<<why<<'\n';std::exit(1);}}
const std::string campaign(64,'a'),session(64,'b'),layout(64,'c');
p2retail::SceneContext* stage=nullptr;const p2retail::SceneContext* prepared=nullptr;
std::thread::id creatingThread;
Factory* factory=nullptr;GameSystem* game=nullptr;SystemClock* clockOwner=nullptr;
struct Status {bool stage=false,game=false,bound=false,clock=false;} status;
bool original=true,threadRevoked=false,missingAI=false,missingTime=false,authRefusal=false;
std::string selectedCampaign=campaign,selectedSession=session,aiBytes,timeBytes;
std::uint64_t selectedRevision=3;
unsigned inputReads=0,sdkReads=0,readTarget=0,sdkTarget=0;
std::size_t bodyRoots=0,poolRoots=0;
enum class Change {None,Pointer,Phase,Serial,Revision,Campaign,Session,Visit,Seed,Layout,Map,Routes,Selection,Reentry,RetireReentry,Exception};
Change change=Change::None;
bool worldCallback=false;
void callback();
struct World final:captain::World {
 captain::Phase p=captain::Phase::Inactive;
 const std::string& selectedCampaign()const override{return campaign;}
 const std::string& selectedFingerprint()const override{return session;}
 const std::string& sourceCatalog()const override{return layout;}
 std::uint64_t incarnation()const override{return 8;}
 captain::Phase phase()const override{if(worldCallback)callback();return p;}
 captain::Demo demo()const override{return captain::Demo::Absent;}
 Navi* captainAt(unsigned)const override{return nullptr;}
} world;
bool worldAvailable=false;
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
  case Change::Seed:s.mSnapshot.scene.seed=std::string(64,'d');break;
  case Change::Layout:s.mSnapshot.scene.layoutSha256=std::string(64,'d');break;
  case Change::Map:s.mMap=nullptr;break;case Change::Routes:s.mRoutes=nullptr;break;
  default:break;
 }}
 static bool begin(std::string& e){return clockOwner->sourceBaseGameSectionUpdateBegin(e);}
 static bool end(std::string& e){return clockOwner->sourceBaseGameSectionUpdateEnd(e);}
 static bool pause(std::string& e){return game->sourceWaitSyncLoadPause(e);}
 static bool frameOpen(){return clockOwner->mFrameOpen;}
};
bool SceneContext::ownsCurrentThread()const noexcept{return !threadRevoked&&std::this_thread::get_id()==creatingThread;}
}
const p2retail::SceneContext* pc_p2_retail_scene_prepared()noexcept{return prepared;}
const p2retail::SceneContext* pc_p2_retail_scene_committed()noexcept{return nullptr;}
bool pc_randomizer_original_session(){return original;}
std::string pc_randomizer_original_campaign(){if(++sdkReads==sdkTarget)callback();return selectedCampaign;}
std::string pc_randomizer_session_fingerprint(){return selectedSession;}
std::uint64_t pc_randomizer_original_selection_revision()noexcept{return selectedRevision;}
bool pc_randomizer_original_input(const std::string& role,std::string& out,std::string& e){
 ++inputReads;
 if(authRefusal||(role=="p2-original/system/aiConstants.txt"&&missingAI)
  ||(role=="p2-original/system/time.ini"&&missingTime)){e="explicit original_input auth/role refusal double";return false;}
 if(role=="p2-original/system/aiConstants.txt")out=aiBytes;
 else if(role=="p2-original/system/time.ini")out=timeBytes;
 else {e="noncanonical role requested";return false;}
 if(inputReads==readTarget)callback();e.clear();return true;
}
const captain::World* pc_p2_original_captain_world(){return worldAvailable?&world:nullptr;}
const captain::LoadedScene* pc_p2_original_captain_loaded_scene(){return nullptr;}
namespace p2original {namespace piki {
// Explicit isolated native allocation/physical census doubles only. The five
// real Environment/System/Clock/selected/raw-parser TUs are linked unchanged.
NativeBodyFactory& NativeBodyFactory::instance(){static auto* owner=new NativeBodyFactory;
 factory=owner;game=&owner->sourceSystem;clockOwner=&owner->sourceClock;
 status={owner->systemStage!=nullptr,owner->systemInitialized,owner->clockBound,owner->clockInitialized};return *owner;}
bool NativeBodyFactory::owned()const noexcept{return bodyRoots!=0;}
bool poolOwned()noexcept{return poolRoots!=0;}
NativePhysicalBootstrap::~NativePhysicalBootstrap(){check(!stage,"test did not construct physical roots");}
HostUpdateBinding::~HostUpdateBinding(){check(mPhase==Phase::Empty,"test did not register host contexts");}
bool nativeSceneBinding(SceneBinding&,bool,std::string& e){e="canonical roster deliberately absent in early source-section test";return false;}
bool nativeSceneCurrent(const SceneBinding&,bool,std::string& e){e="canonical roster absent";return false;}
bool nativeCaptainFacts(const SceneBinding&,const Navi*,bool,std::string& e){e="canonical roster absent";return false;}
} }
namespace {
void callback(){const auto c=change;change=Change::None;
 switch(c){case Change::None:break;case Change::Pointer:prepared=nullptr;break;
  case Change::Selection:++selectedRevision;break;
  case Change::Exception:throw std::runtime_error("SDK boundary exception control");
  case Change::Reentry:{float dt=99;std::string e;check(!factory->sourceDeltaTime(dt,e)&&dt==99,"nested Environment output unchanged");
   check(!factory->prepareSourceSection(*stage,e),"nested source-section mutation refused");break;}
  case Change::RetireReentry:{std::string e;check(!factory->retireSourceSection(e),"nested source retirement refused");break;}
  default:p2retail::SceneRuntime::mutate(*stage,c);break;}
}
void context(){p2retail::SceneRuntime::reset(*stage);prepared=stage;creatingThread=std::this_thread::get_id();
 original=true;threadRevoked=false;selectedCampaign=campaign;selectedSession=session;selectedRevision=3;
 missingAI=false;missingTime=false;authRefusal=false;readTarget=0;sdkTarget=0;change=Change::None;
 bodyRoots=0;poolRoots=0;worldAvailable=false;worldCallback=false;world.p=captain::Phase::Inactive;inputReads=0;sdkReads=0;}
void refresh(){Factory::instance();}
auto pairFields(const SelectedSystemParameters& p){return std::tie(p.ai.gravity,p.ai.dopeCount,p.ai.debt,p.ai.cameraAngle,
 p.time.dayStart,p.time.dayEnd,p.time.dayLengthSeconds,p.time.morningStart,p.time.midMorning,p.time.morningEnd,
 p.time.eveningStart,p.time.midEveningStart,p.time.midEveningEnd,p.time.eveningEnd,p.time.sundownAlert,p.time.countdown);}
auto stateFields(const GameSystemState& s){return std::tie(s.flags,s.pauseCountdown,s.unused,s.mode,s.frozen,s.softPause,s.moviePause,s.frameTimer);}
void unavailable(){float dt=99;GameSystemState gs;gs.flags=99;gs.pauseCountdown=98;gs.unused=97;gs.mode=GameSystemMode::Versus;
 gs.frozen=gs.softPause=gs.moviePause=true;gs.frameTimer=96;const auto beforeState=gs;
 SelectedSystemParameters p;p.ai={91,92,93,94};p.time={1,2,3,4,5,6,7,8,9,10,11,12};const auto beforePair=p;std::string e;
 check(!factory->sourceDeltaTime(dt,e)&&dt==99,"failed factory dt output unchanged");
 check(!factory->sourceGameSystemState(gs,e)&&stateFields(gs)==stateFields(beforeState),"failed factory ALL scalar state fields unchanged");
 check(!factory->sourceParameters(p,e)&&pairFields(p)==pairFields(beforePair),"failed factory ALL parameter pair fields unchanged");}
void cleanup(){context();std::string e;if(p2retail::SceneRuntime::frameOpen())check(p2retail::SceneRuntime::end(e),"close genuine test clock bracket");
 check(factory->retireSourceSection(e)&&!factory->sourceSectionOwned(),"retire all retained actual scalar owners");}
void ready(){context();std::string e;check(factory->prepareSourceSection(*stage,e)&&e.empty(),"genuine source-section preparation");}
std::string read(const char* path){std::ifstream f(path,std::ios::binary);check(bool(f),"actual staged input exists");return {std::istreambuf_iterator<char>(f),{}};}
}
int main(int argc,char** argv){
 check(argc==3,"actual staged AI/Time paths required");aiBytes=read(argv[1]);timeBytes=read(argv[2]);const auto ai=aiBytes,time=timeBytes;
 std::unique_ptr<p2retail::SceneContext> owner(p2retail::SceneRuntime::make());stage=owner.get();Factory::instance();context();unavailable();
 std::string e;check(factory->retireSourceSection(e),"empty retirement is idempotent");
 for(unsigned test=0;test<3;++test){context();missingAI=test==0;missingTime=test==1;authRefusal=test==2;
  check(!factory->prepareSourceSection(*stage,e)&&!factory->sourceSectionOwned(),"missing/unauthenticated role acquires no source Stage");unavailable();cleanup();}
 context();aiBytes="end";check(!factory->prepareSourceSection(*stage,e)&&!factory->sourceSectionOwned(),"real AI parser refuses no gravity");aiBytes=ai;cleanup();
 context();timeBytes="{_eof}";check(!factory->prepareSourceSection(*stage,e)&&!factory->sourceSectionOwned(),"real Time parser refuses truncated fields");timeBytes=time;cleanup();
 for(unsigned at=1;at<=2;++at)for(auto c:{Change::Pointer,Change::Phase,Change::Serial,Change::Revision,Change::Campaign,
  Change::Session,Change::Visit,Change::Seed,Change::Layout,Change::Map,Change::Routes,Change::Selection,Change::Reentry,Change::Exception}){
  context();readTarget=at;change=c;check(!factory->prepareSourceSection(*stage,e)&&!factory->sourceSectionOwned(),"read callback refusal publishes no Environment Stage");unavailable();cleanup();}
 ready();float dt=99;GameSystemState gs;SelectedSystemParameters p;
 check(factory->sourceDeltaTime(dt,e)&&dt==2.0f/60.0f&&factory->sourceGameSystemState(gs,e)&&gs.flags==0&&!gs.frozen
  &&!gs.softPause&&!gs.moviePause&&gs.pauseCountdown==0&&gs.frameTimer==0&&gs.mode==GameSystemMode::Story,"REAL GameSystem defaults and SourceClock dt2/60 before roster");
 check(factory->sourceParameters(p,e)&&p.ai.gravity==560&&p.ai.dopeCount==10&&p.ai.debt==10000&&p.ai.cameraAngle==290
  &&p.time.dayLengthSeconds==1560&&p.time.midEveningStart==16.5f&&p.time.midEveningEnd==17.5f,"actual retained authenticated parser pair");
 const auto events=sdkReads;check(inputReads==2&&factory->prepareSourceSection(*stage,e)&&inputReads==2,"repeat preparation preserves completed events/resources");cleanup();
 // Enumerate actual SDK call boundaries, including first Clock bind and its
 // section-init event AFTER GameSystem initialization; no clock TU is mocked.
 unsigned partial=0,gameBeforeClock=0;
 for(unsigned at=1;at<=events;++at){context();sdkTarget=at;change=Change::Reentry;
  const bool accepted=factory->prepareSourceSection(*stage,e);refresh();
  if(!accepted&&status.stage){++partial;const bool bound=status.bound,initialized=status.game,clock=status.clock;
   check(!clock||bound,"real Clock section event retains its Stage owner");
   if(!initialized||!clock)unavailable();
   else check(factory->sourceDeltaTime(dt,e)&&dt==2.0f/60.0f,"completed real events remain observable after final outer refusal");
   if(initialized&&!clock){++gameBeforeClock;check(p2retail::SceneRuntime::pause(e),"change real initialized GameSystem before retry");}
   sdkTarget=0;change=Change::None;inputReads=0;
   check(factory->prepareSourceSection(*stage,e)&&inputReads==0,"partial retry preserves prior authenticated bytes and completed events");
   check(factory->sourceGameSystemState(gs,e)&&(!initialized||clock||gs.pauseCountdown==3),"partial retry never resets initialized GameSystem");
  }
  cleanup();
 }
 check(partial>0&&gameBeforeClock>0,"enumeration exercised real partial initialization stages");
 ready();bodyRoots=1;check(!factory->retireSourceSection(e)&&factory->sourceSectionOwned()&&factory->sourceDeltaTime(dt,e),"unassociated/stale native body root retains Environment");bodyRoots=0;
 poolRoots=1;check(!factory->retireSourceSection(e)&&factory->sourceSectionOwned(),"independent raw pool allocation retains Environment");poolRoots=0;
 worldAvailable=true;world.p=captain::Phase::GameWorldActive;
 check(!factory->retireSourceSection(e)&&factory->sourceSectionOwned()&&factory->sourceGameSystemState(gs,e),"actual Active World forbids scalar retirement");world.p=captain::Phase::Inactive;worldAvailable=false;
 check(p2retail::SceneRuntime::begin(e)&&!factory->retireSourceSection(e),"real pending clock bracket blocks retirement after GameSystem release");refresh();
 check(status.stage&&!status.game&&status.bound&&status.clock&&p2retail::SceneRuntime::frameOpen(),"partial retirement retains exact clock/Stage continuation owner");unavailable();
 check(p2retail::SceneRuntime::end(e)&&factory->retireSourceSection(e)&&!factory->sourceSectionOwned(),"genuine clock end followed by retirement retry releases continuation");
 ready();threadRevoked=true;unavailable();check(!factory->retireSourceSection(e),"wrong creating-thread authority cannot retire");threadRevoked=false;
 bool accepted=true;float threaded=99;std::string threadError;
 std::thread wrong([&]{accepted=factory->sourceDeltaTime(threaded,threadError);});wrong.join();
 check(!accepted&&threaded==99&&!threadError.empty(),"actual other OS thread cannot read Environment");
 for(auto c:{Change::Pointer,Change::Phase,Change::Serial,Change::Revision,Change::Campaign,Change::Session,Change::Visit,
  Change::Seed,Change::Layout,Change::Map,Change::Routes,Change::Selection,Change::Reentry}){
  context();sdkReads=0;sdkTarget=1;change=c;dt=99;
  check(!factory->sourceDeltaTime(dt,e)&&dt==99,"current SDK callback cannot publish stale dt");
 }
 cleanup();ready();p2retail::SceneRuntime::phase(*stage,p2retail::ScenePhase::Releasing);unavailable();
 check(factory->retireSourceSection(e)&&!factory->sourceSectionOwned(),"actual Releasing Stage supports checked scalar cleanup");
 for(auto c:{Change::Pointer,Change::Serial,Change::Revision,Change::Campaign,Change::Session,Change::Visit,
  Change::Seed,Change::Layout,Change::Map,Change::Routes,Change::Selection,Change::Reentry,Change::RetireReentry}){
  ready();check(p2retail::SceneRuntime::pause(e),"set real source pause state before retirement callback");
  worldAvailable=true;worldCallback=true;change=c;
  check(!factory->retireSourceSection(e),"World phase callback refusal BEFORE first source release");refresh();
  check(status.stage&&status.game&&status.bound&&status.clock,"phase callback preserves ALL retained continuation owners");
  context();check(factory->sourceGameSystemState(gs,e)&&gs.pauseCountdown==3&&gs.softPause,"phase callback never clears initialized real GameSystem state");cleanup();
 }
 std::cout<<checks<<" REAL Environment+GameSystem+Clock+SelectedSYSTEM+Parser controls passed; explicit Stage/SDK/physical census doubles, no runtime grant\n";
}
