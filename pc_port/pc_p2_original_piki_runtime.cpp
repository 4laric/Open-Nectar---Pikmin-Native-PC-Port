#include "pc_p2_original_piki_runtime.h"
#include "pc_p2_original_piki_brain.h"
#include "pc_p2_original_captain_damage.h"
#include "pc_p2_original_piki_recruit.h"
#include "Piki.h"
#include "Navi.h"
#include "MapCode.h"
#include "MapMgr.h"
#include <cmath>
#include <algorithm>
#include <sstream>
#include <unordered_map>
namespace p2original { namespace piki {
namespace {
struct Entry {
 Handle handle; RuntimeState runtime; Parameters params;
 std::uint64_t scene=0;
 std::string pikiBytes,naviBytes;
};
Services* services=nullptr;
std::unordered_map<const Piki*,Entry> actors;
bool fail(std::string& e,const char* text){e=text;return false;}
bool finite(const Vector3f& v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
bool parameter(const std::string& raw,const char* key,float& out){
 const std::string token=std::string("{")+key+"}";
 auto pos=raw.find(token);
 if(pos==std::string::npos || raw.find(token,pos+token.size())!=std::string::npos)return false;
 std::istringstream stream(raw.substr(pos+token.size())); unsigned type=0; float value=0;
 if(!(stream>>type>>value)||type!=4||!std::isfinite(value))return false;
 out=value;return true;
}
bool canonical(std::string& e){
 auto* loaded=pc_p2_original_captain_loaded_scene();auto* world=pc_p2_original_captain_world();
 if(!services||!loaded||&services->scene()!=loaded||!world||!loaded->incarnation()
 ||loaded->incarnation()!=world->incarnation()||loaded->selectedCampaign()!=world->selectedCampaign()
 ||loaded->selectedFingerprint()!=world->selectedFingerprint()||loaded->sourceCatalog()!=world->sourceCatalog()
 ||world->phase()!=captain::Phase::GameWorldActive)return fail(e,"SourcePiki has no canonical active source scene");
 return true;
}
bool body(const Piki* p,PcP2SourceBody& out,std::string& e){
 if(!canonical(e)||!p)return false;
 auto kind=pc_p2_source_body_query(p,out);
 if(kind!=PcP2SourceBodyKind::GenPiki)return fail(e,"SourcePiki runtime currently requires actual GenPiki lifetime producer");
 if(kind==PcP2SourceBodyKind::GenPiki){
  OriginalPikiBodyHandle current;if(!pc_p2_original_piki_body_handle(p,current))return fail(e,"missing GenPiki native lifetime");
  out.nativeBody=p;out.nativeLifetime=current.nativeLifetime;
 }
 if(kind==PcP2SourceBodyKind::None||kind==PcP2SourceBodyKind::Unavailable||!out.nativeLifetime)
 return fail(e,"SourcePiki lacks current canonical native lifetime");
 return pc_p2_source_body_admitted(out,services->scene().selectedCampaign(),services->scene().sourceCatalog(),e);
}
Entry* current(Handle h,std::string& e){
 PcP2SourceBody b;
 if(!h.body||!h.lifetime||!body(h.body,b,e)||b.nativeLifetime!=h.lifetime)return fail(e,"stale SourcePiki handle"),nullptr;
 auto i=actors.find(h.body);
 if(i==actors.end()||i->second.handle.lifetime!=h.lifetime||i->second.scene!=services->scene().incarnation())
 return fail(e,"SourcePiki FSM not initialized in this lifetime/scene"),nullptr;
 if(i->second.pikiBytes!=services->pikiParameterBytes()||i->second.naviBytes!=services->naviParameterBytes())
 return fail(e,"SourcePiki selected parameter bytes changed"),nullptr;
 return &i->second;
}
Motion motionFor(State s){switch(s){case State::GoHang:return Motion::Run2;case State::Hanged:return Motion::Hang;case State::Flying:return Motion::RollJump;case State::LookAt:return Motion::Notice;default:return Motion::Wait;}}
bool cleanup(Entry& x,std::string& e){
 auto& r=x.runtime;
 if(r.state==State::GoHang)r.collisionFlick=true;
 if(r.state==State::Hanged)r.atari=true;
 if(r.state==State::Flying){
  if(!services->throwEffects(x.handle,false,e))return false;
  r.forceActive=false;r.moveVelocity=true;
 }
 return true;
}
bool enter(Entry& x,State state,std::string& e){
 if(state!=State::Walk&&state!=State::GoHang&&state!=State::Hanged&&state!=State::Flying&&state!=State::LookAt)
  return fail(e,"unsupported source Piki state identity");
 if(!services->supports(x.handle,motionFor(state),e))return false;
 // Purple HipDrop is a separate source state: never silently substitute Walk.
 PcP2SourceBody b;if(!body(x.handle.body,b,e))return false;
 if(state==State::Flying&&b.state.species==3)return fail(e,"SourcePiki Purple Flying requires unported HipDrop");
 if(!cleanup(x,e)||!services->motion(x.handle,motionFor(state),e))return false;
 auto& r=x.runtime;auto* p=x.handle.body;
 r.state=state;
 if(state==State::GoHang)r.collisionFlick=false;
 if(state==State::Hanged){
  p->mVelocity.set(0,0,0);p->mTargetVelocity.set(0,0,0);r.atari=false;
  return services->hangSound(x.handle,e);
 }
 if(state==State::Flying){
  r.flowerFalling=false;r.flyingFrames=0;r.moveVelocity=false;r.forceActive=true;
  return services->throwEffects(x.handle,true,e);
 }
 return true;
}
}
bool parseParameters(const std::string& pb,const std::string& nb,float g,Parameters& out,std::string& e){
 Parameters p;
 if(!parameter(pb,"p000",p.walk)||!parameter(pb,"p001",p.run)||!parameter(pb,"p054",p.flowerRun)
 ||!parameter(pb,"p065",p.budRun)||!parameter(pb,"P001",p.whiteMultiplier)||!parameter(pb,"P002",p.purpleMultiplier)
 ||!parameter(pb,"p048",p.flowerGravity)||!parameter(pb,"p029",p.whiteDistance)||!parameter(pb,"p030",p.grayDistance)
 ||!parameter(pb,"p031",p.lostTime)||!parameter(pb,"s003",p.acceleration)
 ||!parameter(pb,"p003",p.referenceHealth)||!parameter(pb,"P004",p.whiteAttackDamage)
 ||!parameter(nb,"p037",p.grabRange)||!parameter(nb,"p026",p.landingTime)||!parameter(nb,"p025",p.heightMin)
 ||!parameter(nb,"p024",p.heightMax)||!parameter(nb,"p054",p.heightYellow)||!parameter(nb,"q000",p.heightPurple)
 ||!parameter(nb,"q001",p.heightWhite))return fail(e,"missing/duplicate/nonfinite selected source Piki/Navi parameter");
 p.gravity=g;
 if(!std::isfinite(g)||g<=0||p.run<=0||p.walk<0||p.flowerRun<=0||p.budRun<=0||p.whiteMultiplier<=0
 ||p.purpleMultiplier<=0||p.flowerGravity<=0||p.flowerGravity>1||p.whiteDistance<0||p.grayDistance<p.whiteDistance
 ||p.lostTime<0||p.acceleration<=0||p.grabRange<0||p.landingTime<=0||p.heightMin<0||p.heightMax<0
 ||p.heightYellow<0||p.heightWhite<0||p.heightPurple<0||p.referenceHealth<0||p.whiteAttackDamage<0)return fail(e,"invalid selected source Piki/Navi physics parameter");
 out=p;return true;
}
bool installServices(Services& s)noexcept {if(services)return services==&s;services=&s;return true;}
bool initialize(Piki* p,std::string& e){
 PcP2SourceBody b;if(!body(p,b,e))return false;
 if(actors.count(p))return fail(e,"SourcePiki already initialized; forget old lifetime before reuse");
 Entry x;x.handle={p,b.nativeLifetime};x.scene=services->scene().incarnation();
 x.pikiBytes=services->pikiParameterBytes();x.naviBytes=services->naviParameterBytes();
 float g=0;if(!services->gravity(g,e)||!parseParameters(x.pikiBytes,x.naviBytes,g,x.params,e))return false;
 for(auto m:{Motion::Wait,Motion::Walk,Motion::Run2,Motion::Hang,Motion::RollJump,Motion::Notice})if(!services->supports(x.handle,m,e))return false;
 if(!services->motion(x.handle,Motion::Wait,e)||!brainFree(x.handle,x.runtime,*services,e))return false;
 actors.emplace(p,std::move(x));return true;
}
bool handle(const Piki* p,Handle& out){std::string e;auto i=actors.find(p);if(i==actors.end()||!current(i->second.handle,e))return false;out=i->second.handle;return true;}
bool snapshot(Handle h,RuntimeState& out){std::string e;auto* x=current(h,e);if(!x)return false;out=x->runtime;return true;}
bool frame(Handle h,Frame& out,std::string& e){
 auto* x=current(h,e);if(!x)return false;
 PcP2SourceBody source;if(!body(h.body,source,e)||!finite(h.body->mSRT.t)||h.body->mHappa<0||h.body->mHappa>2)return false;
 Frame next;next.handle=h;next.position=h.body->mSRT.t;next.state=x->runtime.state;next.species=source.state.species;
 next.happa=h.body->mHappa;next.captain=h.body->mNavi;
 if(x->runtime.brain.action==Action::Formation)next.formationSlot=x->runtime.brain.slot;
 next.throwable=next.state==State::Walk||next.state==State::GoHang||next.state==State::Hanged;
 next.releasable=next.state==State::Walk;
 out=next;return true;
}
bool squad(Navi* n,std::vector<Frame>& out,std::string& e){
 if(!canonical(e)||(services->scene().captainAt(0)!=n&&services->scene().captainAt(1)!=n))return false;
 std::vector<Frame> next;
 for(const auto& pair:actors){
  const auto& x=pair.second;
  if(x.runtime.brain.action==Action::Formation&&x.runtime.brain.navi==n){
   Frame member;if(!frame(x.handle,member,e)||member.captain!=n||member.formationSlot<0)return false;
   next.push_back(member);
  }
 }
 // Source CPlate iteration is slot order, never pointer/hash-map iteration.
 std::sort(next.begin(),next.end(),[](const Frame& a,const Frame& b){return a.formationSlot<b.formationSlot;});
 for(std::size_t i=1;i<next.size();++i)if(next[i-1].formationSlot==next[i].formationSlot)return fail(e,"duplicate source CPlate slot");
 out=std::move(next);return true;
}
bool sortFormation(Handle h,unsigned happa,std::string& e){
 auto* x=current(h,e);if(!x||happa>2||x->runtime.brain.action!=Action::Formation||x->runtime.brain.slot<0)return false;
 if(!services->sortSlot(h,x->runtime.brain.navi,x->runtime.brain.slot,happa,e))return false;
 x->runtime.brain.sortState=2;return true;
}
bool transition(Handle h,State s,std::string& e){
 auto* x=current(h,e);if(!x)return false;
 if(s==State::Flying)return x->runtime.state==State::Flying||fail(e,"SourcePiki Flying entry requires actual source launch mechanics");
 if(s==State::LookAt)return fail(e,"SourcePiki LookAt entry requires actual source whistle receiver");
 if(s==State::GoHang||s==State::Hanged){
  if(x->runtime.state!=State::Walk&&x->runtime.state!=State::GoHang)return fail(e,"invalid source grab transition");
  CaptainFrame frame;
  if(x->runtime.brain.action!=Action::Formation||!h.body->mNavi
   ||!services->captainFrame(h.body->mNavi,frame,e)||!frame.throwWait||!frame.alive)return fail(e,"source grab requires actual Formation captain ThrowWait");
 }
 return enter(*x,s,e);
}
bool animate(Handle h,float dt,std::string& e){auto* x=current(h,e);return x&&std::isfinite(dt)&&dt>=0&&services->animate(h,dt,e);}
bool animationKey(Handle h,unsigned type,std::string& e){
 auto* x=current(h,e);if(!x)return false;
 if(x->runtime.state==State::LookAt&&type==1000)x->runtime.lookSubState=2;
 return true;
}
bool moveVelocity(Handle h,float dt,std::string& e){
 auto* x=current(h,e);if(!x||!std::isfinite(dt)||dt<0)return false;
 if(!x->runtime.moveVelocity)return true;
 auto* p=h.body;Vector3f velocity=p->mTargetVelocity,extra(0,0,0);
 if(p->mGroundTriangle){
  Vector3f normal=p->mGroundTriangle->mTriangle.mNormal;
  float speed=velocity.length();velocity=velocity-normal*velocity.dot(normal);velocity.normalise();velocity=velocity*speed;
  int slip=MapCode::getSlipCode(p->mGroundTriangle);
  Vector3f falling(0,-x->params.gravity*dt,0);falling=falling-normal*falling.dot(normal);
  if(slip==0){if(speed<0.1f)extra=falling*-1;}
  else{falling.normalise();extra=falling*(x->params.gravity*dt*(slip==2?2.5f:1));}
 }
 // Literal FakePiki::moveVelocity interpolation (0.1 seconds), independent of
 // the native P1 Creature property and all randomizer color multipliers.
 p->mVelocity=p->mVelocity+(velocity+p->_B0-p->mVelocity)*(dt/0.1f)+extra;
 return finite(p->mVelocity);
}
bool applyGravity(Handle h,float dt,std::string& e){
 auto* x=current(h,e);if(!x||!std::isfinite(dt)||dt<0)return false;
 if(x->runtime.state!=State::Hanged)h.body->mVelocity.y-=x->params.gravity*dt;
 return std::isfinite(h.body->mVelocity.y);
}
bool position(Handle h,const Vector3f& v,std::string& e){auto* x=current(h,e);if(!x||!finite(v))return false;h.body->mSRT.t=v;return true;}
bool whistle(Handle h,Navi* n,std::string& e){
 auto* x=current(h,e);if(!x||x->runtime.state!=State::Walk)return fail(e,"SourcePiki whistle requires source Walk");
 CaptainFrame f;if(!services->captainFrame(n,f,e)||!f.alive||!f.formationable)return false;
 unsigned captain=services->scene().captainAt(0)==n?0:services->scene().captainAt(1)==n?1:2;
 if(captain>1)return fail(e,"whistle source captain outside canonical roster");
 if(x->runtime.brain.action==Action::Formation)return false; // InteractFue(false,true), no party combining.
 auto* world=pc_p2_original_captain_world();
 if(world->demo()==captain::Demo::Unknown||world->demo()==captain::Demo::Absent)return fail(e,"source whistle movie authority absent");
 bool movie=world->demo()==captain::Demo::Playing;
 // All eligibility comes from this exact source lifetime + actual Walk state;
 // the legacy callback's nativeEligible argument is not exposed to callers.
 // It owns source day-0/reunion partition and transactional wild/progress write.
 if(!pc_p2_original_piki_recruit_allowed(h.body,captain,movie,true,e)||!services->supports(h,Motion::Notice,e))return false;
 if(!pc_p2_original_piki_recruit_accepted(h.body,captain,movie,true,e))return false;
 if(!brainCleanup(h,x->runtime,*services,e))return false;
 x->runtime.brain.action=Action::None;
 h.body->mNavi=n;
 float random=0;if(!services->random(random,e)||!std::isfinite(random)||random<0||random>1)return false;
 x->runtime.state=State::LookAt;x->runtime.lookSubState=0;x->runtime.lookWaitTime=0.3f*random;
 return services->calledSound(h,e);
}
bool launch(Handle h,Navi* n,const Vector3f& cursor,std::string& e){
 auto* x=current(h,e);if(!x||!finite(cursor)||h.body->mNavi!=n||x->runtime.brain.action!=Action::Formation
  ||(x->runtime.state!=State::Walk&&x->runtime.state!=State::GoHang&&x->runtime.state!=State::Hanged))
 return fail(e,"SourcePiki throw requires exact throwable source body/captain");
 // Hanged::exec returns Walk as soon as Navi enters Throw. The real Throw
 // animation key 2 still accepts that throwable Walk body; require SourceThrow
 // rather than incorrectly requiring ThrowWait/Hanged until the release key.
 CaptainFrame f;if(!services->captainFrame(n,f,e)||!f.throwing||!f.alive||!finite(f.position)||!finite(f.velocity)||!std::isfinite(f.face))return false;
 PcP2SourceBody b;if(!body(h.body,b,e))return false;
 auto& p=x->params;float height=b.state.species==2?p.heightYellow:b.state.species==4?p.heightWhite:b.state.species==3?p.heightPurple:p.heightMin;
 if(b.state.species==3)return fail(e,"Purple throw requires source HipDrop");
 const float peak=p.landingTime*0.5f,y=peak*p.gravity*0.5f+height/peak,time=y/p.gravity;
 if(!(time>0)||!std::isfinite(y))return fail(e,"invalid source throw arc");
 Vector3f start=f.position+Vector3f(-15*std::sin(f.face),10,-15*std::cos(f.face));
 const float dx=cursor.x-start.x,dz=cursor.z-start.z,angle=std::atan2(dx,dz),speed=std::sqrt(dx*dx+dz*dz)/(2*time);
 Vector3f velocity(speed*std::sin(angle),y,speed*std::cos(angle));
 if(f.sceneAnimationTimer<=0){velocity.x+=f.velocity.x;velocity.z+=f.velocity.z;}
 if(!finite(velocity)||!services->supports(h,Motion::RollJump,e))return false;
 if(!enter(*x,State::Flying,e))return false;
 h.body->mSRT.t=start;h.body->mFaceDirection=angle;h.body->mVelocity=velocity;h.body->mTargetVelocity=velocity;
 return true;
}
bool bounce(Handle h,std::string& e){
 auto* x=current(h,e);if(!x||x->runtime.state!=State::Flying)return false;
 if(!enter(*x,State::Walk,e)||!brainFree(h,x->runtime,*services,e))return false;
 // invokeAI task search is not yet ported; no P1 mActiveAction fallback.
 return services->landSound(h,e);
}
bool update(Handle h,float dt,std::string& e){
 auto* x=current(h,e);if(!x||!std::isfinite(dt)||dt<0)return false;
 auto& r=x->runtime;auto* p=h.body;
 if(r.state==State::Walk)return brainExec(h,r,*services,x->params,dt,e);
 if(r.state==State::LookAt){
  p->mTargetVelocity.set(0,0,0);
  switch(r.lookSubState){
  case 0:r.lookWaitTime-=dt;if(r.lookWaitTime<0){r.lookWaitTime=0;if(!services->motion(h,Motion::Notice,e))return false;r.lookSubState=1;}break;
  case 1:{Motion motion;if(!services->currentMotion(h,motion,e))return false;if(motion!=Motion::Notice)r.lookSubState=2;break;}
  case 2:if(p->mNavi&&!brainFormation(h,r,*services,p->mNavi,e))return false;return enter(*x,State::Walk,e);
  default:return fail(e,"invalid source LookAt substate");
  }return true;
 }
 if(r.state==State::GoHang||r.state==State::Hanged){
  if(!p->mNavi)return enter(*x,State::Walk,e);
  CaptainFrame f;if(!services->captainFrame(p->mNavi,f,e)||!finite(f.rhnd)||!finite(f.velocity))return false;
  if(r.state==State::GoHang){Vector3f diff=f.rhnd-p->mSRT.t;float length=diff.normalise();float scale=length>2*x->params.grabRange?2:1;
   p->mTargetVelocity=diff*(scale*x->params.run+f.velocity.length());}
  return f.throwWait||enter(*x,State::Walk,e);
 }
 if(++r.flyingFrames>=240)return bounce(h,e); // Retail timeout, distinct from a physical floor bounce.
 if(p->mHappa==2&&p->mVelocity.y<=0&&!r.flowerFalling){
  CaptainFrame f;if(!p->mNavi||!services->captainFrame(p->mNavi,f,e))return false;
  PcP2SourceBody b;if(!body(p,b,e))return false;
  auto& q=x->params;float normal=q.gravity*0.8f,flower=q.gravity*q.flowerGravity;
  float difference=normal-flower,fall=(normal*0.15f-0.075f*difference)-flower*0.15f;
  float height=b.state.species==2?q.heightYellow:q.heightMax;
  float heightFactor=-fall+std::sqrt(fall*fall+height*2*flower);
  if(!(heightFactor>0))return fail(e,"invalid source flower fall arc");
  if(!services->motion(h,Motion::Hang,e))return false;
  r.velocityDirection=Vector3f(p->mVelocity.x,0,p->mVelocity.z);r.directionalSpeed=r.velocityDirection.normalise();r.halfDirectionalSpeed=0.5f*r.directionalSpeed;
  float magnitude=q.landingTime*0.5f/(heightFactor/flower);
  p->mVelocity.x*=magnitude;p->mVelocity.z*=magnitude;p->mVelocity.y=0;
  p->mTargetVelocity=p->mVelocity;r.flowerFalling=true;r.slowFallTimer=0;
 }else if(r.flowerFalling){
  float random=0;if(!services->random(random,e)||!std::isfinite(random)||random<0||random>1)return false;
  p->mFaceDirection=std::fmod(p->mFaceDirection+3.14159265358979323846f*dt/0.42f,6.28318530717958647692f);
  r.slowFallTimer+=dt;float normal=x->params.gravity*0.8f,flower=x->params.gravity*x->params.flowerGravity;
  float active=r.slowFallTimer<0.15f?normal-r.slowFallTimer*((normal-flower)/0.15f):flower;
  p->mVelocity.y+=(1+(random-0.5f)*0.01f)*(x->params.gravity-active)*dt;
 }
 return true;
}
bool ignoreAtari(Handle h,const Creature* c,bool& out){std::string e;auto* x=current(h,e);if(!x||!c)return false;
 out=(x->runtime.state==State::Hanged||x->runtime.state==State::Flying)&&(c->mObjType==OBJTYPE_Navi||c->mObjType==OBJTYPE_Piki);return true;}
bool collision(Handle h,const CollEvent& event,std::string& e){
 auto* x=current(h,e);if(!x||!event.mCollider)return false;
 if(x->runtime.state==State::Flying){
  bool ignored=false;if(!ignoreAtari(h,event.mCollider,ignored))return false;if(ignored)return true;
  // Requires source InteractPress/FlyCollision, stick/crop/invokeAI. An absent
  // source receiver is explicit refusal; never call P1 event.stimulate.
  return fail(e,"SourcePiki Flying collision receiver closure not ported");
 }
 if(x->runtime.state==State::Walk&&x->runtime.brain.action==Action::Free
  &&event.mCollider->mObjType==OBJTYPE_Navi){
  // Only exact LoadedScene roster members are genuine SourceNavi receivers.
  Navi* n=services->scene().captainAt(0);
  if(static_cast<const Creature*>(n)!=event.mCollider)n=services->scene().captainAt(1);
  if(!n||static_cast<const Creature*>(n)!=event.mCollider)return false;
  CaptainFrame frame;if(!services->captainFrame(n,frame,e))return false;
  if(!frame.alive||!frame.controller||!frame.formationable)return true;
  return services->nudgeRumble(h,n,e)&&whistle(h,n,e);
 }
 return true;
}
void forget(Piki* p)noexcept{
 auto i=actors.find(p);if(i==actors.end())return;
 // Root calls before origin/body lifetime retirement and before CPlate release.
 // A reused native address must never receive cleanup from the old lifetime.
 try{std::string error;if(current(i->second.handle,error)){
  cleanup(i->second,error);brainCleanup(i->second.handle,i->second.runtime,*services,error);
 }}catch(...){ } actors.erase(i);
}
void sceneExit()noexcept{while(!actors.empty())forget(const_cast<Piki*>(actors.begin()->first));}
} }
