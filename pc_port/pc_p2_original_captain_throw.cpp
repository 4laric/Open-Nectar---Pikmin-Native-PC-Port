#include "pc_p2_original_captain_throw.h"
#include "pc_p2_original_captain_states.h"
#include "pc_p2_original_captain_motion.h"
#include "pc_p2_original_captain_control.h"
#include "netplay/pc_netplay_sha256.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <sstream>
#include <unordered_map>

extern p2original::captain::actions::ActionSource* pc_p2_original_captain_action_source(const Navi*) __attribute__((weak));
namespace p2original { namespace captain {
namespace {
using namespace actions;
struct ThrowParameters { float grab=0,time=0,minDistance=0,maxDistance=0,minHeight=0,maxHeight=0; };
struct Pending { PikiHandle piki; std::uint64_t incarnation; ActionSource* source; };
std::unordered_map<Navi*,Pending> pending;
struct GatherPending { GatherMode mode;std::uint64_t incarnation;ActionSource* source; };
std::unordered_map<Navi*,GatherPending> gatherPending;
bool fail(std::string& e,const char* reason){e=reason;return false;}
bool finite(Vec3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
float distance(Vec3 a,Vec3 b){float x=a.x-b.x,y=a.y-b.y,z=a.z-b.z;return std::sqrt(x*x+y*y+z*z);}
bool same(PikiHandle a,PikiHandle b){return a.actor==b.actor&&a.lifetime==b.lifetime;}
bool parameter(const std::string& raw,const char* id,float& out){
 const auto pos=raw.find(std::string("{")+id+"}");if(pos==std::string::npos)return false;
 std::istringstream stream(raw.substr(pos+6));unsigned type=0;return bool(stream>>type>>out)&&type==4&&std::isfinite(out);
}
bool parameters(const std::string& raw,ThrowParameters& p,std::string& e){
 std::uint8_t hash[32];pc_netplay_sha::sha256(raw.data(),raw.size(),hash);
 if(pc_netplay_sha::hex(hash,32)!=control::parameterSha256())return fail(e,"original action parameter SHA mismatch");
 if(!parameter(raw,"p037",p.grab)||!parameter(raw,"p009",p.time)||!parameter(raw,"p010",p.maxDistance)||!parameter(raw,"p011",p.minDistance)||!parameter(raw,"p024",p.maxHeight)||!parameter(raw,"p025",p.minHeight))return fail(e,"missing source action parameter");
 return true;
}
ActionSource* canonical(Navi* n,SourceBank*& bank,ThrowParameters& p,std::string& e,bool active=true){
 if(!n||!pc_p2_original_captain_action_source)return fail(e,"missing original action provider"),nullptr;
 auto* source=pc_p2_original_captain_action_source(n);auto* scene=pc_p2_original_captain_loaded_scene();auto* world=pc_p2_original_captain_world();
 if(!source||!scene||&source->scene()!=scene||!world||!scene->incarnation()||world->incarnation()!=scene->incarnation()||world->selectedCampaign()!=scene->selectedCampaign()||world->selectedFingerprint()!=scene->selectedFingerprint()||world->sourceCatalog()!=scene->sourceCatalog()||(scene->captainAt(0)!=n&&scene->captainAt(1)!=n))return fail(e,"action provider is not canonical source scene/actor"),nullptr;
 if(active&&(world->phase()!=Phase::GameWorldActive||!pc_p2_original_captain_actor_alive(n)))return fail(e,"original action actor/world inactive"),nullptr;
 bank=pc_p2_original_captain_source_bank();MotionState state;
 if(!bank||!bank->state(n,state,e)||!parameters(source->parameterBytes(),p,e))return nullptr;
 return source;
}
bool validPiki(const PikiFrame& p){return p.handle.actor&&p.handle.lifetime&&finite(p.position)&&p.kind<7&&p.happa<3;}
bool readPiki(ActionSource& s,Navi& n,PikiHandle h,PikiFrame& f,std::string& e){
 if(!s.piki(n,h,f,e)||!validPiki(f)||!same(h,f.handle)) {
  if(e.empty())e="missing matching source Piki lifetime/frame";
  return false;
 }
 return true;
}
bool frame(ActionSource& s,Navi& n,ActorFrame& a,std::string& e){
 if(!s.frame(n,a,e)||!finite(a.position)||!finite(a.velocity)||!finite(a.hand)||!finite(a.cursor)||!std::isfinite(a.face)||!std::isfinite(a.delta)||a.delta<0||!std::isfinite(a.sceneAnimationTimer))return fail(e,"missing finite actual source action frame");
 return true;
}
class ActionState:public NativeState {
protected:
 ActionSource* source_=nullptr;SourceBank* bank_=nullptr;ThrowParameters params_;std::uint64_t incarnation_=0;bool enabled_=false;std::string error_;
 explicit ActionState(StateId id):NativeState(id){}
 bool bind(Navi* n){source_=canonical(n,bank_,params_,error_);enabled_=source_!=nullptr;if(enabled_)incarnation_=source_->scene().incarnation();else report();return enabled_;}
 bool live(Navi* n,bool active=true){
  if(!enabled_||!n||n->getCurrState()!=static_cast<NaviState*>(this))return false;
  SourceBank* bank=nullptr;ThrowParameters p;auto* s=canonical(n,bank,p,error_,active);
  if(s!=source_||bank!=bank_||!s||s->scene().incarnation()!=incarnation_){enabled_=false;report();return false;}return true;
 }
 void report(){std::fprintf(stderr,"P2_ORIGINAL_ACTION_REFUSED state=%d reason=%s\n",int(id_),error_.c_str());}
 bool require(bool result){if(!result){enabled_=false;report();}return result;}
 bool change(Navi* n,StateId id){return require(pc_p2_original_captain_transit(n,id,error_));}
 bool motion(Navi* n,Motion m){return require(bank_->start(n,m,error_));}
 bool animate(Navi* n,float frames){
  if(!live(n))return false;
  return require(bank_->advance(n,frames,[&](int key){if(!live(n))return false;onKey(n,key);return n->getCurrState()==static_cast<NaviState*>(this)&&enabled_;},error_));
 }
 virtual void onKey(Navi*,int){}
public:
 bool sourceAnimationKey(Navi* n,int key,std::string& error)override{
  if(!live(n)){error=error_.empty()?"source action key actor/state is not current":error_;return false;}
  error_.clear();onKey(n,key);
  if(n->getCurrState()!=static_cast<NaviState*>(this)){error.clear();return false;}
  if(!enabled_||!live(n)){error=error_.empty()?"source action key operation refused":error_;return false;}
  error.clear();return true;
 }
 bool sourceInvincible()const final{return false;} // literal inherited retail NaviState
 bool advanceSourceAnimation(Navi* n,float frames,std::string& error){bool result=animate(n,frames);error=error_;return result;}
};
class GatherState final:public ActionState {
 GatherMode mode_=GatherMode::Player;
public:
 GatherState():ActionState(StateId::Gather){}
 void init(Navi* n)override{
  mode_=GatherMode::Player;if(!bind(n))return;
  auto invocation=gatherPending.find(n);
  if(invocation!=gatherPending.end()){
   if(invocation->second.incarnation!=incarnation_||invocation->second.source!=source_){gatherPending.erase(invocation);require(fail(error_,"stale source GatherArg transfer"));return;}
   mode_=invocation->second.mode;gatherPending.erase(invocation);
  }
  if(!motion(n,Motion::Fue))return;
  require(source_->startWhistle(*n,error_)&&source_->feedback(*n,mode_==GatherMode::Player?Feedback::GatherStart:Feedback::AutomaticGatherStart,{},error_));
 }
 void exec(Navi* n)override{
  if(!live(n))return;
  if(mode_==GatherMode::Automatic){
   if(!require(source_->updateWhistle(*n,{},true,error_)&&source_->callPikis(*n,error_)))return;
   WhistleFrame w;if(!require(source_->whistle(*n,w,error_)))return;if(w.timedOut)change(n,StateId::Walk);return;
  }
  ActorFrame a;if(!require(frame(*source_,*n,a,error_)))return;
  if(!a.controller)return;
  if(!require(source_->feedback(*n,Feedback::GatherLoop,{},error_)&&source_->control(*n,error_)&&source_->callPikis(*n,error_)))return;
  WhistleFrame w;if(!require(source_->whistle(*n,w,error_)))return;
  if(a.releasedB){if(require(source_->stopWhistle(*n,error_)))change(n,StateId::Walk);return;}
  if(w.timedOut){change(n,StateId::Walk);return;}
 }
 void cleanup(Navi* n)override{if(live(n,false))require(source_->feedback(*n,mode_==GatherMode::Player?Feedback::GatherStop:Feedback::AutomaticGatherStop,{},error_));enabled_=false;}
};
class ThrowWaitState final:public ActionState {
 PikiHandle held_,next_;bool hasHeld_=false,lockEnabled_=false;unsigned charge_=0;int happa_=-1;float nextTime_=3,sortTime_=.1f,holdTime_=0;
 bool fields(Navi* n){return require(source_->holdFields(*n,holdTime_,float(charge_)/3*(params_.maxDistance-params_.minDistance)+params_.minDistance,float(charge_)/3*(params_.maxHeight-params_.minHeight)+params_.minHeight,error_));}
 bool grab(Navi* n,PikiHandle p){held_=p;next_={};hasHeld_=true;return motion(n,Motion::ThrowWait)&&require(source_->feedback(*n,Feedback::Grab,p,error_)&&source_->transitionPiki(*n,p,PikiState::Hanged,error_));}
 bool sort(Navi* n){return require(source_->sortFormation(*n,held_,happa_,error_));}
 PikiHandle nearest(Navi* n,unsigned kind,int happa){
  std::vector<PikiFrame> party;if(!require(source_->squad(*n,party,error_)))return {};ActorFrame a;if(!require(frame(*source_,*n,a,error_)))return {};
  float best=140;PikiHandle result;for(const auto& p:party){if(!validPiki(p)){require(fail(error_,"invalid source squad identity"));return {};}
   float d=distance(p.position,a.position);if(p.kind==kind&&(happa==-1||int(p.happa)==happa)&&d<best&&p.state==PikiState::Walk&&p.throwable){best=d;result=p.handle;}}
  return result;
 }
 bool exchange(Navi* n,PikiHandle next){auto old=held_;return require(source_->feedback(*n,Feedback::StopHold,old,error_)&&source_->transitionPiki(*n,old,PikiState::Walk,error_))&&grabExchange(n,next);}
 bool grabExchange(Navi* n,PikiHandle p){held_=p;return require(source_->transitionPiki(*n,p,PikiState::Hanged,error_))&&sort(n)&&require(source_->feedback(*n,Feedback::PikiChange,p,error_)&&source_->feedback(*n,Feedback::Grab,p,error_));}
 void onKey(Navi*,int key)override{if(key==1&&charge_<3)++charge_;}
public:
 ThrowWaitState():ActionState(StateId::ThrowWait){}
 void init(Navi* n)override{
  held_={};next_={};hasHeld_=false;lockEnabled_=true;charge_=0;happa_=-1;nextTime_=3;sortTime_=.1f;holdTime_=0;
  if(!bind(n))return;
  ActorFrame a;std::vector<PikiFrame> party;if(!require(frame(*source_,*n,a,error_)&&source_->squad(*n,party,error_)))return;
  float best=80;PikiHandle candidate;
  for(const auto& p:party){if(!validPiki(p)){require(fail(error_,"invalid source squad identity"));return;}float dy=p.position.y-a.position.y;if(std::fabs(dy)>15)continue;
   float d=distance(p.position,a.position),dot=(p.position.x-a.position.x)*std::sin(a.face)+(p.position.z-a.position.z)*std::cos(a.face);if(dot>-.1f)d+=10;
   if(d<best&&p.state==PikiState::Walk&&p.throwable){best=d;candidate=p.handle;}}
  if(best<=params_.grab){if(!grab(n,candidate))return;}
  else {next_=candidate;if(next_.actor&&!require(source_->transitionPiki(*n,next_,PikiState::GoHang,error_)))return;}
  fields(n);
 }
 void resume(Navi* n)override{if(live(n,false))require(source_->feedback(*n,Feedback::StopHold,held_,error_));lockEnabled_=false;} // source resume calls cleanup, remains in state
 void restart(Navi* n)override{change(n,StateId::Walk);}
 void exec(Navi* n)override{
  if(!live(n))return;
  const auto demo=pc_p2_original_captain_world()->demo();
  if(demo==Demo::Playing){change(n,StateId::Walk);return;}
  if(demo==Demo::Unknown){require(fail(error_,"missing actual demo authority"));return;}
  ActorFrame a;if(!require(frame(*source_,*n,a,error_)))return;if(!a.controller)return;if(!require(source_->control(*n,error_)))return;
  if(!held_.actor){
   if(next_.actor){nextTime_-=a.delta;if(nextTime_<0||a.pressedB){change(n,StateId::Walk);return;}PikiFrame p;if(!require(readPiki(*source_,*n,next_,p,error_)))return;
    if(distance(a.hand,p.position)>32.5f)return;
    if(!grab(n,next_))return;
   }else{change(n,StateId::Punch);return;}
  }
  if(!require(source_->nextThrowPiki(*n,held_,error_))||!fields(n))return;
  PikiFrame held;if(!require(readPiki(*source_,*n,held_,held,error_)))return;
  if(hasHeld_&&held.state!=PikiState::Hanged&&held.state!=PikiState::GoHang){change(n,StateId::Walk);return;}
  if(a.right||a.left){happa_=-1;for(unsigned i=0;i<6;++i){unsigned k=(held.kind+(a.right?i+1:6-i))%7;auto p=nearest(n,k,-1);if(!enabled_)return;if(p.actor){exchange(n,p);return;}}}
  else if(a.up||a.down){for(unsigned i=0;i<2;++i){happa_=a.down?(happa_+2)%3:(happa_+1)%3;auto p=nearest(n,held.kind,happa_);if(!enabled_)return;if(p.actor){PikiFrame f;if(!require(readPiki(*source_,*n,p,f,error_)))return;if(f.happa!=held.happa){exchange(n,p);return;}}}}
  if(!a.heldA){if(!sort(n))return;holdTime_=float(charge_)/3*params_.time;if(!fields(n))return;pending[n]={held_,incarnation_,source_};if(!change(n,StateId::Throw))pending.erase(n);return;}
  holdTime_=std::min(holdTime_+a.delta,params_.time);if(!fields(n))return;
  if(sortTime_>0){sortTime_-=a.delta;if(sortTime_<=0&&!sort(n))return;}
  else if(a.firstFormationSlot&&distance(*a.firstFormationSlot,a.position)>30){if(!sort(n))return;}
 }
 bool lockHand(Navi* n){if(!live(n)||!held_.actor||!hasHeld_||!lockEnabled_)return false;ActorFrame a;PikiFrame p;if(!require(frame(*source_,*n,a,error_)&&readPiki(*source_,*n,held_,p,error_)))return false;a.hand.y-=6;return require(source_->positionPiki(*n,held_,a.hand,error_));}
 void cleanup(Navi* n)override{if(live(n,false))require(source_->feedback(*n,Feedback::StopHold,held_,error_));enabled_=false;}
};
class ThrowState final:public ActionState {
 PikiHandle piki_;bool hasThrown_=false,didCancel_=false;
 void onKey(Navi* n,int key)override{
  if(key==2){PikiFrame p;if(!require(readPiki(*source_,*n,piki_,p,error_)))return;
   if(p.throwable){WhistleFrame w;if(!require(source_->whistle(*n,w,error_)&&finite(w.cursor)))return;
    if(!require(source_->throwPiki(*n,piki_,w.cursor,error_)&&source_->transitionPiki(*n,piki_,PikiState::Flying,error_)))return;}
   hasThrown_=true;
  }else if(key==1000)change(n,StateId::Walk);
 }
public:
 ThrowState():ActionState(StateId::Throw){}
 void init(Navi* n)override{
  hasThrown_=false;didCancel_=false;piki_={};if(!bind(n))return;
  auto it=pending.find(n);if(it==pending.end()||it->second.incarnation!=incarnation_||it->second.source!=source_){require(fail(error_,"missing exact source ThrowInit Piki transfer"));return;}
  piki_=it->second.piki;pending.erase(it);PikiFrame p;if(!require(readPiki(*source_,*n,piki_,p,error_)))return;motion(n,Motion::Throw);
 }
 void exec(Navi* n)override{
  if(!live(n))return;
  ActorFrame a;if(!require(frame(*source_,*n,a,error_)))return;
  if(a.controller){if(!require(source_->control(*n,error_)))return;if(a.heldB)didCancel_=true;if(!require(source_->findNextThrowPiki(*n,error_)))return;
   if(hasThrown_&&a.pressedA){if(!a.throwDisableFrames){require(fail(error_,"missing source Navi throw-disable timer"));return;}if(*a.throwDisableFrames==0){change(n,StateId::ThrowWait);return;}}
   if(hasThrown_&&a.pressedB){change(n,StateId::Gather);return;}}
 }
 void cleanup(Navi*)override{enabled_=false;}
};
} // anonymous
void registerThrowStates(NaviStateMachine& machine){machine.registerState(new GatherState);machine.registerState(new ThrowWaitState);machine.registerState(new ThrowState);}
} }
bool pc_p2_original_captain_throw_preflight(Navi* n,p2original::captain::StateId id,std::string& e){
 using namespace p2original::captain;if(id!=StateId::Gather&&id!=StateId::ThrowWait&&id!=StateId::Throw)return false;
 SourceBank* bank=nullptr;ThrowParameters p;auto* source=canonical(n,bank,p,e);if(!source)return false;
 if(id==StateId::Throw){auto it=pending.find(n);if(it==pending.end()||it->second.incarnation!=source->scene().incarnation()||it->second.source!=source)return fail(e,"missing authenticated ThrowInit transfer");}
 return true;
}
bool pc_p2_original_captain_throw_after_animation(Navi* n,std::string& e){
 using namespace p2original::captain;if(!n)return fail(e,"missing source captain");auto* state=dynamic_cast<ThrowWaitState*>(n->getCurrState());if(!state)return fail(e,"source ThrowWait is not current state");return state->lockHand(n);
}
bool pc_p2_original_captain_throw_advance_animation(Navi* n,float frames,std::string& e){
 using namespace p2original::captain;
 if(!n||!std::isfinite(frames)||frames<0)return fail(e,"invalid source action animation advance");
 auto* state=dynamic_cast<ActionState*>(n->getCurrState());
 if(!state)return fail(e,"original action state is not current");
 return state->advanceSourceAnimation(n,frames,e);
}
bool pc_p2_original_captain_begin_gather(Navi* n,p2original::captain::actions::GatherMode mode,std::string& e){
 using namespace p2original::captain;using namespace actions;
 if(mode!=GatherMode::Player&&mode!=GatherMode::Automatic)return fail(e,"invalid source GatherArg mode");
 SourceBank* bank=nullptr;ThrowParameters p;auto* source=canonical(n,bank,p,e);if(!source)return false;
 gatherPending[n]={mode,source->scene().incarnation(),source};
 if(!pc_p2_original_captain_transit(n,StateId::Gather,e)){gatherPending.erase(n);return false;}return true;
}
