#include "pc_p2_original_captain_pluck.h"
#include "pc_p2_original_captain_states.h"
#include "pc_p2_original_captain_motion.h"
#include "pc_p2_original_captain_control.h"
#include "netplay/pc_netplay_sha256.h"
#include <cmath>
#include <cstdio>
#include <sstream>
#include <unordered_map>
extern p2original::captain::pluck::PluckSource* pc_p2_original_captain_pluck_source(const Navi*) __attribute__((weak));
extern p2original::captain::actions::ActionSource* pc_p2_original_captain_action_source(const Navi*) __attribute__((weak));
extern bool pc_p2_original_captain_motion_preflight(Navi*,unsigned,std::string&) __attribute__((weak));
namespace p2original {namespace captain {namespace pluck {namespace {
bool fail(std::string& e,const char* s){e=s;return false;}
bool finite(Vec3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
Vec3 subtract(Vec3 a,Vec3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
Vec3 scale(Vec3 a,float k){return {a.x*k,a.y*k,a.z*k};}
float normalize(Vec3& v){float d=std::sqrt(v.x*v.x+v.y*v.y+v.z*v.z);if(d>0)v=scale(v,1/d);return d;}
constexpr float pi=3.14159265358979323846f;
float roundAngle(float f){f=std::fmod(f,2*pi);return f<0?f+2*pi:f;}
float angleDistance(float a,float b){float d=roundAngle(a)-roundAngle(b);if(d>pi)d-=2*pi;if(d<-pi)d+=2*pi;return d;}
struct Providers {PluckSource* p=nullptr;actions::ActionSource* a=nullptr;SourceBank* bank=nullptr;const LoadedScene* scene=nullptr;const World* world=nullptr;int loops=0;};
bool providers(Navi* n,Providers& o,std::string& e){
 if(!n||!pc_p2_original_captain_pluck_source||!pc_p2_original_captain_action_source)return fail(e,"missing genuine source pluck providers");
 auto* p=pc_p2_original_captain_pluck_source(n);auto* a=pc_p2_original_captain_action_source(n);auto* s=pc_p2_original_captain_loaded_scene();auto* w=pc_p2_original_captain_world();auto* b=pc_p2_original_captain_source_bank();
 if(!p||!a||!s||!w||!b||&p->scene()!=s||&a->scene()!=s||!s->incarnation()||s->selectedCampaign().empty()||s->selectedFingerprint().empty()||s->sourceCatalog().empty()||w->incarnation()!=s->incarnation()||w->selectedCampaign()!=s->selectedCampaign()||w->selectedFingerprint()!=s->selectedFingerprint()||w->sourceCatalog()!=s->sourceCatalog()||!s->captainAt(0)||!s->captainAt(1)||s->captainAt(0)==s->captainAt(1)||(n!=s->captainAt(0)&&n!=s->captainAt(1))||w->phase()!=Phase::GameWorldActive||!pc_p2_original_captain_actor_alive(n)||w->demo()==Demo::Unknown)return fail(e,"pluck provider is not canonical live source scene/actor/demo");
 MotionState state;if(!b->state(n,state,e))return false;
 const auto& raw=a->parameterBytes();std::uint8_t hash[32];pc_netplay_sha::sha256(raw.data(),raw.size(),hash);
 if(pc_netplay_sha::hex(hash,32)!=control::parameterSha256())return fail(e,"source pluck parameter SHA mismatch");
 auto pos=raw.find("{p042}");if(pos==std::string::npos)return fail(e,"missing source pluck loop parameter");
 unsigned type=0;int loops=0;std::istringstream stream(raw.substr(pos+6));if(!(stream>>type>>loops)||type!=4||loops<0||loops>10)return fail(e,"invalid source pluck loop parameter");
 o={p,a,b,s,w,loops};return true;
}
bool motions(Navi* n,StateId id,std::string& e){
 if(!pc_p2_original_captain_motion_preflight)return fail(e,"missing source pluck motion preflight");
 if(id==StateId::NukuAdjust)return pc_p2_original_captain_motion_preflight(n,unsigned(Motion::Walk),e)&&pc_p2_original_captain_motion_preflight(n,unsigned(Motion::Nigeru),e);
 return pc_p2_original_captain_motion_preflight(n,unsigned(Motion::Nuku),e)&&pc_p2_original_captain_motion_preflight(n,unsigned(Motion::Nuku3),e);
}
struct Pending {Providers owner;HeadHandle head;bool following=false;StateId id=StateId::Nuku;std::uint64_t epoch=0;};std::unordered_map<Navi*,Pending> pending;
class PluckState:public NativeState {
protected:
 Providers owner;std::uint64_t epoch=0;bool enabled=false,following=false;std::string error;
 bool require(bool r){if(!r){enabled=false;if(error.empty())error="source pluck operation refused";std::fprintf(stderr,"P2_ORIGINAL_PLUCK_REFUSED reason=%s\n",error.c_str());}return r;}
 bool live(Navi* n){
  if(!enabled||n->getCurrState()!=static_cast<NaviState*>(this))return false;
  Providers now;if(!require(providers(n,now,error)))return false;
  return require((now.p==owner.p&&now.a==owner.a&&now.bank==owner.bank&&now.scene->incarnation()==epoch)||fail(error,"stale source pluck provider/incarnation"));
 }
 bool activate(Navi* n,bool needsArgument,HeadHandle& h){
  enabled=false;following=false;if(!require(providers(n,owner,error)&&motions(n,id_,error)))return false;epoch=owner.scene->incarnation();
  auto it=pending.find(n);if(it!=pending.end()){
   auto arg=it->second;pending.erase(it);
   if(!require((arg.id==id_&&arg.owner.p==owner.p&&arg.owner.scene==owner.scene&&arg.epoch==epoch)||fail(error,"stale source pluck argument")))return false;
   following=arg.following;h=arg.head;
  }else if(needsArgument)return require(fail(error,"NukuAdjust requires actual source head argument"));
  enabled=true;return true;
 }
 bool exit(Navi* n,bool notNew){return require(following?owner.p->follow(*n,notNew,error):pc_p2_original_captain_transit(n,StateId::Walk,error));}
public:
 explicit PluckState(StateId id):NativeState(id){}bool sourceInvincible()const override{return false;}
 virtual bool key(Navi*,int){return true;}
 bool advance(Navi* n,float frames,std::string& e){
  if(!live(n)){e=error;return false;}
  MotionState bound;if(!require(owner.bank->stateAnimator(n,Animator::Bound,bound,error))){e=error;return false;}
  if(!require(owner.bank->advanceAnimator(n,Animator::Self,frames,[&](int k){return key(n,k)&&live(n);},error))){e=error;return false;}
  if(n->getCurrState()!=static_cast<NaviState*>(this)){e.clear();return true;}
  if(!live(n)){e=error;return false;}
  MotionState current;if(!require(owner.bank->stateAnimator(n,Animator::Bound,current,error))){e=error;return false;}
  if(current.generation!=bound.generation){e.clear();return true;}
  // enableMotionBlend installs SourceActor on actual Bound NIGERU. Deliver
  // both channels explicitly, with the genuine current leaf callback.
  bool r=require(owner.bank->advanceAnimator(n,Animator::Bound,frames,[&](int k){return key(n,k)&&live(n);},error));e=error;return r;
 }
};
class NukuState final:public PluckState {
 Motion motion=Motion::Nuku;std::uint16_t counter=0;bool active=false,didPress=false;
public:
 NukuState():PluckState(StateId::Nuku){}
 bool sourceInvincible()const override{return true;}
 void init(Navi* n)override{
  HeadHandle ignored;if(!activate(n,false,ignored))return;BodyFrame body;
  if(!require(owner.p->body(*n,body,error)))return;
  motion=body.pluckingCounter?Motion::Nuku3:Motion::Nuku;counter=owner.loops;active=didPress=false;
  require(owner.bank->startMotion(n,motion,motion,Listener::SourceActor,Listener::None,error)&&owner.p->feedback(*n,Feedback::Pulling,error)&&owner.p->setMass(*n,0,error));
 }
 void exec(Navi* n)override{
  if(!live(n))return;
  if(owner.world->demo()==Demo::Playing){exit(n,true);return;}
  if(!require(owner.p->setVelocities(*n,{},{},error)))return;
  MotionState state;if(!require(owner.bank->state(n,state,error)))return;
  if(state.motion!=motion){if(exit(n,true))require(owner.p->setPluckingCounter(*n,0,error));return;}
  if(!following){actions::ActorFrame actor;if(!require(owner.a->frame(*n,actor,error)))return;
   if(!actor.controller){require(fail(error,"source Nuku requires controller"));return;}
   if(!didPress&&actor.heldA)didPress=true;
   if(didPress&&!actor.heldA){active=true;BodyFrame body;if(require(owner.p->body(*n,body,error)))require(owner.p->setPluckingCounter(*n,std::uint8_t(body.pluckingCounter+1),error));}
  }
 }
 bool key(Navi* n,int k)override{
  if(k==2){if(--counter==0)return require(owner.p->feedback(*n,Feedback::Pullout,error));}
  if(k==1000){bool handled=false;if(active&&!following){if(!require(owner.p->actionButton(*n,handled,error)))return false;if(handled)return true;}
   active=false;if(!exit(n,true))return false;return require(owner.p->setPluckingCounter(*n,0,error));
  }return true;
 }
 void cleanup(Navi* n)override{if(enabled)require(owner.p->setMass(*n,1,error)&&owner.p->startThrowDisable(*n,error));enabled=false;}
};
class AdjustState final:public PluckState {
 HeadHandle head;Vec3 target,collided;float angle=0;std::uint8_t walls=0;bool moving=false;
 bool readHead(HeadFrame& out){if(!require(owner.p->head(head,out,error)))return false;return require((out.handle.actor==head.actor&&out.handle.lifetime==head.lifetime&&finite(out.position))||fail(error,"stale source ItemPikihead"));}
public:
 AdjustState():PluckState(StateId::NukuAdjust){}
 void init(Navi* n)override{
  if(!activate(n,true,head))return;
  HeadFrame h;actions::ActorFrame actor;
  if(!readHead(h)||!require(owner.a->frame(*n,actor,error)))return;
  if(!require(h.alive&&finite(actor.position)))return;
  Vec3 diff=subtract(h.position,actor.position);angle=std::atan2(diff.x,diff.z);normalize(diff);target=subtract(h.position,scale(diff,6));walls=0;moving=false;
  require(owner.p->markFirstPluck(error)&&owner.bank->startMotion(n,Motion::Walk,Motion::Walk,Listener::None,Listener::None,error)&&owner.bank->enableMotionBlend(n,error)&&owner.p->setMoveRotation(*n,false,error)&&owner.p->setMass(*n,0,error));
 }
 void exec(Navi* n)override{
  if(!live(n))return;
  if(owner.world->demo()==Demo::Playing){exit(n,false);return;}
  HeadFrame h;if(!readHead(h))return;if(!h.alive){exit(n,false);return;}
  if(!following&&!require(owner.p->makeCStick(*n,false,error)))return;
  actions::ActorFrame actor;if(!require(owner.a->frame(*n,actor,error)))return;
  if(actor.controller&&actor.heldB){if(require(owner.p->setPluckingCounter(*n,0,error)))require(pc_p2_original_captain_transit(n,StateId::Walk,error));return;}
  if(!finite(actor.position)||!std::isfinite(actor.face)||!std::isfinite(actor.delta)||actor.delta<=0){require(fail(error,"invalid actual source adjustment body/delta"));return;}
  Vec3 diff=subtract(target,actor.position);float horizontal=std::sqrt(diff.x*diff.x+diff.z*diff.z),vertical=std::fabs(diff.y),distance=normalize(diff),delta=angleDistance(angle,actor.face);
  Vec3 velocity;
  if(std::fabs(delta)<pi/10&&horizontal<2&&vertical<10){
   if(!require(owner.p->setFace(*n,angle,error)))return;
   std::optional<PikiHandle> born;if(!require(owner.p->birthForced(born,error)))return;
   if(!born){exit(n,false);return;}if(!born->actor||!born->lifetime){require(fail(error,"missing actual forced-birth Piki lifetime"));return;}
   // Retail re-queries head position after birth before initializing/kill.
   if(!readHead(h))return;
   BodyFrame body;if(!require(owner.p->body(*n,body,error)))return;
   if(!require(owner.p->initializeBorn(*born,h.color,h.happa,h.position,error)&&owner.p->killHead(head,error)&&owner.p->enterNukare(*born,*n,body.pluckingCounter!=0,error)))return;
   head={};if(!require(beginNuku(n,following,error)))return;
  }else{
   if(!require(owner.p->setFace(*n,roundAngle(actor.face+.2f*delta),error)))return;
   float speed=100;if(speed*actor.delta>distance)speed=.5f/actor.delta;
   velocity=scale(diff,speed);if(!require(owner.p->setVelocities(*n,velocity,velocity,error)))return;
  }
  if(walls>10){exit(n,false);return;}if(!moving)return;moving=false;
  // Actual post-transition actor velocity is required, matching retail.
  if(!require(owner.a->frame(*n,actor,error)))return;
  velocity=actor.velocity;
  Vec3 to=subtract(collided,actor.position);if(!(normalize(to)>0))return;
  float cross=to.z*velocity.x-to.x*velocity.z,speed=std::sqrt(velocity.x*velocity.x+velocity.y*velocity.y+velocity.z*velocity.z);
  Vec3 slide=scale({-to.z,0,to.x},speed);if(!(cross<0))slide=scale(slide,-1);
  Vec3 blend{velocity.x*.35f+slide.x*.65f,velocity.y*.35f+slide.y*.65f,velocity.z*.35f+slide.z*.65f};
  if(normalize(blend)!=0){blend=scale(blend,speed);require(owner.p->setVelocities(*n,blend,blend,error));}
 }
 void cleanup(Navi* n)override{walls=0;if(enabled)require(owner.p->setMoveRotation(*n,true,error)&&owner.p->setMass(*n,1,error));enabled=false;}
 bool wallEvent(Navi* n){if(!live(n))return false;++walls;return true;}
 bool collisionEvent(Navi* n,Vec3 position,bool piki,bool navi,bool flick){if(!live(n))return false;if(piki||navi||!flick)return true;if(!finite(position))return require(fail(error,"invalid actual collision position"));moving=true;collided=position;return true;}
};
} // anonymous
bool beginNuku(Navi* n,bool following,std::string& e){Providers p;if(!providers(n,p,e)||!motions(n,StateId::Nuku,e))return false;pending[n]={p,{},following,StateId::Nuku,p.scene->incarnation()};if(!pc_p2_original_captain_transit(n,StateId::Nuku,e)){pending.erase(n);return false;}return true;}
bool beginAdjust(Navi* n,HeadHandle head,bool following,std::string& e){Providers p;if(!head.actor||!head.lifetime)return fail(e,"missing source head identity");if(!providers(n,p,e)||!motions(n,StateId::NukuAdjust,e))return false;HeadFrame h;if(!p.p->head(head,h,e)||h.handle.actor!=head.actor||h.handle.lifetime!=head.lifetime||!h.alive||!finite(h.position))return fail(e,"invalid actual live source head");pending[n]={p,head,following,StateId::NukuAdjust,p.scene->incarnation()};if(!pc_p2_original_captain_transit(n,StateId::NukuAdjust,e)){pending.erase(n);return false;}return true;}
bool wall(Navi* n,std::string& e){auto* s=n?dynamic_cast<AdjustState*>(n->getCurrState()):nullptr;return s?s->wallEvent(n):fail(e,"source NukuAdjust not current");}
bool collision(Navi* n,Vec3 p,bool isPiki,bool isNavi,bool flick,std::string& e){auto* s=n?dynamic_cast<AdjustState*>(n->getCurrState()):nullptr;return s?s->collisionEvent(n,p,isPiki,isNavi,flick):fail(e,"source NukuAdjust not current");}
bool ignoreAtari(bool navi,bool onyon){return navi||onyon;}
} void registerPluckStates(NaviStateMachine& fsm){fsm.registerState(new pluck::NukuState);fsm.registerState(new pluck::AdjustState);}
}}
bool pc_p2_original_captain_pluck_preflight(Navi* n,p2original::captain::StateId id,std::string& e){using namespace p2original::captain;using namespace pluck;Providers p;if(id!=StateId::Nuku&&id!=StateId::NukuAdjust)return fail(e,"unsupported source pluck state");return providers(n,p,e)&&motions(n,id,e);}
bool pc_p2_original_captain_pluck_advance_animation(Navi* n,float f,std::string& e){using namespace p2original::captain::pluck;if(!n||!std::isfinite(f)||f<0)return fail(e,"invalid source pluck animation advance");auto* s=dynamic_cast<PluckState*>(n->getCurrState());return s?s->advance(n,f,e):fail(e,"source pluck state not current");}
