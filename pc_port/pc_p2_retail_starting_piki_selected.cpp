#include "pc_p2_retail_starting_piki.h"
#include "pc_p2_retail_scene.h"
#include "pc_p2_retail_start.h"
#include "pc_randomizer.h"
#include "netplay/pc_netplay_sha256.h"
namespace p2retail { namespace {
std::string hash(const std::string& s){unsigned char d[32];pc_netplay_sha::sha256(s.data(),s.size(),d);std::string h;for(auto c:d){h+="0123456789abcdef"[c>>4];h+="0123456789abcdef"[c&15];}return h;}
}
bool selectedStartingPiki(const SceneContext& scene,StartingPikiInputs& out,std::string& e){
 const auto current=[&](){return pc_randomizer_original_session()&&pc_p2_retail_scene_prepared()==&scene&&scene.phase()==ScenePhase::Prepared&&scene.nativeSerial()!=0&&scene.selectionRevision()==pc_randomizer_original_selection_revision()&&scene.campaignSha256()==pc_randomizer_original_campaign()&&scene.sessionSha256()==pc_randomizer_session_fingerprint();};
 if(!current()){e="starting Piki actual prepared scene expired";return false;}
 std::string contract,manifest,provenance;StartingPikiInputs next;
 if(!pc_randomizer_original_input(startingPikiRole,contract,e)||!pc_randomizer_original_input(startingManifestRole,manifest,e)||!pc_randomizer_original_input(startingProvenanceRole,provenance,e)||!parseStartingPiki(contract,manifest,provenance,next,e))return false;
 SelectedSceneInputs selected;bool present=false;SourceStart start;
 if(!pc_p2_retail_scene_selection(selected,present,e)||!present||!parseSourceStart(selected,start,e))return false;
 if(next.manifest.campaign!=scene.campaignSha256()||selected.campaign!=scene.campaignSha256()||selected.session!=scene.sessionSha256()||selected.revision!=scene.selectionRevision()||next.cave!=scene.plan().cave||next.floor!=scene.plan().floor||next.planSha!=scene.plan().layoutSha256||next.planSha!=selected.plan.layoutSha256||next.startSha!=selected.selection.sha256[unsigned(SceneInput::Start)]){e="starting Piki selected scene bindings differ";return false;}
 if(!validateStartingPikiPlacement(next,start,e))return false;
 std::size_t total=0;for(const auto& member:next.bank){std::string bytes;if(!pc_randomizer_original_input(member.first,bytes,e))return false;if(bytes.empty()||bytes.size()>8*1024*1024||total>128*1024*1024-bytes.size()||hash(bytes)!=member.second){e="starting Piki selected bank bytes differ";return false;}total+=bytes.size();}
 if(!current()){e="starting Piki scene changed during selected reads";return false;}out=std::move(next);e.clear();return true;
}
}
