#include "pc_p2_original_piki_hipdrop.h"
#include "Piki.h"
#include <unordered_map>
#include <cmath>
#include <sstream>
namespace p2original { namespace piki {
namespace {
bool fail(std::string& e,const char* text){e=text;return false;}
bool finite(const Vector3f& p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);}
}
bool parseHipParameters(const std::string& raw,float gravity,HipParameters& out,std::string& e){
 const auto read=[&](const char* key,float& value){
  const std::string token=std::string("{")+key+"}";const auto at=raw.find(token);
  if(at==std::string::npos||raw.find(token,at+token.size())!=std::string::npos)return false;
  unsigned type=0;std::istringstream input(raw.substr(at+token.size()));
  return bool(input>>type>>value)&&type==4&&std::isfinite(value)&&value>=0;
 };
 HipParameters next;next.gravity=gravity;
 if(!std::isfinite(gravity)||gravity<=0||!read("P017",next.earthquakeRange)||!read("P022",next.poundDamage))
  return fail(e,"source HipDrop needs selected P017/P022 and actual gravity");
 out=next;return true;
}
struct NativeHipDrop::Impl {
 struct Entry {Handle handle;HipSnapshot state;HipParameters params;bool committed=false;};
 HipDropServices& source;std::unordered_map<Piki*,Entry> actors;
 bool busy=false,reentered=false;
 explicit Impl(HipDropServices& s):source(s){}
 struct Operation {
  Impl& p;bool entered=false;
  Operation(Impl& value,std::string& e):p(value){if(p.busy){p.reentered=true;e="reentrant source HipDrop mutation refused";}else{p.busy=true;p.reentered=false;entered=true;}}
  ~Operation(){if(entered)p.busy=false;}
  bool complete(bool value,std::string& e){return p.reentered?fail(e,"HipDrop callback attempted owner mutation"):value;}
 };
 bool current(Handle h,bool cleanup,std::string& e){
  if(!h.body||!h.lifetime||!pc_p2_original_piki_body_current(h.body,h.lifetime))return fail(e,"stale source HipDrop native lifetime");
  if(!source.current(h,cleanup,e)||!pc_p2_original_piki_body_current(h.body,h.lifetime)||reentered)return false;
  return true;
 }
 Entry* entry(Handle h,bool cleanup,std::string& e){
  auto i=actors.find(h.body);
  if(i==actors.end()||i->second.handle.lifetime!=h.lifetime||!current(h,cleanup,e))return fail(e,"source HipDrop owner unavailable"),nullptr;
  return &i->second;
 }
 bool active(Handle h,std::string& e){bool hip=false;return current(h,false,e)&&source.stateIsHipDrop(h,hip,e)&&current(h,false,e)&&hip;}
 bool clean(Entry& x,std::string& e){
  if(!current(x.handle,true,e))return false;
  if(x.state.blackDownOwned){if(!source.blackDown(x.handle,false,e)||!current(x.handle,true,e))return false;x.state.blackDownOwned=false;}
  if(x.state.forceOwned){if(!source.forceActive(x.handle,false,e)||!current(x.handle,true,e))return false;x.state.forceOwned=false;}
  return true;
 }
 bool wave(Entry& x,std::string& e){
  std::vector<HipTarget> targets;
  if(!source.cellCensus(x.handle,x.handle.body->mSRT.t,x.params.earthquakeRange,targets,e)||!current(x.handle,false,e))return false;
  for(auto t:targets){
   HipTargetFacts facts;bool accepted=false;
   if(!source.target(t,facts,e)||!current(x.handle,false,e))return false;
   if(!source.earthquake(x.handle,t,1.0f,accepted,e)||!current(x.handle,false,e))return false;
  }
  return true;
 }
 bool impact(Entry& x,bool sound,std::string& e){
  auto h=x.handle;const auto p=h.body->mSRT.t;
  if(!finite(p)||!source.blackDrop(h,p,e)||!current(h,false,e)
   ||!source.rumbleBoth(h,p,e)||!current(h,false,e)
   ||!source.vibrationBoth(h,p,e)||!current(h,false,e))return false;
  return !sound||(source.dosunSound(h,false,e)&&current(h,false,e));
 }
 bool recover(Entry& x,const HipTarget* t,const HipContact* c,std::string& e){
  const auto h=x.handle;
  // FSM::transit cleans the outgoing HipDrop BEFORE Walk initialization and
  // its subsequent invokeAI/Free selection, including BlackDown/force flags.
  if(!clean(x,e))return false;
  if(!source.walk(h,e)||!current(h,false,e))return false;
  bool selected=false;
  if(!source.invokeAI(h,t,c,t!=nullptr,selected,e)||!current(h,false,e))return false;
  if(!selected&&(!source.free(h,e)||!current(h,false,e)))return false;
  actors.erase(h.body);return true;
 }
 bool dosin(Entry& x,std::string& e){
  if(!impact(x,true,e))return false;
  x.state.phase=HipPhase::Recovery;x.state.timer=.3f;
  return wave(x,e);
 }
};
NativeHipDrop::NativeHipDrop(HipDropServices& s):impl(new Impl(s)){}
NativeHipDrop::~NativeHipDrop()=default;
bool NativeHipDrop::begin(Handle h,std::string& e){
 auto& p=*impl;Impl::Operation op(p,e);if(!op.entered)return false;
 try{
  if(p.actors.count(h.body)||!p.active(h,e)||!p.source.supportsFall(h,e)||!p.current(h,false,e))return false;
  OriginalPikiBodyHandle body;HipParameters params;
  if(!pc_p2_original_piki_body_handle(h.body,body)||body.nativeLifetime!=h.lifetime||body.body.state.species!=3
   ||!p.source.parameters(h,params,e)||!p.current(h,false,e)||!std::isfinite(params.gravity)||params.gravity<=0
   ||!std::isfinite(params.poundDamage)||params.poundDamage<0||!std::isfinite(params.earthquakeRange)||params.earthquakeRange<0)
   return fail(e,"source HipDrop selected Purple parameters unavailable");
  Impl::Entry pending;pending.handle=h;pending.params=params;pending.state.timer=.25f;
  auto pair=p.actors.emplace(h.body,pending);auto& x=pair.first->second;
  h.body->mVelocity.set(0,0,0);
  if(!p.source.killThrow(h,e)||!p.active(h,e))return false;
  x.state.blackDownOwned=true;
  if(!p.source.blackDown(h,true,e)||!p.active(h,e))return false;
  x.state.forceOwned=true;
  if(!p.source.forceActive(h,true,e)||!p.active(h,e))return false;
  x.committed=true;return op.complete(true,e);
 }catch(...){return fail(e,"source HipDrop init threw; pending cleanup retained");}
}
bool NativeHipDrop::update(Handle h,float dt,std::string& e){
 auto& p=*impl;Impl::Operation op(p,e);if(!op.entered)return false;
 try{
  auto* x=p.entry(h,false,e);if(!x||!x->committed||!p.active(h,e)||!std::isfinite(dt)||dt<0)return false;
  auto& s=x->state;
  if(s.phase==HipPhase::Pause){
   h.body->mVelocity.y=0;s.timer-=dt;
   if(s.timer<=0){
    h.body->mVelocity.y=-x->params.gravity*.5f;
    std::vector<HipTarget> targets;
    const auto start=h.body->mSRT.t;
    if(!finite(start)||!p.source.cellCensus(h,start,50,targets,e)||!p.active(h,e))return false;
    HipTarget nearest;Vector3f destination;float minDistance=12800;
    for(auto t:targets){HipTargetFacts facts;
     if(!p.source.target(t,facts,e)||!p.active(h,e)||!finite(facts.position))return false;
     if(facts.teki&&facts.alive&&facts.living){const auto diff=facts.position-start;const float distance=std::sqrt(diff.dot(diff));
      if(distance<minDistance){minDistance=distance;nearest=t;destination=facts.position;}}
    }
    if(nearest.body){HipTargetFacts facts;if(!p.source.target(nearest,facts,e)||!p.active(h,e))return false;destination=facts.position;
     const auto offset=destination-start;const float distance=std::sqrt(offset.x*offset.x+offset.z*offset.z);
     if(distance>0){h.body->mVelocity.x=offset.x*120/distance;h.body->mVelocity.z=offset.z*120/distance;}
    }
    s.phase=HipPhase::Descent;
    if(!p.source.fallMotion(h,e)||!p.active(h,e))return false;
   }
  }else if(s.phase==HipPhase::Descent){
   constexpr float pi=3.14159265358979323846f;
   h.body->mFaceDirection=std::fmod(h.body->mFaceDirection+dt*pi/.2f,2*pi);
   if(h.body->mFaceDirection<0)h.body->mFaceDirection+=2*pi;
  }else{
   h.body->mTargetVelocity.set(0,0,0);s.timer-=dt;
   if(s.timer<=0)return op.complete(p.recover(*x,nullptr,nullptr,e),e);
  }
  return op.complete(true,e);
 }catch(...){return fail(e,"source HipDrop exec threw; owner retained");}
}
bool NativeHipDrop::bounce(Handle h,std::string& e){
 auto& p=*impl;Impl::Operation op(p,e);if(!op.entered)return false;
 try{auto* x=p.entry(h,false,e);return x&&x->committed&&p.active(h,e)&&op.complete(x->state.phase==HipPhase::Recovery||p.dosin(*x,e),e);}
 catch(...){return fail(e,"source HipDrop floor callback threw; owner retained");}
}
bool NativeHipDrop::platform(Handle h,std::string& e){return bounce(h,e);}
bool NativeHipDrop::collision(Handle h,HipTarget target,HipContact contact,std::string& e){
 auto& p=*impl;Impl::Operation op(p,e);if(!op.entered)return false;
 try{
  auto* x=p.entry(h,false,e);HipTargetFacts facts;
  if(!x||!x->committed||!p.active(h,e)||!p.source.target(target,facts,e)||!p.current(h,false,e))return false;
  if(facts.piki)return op.complete(true,e);
  if(!p.impact(*x,!facts.teki,e))return false;
  if(facts.teki){
   bool accepted=false;
   if(h.body->mVelocity.y<0){
    if(!p.source.hipdrop(h,target,x->params.poundDamage,contact,accepted,e)||!p.current(h,false,e)||!p.wave(*x,e))return false;
   }
   // Read REAL velocity again after hipdrop/earthquake callbacks. Press may
   // overwrite the Hipdrop accepted result exactly as the retail routine.
   if(h.body->mVelocity.y<0){
    HipTargetFacts reread;if(!p.source.target(target,reread,e)||!p.current(h,false,e)
     ||!p.source.press(h,target,10.0f,contact,accepted,e)||!p.current(h,false,e))return false;
   }
   if(!p.source.dosunSound(h,true,e)||!p.current(h,false,e))return false;
   if(!accepted&&contact.part){
    bool stickable=false;HipTargetFacts reread;
    if(!p.source.contact(contact,stickable,e)||!p.current(h,false,e)||!p.source.target(target,reread,e)||!p.current(h,false,e))return false;
    if(stickable&&reread.living){
     if(!p.source.stick(h,target,contact,e)||!p.current(h,false,e)||!p.source.attachSound(h,target,e)||!p.current(h,false,e))return false;
    }
   }
  }
  bool hip=false;if(!p.source.stateIsHipDrop(h,hip,e)||!p.current(h,false,e))return false;
  if(hip)return op.complete(p.recover(*x,&target,&contact,e),e);
  return op.complete(true,e);
 }catch(...){return fail(e,"source HipDrop contact callback threw; owner retained");}
}
bool NativeHipDrop::snapshot(Handle h,HipSnapshot& out)const noexcept{
 auto& p=*impl;auto i=p.actors.find(h.body);if(i==p.actors.end()||i->second.handle.lifetime!=h.lifetime||!pc_p2_original_piki_body_current(h.body,h.lifetime))return false;out=i->second.state;return true;
}
bool NativeHipDrop::retire(Handle h,std::string& e){
 auto& p=*impl;Impl::Operation op(p,e);if(!op.entered)return false;
 try{auto* x=p.entry(h,true,e);if(!x||!p.clean(*x,e)||!op.complete(true,e))return false;p.actors.erase(h.body);return true;}
 catch(...){return fail(e,"source HipDrop cleanup threw; owner retained");}
}
bool NativeHipDrop::retireScene(std::string& e){
 auto& p=*impl;Impl::Operation op(p,e);if(!op.entered)return false;
 try{for(auto i=p.actors.begin();i!=p.actors.end();){if(!p.clean(i->second,e)||!op.complete(true,e))return false;i=p.actors.erase(i);}return op.complete(true,e);}
 catch(...){return fail(e,"source HipDrop scene cleanup threw; owner retained");}
}
bool NativeHipDrop::owned()const noexcept{return impl->busy||!impl->actors.empty();}
} }
