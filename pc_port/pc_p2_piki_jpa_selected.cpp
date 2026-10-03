#include "pc_p2_piki_jpa_selected.h"
#include "pc_p2_original_captain_damage.h"
#include "pc_p2_retail_scene.h"
#include "pc_randomizer.h"
#include <utility>
namespace p2original {namespace pikiJPA {
namespace {
bool fail(std::string& e,const char* s){e=s;return false;}
bool selected(const p2retail::SceneContext& s){
 const auto* actual=pc_p2_retail_scene_prepared();
 return actual==&s&&actual&&pc_randomizer_original_session()&&s.nativeSerial()&&s.selectionRevision()&&
  s.campaignSha256()==pc_randomizer_original_campaign()&&s.sessionSha256()==pc_randomizer_session_fingerprint()&&
  s.selectionRevision()==pc_randomizer_original_selection_revision();
}
void field(std::string& out,const std::string& value){out+=std::to_string(value.size())+":"+value;}
// Lossless base64url representation of a checked32-byte digest, no rehash.
std::string compactDigest(const std::string& hex){
 const char* alphabet="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
 const auto nibble=[](char c){return unsigned(c<='9'?c-'0':c-'a'+10);};
 unsigned accumulator=0,bits=0;std::string result;result.reserve(43);
 for(unsigned i=0;i<64;i+=2){accumulator=(accumulator<<8)|(nibble(hex[i])<<4)|nibble(hex[i+1]);bits+=8;
  while(bits>=6){bits-=6;result+=alphabet[(accumulator>>bits)&63];}}
 if(bits){result+=alphabet[(accumulator<<(6-bits))&63];}
 return result;
}
}
bool selectedIdentity(const p2retail::SceneContext& scene,SelectedIdentity& out,std::string& e){
 if(!selected(scene))return fail(e,"Piki JPA actual selected scene missing or expired");
 const auto& identity=scene.snapshot().scene;
 if(!p2retail::hex64(scene.campaignSha256())||!p2retail::hex64(scene.sessionSha256())||
    identity.seed!=scene.sessionSha256()||identity.visit.empty()||
    identity.layoutSha256!=scene.plan().layoutSha256||!p2retail::hex64(identity.layoutSha256)||
    identity.serial!=scene.nativeSerial())return fail(e,"Piki JPA full native session identity differs");
 SelectedIdentity next{scene.campaignSha256(),scene.sessionSha256(),"P2JPA_SESSION_2:"};
 field(next.session,compactDigest(identity.seed));field(next.session,identity.visit);field(next.session,compactDigest(identity.layoutSha256));
 field(next.session,std::to_string(identity.serial));field(next.session,std::to_string(scene.selectionRevision()));
 if(next.session.size()>256)return fail(e,"Piki JPA full native session exceeds bank bound");
 if(!selected(scene))return fail(e,"Piki JPA actual selection changed during identity read");
 out=std::move(next);e.clear();return true;
}
bool selectedBank(const p2retail::SceneContext& scene,Bank& bank,std::string& provenance,std::string& e){
 SelectedIdentity before;if(!selectedIdentity(scene,before,e))return false;
 std::string retained;
 if(!pc_randomizer_original_input("p2-original/piki-jpa-provenance.json",retained,e))return false;
 // Immutable selected descriptive provenance; source authority is the16
 // fixed raw hashes validated independently by Bank, not this JSON metadata.
 if(retained.empty()||retained.size()>65536)return fail(e,"Piki JPA selected provenance size differs");
 std::size_t count=0;const auto* roles=resourceRoles(count);
 if(count!=16)return fail(e,"Piki JPA selected source role census differs");
 std::vector<SelectedBytes> inputs;inputs.reserve(count);
 for(std::size_t i=0;i<count;++i){std::string bytes;
  if(!pc_randomizer_original_input(std::string("p2-original/piki-jpa/")+roles[i].role,bytes,e))return false;
  if(bytes.size()!=roles[i].bytes)return fail(e,"Piki JPA selected source role size differs");
  SelectedBytes input;input.role=roles[i].role;input.archiveMember=sourceArchive;input.archiveSHA=sourceArchiveSHA;
  input.memberOffset=roles[i].offset;input.memberBytes=roles[i].bytes;input.bytes.assign(bytes.begin(),bytes.end());
  inputs.push_back(std::move(input));
 }
 SelectedIdentity after;if(!selectedIdentity(scene,after,e))return false;
 if(before.campaignSHA!=after.campaignSHA||before.packetSHA!=after.packetSHA||before.session!=after.session)
  return fail(e,"Piki JPA selected identity changed during source reads");
 // Stage separately so a callback/refusal never overwrites the previous bank.
 Bank next;if(!next.load(before,inputs,e))return false;
 if(!selectedIdentity(scene,after,e)||before.campaignSHA!=after.campaignSHA||
    before.packetSHA!=after.packetSHA||before.session!=after.session)
  return fail(e,"Piki JPA selected identity changed before publication");
 bank=std::move(next);provenance=std::move(retained);e.clear();return true;
}
bool currentIdentity(const p2retail::SceneContext& scene,const captain::LoadedScene& descriptor,
 const SelectedIdentity& expected,std::string& e){
 const auto* actual=pc_p2_original_captain_loaded_scene();
 if(actual!=&descriptor||!actual)return fail(e,"Piki JPA canonical captain descriptor unavailable");
 SelectedIdentity current;if(!selectedIdentity(scene,current,e))return false;
 if(descriptor.selectedCampaign()!=current.campaignSHA||descriptor.selectedFingerprint()!=current.packetSHA||
    descriptor.incarnation()!=scene.nativeSerial()||descriptor.sourceCatalog()!=scene.plan().layoutSha256||
    !descriptor.captainAt(0)||!descriptor.captainAt(1)||descriptor.captainAt(0)==descriptor.captainAt(1)||
    expected.campaignSHA!=current.campaignSHA||expected.packetSHA!=current.packetSHA||expected.session!=current.session)
  return fail(e,"Piki JPA retained descriptor or full session differs");
 e.clear();return true;
}
} }
