#include "pc_p2_original_captain_punch.h"
#include "pc_p2_original_captain_states.h"
#include "pc_p2_original_captain_motion.h"
#include "pc_p2_equipment.h"
#include <cmath>
#include <cstdio>
#include <unordered_map>

extern p2original::captain::punch::PunchSource* pc_p2_original_captain_punch_source(const Navi*) __attribute__((weak));
extern p2original::captain::actions::ActionSource* pc_p2_original_captain_action_source(const Navi*) __attribute__((weak));
extern bool pc_p2_original_captain_motion_preflight(Navi*,unsigned,std::string&) __attribute__((weak));
namespace p2original { namespace captain { namespace punch {
namespace {
bool finite(Vec3 p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);}
bool fail(std::string& e,const char* text){e=text;return false;}
}
bool hitSphere(const actions::ActorFrame& frame,bool third,Sphere& out,std::string& e){
 if(!finite(frame.position)||!std::isfinite(frame.face))return fail(e,"missing finite source punch position/face");
 const float reach=third?25.0f:15.0f,radius=third?35.0f:20.0f;
 out={{frame.position.x+reach*std::sin(frame.face),frame.position.y+radius,frame.position.z+reach*std::cos(frame.face)},radius};return true;
}
bool effectPosition(Vec3 part,Vec3 hand,Vec3& out,std::string& e){
 if(!finite(part)||!finite(hand))return fail(e,"missing finite source collision-part/hand position");
 Vec3 v{part.x-hand.x,part.y-hand.y,part.z-hand.z};
 float length=std::sqrt(v.x*v.x+v.y*v.y+v.z*v.z);
 if(length>0){v.x/=length;v.y/=length;v.z/=length;}
 out={hand.x+v.x*15,hand.y+v.y*15,hand.z+v.z*15};return true;
}
namespace {
struct Providers {PunchSource* punch=nullptr;actions::ActionSource* action=nullptr;SourceBank* bank=nullptr;const LoadedScene* scene=nullptr;};
bool providers(Navi* n,Providers& out,std::string& e){
 if(!n||!pc_p2_original_captain_punch_source||!pc_p2_original_captain_action_source)return fail(e,"missing genuine source Punch/Action provider");
 auto* p=pc_p2_original_captain_punch_source(n);auto* a=pc_p2_original_captain_action_source(n);
 auto* s=pc_p2_original_captain_loaded_scene();auto* w=pc_p2_original_captain_world();auto* b=pc_p2_original_captain_source_bank();
 if(!p||!a||!s||!w||!b||&p->scene()!=s||&a->scene()!=s||!s->incarnation()||s->selectedCampaign().empty()||s->selectedFingerprint().empty()||s->sourceCatalog().empty()||w->incarnation()!=s->incarnation()||w->selectedCampaign()!=s->selectedCampaign()||w->selectedFingerprint()!=s->selectedFingerprint()||w->sourceCatalog()!=s->sourceCatalog()||!s->captainAt(0)||!s->captainAt(1)||s->captainAt(0)==s->captainAt(1)||(n!=s->captainAt(0)&&n!=s->captainAt(1))||w->phase()!=Phase::GameWorldActive||!pc_p2_original_captain_actor_alive(n))return fail(e,"Punch provider is not current canonical source scene/actor");
 MotionState state;if(!b->state(n,state,e))return false;
 RedPikminFlag flag=RedPikminFlag::Unknown;
 if(!p->redPikminFlag(flag,e)||flag==RedPikminFlag::Unknown)return fail(e,"missing source DEMO_Meet_Red_Pikmin authority");
 out={p,a,b,s};return true;
}
bool motions(Navi* n,std::string& e){
 if(!pc_p2_original_captain_motion_preflight)return fail(e,"missing selected source motion preflight");
 const Motion required[]={Motion::Punch,Motion::Punch2,Motion::Punch3,Motion::Wait};
 for(Motion m:required)if(!pc_p2_original_captain_motion_preflight(n,unsigned(m),e))return false;
 return true;
}
bool same(TargetHandle a,TargetHandle b){return a.actor==b.actor&&a.lifetime==b.lifetime;}
bool valid(TargetHandle h){return h.actor&&h.lifetime;}
struct Invocation {bool following=false;StateId next=StateId::Walk;std::uint64_t incarnation=0;PunchSource* source=nullptr;};
std::unordered_map<Navi*,Invocation> invocations;
class PunchState final:public NativeState {
 Providers owner_;std::uint64_t incarnation_=0;bool enabled_=false,following_=false,nextReady_=false;
 unsigned combo_=0,idle_=0;StateId next_=StateId::Walk;std::string error_;
 void report(){std::fprintf(stderr,"P2_ORIGINAL_PUNCH_REFUSED reason=%s\n",error_.c_str());}
 bool require(bool result){if(!result){enabled_=false;if(error_.empty())error_="source Punch operation refused";report();}return result;}
 bool live(Navi* n){
  if(!enabled_||!n||n->getCurrState()!=static_cast<NaviState*>(this))return false;
  Providers current;
  if(!providers(n,current,error_)){enabled_=false;report();return false;}
  if(current.punch!=owner_.punch||current.action!=owner_.action||current.bank!=owner_.bank||current.scene->incarnation()!=incarnation_){error_="stale source Punch provider/scene incarnation";enabled_=false;report();return false;}return true;
 }
 bool start(Navi* n,Motion motion){
  return require(owner_.bank->start(n,motion,error_)&&owner_.punch->feedback(*n,Feedback::Swing,{},error_)&&owner_.punch->enableMotionBlend(*n,error_));
 }
 bool transition(Navi* n){return require(pc_p2_original_captain_transit(n,next_,error_));}
 bool punchMotion(Navi* n){MotionState state;if(!require(owner_.bank->state(n,state,error_)))return false;return state.motion==Motion::Punch||state.motion==Motion::Punch2||state.motion==Motion::Punch3;}
 bool hit(Navi* n,Motion motion){
  actions::ActorFrame actor;if(!require(owner_.action->frame(*n,actor,error_)))return false;
  Sphere sphere;if(!require(hitSphere(actor,motion==Motion::Punch3,sphere,error_)))return false;
  std::vector<TargetFrame> targets;if(!require(owner_.punch->query(*n,sphere,targets,error_)))return false;
  for(const auto& candidate:targets){
   if(!valid(candidate.handle))return require(fail(error_,"missing source CellIterator target identity"));
   if(candidate.navi||candidate.handle.actor==static_cast<Creature*>(n)||!candidate.collisionTree||!candidate.alive)continue;
   TargetFrame target;if(!require(owner_.punch->target(candidate.handle,target,error_)))return false;
   if(!same(candidate.handle,target.handle)||target.collisionTree!=candidate.collisionTree)return require(fail(error_,"stale source punch target/tree"));
   if(target.navi||!target.alive)continue;
   std::vector<PartFrame> parts;if(!require(owner_.punch->collision(target.handle,sphere,parts,error_)))return false;
   for(const auto& part:parts){
    // Original callback explicitly permits a null part and produces no hit.
    if(!part.handle.part)continue;
    if(!same(part.handle.target,target.handle)||part.handle.collisionTree!=target.collisionTree||!finite(part.position))return require(fail(error_,"stale or nonfinite source collision callback"));
    RedPikminFlag flag=RedPikminFlag::Unknown;if(!require(owner_.punch->redPikminFlag(flag,error_)))return false;
    if(flag==RedPikminFlag::Unknown)return require(fail(error_,"missing source Red Pikmin flag at hit"));
    if(flag==RedPikminFlag::NotMet)continue;
    const bool rocket=motion==Motion::Punch3;bool accepted=false;
    if(!require(owner_.punch->attack(*n,part.handle,rocket?18.75f:7.5f,accepted,error_)))return false;
    if(!accepted)continue;
    // The hand pose is re-read after the actual receiver, just as retail.
    if(!live(n))return false;
    actions::ActorFrame current;PartFrame currentPart;
    if(!require(owner_.action->frame(*n,current,error_)&&owner_.punch->part(part.handle,currentPart,error_)))return false;
    if(!same(currentPart.handle.target,target.handle)||currentPart.handle.part!=part.handle.part||currentPart.handle.collisionTree!=part.handle.collisionTree)return require(fail(error_,"stale source part after attack receiver"));
    Vec3 effect;if(!require(effectPosition(currentPart.position,current.hand,effect,error_)&&owner_.punch->feedback(*n,rocket?Feedback::StoneHit:Feedback::Hit,effect,error_)))return false;
    if(!following_&&target.teki){
     auto* other=owner_.scene->captainAt(owner_.scene->captainAt(0)==n?1:0);
     if(other&&pc_p2_original_captain_actor_alive(other))party::assistPunch(other,{target.handle.actor,target.handle.lifetime});
    }
    if(!live(n))return false;
   }
  }
  return true;
 }
 bool key(Navi* n,int key){
  if(!live(n))return false;
  MotionState state;if(!require(owner_.bank->state(n,state,error_)))return false;
  if(key==2){if(state.motion!=Motion::Punch&&state.motion!=Motion::Punch2&&state.motion!=Motion::Punch3)return require(fail(error_,"KEY2 outside source Punch motion"));return hit(n,state.motion);}
  if(key==1000){
   if(nextReady_&&pc_p2_equipment_has(p2equipment::BruteKnuckles)){
    ++combo_;nextReady_=false;return start(n,combo_<=1?Motion::Punch2:Motion::Punch3);
   }
   idle_=4;return require(owner_.bank->start(n,Motion::Wait,error_));
  }
  return true;
 }
public:
 PunchState():NativeState(StateId::Punch){}
 bool sourceInvincible()const override{return false;}
 void init(Navi* n)override{
  enabled_=false;following_=false;next_=StateId::Walk;combo_=idle_=0;nextReady_=false;
  if(!providers(n,owner_,error_)||!motions(n,error_)){report();return;}
  incarnation_=owner_.scene->incarnation();
  auto argument=invocations.find(n);if(argument!=invocations.end()){
   if(argument->second.incarnation!=incarnation_||argument->second.source!=owner_.punch){invocations.erase(argument);error_="stale source PunchArg";report();return;}
   following_=argument->second.following;next_=argument->second.next;invocations.erase(argument);
  }
  enabled_=true;start(n,Motion::Punch);
 }
 void exec(Navi* n)override{
  if(!live(n))return;
  if(idle_){if(--idle_==0)transition(n);else if(!punchMotion(n)&&enabled_)transition(n);return;}
  actions::ActorFrame actor;if(!require(owner_.action->frame(*n,actor,error_)))return;
  if(!following_&&actor.controller&&actor.pressedA&&!nextReady_&&combo_<2)nextReady_=true;
  if(!require(owner_.action->control(*n,error_)&&owner_.action->findNextThrowPiki(*n,error_)))return;
  if(!punchMotion(n)&&enabled_)transition(n);
 }
 void cleanup(Navi*)override{enabled_=false;} // literal source cleanup has no action
 bool advance(Navi* n,float frames,std::string& e){
  if(!live(n)){e=error_;return false;}
  bool result=require(owner_.bank->advance(n,frames,[&](int k){return key(n,k)&&live(n);},error_));e=error_;return result;
 }
};
} // anonymous
bool begin(Navi* n,bool following,StateId next,std::string& e){
 if(next!=StateId::Walk&&next!=StateId::Follow)return fail(e,"unsupported source PunchArg continuation");
 Providers p;if(!providers(n,p,e)||!motions(n,e))return false;
 invocations[n]={following,next,p.scene->incarnation(),p.punch};
 if(!pc_p2_original_captain_transit(n,StateId::Punch,e)){invocations.erase(n);return false;}return true;
}
} // punch
void registerPunchState(NaviStateMachine& fsm){fsm.registerState(new punch::PunchState);}
} }
bool pc_p2_original_captain_punch_preflight(Navi* n,std::string& e){
 using namespace p2original::captain::punch;Providers p;return providers(n,p,e)&&motions(n,e);
}
bool pc_p2_original_captain_punch_advance_animation(Navi* n,float frames,std::string& e){
 using namespace p2original::captain::punch;
 if(!n||!std::isfinite(frames)||frames<0)return fail(e,"invalid source Punch animation advance");
 auto* state=dynamic_cast<PunchState*>(n->getCurrState());if(!state)return fail(e,"source Punch state is not current");return state->advance(n,frames,e);
}
