#include "pc_p2_original_captain_body_borrower.h"
#include "pc_p2_retail_scene.h"
#include "Navi.h"
namespace p2original {namespace captain {namespace bodyphases {
namespace {bool fail(std::string& e,const char* s){e=s;return false;}}
bool BodyBorrowerGuard::capture(const p2retail::SceneContext& c,Navi* n,BodyBorrowerGuard& out,std::string& e){
 BodyBorrowerGuard next;next.context_=&c;next.scene_=pc_p2_original_captain_loaded_scene();next.world_=pc_p2_original_captain_world();
 next.bank_=pc_p2_original_captain_source_bank();next.phases_=pc_p2_original_captain_body_phase_owner(n);next.actor_=n;
 if(!n||!next.scene_||!next.world_||!next.bank_||!next.phases_||pc_p2_retail_scene_committed()!=&c)
  return fail(e,"source Navi trace capture lacks actual committed scene/body owner");
 next.slot_=next.scene_->captainAt(0)==n?0:next.scene_->captainAt(1)==n?1:2;
 next.serial_=next.scene_->incarnation();next.revision_=c.selectionRevision();
 next.campaign_=next.scene_->selectedCampaign();next.session_=next.scene_->selectedFingerprint();next.catalog_=next.scene_->sourceCatalog();
 next.state_=n->getCurrState();Fields fields;MotionState motion;
 if(!next.phases_->readFields(n,fields,e)||!next.bank_->state(n,motion,e))return false;
 next.birth_=fields.initializationSerial;next.motionGeneration_=motion.generation;
 if(!next.current(e))return false;
 out=std::move(next);return true;
}
bool BodyBorrowerGuard::current(std::string& e)const {
 // Authenticate canonical pointers before dereferencing retained borrows.
 if(!context_||!scene_||!world_||!bank_||!phases_||!actor_||slot_>1||!serial_||!birth_
  ||pc_p2_retail_scene_committed()!=context_||pc_p2_original_captain_loaded_scene()!=scene_
  ||pc_p2_original_captain_world()!=world_||pc_p2_original_captain_source_bank()!=bank_
  ||pc_p2_original_captain_body_phase_owner(actor_)!=phases_)
  return fail(e,"source Navi trace canonical scene/body borrower expired");
 if(world_->phase()!=Phase::GameWorldActive||scene_->incarnation()!=serial_||world_->incarnation()!=serial_
  ||scene_->selectedCampaign()!=campaign_||world_->selectedCampaign()!=campaign_
  ||scene_->selectedFingerprint()!=session_||world_->selectedFingerprint()!=session_
  ||scene_->sourceCatalog()!=catalog_||world_->sourceCatalog()!=catalog_
  ||scene_->captainAt(slot_)!=actor_||world_->captainAt(slot_)!=actor_
  ||context_->snapshot().scene.serial!=serial_||context_->selectionRevision()!=revision_
  ||context_->campaignSha256()!=campaign_||context_->sessionSha256()!=session_||actor_->getCurrState()!=state_)
  return fail(e,"source Navi trace changed actual scene/selection/roster/state");
 bool alive;Fields fields;MotionState motion;
 if(!pc_p2_original_captain_actor_lifetime(actor_,alive)||!phases_->readFields(actor_,fields,e)
  ||fields.initializationSerial!=birth_||!bank_->state(actor_,motion,e)||motion.generation!=motionGeneration_)
  return fail(e,"source Navi trace actual body/bank generation expired");
 e.clear();return true;
}
}}}
