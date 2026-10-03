#include "pc_p2_original_captain_pressed.h"
#include "pc_p2_original_captain_states.h"
#include "pc_p2_original_captain_motion.h"
#include <cmath>
#include <cstdio>
#include <unordered_map>
extern p2original::captain::physical::PhysicalSource* pc_p2_original_captain_physical_source(const Navi*) __attribute__((weak));
namespace p2original {namespace captain {namespace physical {namespace {
bool fail(std::string& e,const char* s){e=s;return false;}
bool finite(Vec3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
struct Owner {const LoadedScene* scene=nullptr;PhysicalSource* body=nullptr;SourceBank* bank=nullptr;std::uint64_t epoch=0;};
bool same(const Owner& a,const Owner& b){return a.scene==b.scene&&a.body==b.body&&a.bank==b.bank&&a.epoch==b.epoch;}
bool owner(Navi* n,Owner& o,std::string& e,bool cleanup=false){
 auto* s=pc_p2_original_captain_loaded_scene();auto* w=pc_p2_original_captain_world();auto* b=pc_p2_original_captain_source_bank();
 auto* p=pc_p2_original_captain_physical_source?pc_p2_original_captain_physical_source(n):nullptr;
 if(!n||!s||!w||!b||!p||&p->scene()!=s||!s->incarnation()||s->selectedCampaign().empty()||s->selectedFingerprint().empty()||s->sourceCatalog().empty()||w->incarnation()!=s->incarnation()||w->selectedCampaign()!=s->selectedCampaign()||w->selectedFingerprint()!=s->selectedFingerprint()||w->sourceCatalog()!=s->sourceCatalog()||(w->phase()!=Phase::GameWorldActive&&(!cleanup||w->phase()!=Phase::Inactive))||w->captainAt(0)!=s->captainAt(0)||w->captainAt(1)!=s->captainAt(1)||!s->captainAt(0)||!s->captainAt(1)||s->captainAt(0)==s->captainAt(1)||(n!=s->captainAt(0)&&n!=s->captainAt(1))||(!cleanup&&!pc_p2_original_captain_actor_alive(n)))return fail(e,"missing canonical original physical actor authority");
 BodyFrame f;MotionState motion;if(!p->frame(*n,f,e)||!finite(f.position)||!finite(f.scale)||!finite(f.velocity)||!finite(f.targetVelocity)||!std::isfinite(f.face)||!std::isfinite(f.delta)||f.delta<0||!b->state(n,motion,e))return fail(e,"missing actual original body frame/bank binding");
 o={s,p,b,s->incarnation()};return true;
}
struct Arg {Owner owner;float damage=0;};std::unordered_map<Navi*,Arg> args;
class PhysicalState:public NativeState {
protected:
 Owner bound;bool enabled=false;std::string error;
 bool require(bool r){if(!r){enabled=false;if(error.empty())error="source physical operation refused";std::fprintf(stderr,"P2_ORIGINAL_PHYSICAL_REFUSED state=%d reason=%s\n",int(id_),error.c_str());}return r;}
 bool live(Navi* n){if(!enabled||!n||n->getCurrState()!=static_cast<NaviState*>(this))return false;Owner now;return require(owner(n,now,error)&&same(now,bound)&&n->getCurrState()==static_cast<NaviState*>(this));}
 bool retained(Navi* n,bool cleanup=false){Owner now;return require(owner(n,now,error,cleanup)&&same(now,bound));}
 template<class F> bool apply(Navi* n,F operation,bool cleanup=false){auto* before=n->getCurrState();if(!retained(n,cleanup)||!require(n->getCurrState()==before||fail(error,"source observer changed physical state")))return false;if(!require(operation())||!retained(n,cleanup))return false;return require(n->getCurrState()==before||fail(error,"source callback changed physical state"));}
 bool frame(Navi* n,BodyFrame& f){return require(bound.body->frame(*n,f,error)&&finite(f.position)&&finite(f.scale)&&finite(f.velocity)&&finite(f.targetVelocity)&&std::isfinite(f.face)&&std::isfinite(f.delta)&&f.delta>=0)&&retained(n);}
 bool walk(Navi* n){return require(pc_p2_original_captain_transit(n,StateId::Walk,error));}
public:
 explicit PhysicalState(StateId id):NativeState(id){}bool sourcePressable()const override{return false;}
 bool sourceAnimationKey(Navi* n,int,std::string& e)override{if(!live(n)){e=error.empty()?"physical state not current":error;return false;}e.clear();return true;}
};
// Retail naviState.cpp 6793-6872. In particular the source restore formula
// uses backup.x for both x and z; retaining that authored asymmetry is literal.
class Pressed final:public PhysicalState {
 Vec3 backup;float timer=2;unsigned phase=0;bool cleanupBound=false;
 bool matrix(Navi* n,const BodyFrame& f,Vec3 scale){Vec3 pos=f.position;pos.y+=2;return apply(n,[&]{return bound.body->baseSRT(*n,scale,{0,f.face,0},pos,error);});}
public:
 Pressed():PhysicalState(StateId::Pressed){}
 bool sourceInvincible()const override{return true;}bool sourceVsUsableY()const override{return false;}
 void init(Navi* n)override{enabled=cleanupBound=false;error.clear();if(!require(owner(n,bound,error)))return;BodyFrame f;if(!frame(n,f))return;backup=f.scale;timer=2;phase=0;enabled=cleanupBound=true;
  Vec3 squashed{1.5f,.01f,1.5f};if(!apply(n,[&]{return bound.body->scale(*n,squashed,error);})||!apply(n,[&]{return bound.body->updateTrMatrix(*n,false,error);})||!matrix(n,f,squashed)||!apply(n,[&]{return bound.body->atari(*n,false,error);}))return;
  apply(n,[&]{return bound.body->damageSoundAndOptionalDirector(*n,error);});
 }
 void exec(Navi* n)override{if(!live(n))return;auto* w=pc_p2_original_captain_world();if(w->demo()==Demo::Unknown){require(fail(error,"missing actual movie activity observer"));return;}if(w->demo()==Demo::Playing){walk(n);return;}
  BodyFrame f;if(!frame(n,f))return;timer-=f.delta;
  if(phase==0){if(!matrix(n,f,f.scale))return;if(timer<=0){phase=1;timer=.7f;}}
  else {float y=1-timer/.7f;float wave=(.5f*(1-y))*std::sin(timer*6.2831853071795864769f*4);y+=wave;if(y<0)y=0;Vec3 scale{y*backup.x+1.5f*(1-y),y*backup.y+.01f*(1-y),y*backup.x+1.5f*(1-y)};
   if(!apply(n,[&]{return bound.body->scale(*n,scale,error);})||!matrix(n,f,scale))return;
   if(timer<=0&&!walk(n))return;
  }
  // Retail executes these writes even after the restoring transition cleanup.
  apply(n,[&]{return bound.body->velocities(*n,{},{},error);});
 }
 void cleanup(Navi* n)override{if(cleanupBound&&apply(n,[&]{return bound.body->atari(*n,true,error);},true)&&apply(n,[&]{return bound.body->updateTrMatrix(*n,true,error);},true))apply(n,[&]{return bound.body->scale(*n,backup,error);},true);enabled=cleanupBound=false;}
};
class FallMeck final:public PhysicalState {
 float damage=0;int phase=0;
public:
 FallMeck():PhysicalState(StateId::FallMeck){}bool sourceInvincible()const override{return false;}
 void init(Navi* n)override{enabled=false;error.clear();Owner now;if(!require(owner(n,now,error)))return;auto it=args.find(n);damage=0;if(it!=args.end()){Arg a=it->second;args.erase(it);if(!require(same(a.owner,now)))return;damage=a.damage;}bound=now;enabled=true;phase=0;
  if(!apply(n,[&]{return bound.bank->startMotion(n,Motion::Fall,Motion::Fall,Listener::None,Listener::None,error);})||!apply(n,[&]{return bound.body->endStick(*n,error);}))return;
  BodyFrame f;if(!frame(n,f))return;
  f.velocity.y=damage>0?-400.f:-100.f;f.targetVelocity.y=f.velocity.y;apply(n,[&]{return bound.body->velocities(*n,f.velocity,f.targetVelocity,error);});
 }
 void exec(Navi* n)override{if(!live(n))return;if(phase!=0&&!apply(n,[&]{return bound.body->velocities(*n,{},{},error);}))return;MotionState m;if(!require(bound.bank->stateAnimator(n,p2original::captain::Animator::Self,m,error)))return;if(phase==0&&m.motion!=Motion::Fall)walk(n);}
 void cleanup(Navi*)override{enabled=false;}
 bool sourceAnimationKey(Navi* n,int k,std::string& e)override{if(!live(n)){e=error.empty()?"FallMeck not current":error;return false;}if(k==1000){if(phase==1){if(!apply(n,[&]{return bound.bank->startMotion(n,Motion::Getup,Motion::Getup,Listener::SourceActor,Listener::None,error);})){e=error;return false;}phase=2;}else if(phase==2){if(!walk(n)){e=error;return false;}e.clear();return false;}}e.clear();return true;}
 bool bounce(Navi* n,Contact c,std::string& e){if(!live(n)){e=error.empty()?"FallMeck not current":error;return false;}Landing l;BodyFrame f;if(!c.triangle||!c.lifetime||!bound.body->landing(*n,c,l,e)||l.contact.triangle!=c.triangle||l.contact.lifetime!=c.lifetime||(l.inWater&&!std::isfinite(l.seaHeight)))return fail(e,"missing genuine physical bounce/water contact");
  if(!retained(n)||n->getCurrState()!=static_cast<NaviState*>(this)||!frame(n,f)){e=error.empty()?"source landing observer changed state":error;return false;}if(phase==0&&damage>0&&(!bound.body->preflightKoke(*n,damage,e)||!retained(n)||n->getCurrState()!=static_cast<NaviState*>(this))){if(e.empty())e=error.empty()?"source Koke preflight changed state":error;return false;}Vec3 pos=f.position;if(l.inWater)pos.y=l.seaHeight;
  if(!apply(n,[&]{return bound.body->landingEffect(*n,l.inWater,pos,.5f,e);})){if(e.empty())e=error;return false;}
  if(phase==0){if(damage>0){auto* before=n->getCurrState();auto r=addDamage(n,0,true);if(!retained(n)||n->getCurrState()!=before){e=error.empty()?"source addDamage changed actor lifetime/state":error;return false;}if(r.refusal!=Refusal::None&&r.refusal!=Refusal::DemoPlaying&&r.refusal!=Refusal::ActorInvincible&&r.refusal!=Refusal::NotAlive&&r.refusal!=Refusal::StateInvincible)return fail(e,"canonical source addDamage authority refused");return bound.body->enterKoke(*n,damage,e);}if(!apply(n,[&]{return bound.body->nudgeRumble(*n,e);})){if(e.empty())e=error;return false;}return pc_p2_original_captain_transit(n,StateId::Walk,e);}return true;
 }
};
} // anonymous
bool beginFallMeck(Navi* n,std::optional<float> d,std::string& e){Owner o;if(!owner(n,o,e)||!o.bank->supports(n,Motion::Fall,e)||!o.bank->supports(n,Motion::Getup,e))return false;if(d&&!std::isfinite(*d))return fail(e,"nonfinite source NaviFallMeckArg");args[n]={o,d.value_or(0)};if(!pc_p2_original_captain_transit(n,StateId::FallMeck,e)){args.erase(n);return false;}return true;}
bool bounce(Navi* n,Contact c,std::string& e){auto* state=n?dynamic_cast<FallMeck*>(n->getCurrState()):nullptr;return state?state->bounce(n,c,e):fail(e,"actual source FallMeck state unavailable");}
} void registerPressedFallMeckStates(NaviStateMachine& f){f.registerState(new physical::Pressed);f.registerState(new physical::FallMeck);}
}}
bool pc_p2_original_captain_pressed_fall_preflight(Navi* n,p2original::captain::StateId id,std::string& e){using namespace p2original::captain;using namespace physical;Owner o;if(!owner(n,o,e))return false;if(id==StateId::Pressed){auto* w=pc_p2_original_captain_world();return w->demo()!=Demo::Unknown||fail(e,"missing actual movie activity observer");}if(id==StateId::FallMeck)return o.bank->supports(n,Motion::Fall,e)&&o.bank->supports(n,Motion::Getup,e);return fail(e,"unsupported original physical state");}
