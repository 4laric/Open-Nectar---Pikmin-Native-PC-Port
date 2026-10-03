#include "pc_p2_original_captain_dope.h"
#include "pc_p2_original_captain_states.h"
#include "pc_p2_original_captain_motion.h"
#include "pc_p2_original_resource_state.h"
#include <cmath>
#include <cstdio>
#include <climits>
#include <unordered_map>
extern p2original::captain::dope::DopeSource* pc_p2_original_captain_dope_source(const Navi*) __attribute__((weak));
extern p2original::captain::actions::ActionSource* pc_p2_original_captain_action_source(const Navi*) __attribute__((weak));
namespace p2original {namespace captain {namespace dope {namespace {
bool fail(std::string& e,const char* s){e=s;return false;}
bool valid(Spray t){return t==Spray::Spicy||t==Spray::Bitter;}
bool finite(Vec3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
bool same(Target a,Target b){return a.actor==b.actor&&a.lifetime==b.lifetime;}
Vec3 sub(Vec3 a,Vec3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
Vec3 add(Vec3 a,Vec3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
Vec3 scale(Vec3 a,float k){return {a.x*k,a.y*k,a.z*k};}
float length(Vec3 v){return std::sqrt(v.x*v.x+v.y*v.y+v.z*v.z);}
Vec3 normal(Vec3 v){float d=length(v);return d>0?scale(v,1/d):v;}
p2originalresource::HoneyKind kind(Spray s){return s==Spray::Spicy?p2originalresource::HoneyKind::Spicy:p2originalresource::HoneyKind::Bitter;}
struct Owners {DopeSource* source=nullptr;actions::ActionSource* action=nullptr;SourceBank* bank=nullptr;p2originalresource::ResourceState* inventory=nullptr;const LoadedScene* scene=nullptr;std::uint64_t epoch=0;};
bool owners(Navi* n,Owners& o,std::string& e){
 if(!n||!pc_p2_original_captain_dope_source||!pc_p2_original_captain_action_source)return fail(e,"missing actual source spray providers");
 auto* p=pc_p2_original_captain_dope_source(n);auto* a=pc_p2_original_captain_action_source(n);auto* s=pc_p2_original_captain_loaded_scene();auto* w=pc_p2_original_captain_world();auto* b=pc_p2_original_captain_source_bank();
 if(!p||!a||!s||!w||!b||&p->scene()!=s||&a->scene()!=s||!s->incarnation()||s->selectedCampaign().empty()||s->selectedFingerprint().empty()||s->sourceCatalog().empty()||w->incarnation()!=s->incarnation()||w->selectedCampaign()!=s->selectedCampaign()||w->selectedFingerprint()!=s->selectedFingerprint()||w->sourceCatalog()!=s->sourceCatalog()||!s->captainAt(0)||!s->captainAt(1)||s->captainAt(0)==s->captainAt(1)||(n!=s->captainAt(0)&&n!=s->captainAt(1))||w->phase()!=Phase::GameWorldActive||!pc_p2_original_captain_actor_alive(n))return fail(e,"spray provider is not canonical original scene/actor");
 auto* inventory=p->inventory();p2originalresource::ResourceSnapshot snapshot;MotionState motion;
 if(!inventory||!inventory->snapshot(snapshot,e)||!b->state(n,motion,e))return fail(e,"missing actual story spray inventory/actor bank");
 o={p,a,b,inventory,s,s->incarnation()};return true;
}
bool sameOwners(const Owners& a,const Owners& b){return a.source==b.source&&a.action==b.action&&a.bank==b.bank&&a.inventory==b.inventory&&a.scene==b.scene&&a.epoch==b.epoch;}
bool validFrame(const TargetFrame& f,std::string& e){return (f.handle.actor&&f.handle.lifetime&&finite(f.position))||fail(e,"missing actual source spray target lifetime/position");}
bool support(const Owners& o,Navi* n,std::string& e){return o.bank->supports(n,Motion::GrowUp2,e)&&o.bank->supports(n,Motion::Nigeru,e);}
struct Argument {Owners owner;Spray type=Spray::Spicy;};std::unordered_map<Navi*,Argument> arguments;
class DopeState final:public NativeState {
 Owners owner;Spray type=Spray::Spicy;bool enabled=false,used=false;std::string error;
 bool require(bool result){if(!result){enabled=false;if(error.empty())error="source Dope operation refused";std::fprintf(stderr,"P2_ORIGINAL_DOPE_REFUSED reason=%s\n",error.c_str());}return result;}
 bool live(Navi* n){if(!enabled||!n||n->getCurrState()!=static_cast<NaviState*>(this))return false;Owners current;if(!require(owners(n,current,error)))return false;return require(sameOwners(owner,current)||fail(error,"stale source Dope owner/incarnation"));}
 bool actor(Navi* n,actions::ActorFrame& out){return require(owner.action->frame(*n,out,error)&&((finite(out.position)&&finite(out.cursor))||fail(error,"missing source spray actor/whistle position")));}
 bool target(Target h,TargetFrame& out){return require(owner.source->target(h,out,error)&&validFrame(out,error)&&(same(h,out.handle)||fail(error,"stale source spray target")));}
 bool receive(Navi* n,Target h){bool accepted=false;return require(owner.source->stimulate(*n,h,type,accepted,error));}
 bool apply(Navi* n,Vec3 origin){
  if(type==Spray::Spicy){
   // applyDopes re-iterates the real CPlate after sound/effects, not the init list.
   std::vector<TargetFrame> squad;if(!require(owner.source->squad(*n,squad,error)))return false;
   for(const auto& candidate:squad){if(!require(validFrame(candidate,error)))return false;if(!candidate.piki)continue;
    if(!live(n))return false;
    TargetFrame current;if(!target(candidate.handle,current))return false;
    if(!require(current.piki||fail(error,"source CPlate Piki changed type"))||!receive(n,current.handle))return false;
   }return true;
  }
  std::vector<Target> candidates;if(!require(owner.source->mapSearch({origin,140},candidates,error)))return false;
  for(Target h:candidates){
   if(!h.actor||!h.lifetime)return require(fail(error,"missing source mapSearch callback lifetime"));
   if(!live(n))return false;
   actions::ActorFrame frame;TargetFrame current;
   // Actual applyDopeSmoke re-reads actor/whistle separately for EACH callback.
   if(!actor(n,frame)||!target(h,current))return false;
   Vec3 center=add(frame.position,scale(normal(sub(frame.cursor,frame.position)),50));
   if(length(sub(center,current.position))<=140&&!receive(n,current.handle))return false;
  }return true;
 }
public:
 DopeState():NativeState(StateId::Dope){}bool sourceInvincible()const override{return true;}
 void init(Navi* n)override{
  enabled=false;used=false;auto it=arguments.find(n);if(it==arguments.end()){error="Dope requires actual scoped NaviDopeArg";require(false);return;}
  const Argument arg=it->second;arguments.erase(it);if(!require(owners(n,owner,error)&&sameOwners(owner,arg.owner)&&valid(arg.type)&&support(owner,n,error)))return;
  type=arg.type;enabled=true;
  actions::ActorFrame frame;if(!actor(n,frame))return;std::vector<TargetFrame> squad;if(!require(owner.source->squad(*n,squad,error)))return;
  Vec3 direction;unsigned count=0;for(const auto& follower:squad){if(!require(validFrame(follower,error)))return;if(follower.piki){direction=add(direction,follower.position);++count;}}
  if((type==Spray::Spicy&&count==0)||owner.inventory->sprayCount(kind(type))<=0)return;
  p2originalresource::ResourceSnapshot snapshot;if(!require(owner.inventory->snapshot(snapshot,error)))return;
  if(snapshot.sprayUses[int(type)]==INT_MAX){require(fail(error,"source spray accounting counter overflow"));return;}
  if(!require(owner.bank->startMotion(n,Motion::GrowUp2,Motion::GrowUp2,Listener::SourceActor,Listener::None,error)&&owner.bank->enableMotionBlend(n,error)))return;
  Vec3 origin=frame.position;
  if(type==Spray::Bitter){direction=normal(sub(frame.cursor,frame.position));origin=add(frame.position,scale(direction,70));}
  else direction=normal(sub(scale(direction,1.0f/count),frame.position));
  if(!require((finite(direction)&&finite(origin))||fail(error,"source spray direction overflow")))return;
  if(!require(owner.source->smoke(*n,type,frame.position,direction,error)))return;
  used=true;
  if(!apply(n,origin))return;
  require(owner.inventory->useSpray(kind(type),error)); // exact after all receivers, irrespective of acceptance
 }
 void exec(Navi* n)override{
  if(!live(n)||!require(owner.action->control(*n,error)))return;
  MotionState state;if(!require(owner.bank->stateAnimator(n,Animator::Self,state,error)))return;
  if(!used||state.motion!=Motion::GrowUp2)require(pc_p2_original_captain_transit(n,StateId::Walk,error));
 }
 void cleanup(Navi*)override{enabled=false;} // source cleanup is empty
 bool sourceAnimationKey(Navi* n,int k,std::string& e)override{
  if(!live(n)){e=error.empty()?"source Dope key state is not current":error;return false;}
  if(used&&k==1000){if(!require(pc_p2_original_captain_transit(n,StateId::Walk,error))){e=error;return false;}e.clear();return false;}
  e.clear();return true;
 }
 bool advance(Navi* n,float frames,std::string& e){
  if(!live(n)){e=error;return false;}MotionState before;if(!require(owner.bank->stateAnimator(n,Animator::Bound,before,error))){e=error;return false;}
  auto emit=[&](int key){std::string local;bool keep=sourceAnimationKey(n,key,local);if(!keep&&!local.empty())error=local;return keep;};
  if(!require(owner.bank->advanceAnimator(n,Animator::Self,frames,emit,error))){e=error;return false;}
  if(n->getCurrState()!=static_cast<NaviState*>(this)){e.clear();return true;}
  MotionState now;if(!live(n)||!require(owner.bank->stateAnimator(n,Animator::Bound,now,error))){e=error;return false;}
  if(before.generation!=now.generation){e.clear();return true;}
  bool result=require(owner.bank->advanceAnimator(n,Animator::Bound,frames,emit,error));e=error;return result;
 }
};
} // anonymous
bool begin(Navi* n,Spray type,std::string& e){if(!valid(type))return fail(e,"unsupported source spray type");Owners o;if(!owners(n,o,e)||!support(o,n,e))return false;arguments[n]={o,type};if(!pc_p2_original_captain_transit(n,StateId::Dope,e)){arguments.erase(n);return false;}return true;}
} void registerDopeState(NaviStateMachine& fsm){fsm.registerState(new dope::DopeState);}
}}
bool pc_p2_original_captain_dope_preflight(Navi* n,std::string& e){using namespace p2original::captain::dope;Owners o;if(!owners(n,o,e)||!support(o,n,e))return false;auto arg=arguments.find(n);return (arg!=arguments.end()&&valid(arg->second.type)&&sameOwners(o,arg->second.owner))||fail(e,"missing scoped actual NaviDopeArg");}
bool pc_p2_original_captain_dope_advance_animation(Navi* n,float frames,std::string& e){using namespace p2original::captain::dope;if(!n||!std::isfinite(frames)||frames<0)return fail(e,"invalid source Dope animation amount");auto* s=dynamic_cast<DopeState*>(n->getCurrState());return s?s->advance(n,frames,e):fail(e,"source Dope is not current");}
