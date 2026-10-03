#include "pc_p2_original_captain_container.h"
#include "pc_p2_original_captain_states.h"
#include "pc_p2_original_captain_motion.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <climits>
#include <unordered_map>
extern p2original::captain::items::ContainerSource* pc_p2_original_captain_container_source(const Navi*) __attribute__((weak));
extern p2original::captain::items::AbsorbSource* pc_p2_original_captain_absorb_source(const Navi*) __attribute__((weak));
extern p2original::captain::actions::ActionSource* pc_p2_original_captain_action_source(const Navi*) __attribute__((weak));
namespace p2original {namespace captain {namespace items {namespace {
bool fail(std::string& e,const char* s){e=s;return false;}
bool finite(Vec3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
struct Context {const LoadedScene* scene=nullptr;actions::ActionSource* action=nullptr;SourceBank* bank=nullptr;std::uint64_t epoch=0;};
bool context(Navi* n,Context& out,std::string& e){
 if(!n||!pc_p2_original_captain_action_source)return fail(e,"missing original item action body provider");
 auto* s=pc_p2_original_captain_loaded_scene();auto* w=pc_p2_original_captain_world();auto* a=pc_p2_original_captain_action_source(n);auto* b=pc_p2_original_captain_source_bank();
 if(!s||!w||!a||!b||&a->scene()!=s||!s->incarnation()||s->selectedCampaign().empty()||s->selectedFingerprint().empty()||s->sourceCatalog().empty()||w->incarnation()!=s->incarnation()||w->selectedCampaign()!=s->selectedCampaign()||w->selectedFingerprint()!=s->selectedFingerprint()||w->sourceCatalog()!=s->sourceCatalog()||!s->captainAt(0)||!s->captainAt(1)||s->captainAt(0)==s->captainAt(1)||(n!=s->captainAt(0)&&n!=s->captainAt(1))||w->phase()!=Phase::GameWorldActive||!pc_p2_original_captain_actor_alive(n))return fail(e,"item action owner is not canonical live original scene/actor");
 MotionState m;if(!b->state(n,m,e))return false;out={s,a,b,s->incarnation()};return true;
}
bool same(const Context& a,const Context& b){return a.scene==b.scene&&a.action==b.action&&a.bank==b.bank&&a.epoch==b.epoch;}
bool same(OnyonHandle a,OnyonHandle b){return a.actor==b.actor&&a.lifetime==b.lifetime;}
bool same(HoneyHandle a,HoneyHandle b){return a.actor==b.actor&&a.lifetime==b.lifetime;}
bool same(ScreenHandle a,ScreenHandle b){return a.actor==b.actor&&a.lifetime==b.lifetime;}
bool same(CameraHandle a,CameraHandle b){return a.actor==b.actor&&a.lifetime==b.lifetime;}
struct ContainerArg {Context owner;ContainerSource* source=nullptr;OnyonHandle onyon;ScreenHandle screen;};
struct AbsorbArg {Context owner;AbsorbSource* source=nullptr;HoneyHandle honey;CameraHandle camera;};
std::unordered_map<Navi*,ContainerArg> containerArgs;std::unordered_map<Navi*,AbsorbArg> absorbArgs;
bool containerOwner(Navi* n,OnyonHandle h,ContainerArg& out,std::string& e){
 Context c;if(!context(n,c,e)||!pc_p2_original_captain_container_source)return fail(e,"missing genuine Container source owner");
 auto* p=pc_p2_original_captain_container_source(n);ScreenHandle ui;OnyonFrame onyon;
 if(!p||&p->scene()!=c.scene||!p->screen(ui,e)||!ui.actor||!ui.lifetime)return fail(e,"actual Section Game2DMgr menu observer unavailable");
 if(!h.actor||!h.lifetime||!p->onyon(h,onyon,e)||!same(onyon.handle,h)||onyon.type<0||onyon.type>4)return fail(e,"invalid actual source Onyon lifetime/type");
 out={c,p,h,ui};return true;
}
bool absorbOwner(Navi* n,HoneyHandle h,AbsorbArg& out,std::string& e){
 Context c;if(!context(n,c,e)||!pc_p2_original_captain_absorb_source)return fail(e,"missing genuine Absorb source owner");
 auto* p=pc_p2_original_captain_absorb_source(n);CameraHandle camera;HoneyFrame honey;
 if(!p||&p->scene()!=c.scene||!p->camera(camera,e)||!camera.actor||!camera.lifetime)return fail(e,"actual source CameraMgr observer unavailable");
 if(!h.actor||!h.lifetime||!p->honey(h,honey,e)||!same(honey.handle,h)||!honey.honey||!finite(honey.position)||int(honey.type)<0||int(honey.type)>2)return fail(e,"invalid actual source Honey lifetime/type");
 if(!c.bank->supports(n,Motion::Mizunomi,e))return false;
 out={c,p,h,camera};return true;
}
class ItemState:public NativeState {
protected:
 Context owner;bool enabled=false,bound=false;std::string error;
 bool require(bool r){if(!r){enabled=false;if(error.empty())error="source item action operation refused";std::fprintf(stderr,"P2_ORIGINAL_ITEM_ACTION_REFUSED state=%d reason=%s\n",int(id_),error.c_str());}return r;}
 bool current(Navi* n){if(!enabled||!n||n->getCurrState()!=static_cast<NaviState*>(this))return false;Context now;if(!require(context(n,now,error)))return false;return require(same(now,owner)||fail(error,"stale source item action context"));}
 bool walk(Navi* n){return require(pc_p2_original_captain_transit(n,StateId::Walk,error));}
public:
 explicit ItemState(StateId id):NativeState(id){}bool sourceInvincible()const override{return true;}
 virtual bool animationPreflight(Navi*,std::string&)=0;
};
class ContainerState final:public ItemState {
 ContainerSource* source=nullptr;OnyonHandle onyon;ScreenHandle screen;bool open=false;int shipColor=4;
 bool live(Navi* n){if(!current(n))return false;ContainerArg now;if(!require(containerOwner(n,onyon,now,error)))return false;return require((source==now.source&&same(screen,now.screen))||fail(error,"stale actual Onyon/menu owner"));}
 bool frame(OnyonFrame& f){return require(source->onyon(onyon,f,error)&&(same(f.handle,onyon)||fail(error,"stale source Onyon")));}
 bool enter(Navi* n,int count){
  std::vector<actions::PikiFrame> squad;if(!require(source->squad(*n,squad,error)))return false;OnyonFrame f;if(!frame(f))return false;
  std::vector<PikiHandle> selected;
  for(const auto& p:squad){if(!p.handle.actor||!p.handle.lifetime||p.kind>=7)return require(fail(error,"missing actual source CPlate Piki identity/kind"));
   if(int(p.kind)==(f.type==4?shipColor:f.type)){selected.push_back(p.handle);if(int(selected.size())>=count)break;}
  }
  if(selected.size()>100)return require(fail(error,"source Container CPlate exceeded MAX_PIKI_COUNT"));
  // Selection completes before brain actions mutate the real CPlate.
  for(auto p:selected){if(!require(source->enterPiki(*n,p,onyon,error)))return false;}return true;
 }
 MenuData data(const ContainerFacts& f,int color,int stored){MenuData d;d.color=color;d.inOnyon=stored;d.inSquad=f.squad[color];d.inParty=f.partyTotal;d.onMap=f.mapCount-f.zikatuCount;d.maxPikis=100-f.zikatuCount;return d;}
 bool result(Navi* n,int count,int color){if(count==INT_MIN)return require(fail(error,"invalid actual menu withdrawal result"));if(count<0)return require(source->exitPikis(onyon,-count,color,error));if(count>0){shipColor=color;return enter(n,count);}return true;}
public:
 ContainerState():ItemState(StateId::Container){}
 bool animationPreflight(Navi* n,std::string& e)override{bool valid=live(n);e=valid?std::string():error;return valid;}
 void init(Navi* n)override{
  enabled=bound=false;open=false;auto it=containerArgs.find(n);if(it==containerArgs.end()){error="Container requires actual scoped NaviContainerArg";require(false);return;}
  ContainerArg arg=it->second;containerArgs.erase(it);ContainerArg now;if(!require(containerOwner(n,arg.onyon,now,error)&&same(arg.owner,now.owner)&&arg.source==now.source&&same(arg.screen,now.screen)))return;
  owner=now.owner;source=now.source;onyon=now.onyon;screen=now.screen;enabled=bound=true;
  if(!require(source->setGamePad(screen,*n,error)))return;
  OnyonFrame f;ContainerFacts facts;if(!frame(f)||!require(source->facts(*n,facts,error)))return;
  for(int i=0;i<5;++i)if(facts.stored[i]<0||facts.squad[i]<0){require(fail(error,"invalid actual source container counts"));return;}
  if(facts.partyTotal<0||facts.mapCount<0||facts.zikatuCount<0||facts.zikatuCount>100){require(fail(error,"invalid actual source GameStat counts"));return;}
  shipColor=f.type;
  if(f.type==4){ShipMenu menu;menu.white=data(facts,4,int(std::max<std::int64_t>(0,std::int64_t(facts.stored[4])-f.whitesQueued)));menu.purple=data(facts,3,int(std::max<std::int64_t>(0,std::int64_t(facts.stored[3])-f.purplesQueued)));menu.whiteOwned=facts.whiteOwned;menu.purpleOwned=facts.purpleOwned;menu.debtPaid=facts.debtPaid;
   if(menu.whiteOwned||menu.purpleOwned){if(!require(source->openShip(screen,menu,open,error)))return;}
  }else if(!require(source->openOnyon(screen,data(facts,f.type,facts.stored[f.type]),open,error)))return;
  if(open)require(source->freeze(true,error)&&source->moviePause(true,error));
 }
 void exec(Navi* n)override{
  if(!live(n))return;
  if(!open){walk(n);return;}OnyonFrame f;if(!frame(f))return;
  MenuCheck status;if(!require(source->check(screen,f.type==4,status,error)))return;
  if(status==MenuCheck::Cancel){walk(n);return;}
  if(status==MenuCheck::Confirmed){int a=0,b=0;if(!require(source->result(screen,f.type==4,a,b,error)))return;
   int count=a,color=f.type;if(f.type==4){if(a!=0)color=4;else if(b!=0){count=b;color=3;}else count=0;}
   if(result(n,count,color))walk(n);
   return;
  }
  require(source->velocities(*n,{},{},error)); // source default includes UI error code
 }
 void cleanup(Navi*)override{if(bound){require(source->freeze(false,error)&&source->moviePause(false,error));}enabled=bound=false;}
 bool sourceAnimationKey(Navi* n,int,std::string& e)override{if(!live(n)){e=error.empty()?"source Container key state not current":error;return false;}e.clear();return true;} // inherited source key noop
};
class AbsorbState final:public ItemState {
 AbsorbSource* source=nullptr;HoneyHandle honey;CameraHandle camera;std::uint8_t substate=0;bool absorbed=false,retained=false;
 bool live(Navi* n){if(!current(n))return false;AbsorbArg now;if(!require(absorbOwner(n,honey,now,error)))return false;return require((source==now.source&&same(camera,now.camera))||fail(error,"stale source Honey/camera owner"));}
 bool frame(HoneyFrame& f){return require(source->honey(honey,f,error)&&((same(f.handle,honey)&&f.honey)||fail(error,"stale retained source Honey metadata")));}
public:
 AbsorbState():ItemState(StateId::Absorb){}
 bool sourcePressable()const override{return false;}
 bool sourceVsUsableY()const override{return false;}
 bool animationPreflight(Navi* n,std::string& e)override{bool valid=live(n);e=valid?std::string():error;return valid;}
 void init(Navi* n)override{
  enabled=bound=retained=false;substate=0;absorbed=false;auto it=absorbArgs.find(n);if(it==absorbArgs.end()){error="Absorb requires actual scoped NaviAbsorbArg";require(false);return;}
  AbsorbArg arg=it->second;absorbArgs.erase(it);AbsorbArg now;if(!require(absorbOwner(n,arg.honey,now,error)&&same(arg.owner,now.owner)&&arg.source==now.source&&same(arg.camera,now.camera)))return;
  owner=now.owner;source=now.source;honey=now.honey;camera=now.camera;enabled=bound=true;
  if(!require(source->retain(honey,error)))return;
  retained=true;
  if(!require(owner.bank->startMotion(n,Motion::Mizunomi,Motion::Mizunomi,Listener::SourceActor,Listener::None,error)&&source->drinkSound(*n,error)))return;
  HoneyFrame f;if(!frame(f))return;
  require(source->turnTo(*n,f.position,error)&&source->lockCamera(camera,*n,true,error)&&source->startNearLow(camera,*n,error));
 }
 void exec(Navi* n)override{
  if(!live(n))return;
  actions::ActorFrame body;if(!require(owner.action->frame(*n,body,error)&&finite(body.velocity)))return;
  if(!require(source->velocities(*n,{0,body.velocity.y,0},{},error)))return;
  if(substate==1&&!absorbed){HoneyFrame f;if(!frame(f))return;if(f.alive){bool accepted=false;if(!require(source->absorb(*n,honey,accepted,error)))return;absorbed=true;}}
  MotionState motion;if(!require(owner.bank->stateAnimator(n,p2original::captain::Animator::Self,motion,error)))return;
  if(motion.motion!=Motion::Mizunomi)walk(n);
 }
 bool sourceAnimationKey(Navi* n,int k,std::string& e)override{
  if(!live(n)){e=error.empty()?"source Absorb key state not current":error;return false;}
  if(k==0)substate=1;
  else if(k==1){HoneyFrame f;if(!frame(f)){e=error;return false;}if(!f.alive||!f.shrinking){substate=2;if(!require(owner.bank->finish(n,error))){e=error;return false;}}}
  else if(k==1000){if(absorbed){HoneyFrame f;if(!frame(f)||!require(source->creditSpray(*n,honey,f.type!=Honey::Red,error))){e=error;return false;}}
   if(!walk(n)){e=error;return false;}e.clear();return false;
  }e.clear();return true;
 }
 void cleanup(Navi* n)override{
  if(bound)require(source->lockCamera(camera,*n,false,error)&&source->finishCamera(camera,*n,error));
  if(retained)require(source->release(honey,error));
  enabled=bound=retained=false;
 }
};
} // anonymous
bool beginContainer(Navi* n,OnyonHandle h,std::string& e){ContainerArg arg;if(!containerOwner(n,h,arg,e))return false;containerArgs[n]=arg;if(!pc_p2_original_captain_transit(n,StateId::Container,e)){containerArgs.erase(n);return false;}return true;}
bool beginAbsorb(Navi* n,HoneyHandle h,std::string& e){AbsorbArg arg;if(!absorbOwner(n,h,arg,e))return false;absorbArgs[n]=arg;if(!pc_p2_original_captain_transit(n,StateId::Absorb,e)){absorbArgs.erase(n);return false;}return true;}
} void registerContainerAbsorbStates(NaviStateMachine& fsm){fsm.registerState(new items::ContainerState);fsm.registerState(new items::AbsorbState);}
}}
bool pc_p2_original_captain_container_absorb_preflight(Navi* n,p2original::captain::StateId id,std::string& e){using namespace p2original::captain;using namespace items;
 if(id==StateId::Container){auto it=containerArgs.find(n);if(it==containerArgs.end())return fail(e,"missing scoped NaviContainerArg");ContainerArg now;return containerOwner(n,it->second.onyon,now,e)&&same(it->second.owner,now.owner)&&now.source==it->second.source&&same(now.screen,it->second.screen);}
 if(id==StateId::Absorb){auto it=absorbArgs.find(n);if(it==absorbArgs.end())return fail(e,"missing scoped NaviAbsorbArg");AbsorbArg now;return absorbOwner(n,it->second.honey,now,e)&&same(it->second.owner,now.owner)&&now.source==it->second.source&&same(now.camera,it->second.camera);}
 return fail(e,"unsupported source item action state");
}
bool pc_p2_original_captain_container_absorb_advance_animation(Navi* n,float frames,std::string& e){using namespace p2original::captain;using namespace items;
 if(!n||!std::isfinite(frames)||frames<0)return fail(e,"invalid source item action animation amount");
 auto* state=dynamic_cast<ItemState*>(n->getCurrState());auto* bank=pc_p2_original_captain_source_bank();if(!state||!bank)return fail(e,"source Container/Absorb state/bank not current");
 if(!state->animationPreflight(n,e))return false;
 MotionState before;if(!bank->stateAnimator(n,p2original::captain::Animator::Bound,before,e))return false;auto emit=[&](int key){return state->sourceAnimationKey(n,key,e);};
 if(!bank->advanceAnimator(n,p2original::captain::Animator::Self,frames,emit,e))return false;
 if(n->getCurrState()!=static_cast<NaviState*>(state)){e.clear();return true;}
 MotionState now;if(!bank->stateAnimator(n,p2original::captain::Animator::Bound,now,e))return false;if(before.generation!=now.generation){e.clear();return true;}return bank->advanceAnimator(n,p2original::captain::Animator::Bound,frames,emit,e);
}
