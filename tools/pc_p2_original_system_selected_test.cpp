#include "pc_p2_original_system_selected.h"
#include "pc_p2_retail_scene.h"
#include "pc_randomizer.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <thread>
#include <tuple>
#include <vector>
using namespace p2original;
namespace {
unsigned checks=0;
void check(bool value,const char* why){++checks;if(!value){std::cerr<<why<<'\n';std::exit(1);}}
const std::string campaign(64,'a'),session(64,'b'),layout(64,'c');
const char* aiRole="p2-original/system/aiConstants.txt";
const char* timeRole="p2-original/system/time.ini";
std::string aiBytes,timeBytes,currentCampaign,currentSession;
std::uint64_t revision=3;
bool original=true,threadRevoked=false,missingAI=false,missingTime=false,authRefusal=false;
p2retail::SceneContext* stage=nullptr;
const p2retail::SceneContext* prepared=nullptr;
std::thread::id creatingThread;
std::vector<std::string> reads;
unsigned boundary=0,target=0;
enum class Change {None,Pointer,Thread,Phase,Serial,Revision,Campaign,Session,Map,Stage,Routes,
 Layout,Visit,Seed,SelectionRevision,SelectionCampaign,SelectionSession,Original,Reentry,Exception};
Change change=Change::None;
SelectedSystemParameters sentinel(){SelectedSystemParameters p;
 p.ai={91,92,93,94};p.time={1,2,3,4,5,6,7,8,9,10,11,12};return p;}
auto fields(const SelectedSystemParameters& p){return std::tie(p.ai.gravity,p.ai.dopeCount,p.ai.debt,p.ai.cameraAngle,
 p.time.dayStart,p.time.dayEnd,p.time.dayLengthSeconds,p.time.morningStart,p.time.midMorning,p.time.morningEnd,
 p.time.eveningStart,p.time.midEveningStart,p.time.midEveningEnd,p.time.eveningEnd,p.time.sundownAlert,p.time.countdown);}
void callback();
}
namespace p2retail {
class SceneRuntime {public:
 static SceneContext* make(){return new SceneContext;}
 static void reset(SceneContext& s){
  s.mStage=reinterpret_cast<StageInfo*>(0x2000);s.mMap=reinterpret_cast<MapMgr*>(0x3000);
  s.mRoutes=reinterpret_cast<RouteMgr*>(0x4000);s.mPhase=ScenePhase::Prepared;
  s.mSnapshot.scene={session,"visit-actual-context-test",layout,8};s.mPlan.layoutSha256=layout;
  s.mRevision=3;s.mCampaign=campaign;s.mSession=session;
 }
 static void mutate(SceneContext& s,Change c){switch(c){
  case Change::Phase:s.mPhase=ScenePhase::Installing;break;
  case Change::Serial:++s.mSnapshot.scene.serial;break;
  case Change::Revision:++s.mRevision;break;
  case Change::Campaign:s.mCampaign=std::string(64,'d');break;
  case Change::Session:s.mSession=std::string(64,'d');break;
  case Change::Map:s.mMap=reinterpret_cast<MapMgr*>(0x9000);break;
  case Change::Stage:s.mStage=reinterpret_cast<StageInfo*>(0x9000);break;
  case Change::Routes:s.mRoutes=nullptr;break;
  case Change::Layout:s.mPlan.layoutSha256=std::string(64,'d');break;
  case Change::Visit:s.mSnapshot.scene.visit+="changed";break;
  case Change::Seed:s.mSnapshot.scene.seed=std::string(64,'d');break;
  default:break;
 }}
 static void phase(SceneContext& s,ScenePhase p){s.mPhase=p;}
 static void noStage(SceneContext& s){s.mStage=nullptr;}
 static void noMap(SceneContext& s){s.mMap=nullptr;}
 static void noSerial(SceneContext& s){s.mSnapshot.scene.serial=0;}
 static void noRevision(SceneContext& s){s.mRevision=0;}
 static void noVisit(SceneContext& s){s.mSnapshot.scene.visit.clear();}
 static void badHash(SceneContext& s){s.mCampaign="invalid";}
};
bool SceneContext::ownsCurrentThread()const noexcept{return !threadRevoked&&std::this_thread::get_id()==creatingThread;}
}
const p2retail::SceneContext* pc_p2_retail_scene_prepared()noexcept{return prepared;}
bool pc_randomizer_original_session(){return original;}
std::string pc_randomizer_original_campaign(){return currentCampaign;}
std::string pc_randomizer_session_fingerprint(){return currentSession;}
std::uint64_t pc_randomizer_original_selection_revision()noexcept{return revision;}
bool pc_randomizer_original_input(const std::string& role,std::string& out,std::string& e){
 reads.push_back(role);
 if(authRefusal||(role==aiRole&&missingAI)||(role==timeRole&&missingTime)){
  e="test actual original_input authentication/role refusal";return false;
 }
 if(role==aiRole)out=aiBytes;else if(role==timeRole)out=timeBytes;
 else {e="unexpected noncanonical selected role";return false;}
 callback();e.clear();return true;
}
namespace p2original {
// Compile the unchanged real parser with only these two exported symbol names
// renamed. Interposition tests each parse boundary; production has no callbacks.
bool parseAIConstantsParametersRaw(const std::string&,AIConstantsParameters&,std::string&);
bool parseTimeParametersRaw(const std::string&,TimeParameters&,std::string&);
bool parseAIConstantsParameters(const std::string& b,AIConstantsParameters& out,std::string& e){
 const bool result=parseAIConstantsParametersRaw(b,out,e);callback();return result;
}
bool parseTimeParameters(const std::string& b,TimeParameters& out,std::string& e){
 const bool result=parseTimeParametersRaw(b,out,e);callback();return result;
}
}
namespace {
void callback(){if(++boundary!=target)return;
 switch(change){
  case Change::Pointer:prepared=nullptr;break;
  case Change::Thread:threadRevoked=true;break;
  case Change::SelectionRevision:++revision;break;
  case Change::SelectionCampaign:currentCampaign=std::string(64,'d');break;
  case Change::SelectionSession:currentSession=std::string(64,'d');break;
  case Change::Original:original=false;break;
  case Change::Exception:throw std::runtime_error("actual input/parse fault control");
  case Change::Reentry:{auto out=sentinel();std::string e;
   check(!readSelectedSystemParameters(*stage,out,e)&&fields(out)==fields(sentinel())&&!e.empty(),"nested selected read refused unchanged");break;}
  default:p2retail::SceneRuntime::mutate(*stage,change);break;
 }
}
void reset(){p2retail::SceneRuntime::reset(*stage);prepared=stage;creatingThread=std::this_thread::get_id();
 currentCampaign=campaign;currentSession=session;revision=3;original=true;threadRevoked=false;
 missingAI=false;missingTime=false;authRefusal=false;reads.clear();target=0;boundary=0;change=Change::None;}
void refused(){auto out=sentinel();std::string e;
 check(!readSelectedSystemParameters(*stage,out,e)&&fields(out)==fields(sentinel())&&!e.empty(),"selected refusal leaves entire pair unchanged");}
std::string read(const char* path){std::ifstream in(path,std::ios::binary);check(bool(in),"staged source bytes readable");return {std::istreambuf_iterator<char>(in),{}};}
}
int main(int argc,char** argv){
 check(argc==3,"actual staged AI and Time byte paths required");aiBytes=read(argv[1]);timeBytes=read(argv[2]);
 const auto ai=aiBytes,time=timeBytes;
 std::unique_ptr<p2retail::SceneContext> owned(p2retail::SceneRuntime::make());stage=owned.get();reset();
 SelectedSystemParameters out;std::string e="previous refusal";
 check(readSelectedSystemParameters(*stage,out,e)&&e.empty()&&out.ai.gravity==560&&out.ai.dopeCount==10
  &&out.time.dayLengthSeconds==1560&&out.time.midEveningStart==16.5f&&out.time.midEveningEnd==17.5f,"authenticated canonical pair parses actual retail bytes");
 check(reads==std::vector<std::string>{aiRole,timeRole}&&boundary==4,"exact two roles and four read/parse boundaries");
 reset();missingAI=true;refused();check(reads==std::vector<std::string>{aiRole},"missing AI never defaults or reads alternate role");
 reset();missingTime=true;refused();check(reads==std::vector<std::string>{aiRole,timeRole},"missing Time never publishes AI alone");
 reset();authRefusal=true;refused();check(boundary==0,"authentication refusal never parses unauthenticated bytes");
 reset();aiBytes="end";refused();check(boundary==2&&reads.size()==1,"AI parser refusal stops before Time read");aiBytes=ai;
 reset();timeBytes="{_eof}";refused();check(boundary==4,"Time parser refusal cannot publish prior AI");timeBytes=time;
 for(unsigned at=1;at<=4;++at)for(auto c:{Change::Pointer,Change::Thread,Change::Phase,Change::Serial,
  Change::Revision,Change::Campaign,Change::Session,Change::Map,Change::Stage,Change::Routes,
  Change::Layout,Change::Visit,Change::Seed,Change::SelectionRevision,Change::SelectionCampaign,
  Change::SelectionSession,Change::Original,Change::Reentry,Change::Exception}){
  reset();target=at;change=c;refused();check(boundary==at,"revocation caught immediately after EACH reader/parse");
 }
 reset();prepared=nullptr;refused();check(reads.empty(),"expired pointer checked before borrowed Stage dereference");
 reset();std::unique_ptr<p2retail::SceneContext> foreign(p2retail::SceneRuntime::make());prepared=foreign.get();refused();
 reset();threadRevoked=true;refused();reset();original=false;refused();reset();revision=0;refused();
 reset();currentCampaign=std::string(64,'d');refused();reset();currentSession=std::string(64,'d');refused();
 reset();p2retail::SceneRuntime::noStage(*stage);refused();reset();p2retail::SceneRuntime::noMap(*stage);refused();
 reset();p2retail::SceneRuntime::noSerial(*stage);refused();reset();p2retail::SceneRuntime::noRevision(*stage);refused();
 reset();p2retail::SceneRuntime::noVisit(*stage);refused();reset();p2retail::SceneRuntime::badHash(*stage);refused();
 reset();p2retail::SceneRuntime::phase(*stage,p2retail::ScenePhase::Releasing);refused();
 reset();p2retail::SceneRuntime::phase(*stage,static_cast<p2retail::ScenePhase>(99));refused();
 reset();auto threaded=sentinel();bool accepted=true;std::string threadError;
 std::thread wrong([&]{accepted=readSelectedSystemParameters(*stage,threaded,threadError);});wrong.join();
 check(!accepted&&fields(threaded)==fields(sentinel())&&!threadError.empty()&&reads.empty(),"actual other OS thread cannot read creating-thread Stage");
 for(auto p:{p2retail::ScenePhase::Prepared,p2retail::ScenePhase::Installing,p2retail::ScenePhase::Committed}){
  reset();p2retail::SceneRuntime::phase(*stage,p);e="stale diagnostic";
  check(readSelectedSystemParameters(*stage,out,e)&&e.empty(),"resource read across actual non-releasing phases grants no activity");
 }
 reset();target=4;change=Change::Reentry;refused();reset();e="stale refusal";
 check(readSelectedSystemParameters(*stage,out,e)&&e.empty(),"success after refusal clears transaction poison and diagnostic");
 std::cout<<checks<<" selected SYSTEM controls passed; synthetic Stage/authentication boundaries only, no runtime grant\n";
}
