#include "pc_p2_original_captain_actions_party.h"
#include <cmath>
#include <set>
namespace p2original { namespace captain { namespace party {
bool switchAllowed(const WorldFacts& w,const CaptainFacts& c){
 return w.active&&!w.softPaused&&!w.multiplayer&&w.demoInactive&&w.switchUnlocked&&c.alive
  &&c.state!=StateId::Nuku&&c.state!=StateId::NukuAdjust&&c.state!=StateId::Punch;
}
bool whistleAllowed(const WorldFacts& w,const CaptainFacts& c){
 return c.alive&&!c.controller&&(c.state==StateId::Walk||c.state==StateId::Pellet)
  &&(w.mode!=0||w.day!=0||w.reunited);
}
bool needsChange(StateId s){return s==StateId::Walk||s==StateId::Follow;}
namespace {
bool finite(Vec3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
Vec3 difference(Vec3 a,Vec3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
float normalize(Vec3& v){float d=std::sqrt(v.x*v.x+v.y*v.y+v.z*v.z);if(d>0){v.x/=d;v.y/=d;v.z/=d;}return d;}
void move(Vec3& v,Vec3 direction,float d){v.x+=direction.x*d;v.y+=direction.y*d;v.z+=direction.z*d;}
void separate(Group& g,Vec3 captain){Vec3 diff=difference(g.center,captain);float d=normalize(diff)-g.radius-25;if(d<20)move(g.center,diff,20-d);}
}
bool dismissGroups(const std::vector<Member>& members,Vec3 captain,Vec3 other,bool otherAlive,
 std::array<Group,8>& output,std::string& error){
 if(!finite(captain)||!finite(other)){error="nonfinite actual captain position";return false;}
 std::array<Group,8> groups{};std::set<std::pair<Piki*,std::uint64_t>> identities;
 unsigned count=0;
 for(const auto& m:members){
  if(!m.alive||!m.releasable)continue;
  if(!m.handle.actor||!m.handle.lifetime||m.kind>=8||!finite(m.position)
   ||!identities.emplace(m.handle.actor,m.handle.lifetime).second||++count>100){error="invalid source dismissal member";return false;}
  auto& g=groups[m.kind];++g.count;move(g.center,m.position,1);
 }
 for(auto& g:groups)if(g.count){float scale=1.0f/g.count;g.center.x*=scale;g.center.y*=scale;g.center.z*=scale;g.radius=std::sqrt(float(g.count))*6.25f;}
 for(unsigned pass=0;pass<4;++pass)for(unsigned c=0;c<8;++c){
  if(groups[c].count){separate(groups[c],captain);if(otherAlive)separate(groups[c],other);}
  for(unsigned j=c+1;j<8;++j)if(groups[c].count&&groups[j].count){
   auto diff=difference(groups[c].center,groups[j].center);
   float d=normalize(diff)-groups[c].radius-groups[j].radius;
   if(d<20){move(groups[c].center,diff,20-d);move(groups[j].center,diff,-(20-d));}
  }
 }
 output=groups;return true;
}
bool followVelocity(Vec3 self,const FollowFrame& f,float speed,Vec3& out,bool& tooFar,std::string& error){
 if(!finite(self)||!finite(f.leaderPosition)||!finite(f.leaderVelocity)||!finite(f.leaderTargetVelocity)
  ||!std::isfinite(speed)||speed<0||!std::isfinite(f.leaderFace)||!std::isfinite(f.plateRadius)||f.plateRadius<0){error="invalid actual Follow frame";return false;}
 auto target=f.leaderPosition;auto velocity=f.leaderVelocity;
 float leaderSpeed=std::sqrt(velocity.x*velocity.x+velocity.y*velocity.y+velocity.z*velocity.z);
 if(leaderSpeed<=20&&(f.leaderState==StateId::Throw||f.leaderState==StateId::ThrowWait)){
  target.x+=std::sin(f.leaderFace+1.4137167f)*30;target.z+=std::cos(f.leaderFace+1.4137167f)*30;
 }else if(f.leaderState==StateId::Punch){target.x-=std::sin(f.leaderFace)*f.plateRadius;target.z-=std::cos(f.leaderFace)*f.plateRadius;}
 auto diff=difference(target,self);float dist=normalize(diff);
 if(dist<30)speed=0;
 Vec3 next{diff.x*speed,diff.y*speed,diff.z*speed};
 if(dist<60){next.x=(next.x+f.leaderTargetVelocity.x)*.5f;next.y=(next.y+f.leaderTargetVelocity.y)*.5f;next.z=(next.z+f.leaderTargetVelocity.z)*.5f;}
 tooFar=dist>430;out=next;return true;
}
} } }
#if defined(PIKI_PC_PORT)
#include "pc_p2_original_captain_states.h"
#include "pc_p2_original_captain_motion.h"
#include "Navi.h"
#include "pc_p2_equipment.h"
extern bool pc_p2_original_captain_motion_preflight(Navi*,unsigned,std::string&) __attribute__((weak));
namespace p2original { namespace captain { namespace party {
namespace {
bool fail(std::string& e,const char* s){e=s;return false;}
PartySource* source(Navi* n,std::string& error){
 auto* p=pc_p2_original_captain_party_source(n);auto* scene=pc_p2_original_captain_loaded_scene();
 auto* world=pc_p2_original_captain_world();
 if(!n||!p||!scene||&p->scene()!=scene||!world||world->phase()!=Phase::GameWorldActive
  ||!scene->incarnation()||scene->selectedCampaign().empty()||scene->selectedFingerprint().empty()||scene->sourceCatalog().empty()
  ||world->incarnation()!=scene->incarnation()
  ||world->selectedCampaign()!=scene->selectedCampaign()||world->selectedFingerprint()!=scene->selectedFingerprint()
  ||world->sourceCatalog()!=scene->sourceCatalog()
  ||!scene->captainAt(0)||!scene->captainAt(1)||scene->captainAt(0)==scene->captainAt(1)
  ||(n!=scene->captainAt(0)&&n!=scene->captainAt(1))){fail(error,"missing actual source party scene/roster");return nullptr;}
 return const_cast<PartySource*>(p);
}
Navi* other(const PartySource& p,Navi* n){return p.scene().captainAt(p.scene().captainAt(0)==n?1:0);}
}
bool whistleCaptain(Navi* n,Navi* caller,bool combine,bool newToParty,std::string& e){
 (void)combine; // Retail actNavi does not inspect mDoCombine; Piki receiver does.
 auto* p=source(n,e);if(!p||caller!=other(*p,n))return fail(e,"whistle caller not other actual source captain");
 WorldFacts w;CaptainFacts c;Vec3 position;
 if(!p->world(w,e)||!p->captain(*n,c,position,e))return false;
 // Already-Follow is NOT callable in retail; do not invent a no-op success.
 if(!whistleAllowed(w,c))return fail(e,"source captain whistle not admitted");
 if(!enterFollow(n,newToParty,e))return false;
 std::vector<Member> members;if(!p->members(*n,members,e))return false;
 // Snapshot BEFORE stimuli mutate the CPlate, exactly as source actNavi.
 for(const auto& m:members)if(!p->whistleMember(*caller,m.handle,true,true,e))return false;
 return true;
}
bool dismissCaptain(Navi* n,std::string& e){
 auto* p=source(n,e);if(!p)return false;CaptainFacts c;Vec3 pos;
 if(!p->captain(*n,c,pos,e))return false;
 if(c.controller||c.state!=StateId::Follow)return fail(e,"source Kaisan recipient not uncontrolled Follow");
 return pc_p2_original_captain_transit(n,StateId::Walk,e);
}
bool releasePikis(Navi* n,bool& released,std::string& e){
 auto* p=source(n,e);if(!p)return false;WorldFacts w;
 if(!p->world(w,e))return false;
 if(!w.active){released=false;return true;}
 Navi* partner=other(*p,n);CaptainFacts self,op;Vec3 pos,opos;
 if(!partner||!p->captain(*n,self,pos,e)||!p->captain(*partner,op,opos,e))return false;
 bool dismissNavi=op.state==StateId::Follow;
 if(!op.controller&&dismissNavi&&!pc_p2_original_captain_transit(partner,StateId::Walk,e))return false;
 std::vector<Member> members;if(!p->members(*n,members,e))return false;
 std::array<Group,8> groups;
 if(!dismissGroups(members,pos,opos,op.alive,groups,e))return false;
 unsigned count=0;for(const auto& g:groups)count+=g.count;
 if((dismissNavi||count)&&!p->dismissSound(*n,e))return false;
 if(!count){released=dismissNavi;return true;}
 for(const auto& m:members)if(m.alive&&m.releasable){const auto& g=groups[m.kind];if(!p->freeMember(*n,m.handle,g.radius,g.center,true,e))return false;}
 if(!p->disbandTimer(*n,60,e))return false;
 released=true;return true;
}
bool switchCaptain(Navi* n,std::string& e){
 auto* p=source(n,e);if(!p)return false;Navi* partner=other(*p,n);
 WorldFacts w;CaptainFacts c;Vec3 pos;
 if(!partner||!p->world(w,e)||!p->captain(*partner,c,pos,e))return false;
 if(!switchAllowed(w,c))return fail(e,"source Y switch not admitted");
 if(!p->togglePlayer(*n,*partner,e)||!p->changeVoice(*partner,e))return false;
 if(c.state==StateId::Follow){
  // Retail stimulate's result is ignored here. A noncallable old captain
  // must not undo the already-completed toggle or skip target Change.
  std::string ignored;whistleCaptain(n,partner,false,false,ignored);
 }
 if(!p->captain(*partner,c,pos,e))return false;
 return !needsChange(c.state)||pc_p2_original_captain_transit(partner,StateId::Change,e);
}
} // party
namespace {
enum class FollowMode {Alert,Normal,Idle,Punch};
class Follow final:public NativeState {
 FollowMode mode_=FollowMode::Normal;unsigned idle_=0,seek_=0,idleMotion_=31;
 Navi* entering_=nullptr;bool newToParty_=false;
 party::EnemyHandle enemy_;
 bool motion(Navi* n,unsigned id){std::string e;auto* b=pc_p2_original_captain_source_bank();return b&&b->startMotion(n,static_cast<Motion>(id),static_cast<Motion>(id),(id==30||id==31)?Listener::None:Listener::SourceActor,Listener::None,e);}
public:
 Follow():NativeState(StateId::Follow){}
 bool sourceInvincible()const final{return false;}
 void arm(Navi* n,bool isNew){entering_=n;newToParty_=isNew;}
 void disarm(){entering_=nullptr;newToParty_=false;}
 void init(Navi* n)override{
  bool isNew=entering_==n&&newToParty_;disarm();
  mode_=isNew?FollowMode::Alert:FollowMode::Normal;idle_=0;seek_=0;enemy_={};
  motion(n,isNew?32:30);std::string e;auto* p=pc_p2_original_captain_party_source(n);
  if(p){auto* source=const_cast<party::PartySource*>(p);if(isNew)source->followFeedback(*n,party::FollowFeedback::Alert,e);source->moveRotation(*n,true,e);}
 }
 bool assist(party::EnemyHandle enemy){if(mode_!=FollowMode::Normal&&mode_!=FollowMode::Idle)return false;enemy_=enemy;mode_=FollowMode::Punch;seek_=idle_=0;return true;}
 bool sourceAnimationKey(Navi* n,int key,std::string& e)override{return sourceKey(n,key,e);}
 bool sourceKey(Navi* n,int key,std::string& e){
  auto* p=pc_p2_original_captain_party_source(n);party::FollowFrame f;
  if(key==1000){if(mode_==FollowMode::Alert){mode_=FollowMode::Normal;if(!motion(n,30))return false;}else if(mode_==FollowMode::Idle){idle_=0;mode_=FollowMode::Normal;if(!motion(n,31))return false;}}
  auto* bank=pc_p2_original_captain_source_bank();MotionState state;
  if(p&&p->followFrame(*n,f,e)&&!f.frozen&&key==200&&bank&&bank->state(n,state,e)&&unsigned(state.motion)==50)
   return const_cast<party::PartySource*>(p)->followFeedback(*n,party::FollowFeedback::Land,e);
  return true;
 }
 void exec(Navi* n)override{
  std::string e;auto* raw=pc_p2_original_captain_party_source(n);
  auto* action=pc_p2_original_captain_action_source(n);auto* bank=pc_p2_original_captain_source_bank();
  if(!raw||!action||!bank)return;
  auto* p=const_cast<party::PartySource*>(raw);auto* a=const_cast<actions::ActionSource*>(action);
  party::CaptainFacts c;actions::Vec3 pos;party::WorldFacts w;party::FollowFrame f;MotionState motionState;
  if(!p->world(w,e)||!p->captain(*n,c,pos,e)||!p->followFrame(*n,f,e)||!bank->state(n,motionState,e))return;
  if(!w.demoInactive)return;
  if(c.controller){pc_p2_original_captain_transit(n,StateId::Walk,e);return;}
  if(mode_==FollowMode::Alert){if(unsigned(motionState.motion)!=32){mode_=FollowMode::Normal;motion(n,30);}n->mTargetVelocity.set(0,0,0);return;}
  if(f.leaderStuck){pc_p2_original_captain_transit(n,StateId::Walk,e);return;}
  if(mode_==FollowMode::Punch){
   party::EnemyFrame target;
   if(!enemy_.actor||!p->enemy(enemy_,target,e)||!target.alive||target.flying||target.underground){mode_=FollowMode::Normal;enemy_={};motion(n,30);return;}
   auto diff=party::difference(target.center,pos);float distance=party::normalize(diff);
   if(distance-target.radius<8){p->followPunch(*n,enemy_,target.center,e);return;}
   if(++seek_>=60){mode_=FollowMode::Normal;enemy_={};motion(n,30);return;}
   if(a->control(*n,e))n->mTargetVelocity.set(diff.x*f.sourceMoveSpeed*.5f,diff.y*f.sourceMoveSpeed*.5f,diff.z*f.sourceMoveSpeed*.5f);
   return;
  }
  auto lv=f.leaderVelocity;float speed=std::sqrt(lv.x*lv.x+lv.y*lv.y+lv.z*lv.z);bool moving=speed>20;
  if(mode_==FollowMode::Idle){if(moving){motion(n,32);mode_=FollowMode::Alert;}else{n->mTargetVelocity.set(0,0,0);if(unsigned(motionState.motion)!=idleMotion_){mode_=FollowMode::Normal;motion(n,30);}}return;}
  if(moving)idle_=0;else if(idle_<90)++idle_;else{
   float choice;if(!p->randomChoice(choice,e)||!std::isfinite(choice)||choice<0||choice>1)return;
   const unsigned clips[]={50,0,3,54};unsigned index=choice<.25f?0:choice<.5f?1:choice<.75f?2:3;
   idleMotion_=clips[index];if(!motion(n,idleMotion_))return;mode_=FollowMode::Idle;
   if(!f.frozen){const party::FollowFeedback sounds[]={party::FollowFeedback::Jump,party::FollowFeedback::Yawn,party::FollowFeedback::Chat,party::FollowFeedback::Look};p->followFeedback(*n,sounds[index],e);}
  }
  actions::Vec3 velocity;bool tooFar;
  if(!party::followVelocity(pos,f,pc_p2_equipment_speed(f.sourceMoveSpeed),velocity,tooFar,e))return;
  if(tooFar){pc_p2_original_captain_transit(n,StateId::Walk,e);return;}
  if(a->control(*n,e))n->mTargetVelocity.set(velocity.x,velocity.y,velocity.z);
 }
};
class Change final:public NativeState {
 bool finished_=false;
public:
 Change():NativeState(StateId::Change){}
 bool sourceInvincible()const final{return false;}
 void init(Navi* n)override{
  finished_=false;std::string e;auto* s=pc_p2_original_captain_party_source(n);
  party::CaptainFacts c;actions::Vec3 pos;
  if(!s||!s->captain(*n,c,pos,e))return;
  if(!c.movieActor){auto* bank=pc_p2_original_captain_source_bank();if(bank)bank->start(n,static_cast<Motion>(32),e);}
 }
 void exec(Navi* n)override{
  std::string e;auto* s=pc_p2_original_captain_party_source(n);party::CaptainFacts c;actions::Vec3 pos;
  if(!s||!s->captain(*n,c,pos,e))return;
  if(c.movieActor)pc_p2_original_captain_transit(n,StateId::Walk,e);
  n->mTargetVelocity.set(0,0,0);
  if(finished_)pc_p2_original_captain_transit(n,StateId::Walk,e);
 }
 bool sourceAnimationKey(Navi* n,int key,std::string& e)override{return sourceKey(n,key,e);}
 bool sourceKey(Navi* n,int key,std::string& e){if(key==1000){finished_=true;auto* b=pc_p2_original_captain_source_bank();return b&&b->startMotion(n,Motion::Walk,Motion::Walk,Listener::None,Listener::None,e);}return true;}
};
}
namespace party {
bool enterFollow(Navi* n,bool isNew,std::string& e){
 if(!n||!n->mStateMachine){e="missing source Follow FSM";return false;}
 auto* fsm=n->mStateMachine;Follow* state=nullptr;
 for(int i=0;i<fsm->mStateCount;++i)if(fsm->mStates[i]->getID()==nativeId(StateId::Follow)){state=dynamic_cast<Follow*>(fsm->mStates[i]);break;}
 if(!state){e="missing actual source Follow state";return false;}
 state->arm(n,isNew);
 bool result=pc_p2_original_captain_transit(n,StateId::Follow,e);
 state->disarm();return result;
}
bool assistPunch(Navi* n,EnemyHandle enemy){auto* state=n?dynamic_cast<Follow*>(n->getCurrState()):nullptr;return state&&state->assist(enemy);}
}
void registerPartyStates(NaviStateMachine& fsm){fsm.registerState(new Change);fsm.registerState(new Follow);}
bool partyKey(Navi* n,int key){
 std::string e;return partyKey(n,key,e);
}
bool partyKey(Navi* n,int key,std::string& e){
 if(!n)return false;
 auto* change=dynamic_cast<Change*>(n->getCurrState());
 if(change)return change->sourceKey(n,key,e);
 auto* follow=dynamic_cast<Follow*>(n->getCurrState());if(!follow)return false;
 return follow->sourceKey(n,key,e);
}
} }
bool pc_p2_original_captain_party_preflight(Navi* n,p2original::captain::StateId id,std::string& e){
 using namespace p2original::captain;
 if(id!=StateId::Follow&&id!=StateId::Change){e="not a source party state";return false;}
 auto* p=party::source(n,e);if(!p)return false;
 auto* bank=pc_p2_original_captain_source_bank();MotionState bound;
 if(!bank||!bank->ready()||!bank->state(n,bound,e)){e="missing actual source party bank/roster binding";return false;}
 bool registered=false;auto* machine=n->mStateMachine;
 if(machine)for(int i=0;i<machine->mStateCount;++i){auto* s=machine->mStates[i];auto* typed=dynamic_cast<NativeState*>(s);if(s->getID()==nativeId(id)&&typed&&typed->sourceStateId()==id&&typed->nativeState()==s){registered=true;break;}}
 if(!registered){e="missing registered native source party state";return false;}
 party::WorldFacts w;party::CaptainFacts self,partner;actions::Vec3 pos,otherPos;
 auto* other=party::other(*p,n);
 if(!p->world(w,e)||!p->captain(*n,self,pos,e)||!other||!p->captain(*other,partner,otherPos,e)
  ||!w.active||!self.alive||!party::finite(pos)||!party::finite(otherPos)){e="invalid source party actor/world observations";return false;}
 bool selfAlive=false,partnerAlive=false;
 if(!pc_p2_original_captain_actor_lifetime(n,selfAlive)||!pc_p2_original_captain_actor_lifetime(other,partnerAlive)
  ||self.alive!=selfAlive||partner.alive!=partnerAlive){e="party observation differs from actual source actor lifetime";return false;}
 if(!pc_p2_original_captain_motion_preflight){e="selected authored party motion query unavailable";return false;}
 const unsigned clips[]={30,31,32,50,0,3,54};
 unsigned count=id==StateId::Change?3:7;
 for(unsigned i=0;i<count;++i)if(!pc_p2_original_captain_motion_preflight(n,clips[i],e))return false;
 if(id==StateId::Follow){
  auto* action=pc_p2_original_captain_action_source(n);party::FollowFrame f;actions::ActorFrame frame;
  if(!action||&action->scene()!=&p->scene()||!action->frame(*n,frame,e)||!p->followFrame(*n,f,e)
   ||!std::isfinite(f.sourceMoveSpeed)||f.sourceMoveSpeed<=0){e="missing actual source Follow control/body observation";return false;}
  actions::Vec3 velocity;bool far;
  if(!party::followVelocity(pos,f,f.sourceMoveSpeed,velocity,far,e))return false;
 }
 if(party::source(n,e)!=p||pc_p2_original_captain_source_bank()!=bank){e="party scene changed during preflight";return false;}
 return true;
}
bool pc_p2_original_captain_party_advance_animation(Navi* n,float frames,std::string& e){
 using namespace p2original::captain;
 if(!n||!std::isfinite(frames)||frames<0){e="invalid source party animation advance";return false;}
 auto* state=dynamic_cast<State*>(n->getCurrState());
 if(!state||state->nativeState()!=n->getCurrState()
  ||(state->sourceStateId()!=StateId::Follow&&state->sourceStateId()!=StateId::Change)){e="source party state is not current";return false;}
 if(!pc_p2_original_captain_party_preflight(n,state->sourceStateId(),e))return false;
 auto* bank=pc_p2_original_captain_source_bank();auto* exact=n->getCurrState();
 return bank->advance(n,frames,[&](int key){
  // Authored callbacks may transit; SourceBank generation rules terminate the
  // old delivery. Never deliver another old-state key to a new native state.
  if(n->getCurrState()!=exact)return true;
  if(!party::source(n,e))return false;
  return partyKey(n,key,e);
 },e);
}
#endif
