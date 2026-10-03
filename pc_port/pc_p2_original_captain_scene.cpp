#include "pc_p2_original_captain_scene.h"
#include "pc_p2_original_captain_damage.h"
#include "pc_p2_original_captain_motion.h"
#include "pc_p2_original_captain_native_control.h"
#include "pc_p2_original_captain_states.h"
#include "pc_p2_retail_scene.h"
#include "pc_p2_retail_start.h"
#include "pc_randomizer.h"
#include "Navi.h"
#include "NaviMgr.h"
#include "gameflow.h"
#include <array>
#include <memory>

namespace {
using namespace p2original::captain;
bool fail(std::string& error,const char* reason){error=reason;return false;}
class CaptainScene final : public LoadedScene {
public:
 const p2retail::SceneContext* context;
 NaviMgr* manager;
 std::array<Navi*,2> actors;
 std::uint64_t serial,revision;
 std::string campaign,session,catalog;
 MoviePlayer* player;
 std::unique_ptr<SourceBank> bank;
 bool published=false;
 CaptainScene(const p2retail::SceneContext& scene,std::unique_ptr<SourceBank> source)
  :context(&scene),manager(naviMgr),actors{naviMgr->getNavi(0),naviMgr->getNavi(1)},
   serial(scene.nativeSerial()),revision(scene.selectionRevision()),
   campaign(scene.campaignSha256()),session(scene.sessionSha256()),
   catalog(scene.plan().layoutSha256),player(gameflow.mMoviePlayer),bank(std::move(source)){}
 const std::string& selectedCampaign()const override{return campaign;}
 const std::string& selectedFingerprint()const override{return session;}
 const std::string& sourceCatalog()const override{return catalog;}
 MoviePlayer* moviePlayer()const override{return player;}
 std::uint64_t incarnation()const override{return serial;}
 Navi* captainAt(unsigned slot)const override{return slot<2?actors[slot]:nullptr;}
 bool rosterCurrent()const {
  return naviMgr==manager&&manager&&manager->getNaviCount()==2&&
   manager->getNavi(0)==actors[0]&&manager->getNavi(1)==actors[1];
 }
 bool current()const {
  // Test the owner's canonical pointer before dereferencing its borrowed data.
  const auto* scene=pc_p2_retail_scene_prepared();
  return published&&scene==context&&scene&&scene->nativeSerial()==serial&&
   scene->selectionRevision()==revision&&scene->phase()!=p2retail::ScenePhase::Releasing&&
   rosterCurrent()&&bank->ready()&&gameflow.mMoviePlayer==player;
 }
 bool owns(const p2retail::SceneContext& scene)const noexcept {
  // Selection expiry must not conceal a still-owned collision/control bank.
  return context==&scene&&scene.nativeSerial()==serial&&scene.selectionRevision()==revision;
 }
};
// Native roster pointers cannot be touched by a static destructor after the
// stage heap has gone away. Explicit retirement owns deletion; an interrupted
// process retains at most this one scene until process exit.
CaptainScene* owner=nullptr;
}

const p2original::captain::LoadedScene* pc_p2_original_captain_loaded_scene(){
 return owner&&owner->current()?owner:nullptr;
}
p2original::captain::SourceBank* pc_p2_original_captain_source_bank(){
 return owner&&owner->current()?owner->bank.get():nullptr;
}
bool pc_p2_original_captain_scene_reset(std::string& error){
 if(owner)return fail(error,"source captain previous scene still owns its native bank");
 const auto* scene=pc_p2_retail_scene_prepared();
 if(!scene||!scene->startsGrounded()||!scene->nativeSerial()||
    scene->phase()!=p2retail::ScenePhase::Prepared||!naviMgr||naviMgr->getNaviCount()!=2)
  return fail(error,"source captain reset requires actual prepared stage and two native bodies");
 auto* first=naviMgr->getNavi(0);auto* second=naviMgr->getNavi(1);
 if(!first||!second||first==second||scene->campaignSha256().empty()||
    scene->sessionSha256().empty()||scene->plan().layoutSha256.empty())
  return fail(error,"source captain selected stage/roster identity missing");
 auto bank=std::make_unique<SourceBank>();
 if(!bank->prepare(error)||!bank->prepareNativeModels(error))return false;
 if(pc_p2_retail_scene_prepared()!=scene)return fail(error,"source stage changed during captain resource preparation");
 auto next=std::make_unique<CaptainScene>(*scene,std::move(bank));
 // Retain ownership before the first fallible native reset/bind. Refusal leaves
 // a cleanup owner and can never be retried as another fresh reset.
 owner=next.release();
 const auto& base=scene->captainStartBase();
 for(unsigned slot=0;slot<2;++slot){auto* actor=owner->actors[slot];
  actor->mSRT.t.set(base[0]+float(p2retail::SourceStart::captainX[slot]),base[1],
                   base[2]+float(p2retail::SourceStart::captainZ[slot]));
  actor->mLastPosition=actor->mDayEndPosition=actor->mSRT.t;
  actor->mFaceDirection=scene->mapYaw();actor->mSRT.r.set(0,actor->mFaceDirection,0);
  actor->reset();actor->mIsCursorVisible=TRUE;
 }
 if(!owner->rosterCurrent()||pc_p2_retail_scene_prepared()!=scene)
  return fail(error,"source stage/roster changed during actual captain resets");
 if(!owner->bank->bindRoster(first,second,error))return false;
 owner->published=true;
 return pc_p2_original_captain_body_reset_loaded(error);
}
bool pc_p2_original_captain_scene_activate(std::string& error){
 if(!owner||!owner->current())return fail(error,"source captain activation lacks current bound scene");
 // A real committed physical floor is required separately from bank readiness.
 if(pc_p2_retail_scene_committed()!=owner->context)
  return fail(error,"source captain activation requires committed physical retail floor");
 for(auto* actor:owner->actors)
  if(!nativecontrol::resetAfterBootstrap(actor,error))return false;
 if(!pc_p2_original_captain_bootstrap_roster(error))return false;
 return pc_p2_original_captain_activate_after_bootstrap(error);
}
bool pc_p2_original_captain_scene_owned(const p2retail::SceneContext& scene)noexcept{
 return owner&&owner->owns(scene);
}
bool pc_p2_original_captain_scene_can_retire(const p2retail::SceneContext& scene,std::string& error){
 if(!owner){error.clear();return true;}
 if(!owner->owns(scene)||!owner->rosterCurrent())
  return fail(error,"source captain retirement must precede scene or native roster reuse");
 error.clear();return true;
}
bool pc_p2_original_captain_scene_retire(const p2retail::SceneContext& scene,std::string& error){
 if(!pc_p2_original_captain_scene_can_retire(scene,error))return false;
 if(!owner)return true;
 pc_p2_original_captain_main_game_left();
 owner->published=false;
 // Force the actual lifecycle owner to observe canonical revocation now.
 (void)pc_p2_original_captain_world();
 for(auto* actor:owner->actors){nativecontrol::forget(actor);owner->bank->forget(actor);}
 delete owner;owner=nullptr;error.clear();return true;
}
