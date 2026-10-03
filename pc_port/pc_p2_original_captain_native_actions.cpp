#include "pc_p2_original_captain_native_actions.h"
#include "pc_p2_original_captain_native_control.h"
#include "Navi.h"
#include "NaviState.h"
#include "Kontroller.h"
#include <cmath>
namespace p2original { namespace captain { namespace nativeactions {
namespace {
using actions::Vec3;using actions::PikiHandle;using actions::PikiFrame;
bool fail(std::string& e,const char* s){e=s;return false;}
bool finite(Vec3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
Vec3 vec(const Vector3f& v){return {v.x,v.y,v.z};}
Vector3f vec(Vec3 v){return Vector3f(v.x,v.y,v.z);}
p2original::piki::Handle handle(PikiHandle h){return {h.actor,h.lifetime};}
PikiFrame convert(const p2original::piki::Frame& f){
 actions::PikiState s=actions::PikiState::Unsupported;
 switch(f.state){case piki::State::Walk:s=actions::PikiState::Walk;break;case piki::State::GoHang:s=actions::PikiState::GoHang;break;case piki::State::Hanged:s=actions::PikiState::Hanged;break;case piki::State::Flying:s=actions::PikiState::Flying;break;case piki::State::LookAt:break;}
 return {{f.handle.body,f.handle.lifetime},vec(f.position),s,f.species,f.happa,f.throwable,f.captain};
}
}
struct Bridge::Impl {
 ActorSource& source;piki::Services& services;piki::PhysicalSource& physical;piki::Plate& plate;SourceBank& bank;
 const LoadedScene* owner;std::uint64_t epoch;std::string bytes;
 Impl(ActorSource& s,piki::Services& r,piki::PhysicalSource& p,piki::Plate& c,SourceBank& b):source(s),services(r),physical(p),plate(c),bank(b),owner(&s.scene()),epoch(s.scene().incarnation()){}
 bool check(const Navi* n,std::string& e,bool loading=false)const{
  const auto* w=pc_p2_original_captain_world();const auto* s=pc_p2_original_captain_loaded_scene();
  if(s!=owner||!s||s->incarnation()!=epoch||!epoch||&services.scene()!=s||&physical.scene()!=s||&source.scene()!=s||!w
   ||w->incarnation()!=epoch||w->selectedCampaign()!=s->selectedCampaign()||w->selectedFingerprint()!=s->selectedFingerprint()||w->sourceCatalog()!=s->sourceCatalog()
   ||!s->captainAt(0)||!s->captainAt(1)||s->captainAt(0)==s->captainAt(1)||w->captainAt(0)!=s->captainAt(0)||w->captainAt(1)!=s->captainAt(1)
   ||(w->phase()!=Phase::GameWorldActive&&!(loading&&w->phase()==Phase::Loading))||pc_p2_original_captain_source_bank()!=&bank)return fail(e,"native source actions lost canonical scene/roster/composition");
  if(!n)return true;
  if(n!=s->captainAt(0)&&n!=s->captainAt(1))return fail(e,"native source action captain outside actual roster");
  MotionState motion;if(!bank.state(const_cast<Navi*>(n),motion,e))return false;
  bool alive;if(!pc_p2_original_captain_actor_lifetime(n,alive))return fail(e,"native source actions lack actual captain lifetime");
  if(w->phase()==Phase::GameWorldActive){auto* current=const_cast<Navi*>(n)->getCurrState();auto* typed=dynamic_cast<State*>(current);if(!typed||typed->nativeState()!=current)return fail(e,"native source actions lack exact typed current captain");}
  return true;
 }
 template<class F> bool operation(Navi& n,std::string& e,F f)const{
  if(!check(&n,e))return false;
  const auto* state=n.getCurrState();const auto* world=pc_p2_original_captain_world();
  if(!f())return false;
  return check(&n,e)&&n.getCurrState()==state&&pc_p2_original_captain_world()==world?true:fail(e,"native source action callback expired captain binding");
 }
 // Membership observation retains genuine CF-dead bodies so retail dismissal
 // can filter them; mutation/held-body reads separately require CF_IsAlive.
 bool observe(PikiHandle h,piki::Frame& out,piki::PhysicalFacts& facts,std::string& e)const{
  if(!h.actor||!h.lifetime)return fail(e,"native source action missing exact Piki handle");
  piki::Handle current;piki::Frame next;piki::PhysicalFacts physicalFacts;
  if(!piki::handle(h.actor,current)||current.lifetime!=h.lifetime||!piki::frame(current,next,e))return fail(e,"native source action Piki lifetime expired");
  if(!piki::nativePhysicalFacts(current,&physical,physicalFacts,e))return false;
  out=next;facts=physicalFacts;return true;
 }
 bool read(PikiHandle h,piki::Frame& out,std::string& e)const{
  piki::Frame next;piki::PhysicalFacts facts;
  if(!observe(h,next,facts,e))return false;
  if(!facts.alive)return fail(e,"native source action Piki CF is not alive");
  out=next;return true;
 }
};
Bridge::Bridge(std::unique_ptr<Impl> p):m(std::move(p)){}Bridge::~Bridge()=default;
std::unique_ptr<Bridge> Bridge::create(ActorSource& s,piki::Services& r,piki::PhysicalSource& p,piki::Plate& c,SourceBank& b,std::string& e){
 e.clear();auto impl=std::make_unique<Impl>(s,r,p,c,b);control::Params parsed;
 if(!impl->check(nullptr,e,true)||!impl->check(impl->owner->captainAt(0),e,true)||!impl->check(impl->owner->captainAt(1),e,true)||!b.sourceBytes(SourceResource::Parameters,impl->bytes,e)||!control::parseParameters(impl->bytes,parsed,e))return {};
 if(r.naviParameterBytes()!=impl->bytes){fail(e,"SourcePiki Services and captain bank selected different Navi parameters");return {};}
 return std::unique_ptr<Bridge>(new Bridge(std::move(impl)));
}
const LoadedScene& Bridge::scene()const{return *m->owner;}const std::string& Bridge::parameterBytes()const{return m->bytes;}
bool Bridge::frame(const Navi& n,actions::ActorFrame& out,std::string& e)const{
 if(!m->check(&n,e,true))return false;
 const auto* state=const_cast<Navi&>(n).getCurrState();Observation obs;
 if(!m->source.observe(n,obs,e)||!m->check(&n,e,true)||const_cast<Navi&>(n).getCurrState()!=state||!finite(obs.cursor)||!std::isfinite(obs.delta)||obs.delta<0||!std::isfinite(n.mFaceDirection)||!finite(vec(n.mSRT.t))||!finite(vec(n.mVelocity)))return fail(e,"missing finite actual source captain observation");
 const auto timer=nativecontrol::sceneAnimationTimer(&n);PcOriginalCaptainTimers counters;std::array<float,12> joint;
 if(!timer||!pc_p2_original_captain_actor_timers(&n,counters)||!m->bank.jointWorld(const_cast<Navi*>(&n),10,joint,e))return false;
 if(!m->check(&n,e,true)||const_cast<Navi&>(n).getCurrState()!=state)return fail(e,"source joint callback expired current captain");
 actions::ActorFrame f;f.position=vec(n.mSRT.t);f.velocity=vec(n.mVelocity);f.cursor=obs.cursor;f.face=n.mFaceDirection;f.delta=obs.delta;f.sceneAnimationTimer=*timer;f.throwDisableFrames=counters.throwDisable;
 // Authored rhnd source joint10, literal offset (3,0,0), independent of draw.
 f.hand={joint[0]*3+joint[3],joint[4]*3+joint[7],joint[8]*3+joint[11]};if(!finite(f.hand))return fail(e,"nonfinite actual source rhnd pose");
 auto* pad=n.mKontroller;f.controller=pad!=nullptr;
 if(pad){f.heldA=pad->keyDown(KBBTN_A);f.heldB=pad->keyDown(KBBTN_B);f.pressedA=pad->keyClick(KBBTN_A);f.pressedB=pad->keyClick(KBBTN_B);f.releasedB=pad->keyUnClick(KBBTN_B);f.right=pad->keyClick(KBBTN_DPAD_RIGHT);f.left=pad->keyClick(KBBTN_DPAD_LEFT);f.up=pad->keyClick(KBBTN_DPAD_UP);f.down=pad->keyClick(KBBTN_DPAD_DOWN);}
 if(pc_p2_original_captain_world()->phase()==Phase::GameWorldActive){std::vector<piki::Frame> list;if(!piki::squad(const_cast<Navi*>(&n),list,e)||!m->check(&n,e))return false;if(!list.empty()){Vector3f slot;if(!m->plate.slotPosition(list.front().handle,const_cast<Navi*>(&n),list.front().formationSlot,slot,e))return false;f.firstFormationSlot=vec(slot);}}
 if(!m->check(&n,e,true)||const_cast<Navi&>(n).getCurrState()!=state)return fail(e,"source frame callback expired current captain");
 out=f;return true;
}
bool Bridge::squad(const Navi& n,std::vector<PikiFrame>& out,std::string& e)const{std::vector<PikiFrame> next;return m->operation(const_cast<Navi&>(n),e,[&]{std::vector<piki::Frame> list;if(!piki::squad(const_cast<Navi*>(&n),list,e))return false;for(const auto& f:list){piki::Frame current;if(!m->read({f.handle.body,f.handle.lifetime},current,e)||current.captain!=&n)return false;next.push_back(convert(current));}return true;})?(out=std::move(next),true):false;}
bool Bridge::piki(const Navi& n,PikiHandle h,PikiFrame& out,std::string& e)const{piki::Frame f;if(!m->operation(const_cast<Navi&>(n),e,[&]{return m->read(h,f,e);}))return false;out=convert(f);return true;}
bool Bridge::control(Navi& n,std::string& e){return m->operation(n,e,[&]{return nativecontrol::control(&n,e);});}
bool Bridge::whistle(const Navi& n,actions::WhistleFrame& out,std::string& e)const{actions::WhistleFrame f;if(!m->operation(const_cast<Navi&>(n),e,[&]{return m->source.whistle(n,f,e)&&finite(f.cursor)&&std::isfinite(f.radius)&&f.radius>=0;}))return false;out=f;return true;}
bool Bridge::startWhistle(Navi& n,std::string& e){return m->operation(n,e,[&]{return m->source.startWhistle(n,e);});}
bool Bridge::stopWhistle(Navi& n,std::string& e){return m->operation(n,e,[&]{return m->source.stopWhistle(n,e);});}
bool Bridge::updateWhistle(Navi& n,Vec3 v,bool a,std::string& e){if(!finite(v))return fail(e,"nonfinite source action position");return m->operation(n,e,[&]{return m->source.updateWhistle(n,v,a,e);});}
bool Bridge::callPikis(Navi& n,std::string& e){return m->operation(n,e,[&]{std::vector<WhistleCandidate> list;if(!m->source.whistleCandidates(n,list,e))return false;for(const auto& c:list){if(!m->check(&n,e))return false;if(c.captain){party::WhistleOutcome outcome;if(c.piki.actor||!party::invokeWhistleCaptain(c.captain,&n,false,true,outcome,e))return false;}else if(!whistleMember(n,c.piki,false,true,e))return false;}return true;});}
bool Bridge::transitionPiki(Navi& n,PikiHandle h,actions::PikiState state,std::string& e){return m->operation(n,e,[&]{piki::Frame f;if(!m->read(h,f,e)||f.captain!=&n)return false;piki::State target;switch(state){case actions::PikiState::Walk:target=piki::State::Walk;break;case actions::PikiState::GoHang:target=piki::State::GoHang;break;case actions::PikiState::Hanged:target=piki::State::Hanged;break;case actions::PikiState::Flying:target=piki::State::Flying;break;default:return fail(e,"unsupported source Piki transition");}return f.state==target||piki::transition(handle(h),target,e);});}
bool Bridge::positionPiki(Navi& n,PikiHandle h,Vec3 v,std::string& e){if(!finite(v))return fail(e,"nonfinite source action position");return m->operation(n,e,[&]{piki::Frame f;return m->read(h,f,e)&&f.captain==&n&&piki::position(handle(h),vec(v),e);});}
bool Bridge::sortFormation(Navi& n,PikiHandle h,int happa,std::string& e){return m->operation(n,e,[&]{piki::Frame f;return m->read(h,f,e)&&f.captain==&n&&piki::sortFormation(handle(h),happa,e);});}
bool Bridge::holdFields(Navi& n,float t,float d,float h,std::string& e){if(!std::isfinite(t)||!std::isfinite(d)||!std::isfinite(h))return fail(e,"nonfinite source hold fields");return m->operation(n,e,[&]{return m->source.holdFields(n,t,d,h,e);});}
bool Bridge::nextThrowPiki(Navi& n,std::optional<PikiHandle> h,std::string& e){return m->operation(n,e,[&]{piki::Frame f;return (!h||(m->read(*h,f,e)&&f.captain==&n))&&m->source.nextThrowPiki(n,h,e);});}
bool Bridge::findNextThrowPiki(Navi& n,std::string& e){return m->operation(n,e,[&]{std::vector<PikiFrame> list;if(!squad(n,list,e))return false;float best=200;std::optional<PikiHandle> selected;for(const auto& f:list){const float dx=f.position.x-n.mSRT.t.x,dz=f.position.z-n.mSRT.t.z,dist=std::sqrt(dx*dx+dz*dz);if(f.captain==&n&&f.state==actions::PikiState::Walk&&f.throwable&&dist<best){best=dist;selected=f.handle;}}return m->source.nextThrowPiki(n,selected,e);});}
bool Bridge::throwPiki(Navi& n,PikiHandle h,Vec3 v,std::string& e){if(!finite(v))return fail(e,"nonfinite source action position");return m->operation(n,e,[&]{piki::Frame f;return m->read(h,f,e)&&f.captain==&n&&piki::launch(handle(h),&n,vec(v),e);});}
bool Bridge::feedback(Navi& n,actions::Feedback f,PikiHandle h,std::string& e){return m->operation(n,e,[&]{piki::Frame observed;return (!h.actor||m->read(h,observed,e))&&m->source.feedback(n,f,h,e);});}
bool Bridge::world(party::WorldFacts& out,std::string& e)const{party::WorldFacts f;if(!m->check(nullptr,e)||!m->source.world(f,e)||!m->check(nullptr,e))return false;if(!f.active)return fail(e,"source world facts disagree with actual active phase");out=f;return true;}
bool Bridge::captain(const Navi& n,party::CaptainFacts& out,Vec3& position,std::string& e)const{party::CaptainFacts f;Observation observation;if(!m->operation(const_cast<Navi&>(n),e,[&]{if(!m->source.observe(n,observation,e)||!m->check(&n,e)||!pc_p2_original_captain_actor_lifetime(&n,f.alive))return false;auto* s=dynamic_cast<State*>(const_cast<Navi&>(n).getCurrState());f.state=s->sourceStateId();f.controller=n.mKontroller!=nullptr;f.movieActor=observation.movieActor;return finite(vec(n.mSRT.t));}))return false;position=vec(n.mSRT.t);out=f;return true;}
bool Bridge::members(const Navi& n,std::vector<party::Member>& out,std::string& e)const{std::vector<party::Member> next;if(!m->operation(const_cast<Navi&>(n),e,[&]{std::vector<piki::Frame> list;if(!piki::squad(const_cast<Navi*>(&n),list,e))return false;for(const auto& f:list){piki::Frame current;piki::PhysicalFacts facts;if(!m->observe({f.handle.body,f.handle.lifetime},current,facts,e)||current.captain!=&n)return false;next.push_back({{f.handle.body,f.handle.lifetime},vec(current.position),current.species,facts.alive,current.releasable});}return true;}))return false;out=std::move(next);return true;}
bool Bridge::togglePlayer(Navi& a,Navi& b,std::string& e){return m->operation(a,e,[&]{if(&a==&b||!m->check(&b,e))return false;const auto* state=b.getCurrState();return m->source.togglePlayer(a,b,e)&&m->check(&b,e)&&b.getCurrState()==state;});}
bool Bridge::changeVoice(Navi& n,std::string& e){return m->operation(n,e,[&]{return m->source.changeVoice(n,e);});}
bool Bridge::whistleMember(Navi& n,PikiHandle h,bool combine,bool newToParty,std::string& e){return m->operation(n,e,[&]{piki::Frame f;if(!m->read(h,f,e))return false;if(combine)return fail(e,"actual SourcePiki combining whistle receiver is not implemented");if(!newToParty)return fail(e,"actual SourcePiki non-new whistle receiver is not implemented");return piki::whistle(handle(h),&n,e);});}
bool Bridge::dismissSound(Navi& n,std::string& e){return m->operation(n,e,[&]{return m->source.dismissSound(n,e);});}
bool Bridge::freeMember(Navi& n,PikiHandle h,float r,Vec3 v,bool dismiss,std::string& e){if(!std::isfinite(r)||r<0||!finite(v))return fail(e,"invalid source dismissal geometry");return m->operation(n,e,[&]{piki::Frame f;return dismiss&&m->read(h,f,e)&&f.captain==&n&&f.releasable&&piki::gather(handle(h),vec(v),r,e);});}
bool Bridge::disbandTimer(Navi& n,unsigned t,std::string& e){return m->operation(n,e,[&]{return m->source.disbandTimer(n,t,e);});}
bool Bridge::followFrame(const Navi& n,party::FollowFrame& out,std::string& e)const{party::FollowFrame f;if(!m->operation(const_cast<Navi&>(n),e,[&]{return m->source.followFrame(n,f,e);}))return false;out=f;return true;}
bool Bridge::moveRotation(Navi& n,bool enable,std::string& e){return m->operation(n,e,[&]{if(enable)n.resetCreatureFlag(CF_UsePriorityFaceDir);else n.setCreatureFlag(CF_UsePriorityFaceDir);return true;});}
bool Bridge::randomChoice(float& out,std::string& e){float value;if(!m->check(nullptr,e)||!m->source.randomChoice(value,e)||!m->check(nullptr,e)||!std::isfinite(value)||value<0||value>1)return false;out=value;return true;}
bool Bridge::followFeedback(Navi& n,party::FollowFeedback f,std::string& e){return m->operation(n,e,[&]{return m->source.followFeedback(n,f,e);});}
bool Bridge::enemy(party::EnemyHandle h,party::EnemyFrame& out,std::string& e)const{party::EnemyFrame f;if(!m->check(nullptr,e)||!m->source.enemy(h,f,e)||!m->check(nullptr,e))return false;out=f;return true;}
bool Bridge::followPunch(Navi& n,party::EnemyHandle h,Vec3 v,std::string& e){if(!finite(v))return fail(e,"nonfinite source action position");return m->operation(n,e,[&]{return m->source.followPunch(n,h,v,e);});}
}}}
