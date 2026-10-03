#include "pc_p2_original_piki_runtime.h"
#include "pc_p2_original_piki_brain.h"
#include "pc_p2_original_captain_damage.h"
#include "pc_p2_original_piki_recruit.h"
#include "pc_p2_original_progress.h"
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
 const captain::LoadedScene* sceneOwner=nullptr;
 std::uint64_t scene=0;
 std::string pikiBytes,naviBytes;
 bool committed=false;
};
Services* services=nullptr;
std::unordered_map<const Piki*,Entry> actors;
bool fail(std::string& e,const char* text){e=text;return false;}
bool ownerBusy=false,ownerReentered=false,observationOnly=false,ownerMutationActive=false;
class ObservationScope {bool previous;public:ObservationScope():previous(observationOnly){observationOnly=true;}~ObservationScope(){observationOnly=previous;}};
class OwnerOperation {
 bool entered=false;
public:
 explicit OwnerOperation(std::string& e){
  if(ownerBusy){ownerReentered=true;e="reentrant SourcePiki owner operation refused";return;}
  ownerBusy=true;ownerReentered=false;ownerMutationActive=true;entered=true;
 }
 ~OwnerOperation(){if(entered){ownerBusy=false;ownerMutationActive=false;}}
 bool admitted()const{return entered;}
 bool complete(bool result,std::string& e)const{
  if(ownerReentered)return fail(e,"SourcePiki service attempted reentrant owner mutation");
  return result;
 }
 void release(){if(entered){ownerBusy=false;ownerMutationActive=false;entered=false;}}
};
class ReadOperation {
 bool entered=false;
public:
 ReadOperation(){if(!ownerBusy){ownerBusy=true;ownerReentered=false;entered=true;}}
 ~ReadOperation(){if(entered)ownerBusy=false;}
 bool complete(bool result,std::string& e)const{
  return ownerReentered?fail(e,"SourcePiki read callback attempted owner mutation"):result;
 }
};
bool finite(const Vector3f& v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
bool parameter(const std::string& raw,const char* key,float& out){
 const std::string token=std::string("{")+key+"}";
 auto pos=raw.find(token);
 if(pos==std::string::npos || raw.find(token,pos+token.size())!=std::string::npos)return false;
 std::istringstream stream(raw.substr(pos+token.size())); unsigned type=0; float value=0;
 if(!(stream>>type>>value)||type!=4||!std::isfinite(value))return false;
 out=value;return true;
}
enum class PhaseUse { Action, Bootstrap };
bool canonical(std::string& e,PhaseUse use=PhaseUse::Action){
 auto* loaded=pc_p2_original_captain_loaded_scene();auto* world=pc_p2_original_captain_world();
 if(!services||!loaded||&services->scene()!=loaded||!world||!loaded->incarnation()
 ||loaded->incarnation()!=world->incarnation()||loaded->selectedCampaign()!=world->selectedCampaign()
 ||loaded->selectedFingerprint()!=world->selectedFingerprint()||loaded->sourceCatalog()!=world->sourceCatalog()
 ||loaded->captainAt(0)!=world->captainAt(0)||loaded->captainAt(1)!=world->captainAt(1)
 ||(world->phase()!=captain::Phase::GameWorldActive
   &&!(use==PhaseUse::Bootstrap&&world->phase()==captain::Phase::Loading)))return fail(e,"SourcePiki has no canonical active source scene");
 return true;
}
bool body(const Piki* p,PcP2SourceBody& out,std::string& e,PhaseUse use=PhaseUse::Action){
 if(!canonical(e,use)||!p)return false;
 auto kind=pc_p2_source_body_query(p,out);
 if(kind!=PcP2SourceBodyKind::GenPiki)return fail(e,"SourcePiki runtime currently requires actual GenPiki lifetime producer");
 if(kind==PcP2SourceBodyKind::GenPiki){
  OriginalPikiBodyHandle current;if(!pc_p2_original_piki_body_handle(p,current))return fail(e,"missing GenPiki native lifetime");
  out.nativeBody=p;out.nativeLifetime=current.nativeLifetime;
 }
 if(kind==PcP2SourceBodyKind::None||kind==PcP2SourceBodyKind::Unavailable||!out.nativeLifetime)
 return fail(e,"SourcePiki lacks current canonical native lifetime");
 // SceneCatalog and GenPikiCatalog are distinct authorities. Never infer the
 // Piki catalog from LoadedScene::sourceCatalog or from a source actor label.
 const auto& catalog=pc_p2_original_piki_catalog_fingerprint();
 const auto& progress=originalProgress();
 if(catalog.empty()||!pc_p2_original_piki_recruit_pair_ready()||!progress.ready()
  ||progress.snapshot().campaign!=services->scene().selectedCampaign())
  return fail(e,"SourcePiki campaign/PikiCatalog pairing unavailable or mismatched");
 return pc_p2_source_body_admitted(out,services->scene().selectedCampaign(),catalog,e);
}
Entry* current(Handle h,std::string& e){
 PcP2SourceBody b;
 if(!h.body||!h.lifetime||!body(h.body,b,e)||b.nativeLifetime!=h.lifetime)return fail(e,"stale SourcePiki handle"),nullptr;
 auto i=actors.find(h.body);
 if(i==actors.end()||!i->second.committed||i->second.handle.lifetime!=h.lifetime||i->second.sceneOwner!=&services->scene()||i->second.scene!=services->scene().incarnation())
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
 if(r.state==State::Flying||r.throwEffectsOwned){
  if(!services->throwEffects(x.handle,false,e))return false;
  r.throwEffectsOwned=false;
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
  r.throwEffectsOwned=true;return services->throwEffects(x.handle,true,e);
 }
 return true;
}
}
static bool retireImpl(Piki*,std::string&);
static bool bounceImpl(Handle,std::string&);
static bool whistleImpl(Handle,Navi*,std::string&);
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
static bool initializeImpl(Piki* p,std::string& e){
 const auto* sceneOwner=pc_p2_original_captain_loaded_scene();
 const auto sceneIncarnation=sceneOwner?sceneOwner->incarnation():0;
 PcP2SourceBody b;if(!body(p,b,e,PhaseUse::Bootstrap))return false;
 if(actors.count(p))return fail(e,"SourcePiki already initialized; forget old lifetime before reuse");
 Entry x;x.handle={p,b.nativeLifetime};x.sceneOwner=sceneOwner;x.scene=sceneIncarnation;
 x.pikiBytes=services->pikiParameterBytes();x.naviBytes=services->naviParameterBytes();
 float g=0;if(!services->gravity(g,e)||!parseParameters(x.pikiBytes,x.naviBytes,g,x.params,e))return false;
 for(auto m:{Motion::Wait,Motion::Walk,Motion::Run2,Motion::Hang,Motion::RollJump,Motion::Notice,Motion::Yawn,Motion::Chat,Motion::Search,Motion::Irritated,Motion::Sit,Motion::Sleep})if(!services->supports(x.handle,m,e))return false;
 // Allocate/register a pending owner BEFORE any animation/effect callback.
 // Failure leaves an owned cleanup record; an uncommitted entry is never a
 // callable runtime handle. emplace cannot fail after body fields were written.
 if(ownerReentered||!sceneOwner||sceneOwner!=pc_p2_original_captain_loaded_scene()
    ||sceneOwner!=&services->scene()||sceneIncarnation!=services->scene().incarnation())
  return fail(e,"SourcePiki initialization preflight changed canonical scene owner");
 auto inserted=actors.emplace(p,std::move(x));auto& pending=inserted.first->second;
 try{
  if(services->motion(pending.handle,Motion::Wait,e)&&brainFree(pending.handle,pending.runtime,*services,e)&&!ownerReentered){
   PcP2SourceBody after;
   if(body(p,after,e,PhaseUse::Bootstrap)&&pending.sceneOwner==pc_p2_original_captain_loaded_scene()
      &&pending.sceneOwner==&services->scene()&&pending.scene==services->scene().incarnation()
      &&after.nativeLifetime==pending.handle.lifetime
      &&pending.pikiBytes==services->pikiParameterBytes()&&pending.naviBytes==services->naviParameterBytes()
      &&!ownerReentered){pending.committed=true;return true;}
   e="SourcePiki bootstrap lifetime/scene/selected parameters changed during callbacks";
  }
 }catch(...){e="source initialization callback threw; pending ownership retained";}
 std::string cleanupError;
 if(!retireImpl(p,cleanupError))e+="; pending cleanup refused: "+cleanupError;
 return false;
}
bool handle(const Piki* p,Handle& out){ReadOperation op;std::string e;auto i=actors.find(p);if(i==actors.end()||!current(i->second.handle,e)||!op.complete(true,e))return false;out=i->second.handle;return true;}
bool snapshot(Handle h,RuntimeState& out){ReadOperation op;std::string e;auto* x=current(h,e);if(!x||!op.complete(true,e))return false;out=x->runtime;return true;}
static bool frameImpl(Handle h,Frame& out,std::string& e){
 auto* x=current(h,e);if(!x)return false;
 PcP2SourceBody source;if(!body(h.body,source,e)||!finite(h.body->mSRT.t)||h.body->mHappa<0||h.body->mHappa>2)return false;
 Frame next;next.handle=h;next.position=h.body->mSRT.t;next.state=x->runtime.state;next.species=source.state.species;
 next.happa=h.body->mHappa;next.captain=h.body->mNavi;
 if(x->runtime.brain.action==Action::Formation)next.formationSlot=x->runtime.brain.slot;
 next.throwable=next.state==State::Walk||next.state==State::GoHang||next.state==State::Hanged;
 next.releasable=next.state==State::Walk;
 out=next;return true;
}
static bool squadImpl(Navi* n,std::vector<Frame>& out,std::string& e){
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
static bool sortFormationImpl(Handle h,int happa,std::string& e){
 auto* x=current(h,e);if(!x||happa< -1||happa>2||x->runtime.brain.action!=Action::Formation||x->runtime.brain.slot<0)return false;
 if(!services->sortSlot(h,x->runtime.brain.navi,x->runtime.brain.slot,happa,e))return false;
 x->runtime.brain.sortState=2;return true;
}
static bool transitionImpl(Handle h,State s,std::string& e){
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
static bool animateImpl(Handle h,float dt,std::string& e){auto* x=current(h,e);return x&&std::isfinite(dt)&&dt>=0&&services->animate(h,dt,e);}
bool animationKey(Handle h,unsigned type,std::string& e){
 if(observationOnly){ownerReentered=true;return fail(e,"authored key mutation during readonly retirement preflight refused");}
 ReadOperation op;auto* x=current(h,e);if(!x||!op.complete(true,e))return false;
 if(x->runtime.state==State::LookAt&&type==1000)x->runtime.lookSubState=2;
 return brainAnimationKey(h,x->runtime,*services,type,e)&&op.complete(true,e);
}
static bool moveVelocityImpl(Handle h,float dt,std::string& e){
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
static bool applyGravityImpl(Handle h,float dt,std::string& e){
 auto* x=current(h,e);if(!x||!std::isfinite(dt)||dt<0)return false;
 if(x->runtime.state!=State::Hanged)h.body->mVelocity.y-=x->params.gravity*dt;
 return std::isfinite(h.body->mVelocity.y);
}
static bool positionImpl(Handle h,const Vector3f& v,std::string& e){auto* x=current(h,e);if(!x||!finite(v))return false;h.body->mSRT.t=v;return true;}
static bool whistleResultImpl(Handle h,Navi* n,bool combine,bool newToParty,bool& accepted,std::string& e){
 (void)newToParty; // Retail InteractFue::actPiki does not read this Navi flag.
 auto* x=current(h,e);if(!x)return false;
 CaptainFrame f;if(!services->captainFrame(n,f,e)||!current(h,e))return false;
 unsigned captain=services->scene().captainAt(0)==n?0:services->scene().captainAt(1)==n?1:2;
 if(!current(h,e))return false;
 if(captain>1)return fail(e,"whistle source captain outside canonical roster");
 auto* world=pc_p2_original_captain_world();
 if(!world||!current(h,e)||pc_p2_original_captain_world()!=world)return fail(e,"source whistle world changed during inspection");
 const auto demo=world->demo();
 if(!current(h,e)||pc_p2_original_captain_world()!=world)return false;
 if(demo==captain::Demo::Unknown||demo==captain::Demo::Absent)return fail(e,"source whistle movie authority absent");
 bool movie=demo==captain::Demo::Playing;
 PcP2SourceBody source;if(!body(h.body,source,e)||!current(h,e))return false;
 const auto reject=[&](){if(!current(h,e))return false;accepted=false;e.clear();return true;};
 // Genuine retail eligibility rejection is an authenticated result. Source
 // invocations may continue through their ordered roster without string tests.
 if(!f.alive||!f.formationable||!originalProgress().captainAllowed(captain,source.state.wasWild)
   ||(source.state.wild&&(movie||source.state.species>2)))return reject();
 // Of the five owned states, only Walk and GoHang are retail callable.
 if(x->runtime.state!=State::Walk&&x->runtime.state!=State::GoHang)return reject();
 if(x->runtime.brain.action==Action::Formation&&(!combine||h.body->mNavi==n))return reject();
 // All eligibility comes from this exact source lifetime + actual Walk state;
 // the legacy callback's nativeEligible argument is not exposed to callers.
 // It owns source day-0/reunion partition and transactional wild/progress write.
 if(!pc_p2_original_piki_recruit_allowed(h.body,captain,movie,true,e)||!current(h,e)
   ||!services->supports(h,Motion::Notice,e)||!current(h,e))return false;
 if(!pc_p2_original_piki_recruit_accepted(h.body,captain,movie,true,e)||!current(h,e))return false;
 if(!brainCleanup(h,x->runtime,*services,e)||!current(h,e)||!cleanup(*x,e)||!current(h,e))return false;
 x->runtime.brain.action=Action::None;
 h.body->mNavi=n;
 float random=0;if(!services->random(random,e)||!current(h,e)||!std::isfinite(random)||random<0||random>1)return false;
 x->runtime.state=State::LookAt;x->runtime.lookSubState=0;x->runtime.lookWaitTime=0.3f*random;
 if(!services->calledSound(h,e)||!current(h,e))return false;
 accepted=true;return true;
}
static bool whistleImpl(Handle h,Navi* n,std::string& e){bool accepted=false;return whistleResultImpl(h,n,false,true,accepted,e)&&accepted;}
static bool gatherImpl(Handle h,const Vector3f& goal,float radius,std::string& e){
 auto* x=current(h,e);if(!x||x->runtime.state!=State::Walk)return fail(e,"source Gather requires actual Walk");
 return brainGather(h,x->runtime,*services,goal,radius,e);
}
static bool launchImpl(Handle h,Navi* n,const Vector3f& cursor,std::string& e){
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
static bool bounceImpl(Handle h,std::string& e){
 auto* x=current(h,e);if(!x||x->runtime.state!=State::Flying)return false;
 if(!enter(*x,State::Walk,e)||!brainFree(h,x->runtime,*services,e))return false;
 // invokeAI task search is not yet ported; no P1 mActiveAction fallback.
 return services->landSound(h,e);
}
static bool updateImpl(Handle h,float dt,std::string& e){
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
 if(++r.flyingFrames>=240)return bounceImpl(h,e); // Retail timeout, distinct from a physical floor bounce.
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
bool ignoreAtari(Handle h,const Creature* c,bool& out){ReadOperation op;std::string e;auto* x=current(h,e);if(!x||!c||!op.complete(true,e))return false;
 out=(x->runtime.state==State::Hanged||x->runtime.state==State::Flying)&&(c->mObjType==OBJTYPE_Navi||c->mObjType==OBJTYPE_Piki);return true;}
static bool collisionImpl(Handle h,const CollEvent& event,std::string& e){
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
  return services->nudgeRumble(h,n,e)&&whistleImpl(h,n,e);
 }
 return true;
}
bool slotsChanged(const std::vector<SlotChange>& changes,std::string& e){
 if(observationOnly){ownerReentered=true;return fail(e,"slot mutation during readonly retirement preflight refused");}
 ReadOperation op;
 auto* scene=pc_p2_original_captain_loaded_scene();auto* world=pc_p2_original_captain_world();
 if(!services||!scene||&services->scene()!=scene||!world||!scene->incarnation()
    ||scene->incarnation()!=world->incarnation()||scene->selectedCampaign()!=world->selectedCampaign()
    ||scene->selectedFingerprint()!=world->selectedFingerprint()||scene->sourceCatalog()!=world->sourceCatalog())
  return fail(e,"source slot notification lacks exact canonical lifetime scene");
 struct Update {BrainState* brain;bool pending;int slot;};std::vector<Update> updates;updates.reserve(changes.size());
 for(std::size_t j=0;j<changes.size();++j){
  const auto& change=changes[j];OriginalPikiBodyHandle live;auto i=actors.find(change.handle.body);
  bool captain=false;for(unsigned k=0;k<2;++k)if(change.captain&&scene->captainAt(k)==change.captain&&world->captainAt(k)==change.captain)captain=true;
  if(!captain||change.oldSlot<0||change.newSlot< -1||!change.handle.lifetime||i==actors.end()
    ||i->second.sceneOwner!=scene||i->second.scene!=scene->incarnation()||i->second.handle.lifetime!=change.handle.lifetime
    ||!pc_p2_original_piki_body_handle(change.handle.body,live)||live.nativeLifetime!=change.handle.lifetime)
   return fail(e,"stale or foreign source CPlate slot notification");
  for(std::size_t k=0;k<j;++k){const auto& other=changes[k];
   if((other.handle.body==change.handle.body&&other.captain==change.captain)||(other.captain==change.captain
     &&(other.oldSlot==change.oldSlot||(change.newSlot>=0&&other.newSlot==change.newSlot))))
    return fail(e,"duplicate source CPlate slot notification");
  }
  auto& brain=i->second.runtime.brain;
  if(brain.action==Action::Formation&&brain.navi==change.captain&&brain.slot==change.oldSlot)
   updates.push_back({&brain,false,change.newSlot});
  else if(brain.pendingNavi==change.captain&&brain.pendingSlot==change.oldSlot)
   updates.push_back({&brain,true,change.newSlot});
  else return fail(e,"source CPlate notification does not match retained Brain slot ownership");
 }
 if(!op.complete(true,e))return false;
 // Whole batch validates before any slot write. Notifications never erase
 // owners or start actions, so callbacks can safely retain their Entry refs.
 for(auto& update:updates){
  if(update.pending){update.brain->pendingSlot=update.slot;if(update.slot==-1)update.brain->pendingNavi=nullptr;}
  else update.brain->slot=update.slot;
 }
 return true;
}
static bool retireImpl(Piki* p,std::string& e){
 auto i=actors.find(p);if(i==actors.end())return true;
 OriginalPikiBodyHandle live;
 // Teardown is authorized by actual native lifetime, not GameWorldActive.
 // A stale/reused physical address receives no cleanup or field mutations.
 if(!services||&services->scene()!=i->second.sceneOwner||pc_p2_original_captain_loaded_scene()!=i->second.sceneOwner||services->scene().incarnation()!=i->second.scene||!pc_p2_original_piki_body_handle(p,live)||live.nativeLifetime!=i->second.handle.lifetime)
  return fail(e,"SourcePiki retirement requires original exact live native lifetime");
 bool stateClean=false;try{stateClean=cleanup(i->second,e);}catch(...){e="SourcePiki state cleanup callback threw; ownership retained";}
 std::string brainError;bool brainClean=false;
 try{brainClean=brainCleanupAll(i->second.handle,i->second.runtime,*services,brainError);}
 catch(...){brainError="SourcePiki Brain cleanup callback threw; ownership retained";}
 if(!brainClean){if(stateClean)e=brainError;else e+="; "+brainError;}
 if(!stateClean||!brainClean||ownerReentered)return false; // Explicit owner remains for retry.
 actors.erase(i);return true;
}
static bool retireSceneImpl(std::string& e){
 bool complete=true;
 for(auto i=actors.begin();i!=actors.end();){auto* p=const_cast<Piki*>(i->first);++i;
  std::string actorError;if(!retireImpl(p,actorError)){if(complete)e=actorError;complete=false;}
 }
 return complete;
}
bool owned()noexcept{return ownerMutationActive||!actors.empty();}
bool retains(Handle h)noexcept{if(!h.body||!h.lifetime)return false;auto i=actors.find(h.body);return ownerMutationActive||(i!=actors.end()&&i->second.handle.lifetime==h.lifetime);}
bool retired(std::string& e){ReadOperation op;if(!op.complete(true,e))return false;return (!ownerMutationActive&&actors.empty())||fail(e,"SourcePiki runtime retains native owners or an in-flight owner operation");}
bool canRetireScene(std::string& e){
 ReadOperation op;ObservationScope observation;
 if(ownerMutationActive)return fail(e,"SourcePiki retirement preflight refused during in-flight owner operation");
 // Empty observes only this concrete registry, never factory/Shape readiness.
 if(actors.empty())return op.complete(true,e);
 auto* scene=pc_p2_original_captain_loaded_scene();auto* world=pc_p2_original_captain_world();
 if(!services||!scene||&services->scene()!=scene||!world||!scene->incarnation())
  return fail(e,"SourcePiki retirement preflight lost canonical live scene");
 const auto incarnation=scene->incarnation();
 const auto campaign=scene->selectedCampaign(),fingerprint=scene->selectedFingerprint(),catalog=scene->sourceCatalog();
 Navi* captains[2]={scene->captainAt(0),scene->captainAt(1)};
 std::vector<Handle> retained;retained.reserve(actors.size());
 for(const auto& actor:actors)retained.push_back(actor.second.handle);
 auto validate=[&](){
  // Compare pointer identity before dereferencing an old descriptor. Source
  // reference callbacks cannot move this snapshot onto a replacement scene.
  if(pc_p2_original_captain_loaded_scene()!=scene||pc_p2_original_captain_world()!=world||&services->scene()!=scene
    ||scene->incarnation()!=incarnation||world->incarnation()!=incarnation
    ||scene->selectedCampaign()!=campaign||world->selectedCampaign()!=campaign
    ||scene->selectedFingerprint()!=fingerprint||world->selectedFingerprint()!=fingerprint
    ||scene->sourceCatalog()!=catalog||world->sourceCatalog()!=catalog)
   return fail(e,"SourcePiki readonly retirement callback changed canonical scene authority");
  for(unsigned k=0;k<2;++k)if(scene->captainAt(k)!=captains[k]||world->captainAt(k)!=captains[k])
   return fail(e,"SourcePiki readonly retirement callback changed captain lifetime binding");
  if(actors.size()!=retained.size())return fail(e,"SourcePiki readonly retirement callback changed owner registry");
  for(auto h:retained){auto i=actors.find(h.body);OriginalPikiBodyHandle live;
   if(i==actors.end()||i->second.sceneOwner!=scene||i->second.scene!=incarnation||i->second.handle.lifetime!=h.lifetime
      ||!pc_p2_original_piki_body_handle(h.body,live)||live.nativeLifetime!=h.lifetime)
    return fail(e,"SourcePiki readonly retirement callback changed a retained native lifetime");
  }
  return op.complete(true,e);
 };
 try{
  if(!validate())return false;
  for(auto h:retained){const auto& entry=actors.find(h.body)->second;const auto& brain=entry.runtime.brain;
   if((brain.freeEffectsOwned||brain.action==Action::Free)&&(!services->canRemoveFreeEffects(h,e)||!validate()))return false;
   if((entry.runtime.throwEffectsOwned||entry.runtime.state==State::Flying)&&(!services->canRemoveThrowEffects(h,e)||!validate()))return false;
   if(brain.action==Action::Formation&&brain.slot>=0&&(!services->canReleaseSlot(h,brain.navi,brain.slot,e)||!validate()))return false;
   if(brain.pendingSlot>=0&&(!services->canReleaseSlot(h,brain.pendingNavi,brain.pendingSlot,e)||!validate()))return false;
   // Every entry and the complete pass are checked after all callbacks too.
   if(!validate())return false;
  }
  return validate();
 }catch(...){return fail(e,"SourcePiki readonly retirement preflight callback threw");}
}
bool readOwnership(Ownership& out,std::string& e){
 ReadOperation op;Ownership observed;observed.inFlightOwnerOperations=ownerMutationActive?1:0;
 for(const auto& actor:actors){
  const auto& entry=actor.second;const auto& brain=entry.runtime.brain;++observed.entries;
  if(entry.committed)++observed.committed;else ++observed.pendingInitializations;
  if(brain.action==Action::Formation&&brain.slot>=0)++observed.formationSlots;
  if(brain.pendingSlot>=0)++observed.pendingSlots;
  if(brain.freeEffectsOwned)++observed.freeEffectOwners;
  if(entry.runtime.throwEffectsOwned)++observed.throwEffectOwners;
 }
 if(!op.complete(true,e))return false;
 out=observed;return true;
}
bool initialize(Piki* p,std::string& e){OwnerOperation op(e);return op.admitted()&&op.complete(initializeImpl(p,e),e);}
bool frame(Handle h,Frame& out,std::string& e){ReadOperation op;Frame next;if(!frameImpl(h,next,e)||!op.complete(true,e))return false;out=next;return true;}
bool squad(Navi* n,std::vector<Frame>& out,std::string& e){ReadOperation op;std::vector<Frame> next;if(!squadImpl(n,next,e)||!op.complete(true,e))return false;out=std::move(next);return true;}
bool roster(std::vector<Frame>& out,std::string& e){
 ReadOperation op;
 if(ownerMutationActive||!canonical(e))return fail(e,"SourcePiki roster unavailable during owner mutation or inactive scene");
 auto* scene=pc_p2_original_captain_loaded_scene();auto* world=pc_p2_original_captain_world();
 const auto incarnation=scene->incarnation();
 const auto campaign=scene->selectedCampaign(),fingerprint=scene->selectedFingerprint(),catalog=scene->sourceCatalog();
 Navi* captains[2]={scene->captainAt(0),scene->captainAt(1)};
 std::vector<Handle> census;std::vector<Frame> next;
 try{
  for(const auto& actor:actors)if(actor.second.committed)census.push_back(actor.second.handle);
  std::sort(census.begin(),census.end(),[](Handle a,Handle b){return a.lifetime<b.lifetime;});
  for(std::size_t i=1;i<census.size();++i)if(census[i-1].lifetime==census[i].lifetime)
   return fail(e,"SourcePiki roster contains duplicate native lifetimes");
  next.reserve(census.size());
  for(auto h:census){Frame f;if(!frameImpl(h,f,e))return false;next.push_back(f);}
  // Re-read selected/body authority after the complete pass, then inspect the
  // real native associations without another Services callback between tokens.
  for(auto h:census)if(!current(h,e))return false;
  if(&services->scene()!=scene||pc_p2_original_captain_loaded_scene()!=scene||pc_p2_original_captain_world()!=world
    ||scene->incarnation()!=incarnation||world->incarnation()!=incarnation
    ||scene->selectedCampaign()!=campaign||world->selectedCampaign()!=campaign
    ||scene->selectedFingerprint()!=fingerprint||world->selectedFingerprint()!=fingerprint
    ||scene->sourceCatalog()!=catalog||world->sourceCatalog()!=catalog||world->phase()!=captain::Phase::GameWorldActive)
   return fail(e,"SourcePiki roster scene authority changed during inspection");
  for(unsigned k=0;k<2;++k)if(scene->captainAt(k)!=captains[k]||world->captainAt(k)!=captains[k])
   return fail(e,"SourcePiki roster captain binding changed during inspection");
  std::size_t committed=0;
  for(const auto& actor:actors)if(actor.second.committed)++committed;
  if(committed!=census.size())return fail(e,"SourcePiki committed census changed during inspection");
  for(auto h:census)if(!pc_p2_original_piki_body_current(h.body,h.lifetime))
   return fail(e,"SourcePiki roster native lifetime changed during inspection");
  if(!op.complete(true,e))return false;
  out=std::move(next);return true;
 }catch(...){return fail(e,"SourcePiki roster inspection threw; output unchanged");}
}
bool transition(Handle h,State s,std::string& e){OwnerOperation op(e);return op.admitted()&&op.complete(transitionImpl(h,s,e),e);}
bool animate(Handle h,float dt,std::string& e){OwnerOperation op(e);return op.admitted()&&op.complete(animateImpl(h,dt,e),e);}
bool update(Handle h,float dt,std::string& e){OwnerOperation op(e);return op.admitted()&&op.complete(updateImpl(h,dt,e),e);}
bool moveVelocity(Handle h,float dt,std::string& e){OwnerOperation op(e);return op.admitted()&&op.complete(moveVelocityImpl(h,dt,e),e);}
bool applyGravity(Handle h,float dt,std::string& e){OwnerOperation op(e);return op.admitted()&&op.complete(applyGravityImpl(h,dt,e),e);}
bool position(Handle h,const Vector3f& v,std::string& e){OwnerOperation op(e);return op.admitted()&&op.complete(positionImpl(h,v,e),e);}
bool whistle(Handle h,Navi* n,std::string& e){OwnerOperation op(e);return op.admitted()&&op.complete(whistleImpl(h,n,e),e);}
bool whistle(Handle h,Navi* n,bool combine,bool newToParty,bool& accepted,std::string& e){
 OwnerOperation op(e);bool next=false;
 if(!op.admitted()||!op.complete(whistleResultImpl(h,n,combine,newToParty,next,e),e))return false;
 accepted=next;return true;
}
bool gather(Handle h,const Vector3f& goal,float radius,std::string& e){OwnerOperation op(e);return op.admitted()&&op.complete(gatherImpl(h,goal,radius,e),e);}
bool launch(Handle h,Navi* n,const Vector3f& v,std::string& e){OwnerOperation op(e);return op.admitted()&&op.complete(launchImpl(h,n,v,e),e);}
bool bounce(Handle h,std::string& e){OwnerOperation op(e);return op.admitted()&&op.complete(bounceImpl(h,e),e);}
bool collision(Handle h,const CollEvent& event,std::string& e){OwnerOperation op(e);return op.admitted()&&op.complete(collisionImpl(h,event,e),e);}
bool sortFormation(Handle h,int happa,std::string& e){OwnerOperation op(e);return op.admitted()&&op.complete(sortFormationImpl(h,happa,e),e);}
bool retire(Piki* p,std::string& e){OwnerOperation op(e);return op.admitted()&&op.complete(retireImpl(p,e),e);}
bool retireScene(std::string& e){OwnerOperation op(e);return op.admitted()&&op.complete(retireSceneImpl(e),e);}
void forget(Piki* p)noexcept{try{std::string error;retire(p,error);}catch(...){ }}
void sceneExit()noexcept{try{std::string error;retireScene(error);}catch(...){ }}
} }

bool pc_p2_original_piki_runtime_owned()noexcept{return p2original::piki::owned();}
bool pc_p2_original_piki_runtime_can_retire(std::string& e){return p2original::piki::canRetireScene(e);}
bool pc_p2_original_piki_runtime_retire(std::string& e){return p2original::piki::retireScene(e);}
bool pc_p2_original_piki_runtime_retired(std::string& e){return p2original::piki::retired(e);}
