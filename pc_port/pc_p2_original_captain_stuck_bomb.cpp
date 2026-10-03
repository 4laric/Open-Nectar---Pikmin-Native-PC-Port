#include "pc_p2_original_captain_stuck_bomb.h"
#include "pc_p2_original_captain_states.h"
#include "pc_p2_original_captain_motion.h"
#include <cmath>
#include <cstdio>
#include <unordered_map>
extern p2original::captain::attachments::AttachmentSource* pc_p2_original_captain_attachment_source(const Navi*) __attribute__((weak));
namespace p2original {namespace captain {namespace attachments {namespace {
bool fail(std::string& e,const char* s){e=s;return false;}
bool finite(Vec3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
struct Owner {const LoadedScene* scene=nullptr;AttachmentSource* source=nullptr;SourceBank* bank=nullptr;std::uint64_t epoch=0;};
bool same(const Owner& a,const Owner& b){return a.scene==b.scene&&a.source==b.source&&a.bank==b.bank&&a.epoch==b.epoch;}
bool same(BombHandle a,BombHandle b){return a.actor==b.actor&&a.lifetime==b.lifetime;}
bool owner(Navi* n,Owner& o,std::string& e,bool cleanup=false){
 auto* s=pc_p2_original_captain_loaded_scene();auto* w=pc_p2_original_captain_world();auto* b=pc_p2_original_captain_source_bank();auto* p=pc_p2_original_captain_attachment_source?pc_p2_original_captain_attachment_source(n):nullptr;
 if(!n||!s||!w||!b||!p||&p->scene()!=s||!s->incarnation()||s->selectedCampaign().empty()||s->selectedFingerprint().empty()||s->sourceCatalog().empty()||w->incarnation()!=s->incarnation()||w->selectedCampaign()!=s->selectedCampaign()||w->selectedFingerprint()!=s->selectedFingerprint()||w->sourceCatalog()!=s->sourceCatalog()||(w->phase()!=Phase::GameWorldActive&&(!cleanup||w->phase()!=Phase::Inactive))||!s->captainAt(0)||!s->captainAt(1)||s->captainAt(0)==s->captainAt(1)||w->captainAt(0)!=s->captainAt(0)||w->captainAt(1)!=s->captainAt(1)||(n!=s->captainAt(0)&&n!=s->captainAt(1))||(!cleanup&&!pc_p2_original_captain_actor_alive(n)))return fail(e,"missing canonical original attachment actor authority");
 const auto epoch=s->incarnation();Frame f;MotionState m;if(!p->frame(*n,f,e)||!finite(f.position)||!std::isfinite(f.face)||!std::isfinite(f.delta)||f.delta<0||(f.controller&&(!std::isfinite(f.stickX)||!std::isfinite(f.stickY)))||!b->state(n,m,e)||s->incarnation()!=epoch)return fail(e,"missing actual source attachment frame/bank");o={s,p,b,epoch};return true;
}
struct Arg {Owner owner;std::optional<BombHandle> bomb;};std::unordered_map<Navi*,Arg> args;
bool bomb(AttachmentSource* s,BombHandle h,BombFrame& f,std::string& e){return (h.actor&&h.lifetime&&s->bomb(h,f,e)&&f.sourceBomb&&same(h,f.handle))||fail(e,"missing genuine source Bomb lifetime");}
class AttachmentState:public NativeState {
protected:
 Owner bound;bool enabled=false;std::string error;
 bool require(bool r){if(!r){enabled=false;if(error.empty())error="source attachment operation refused";std::fprintf(stderr,"P2_ORIGINAL_ATTACHMENT_REFUSED state=%d reason=%s\n",int(id_),error.c_str());}return r;}
 bool retained(Navi* n,bool cleanup=false){Owner now;return require(owner(n,now,error,cleanup)&&same(now,bound));}
 bool live(Navi* n){return enabled&&n&&n->getCurrState()==static_cast<NaviState*>(this)&&retained(n)&&n->getCurrState()==static_cast<NaviState*>(this);}
 template<class F> bool apply(Navi* n,F f,bool cleanup=false){auto* before=n->getCurrState();if(!retained(n,cleanup)||n->getCurrState()!=before)return require(fail(error,"source observer changed attachment state"));if(!require(f())||!retained(n,cleanup))return false;return require(n->getCurrState()==before||fail(error,"source callback changed attachment state"));}
 bool frame(Navi* n,Frame& f){return apply(n,[&]{return bound.source->frame(*n,f,error)&&finite(f.position)&&std::isfinite(f.face)&&std::isfinite(f.delta)&&f.delta>=0&&(!f.controller||(std::isfinite(f.stickX)&&std::isfinite(f.stickY)));});}
 bool walk(Navi* n){return require(pc_p2_original_captain_transit(n,StateId::Walk,error));}
public:
 explicit AttachmentState(StateId id):NativeState(id){}bool sourceInvincible()const override{return false;}
 bool sourceAnimationKey(Navi* n,int,std::string& e)override{if(!live(n)){e=error.empty()?"source attachment state not current":error;return false;}e.clear();return true;}
};
// naviState.cpp 282-359; previous-stick field is deliberately only assigned
// when retail init has a controller. No controller means no invented vector.
class Stuck final:public AttachmentState {
 std::optional<Vec3> previous;float idle=.45f;int wiggles=0;
 bool random(Navi* n,float& r){return apply(n,[&]{return bound.source->randomFloat(r,error)&&std::isfinite(r)&&r>=0&&r<=1;});}
 bool shake(Navi* n,Vec3 stick){return apply(n,[&]{return bound.source->forEachSticker(*n,[&](std::optional<CreatureHandle> h){if(!h)return true;if(!h->actor||!h->lifetime||!apply(n,[&]{return bound.source->sticker(*n,*h,error);}))return false;float r;if(!random(n,r))return false;if(r<=.05f)return true;if(!random(n,r))return false;float knockback=120+100*r,angle=-1000;if(!random(n,r))return false;if(r>.1f){if(!random(n,r))return false;angle=std::atan2(stick.x,stick.z)+.9424779f*(r-.5f);constexpr float tau=6.2831853071795864769f;while(angle<0)angle+=tau;while(angle>=tau)angle-=tau;}bool accepted=false;return apply(n,[&]{return bound.source->flick(*n,*h,knockback,5,angle,accepted,error);});},error);});}
public:
 Stuck():AttachmentState(StateId::Stuck){}bool sourceVsUsableY()const override{return false;}
 void init(Navi* n)override{enabled=false;error.clear();if(!require(owner(n,bound,error)))return;enabled=true;wiggles=0;idle=.45f;Frame f;if(!frame(n,f))return;if(f.controller)previous=Vec3{f.stickX,0,f.stickY};bool ignored=false;apply(n,[&]{return bound.source->releasePikis(*n,ignored,error);});}
 void exec(Navi* n)override{if(!live(n))return;Frame f;if(!frame(n,f))return;if(!f.controller||!f.stickCount){walk(n);return;}if(!apply(n,[&]{return bound.source->control(*n,error);})||!frame(n,f))return;if(!previous){require(fail(error,"retail Stuck previous stick was never initialized by actual controller"));return;}Vec3 stick{f.stickX,0,f.stickY};float magnitude=std::sqrt(stick.x*stick.x+stick.z*stick.z);if(magnitude>.3f&&stick.x*previous->x+stick.z*previous->z<.5f){++wiggles;if(wiggles>9){if(!shake(n,stick))return;wiggles=0;}previous=stick;idle=.2f;}idle-=f.delta;if(idle<0){previous=stick;wiggles=0;idle=.5f;}}
 void cleanup(Navi*)override{enabled=false;}
};
CaptureMatrix makeTR(Vec3 p,float face){CaptureMatrix m;float s=std::sin(face),c=std::cos(face);m.values={c,0,s,p.x,0,1,0,p.y,-s,0,c,p.z};return m;}
class CarryBomb final:public AttachmentState {
 std::optional<BombHandle> held;CaptureMatrix capture;bool doThrow=false,cleanupBound=false;
 bool bombFrame(Navi* n,BombFrame& f,bool cleanup=false){return held&&apply(n,[&]{return bomb(bound.source,*held,f,error);},cleanup);}
 bool matrix(Navi* n){Frame f;if(!frame(n,f))return false;Vec3 pos=f.position;pos.x+=17*std::sin(f.face);pos.y+=4.8f;pos.z+=17*std::cos(f.face);capture=makeTR(pos,f.face);return true;}
 template<class F> bool heldApply(Navi* n,F operation){BombFrame before,after;if(!bombFrame(n,before))return false;if(!apply(n,operation))return false;return bombFrame(n,after);}
 bool end(Navi* n,bool cleanup=false){BombFrame f;if(!bombFrame(n,f,cleanup))return false;return apply(n,[&]{return bound.source->endCapture(*held,error);},cleanup);}
public:
 CarryBomb():AttachmentState(StateId::CarryBomb){}
 void init(Navi* n)override{enabled=cleanupBound=false;error.clear();held.reset();doThrow=false;auto it=args.find(n);if(it==args.end()){require(fail(error,"CarryBomb requires scoped NaviCarryBombArg"));return;}Arg a=it->second;args.erase(it);Owner now;if(!require(owner(n,now,error)&&same(a.owner,now)))return;bound=now;held=a.bomb;enabled=cleanupBound=true;if(!held){walk(n);return;}BombFrame f;if(!bombFrame(n,f)||!apply(n,[&]{return bound.bank->startMotion(n,Motion::PickPut,Motion::PickPut,Listener::SourceActor,Listener::None,error);})||!apply(n,[&]{return bound.bank->enableMotionBlend(n,error);})||!heldApply(n,[&]{return bound.source->sound(*n,Sound::PickupBomb,error);})||!matrix(n))return;apply(n,[&]{return bound.source->startCapture(*n,*held,&capture,error);});}
 void exec(Navi* n)override{if(!live(n))return;if(!apply(n,[&]{return bound.source->control(*n,error);})||!matrix(n))return;if(held){BombFrame f;Frame body;if(!bombFrame(n,f)||!frame(n,body)||!heldApply(n,[&]{return bound.source->updateCapture(*n,*held,makeTR({},body.face),error);}))return;}
  if(!doThrow){if(!held){walk(n);return;}BombFrame f;if(!bombFrame(n,f))return;if(!f.capturedBy){walk(n);return;}if(f.capturedBy!=&capture){require(fail(error,"source Bomb capture rebound to another lifetime"));return;}Frame body;if(!frame(n,body))return;if(body.controller&&body.pressedA){if(apply(n,[&]{return bound.bank->finish(n,error);}))doThrow=true;}else if(body.controller&&body.pressedB)walk(n);}
 }
 bool sourceAnimationKey(Navi* n,int key,std::string& e)override{if(!live(n)){e=error.empty()?"source CarryBomb not current":error;return false;}if(key==1&&doThrow&&held){Frame f;BombFrame b;if(!frame(n,f)||!bombFrame(n,b)){e=error;return false;}Vec3 offset{260*std::sin(f.face),340,260*std::cos(f.face)};if(!heldApply(n,[&]{return bound.source->sound(*n,Sound::Throw,error);})||!heldApply(n,[&]{return bound.source->bombVelocity(*held,offset,error);})||!end(n)){e=error;return false;}held.reset();}else if(key==1000){if(!walk(n)){e=error;return false;}e.clear();return false;}e.clear();return true;}
 void cleanup(Navi* n)override{if(cleanupBound&&held)end(n,true);enabled=cleanupBound=false;}
};
} // anonymous
bool beginCarryBomb(Navi* n,std::optional<BombHandle> h,std::string& e){Owner o;if(!owner(n,o,e))return false;if(h){BombFrame f;if(!bomb(o.source,*h,f,e)||!o.bank->supports(n,Motion::PickPut,e)||!o.bank->supports(n,Motion::Nigeru,e))return false;}args[n]={o,h};if(!pc_p2_original_captain_transit(n,StateId::CarryBomb,e)){args.erase(n);return false;}return true;}
}void registerStuckCarryBombStates(NaviStateMachine& f){f.registerState(new attachments::Stuck);f.registerState(new attachments::CarryBomb);}
}}
bool pc_p2_original_captain_stuck_bomb_preflight(Navi* n,p2original::captain::StateId id,std::string& e){using namespace p2original::captain;using namespace attachments;Owner o;if(!owner(n,o,e))return false;if(id==StateId::Stuck)return true;if(id==StateId::CarryBomb){auto it=args.find(n);if(it==args.end()||!same(o,it->second.owner))return fail(e,"missing scoped original NaviCarryBombArg");if(!it->second.bomb)return true;BombFrame f;return bomb(o.source,*it->second.bomb,f,e)&&o.bank->supports(n,Motion::PickPut,e)&&o.bank->supports(n,Motion::Nigeru,e);}return fail(e,"unsupported source attachment state");}
