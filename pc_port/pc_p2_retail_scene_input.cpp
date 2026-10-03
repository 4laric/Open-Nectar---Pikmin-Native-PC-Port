#include "pc_p2_retail_scene_input.h"
#include "pc_randomizer.h"

bool pc_p2_retail_scene_selection(p2retail::SelectedSceneInputs& out,bool& present,std::string& error){
 using namespace p2retail;
 SelectedSceneInputs next;
 if(!pc_randomizer_original_session()||!(next.revision=pc_randomizer_original_selection_revision())){
  error="retail development selection requires actual OriginalSession";return false;
 }
 next.campaign=pc_randomizer_original_campaign();next.session=pc_randomizer_session_fingerprint();
 if(!hex64(next.campaign)||!hex64(next.session)){error="retail development selection identity";return false;}
 auto current=[&](){return pc_randomizer_original_session()&&
  pc_randomizer_original_selection_revision()==next.revision&&
  pc_randomizer_original_campaign()==next.campaign&&pc_randomizer_session_fingerprint()==next.session;};
 if(!pc_randomizer_original_has_input(developmentFloorRole)){
  if(!current()){error="retail development selection changed";return false;}
  present=false;error.clear();return true;
 }
 std::string framing;
 if(!pc_randomizer_original_input(developmentFloorRole,framing,error)||
    !parseDevelopmentFloor(framing,next.selection,error))return false;
 static constexpr std::size_t limits[]={1024*1024,64*1024*1024,4*1024*1024,64*1024,1024*1024,1024*1024,1024*1024,64*1024,64*1024};
 for(unsigned i=0;i<(next.selection.version==3?9u:next.selection.version==2?8u:6u);++i){
  const auto role=sceneInputRole(next.selection,static_cast<SceneInput>(i));
  if(!pc_randomizer_original_has_input(role)){error="retail development required selected role missing: "+role;return false;}
  if(!pc_randomizer_original_input(role,next.bytes[i],error))return false;
  unsigned char digest[32];pc_netplay_sha::sha256(next.bytes[i].data(),next.bytes[i].size(),digest);
  if(next.bytes[i].empty()||next.bytes[i].size()>limits[i]||pc_netplay_sha::hex(digest,32)!=next.selection.sha256[i]){
   error="retail development selected buffer size/digest: "+role;return false;
  }
  if(!current()){error="retail development selection changed";return false;}
 }
 if(!parseFloorPlan(next.bytes[0],next.selection.sha256[0],next.plan,error))return false;
 if(next.plan.cave!=next.selection.cave||next.plan.floor!=next.selection.floor||
    next.plan.geometrySha256!=next.selection.sha256[1]||next.plan.routesSha256!=next.selection.sha256[2]){
  error="retail development plan/input binding";return false;
 }
 if(!current()){error="retail development selection changed";return false;}
 out=std::move(next);present=true;error.clear();return true;
}
