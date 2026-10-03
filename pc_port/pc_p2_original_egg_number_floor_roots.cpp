#include "pc_p2_original_egg_number_floor_roots.h"
#include "pc_p2_original_catalog.h"

namespace p2originalresource {
NumberFloorRoots::NumberFloorRoots(const p2retail::SceneContext& owner)
 :mOwner(&owner),mFloor(owner.snapshot()),mCampaign(owner.campaignSha256()),
  mSession(owner.sessionSha256()),mRevision(owner.selectionRevision()),mSerial(owner.nativeSerial()){}

bool NumberFloorRoots::selected()const noexcept{
 const auto* current=pc_p2_retail_scene_committed();
 // Compare before dereference: revocation/owner replacement denies the borrower.
 return current&&current==mOwner&&current->phase()==p2retail::ScenePhase::Committed&&
  mSerial&&mRevision&&current->nativeSerial()==mSerial&&current->selectionRevision()==mRevision&&
  current->campaignSha256()==mCampaign&&current->sessionSha256()==mSession&&
  current->snapshot().scene==mFloor.scene;
}

bool NumberFloorRoots::resolve(const SourceIdentity& root,unsigned& out,std::string& error)const{
 if(!selected()||!p2retail::hex64(mCampaign)||!p2retail::hex64(mSession)||
    !mFloor.inCave||!root.epoch||!root.activation||root.fingerprint!=mFloor.scene.layoutSha256){
  error="numeric floor root selected incarnation differs";return false;
 }
 const auto* cave=p2retail::descriptor(mFloor.cave);
 const auto* definition=cave?p2retail::definition(*cave,mFloor.floor):nullptr;
 const auto& plan=mOwner->plan();
 if(!definition||mFloor.source!=cave->source||mFloor.sourceSha256!=cave->sourceSha256||
    mFloor.catalogSha256!=cave->catalogSha256||mFloor.maxFloor!=cave->maxFloor||
    plan.cave!=mFloor.cave||plan.floor!=mFloor.floor||plan.sourceSha256!=cave->sourceSha256||
    plan.catalogSha256!=cave->catalogSha256||plan.layoutSha256!=root.fingerprint){
  error="numeric floor root immutable definition differs";return false;
 }
 p2retail::BirthIdentity birth;p2retail::Snapshot actual;
 const p2original::InstanceIdentity identity{root.fingerprint,root.uid,root.ordinal,root.epoch,root.activation};
 if(!pc_p2_retail_scene_known_source_birth(identity,birth,actual,error))return false;
 if(!(actual.scene==mFloor.scene)||actual.cave!=mFloor.cave||actual.floor!=mFloor.floor||
    actual.source!=cave->source||actual.sourceSha256!=cave->sourceSha256||
    actual.catalogSha256!=cave->catalogSha256||actual.maxFloor!=cave->maxFloor||
    actual.story!=mFloor.story||!actual.inCave||birth.row>=definition->rows.size()){
  error="numeric floor root retained floor differs";return false;
 }
 const auto& row=definition->rows[birth.row];
 if((row.kind!="enemy"&&row.kind!="cap_enemy")||(row.sourceId!=16&&row.sourceId!=37)||
    birth.ordinal!=root.ordinal||birth.ordinal>=row.minimum()||birth.epoch!=root.epoch||
    birth.activation!=root.activation||birth.instance!=p2retail::instanceKey(*cave,mFloor.floor,row,root.ordinal)||
    !selected()){
  error="numeric floor root retained source differs";return false;
 }
 out=static_cast<unsigned>(row.sourceId);error.clear();return true;
}
}
