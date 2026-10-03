#include "pc_p2_original_system_selected.h"
#include "pc_p2_retail_scene.h"
#include "pc_randomizer.h"
#include <exception>
#include <thread>
namespace p2original {
namespace {
bool fail(std::string& e,const char* why){e=why;return false;}
struct Transaction;
thread_local Transaction* transaction=nullptr;
struct Transaction {
 bool entered=false,reentered=false;
 Transaction(){if(transaction)transaction->reentered=true;else{transaction=this;entered=true;}}
 ~Transaction(){if(entered)transaction=nullptr;}
};
struct Pin {
 std::thread::id thread;
 StageInfo* stage=nullptr;MapMgr* map=nullptr;RouteMgr* routes=nullptr;
 p2retail::ScenePhase phase=p2retail::ScenePhase::Prepared;
 std::uint64_t serial=0,revision=0;
 std::string campaign,session,layout;
 p2retail::SceneIdentity identity;
};
bool phase(p2retail::ScenePhase p){
 return p==p2retail::ScenePhase::Prepared||p==p2retail::ScenePhase::Installing
  ||p==p2retail::ScenePhase::Committed;
}
bool exact(const p2retail::SceneContext& s,const Pin& pin,std::string& e){
 // Compare the live pointer before dereferencing the borrowed Stage. Selection
 // readers can fail/revoke; collect their answers before the final Stage check.
 if(!transaction||transaction->reentered||std::this_thread::get_id()!=pin.thread
  ||pc_p2_retail_scene_prepared()!=&s)return fail(e,"SYSTEM actual Stage/thread expired or reader reentered");
 const bool original=pc_randomizer_original_session();
 const auto campaign=pc_randomizer_original_campaign();
 const auto session=pc_randomizer_session_fingerprint();
 const auto revision=pc_randomizer_original_selection_revision();
 if(transaction->reentered||pc_p2_retail_scene_prepared()!=&s)
  return fail(e,"SYSTEM actual Stage changed during selection read");
 if(!original||campaign!=pin.campaign||session!=pin.session||revision!=pin.revision
  ||!s.ownsCurrentThread()||s.stage()!=pin.stage||s.map()!=pin.map||s.routes()!=pin.routes
  ||s.phase()!=pin.phase||s.nativeSerial()!=pin.serial||s.selectionRevision()!=pin.revision
  ||s.campaignSha256()!=pin.campaign||s.sessionSha256()!=pin.session
  ||s.plan().layoutSha256!=pin.layout||!(s.snapshot().scene==pin.identity))
  return fail(e,"SYSTEM retained Stage/full selected identity differs");
 return true;
}
bool pinStage(const p2retail::SceneContext& s,Pin& pin,std::string& e){
 if(pc_p2_retail_scene_prepared()!=&s)return fail(e,"SYSTEM requires actual prepared Stage");
 if(!s.ownsCurrentThread()||!s.stage()||!s.map()||!phase(s.phase())
  ||!s.nativeSerial()||!s.selectionRevision())return fail(e,"SYSTEM actual Stage lifetime unavailable");
 pin.thread=std::this_thread::get_id();pin.stage=s.stage();pin.map=s.map();pin.routes=s.routes();
 pin.phase=s.phase();pin.serial=s.nativeSerial();pin.revision=s.selectionRevision();
 pin.campaign=s.campaignSha256();pin.session=s.sessionSha256();pin.layout=s.plan().layoutSha256;
 pin.identity=s.snapshot().scene;
 if(!p2retail::hex64(pin.campaign)||!p2retail::hex64(pin.session)||!p2retail::hex64(pin.layout)
  ||pin.identity.seed!=pin.session||pin.identity.visit.empty()||pin.identity.serial!=pin.serial
  ||pin.identity.layoutSha256!=pin.layout)return fail(e,"SYSTEM full native scene identity invalid");
 return exact(s,pin,e);
}
}
bool readSelectedSystemParameters(const p2retail::SceneContext& scene,
 SelectedSystemParameters& out,std::string& e){
 Transaction guard;
 if(!guard.entered)return fail(e,"SYSTEM reentrant selected reader refused");
 try{
  Pin pin;if(!pinStage(scene,pin,e))return false;
  SelectedSystemParameters next;std::string bytes;
  if(!pc_randomizer_original_input("p2-original/system/aiConstants.txt",bytes,e))return false;
  if(!exact(scene,pin,e))return false;
  if(!parseAIConstantsParameters(bytes,next.ai,e))return false;
  if(!exact(scene,pin,e))return false;
  bytes.clear();
  if(!pc_randomizer_original_input("p2-original/system/time.ini",bytes,e))return false;
  if(!exact(scene,pin,e))return false;
  if(!parseTimeParameters(bytes,next.time,e))return false;
  if(!exact(scene,pin,e))return false;
  out=next;e.clear();return true;
 }catch(const std::exception& x){
  e=std::string("SYSTEM selected reader exception: ")+x.what();return false;
 }
}
}
