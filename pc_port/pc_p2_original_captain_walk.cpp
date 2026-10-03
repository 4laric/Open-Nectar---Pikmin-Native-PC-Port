#include "pc_p2_original_captain_walk.h"
#include <cmath>
namespace p2original { namespace captain { namespace walk {
namespace {
constexpr float pi=3.14159265358979323846f;
bool fail(std::string& e,const char* text){e=text;return false;}
bool finite(Vec3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
float angle(float a){if(a<0)a+=2*pi;if(a>=2*pi)a-=2*pi;return a;}
float distanceAngle(float a,float b){float d=angle(a-b);if(d>=pi)d=-angle(2*pi-d);return d;}
Vec3 subtract(Vec3 a,Vec3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
float normalize(Vec3& v){float length=std::sqrt(v.x*v.x+v.y*v.y+v.z*v.z);if(length>0){v.x/=length;v.y/=length;v.z/=length;}return length;}
void emit(Output& out,Kind kind){out.commands.push_back(Command{kind});}
void motion(Output& out,Motion m){Command c{Kind::StartMotion};c.motion=m;out.commands.push_back(c);}
void transit(Output& out,StateId id){Command c{Kind::TransitSelf};c.state=id;out.commands.push_back(c);}
void wait(State& s,Output& out){s.ai=AI::Wait;motion(out,Motion::Step);}
void control(const Actor& a,const World& w,Output& out){
 if(!w.movieFlagActive)emit(out,Kind::MakeVelocity);
 emit(out,Kind::MakeCStick);
 if(!a.movieActor&&(!w.storyMode||w.activeActor))emit(out,Kind::Rappa);
}
bool random(const Frame& f,unsigned& cursor,float& value,std::string& error){
 if(cursor>=f.randomValues.size())return fail(error,"missing actual source RNG sample");
 value=f.randomValues[cursor++];if(!std::isfinite(value)||value<0||value>1)return fail(error,"invalid source RNG sample");return true;
}
bool animation(const Frame& f,unsigned& rng,State& s,Output& out,std::string& error){
 float sample;if(!random(f,rng,sample,error))return false;
 s.ai=AI::Animation;unsigned index=sample<.25f?0:sample<.5f?1:sample<.75f?2:3;
 const Motion choices[]={Motion::Yawn,Motion::LookBack,Motion::LookAround,f.actor->hasController?Motion::Exercise:Motion::Jump};
 s.animation=choices[index];motion(out,*s.animation);
 if(!f.world->frozen){Command c{Kind::IdleVoice};c.motion=*s.animation;c.scalar=float(f.actor->slot==1&&f.world->storyMode&&f.world->debtPaid?2:f.actor->slot);out.commands.push_back(c);}
 return true;
}
bool candidateValid(const Candidate& c){return c.identity&&finite(c.position)&&std::isfinite(c.sphereRadius)&&c.sphereRadius>=0;}
bool checkAI(const Frame& f,unsigned& rng,State& s,Output& out,std::string& error){
 if(f.actor->hasController)return true;
 if(!f.cellCandidates)return fail(error,"missing actual source CellIterator query");
 const Candidate* target=nullptr;
 for(const auto& c:*f.cellCandidates){
  if(!candidateValid(c))return fail(error,"invalid source cell candidate");
  if(c.alive&&c.teki&&c.living&&c.emotionNonzero){target=&c;break;}
  if(!target&&!c.self&&c.navi)target=&c;
 }
 if(target&&target->teki){
  if(!target->weakBitterDrop)return fail(error,"missing source EnemyInfo bitter drop category");
  s.target=target->identity;
  if(*target->weakBitterDrop)wait(s,out);
  else {float sample;if(!random(f,rng,sample,error))return false;s.ai=AI::Escape;s.idleTimer=0;s.escapeCCW=int(2*sample)!=0;s.escapeTimer=0;}
 }else if(target&&target->navi)s.target=target->identity;
 return true;
}
bool target(const Frame& f,const State& s,const Candidate*& output,std::string& error){
 if(!s.target){output=nullptr;return true;}
 if(!f.currentTarget||f.currentTarget->identity!=s.target||!candidateValid(*f.currentTarget))return fail(error,"missing live source target identity");
 output=&*f.currentTarget;return true;
}
bool executeAI(const control::Params& params,const Frame& f,unsigned& rng,State& s,Output& out,std::string& error){
 const auto& actor=*f.actor;const auto& world=*f.world;const Candidate* t=nullptr;
 switch(s.ai){
 case AI::Control:return true;
 case AI::Wait:{
  emit(out,Kind::AddVelocity);s.idleTimer-=f.deltaTime;
  if(s.idleTimer<=0){if(!animation(f,rng,s,out,error))return false;float sample;if(!random(f,rng,sample,error))return false;s.idleTimer=2+sample;}
  else if(s.target){
   if(!target(f,s,t,error))return false;
   Vec3 sep=subtract(actor.position,t->position);
   if(sep.x*sep.x+sep.z*sep.z>100*100)s.target=0;
   else { // Retail assembly at8017F668 uses normalized target-minus-Navi,
          // despite the research C expression naming absolute targetPos.
    Command c{Kind::SetFace};Vec3 toward=subtract(t->position,actor.position);
    c.scalar=angle(actor.faceDirection+.2f*distanceAngle(angle(std::atan2(toward.x,toward.z)),actor.faceDirection));out.commands.push_back(c);
   }
  }
  return checkAI(f,rng,s,out,error);
 }
 case AI::Animation:
  emit(out,Kind::AddVelocity);
  if(!s.animation||!f.assertIdleMotion)return fail(error,"missing actual source assertMotion result");
  if(!*f.assertIdleMotion)wait(s,out);
  return checkAI(f,rng,s,out,error);
 case AI::Attack:
 case AI::Escape:{
  if(!s.target||!target(f,s,t,error))return fail(error,"source attack/escape requires live target");
  bool escaping=s.ai==AI::Escape;
  if(!t->alive){if(escaping)s.idleTimer=5;wait(s,out);s.target=0;return true;}
  if(escaping&&(!s.escapeTimer||!s.escapeCCW))return fail(error,"missing source escape initialization");
  if(escaping&&*s.escapeTimer)--*s.escapeTimer;
  Vec3 sep=escaping?subtract(actor.position,t->position):subtract(t->position,actor.position);
  float length=normalize(sep),dist=length-t->sphereRadius;
  if(escaping&&dist>35){s.idleTimer=5;wait(s,out);return true;}
  if(escaping&&dist>15){float oldX=sep.x,oldZ=sep.z;sep.x=*s.escapeCCW?oldZ:-oldZ;sep.z=*s.escapeCCW?-oldX:oldX;}
  control(actor,world,out);
  float speed=params.values().moveSpeed*(escaping?1:.5f);
  Command c{Kind::AddVelocity};c.vector={sep.x*speed,sep.y*speed,sep.z*speed};out.commands.push_back(c);
  if(!escaping&&dist<10){Command turn{Kind::TurnTo};turn.vector=t->position;out.commands.push_back(turn);transit(out,StateId::Punch);}
  return true;
 }
 }
 return fail(error,"invalid source Walk AI state");
}
bool afterDismiss(const Frame& f,State& s,Output& out,std::string& error){
 const auto& b=*f.buttons;const auto& w=*f.world;
 if(b.xHeld){
  if(!s.dismissTimer)return fail(error,"source dismissal timer not established");
  if(*s.dismissTimer){*s.dismissTimer=std::uint8_t(*s.dismissTimer+1);if(*s.dismissTimer>35){s.dismissTimer=0;if(w.napsackReceipt||!w.storyMode){transit(out,StateId::Pellet);return true;}}}
 }else s.dismissTimer=0;
 if(!w.softPaused&&w.demoInactive&&!w.multiplayer&&b.yDown&&w.switchUnlocked){
  if(!f.other||!f.other->identity)return fail(error,"missing actual other captain/state producer");
  const auto& other=*f.other;
  if(other.alive&&other.state!=StateId::Nuku&&other.state!=StateId::NukuAdjust&&other.state!=StateId::Punch){
   emit(out,Kind::TogglePlayer);Command voice{Kind::ChangeVoice};voice.target=other.identity;out.commands.push_back(voice);
   if(other.state==StateId::Follow){Command whistle{Kind::WhistleSelfPreserveParties};whistle.target=other.identity;out.commands.push_back(whistle);}
   if(other.needYChangeMotion){Command change{Kind::TransitOtherChange};change.target=other.identity;change.state=StateId::Change;out.commands.push_back(change);}
  }
 }
 return true;
}
bool buttons(const Frame& f,State& s,Output& out,std::string& error){
 if(!f.buttons||!f.stickCount)return fail(error,"missing actual source controller/stick producer");
 const auto& b=*f.buttons;
 if(*f.stickCount){transit(out,StateId::Stuck);return true;}
 if(!f.onionQueryComplete)return fail(error,"missing source checkOnyon query");
 if(f.onion&&b.aDown&&!f.onion->isPod){if(!f.onion->identity)return fail(error,"invalid source onion identity");Command c{Kind::TransitSelf};c.state=StateId::Container;c.target=f.onion->identity;out.commands.push_back(c);return true;}
 if(b.bDown){transit(out,StateId::Gather);return true;}
 if(b.aDown){emit(out,Kind::RequestActionButton);out.continuation=Stage::ActionButton;return true;}
 if(b.upDown||b.downDown){Command c{Kind::TransitSelf};c.state=StateId::Dope;c.bitter=b.upDown;out.commands.push_back(c);return true;}
 if(b.xDown){emit(out,Kind::RequestDismiss);out.continuation=Stage::Dismiss;return true;}
 return afterDismiss(f,s,out,error);
}
}
const char* authoredClip(Motion m){switch(m){case Motion::Walk:return "walk.bca";case Motion::Wait:return "wait.bca";case Motion::Step:return "asibumi.bca";case Motion::Yawn:return "akubi.bca";case Motion::LookBack:return "furimuku.bca";case Motion::LookAround:return "sagasu2.bca";case Motion::Jump:return "jump.bca";case Motion::Exercise:return "gattu.bca";default:return nullptr;}}
bool init(const Actor& a,State& s,Output& out,std::string& error){
 error.clear();if(!finite(a.position))return fail(error,"invalid source initial position");
 // Preserve fields source init leaves untouched; do not fabricate a checkpoint.
 s.ai=AI::Control;s.idleTimer=3;s.target=0;s.initialPosition=a.position;s.collisionTimer=0;out={};
 if(a.alive&&!a.movieActor)motion(out,Motion::Walk);
 return true;
}
bool step(const control::Params& params,const Frame& f,State& s,Output& out,std::string& error){
 error.clear();if(!params.authenticated()||!f.actor||!f.world||!std::isfinite(f.deltaTime)||f.deltaTime<0||f.actor->slot>1||!finite(f.actor->position)||!std::isfinite(f.actor->faceDirection)||f.actor->faceDirection<0||f.actor->faceDirection>=2*pi||!std::isfinite(f.actor->sceneAnimationTimer)||f.actor->sceneAnimationTimer<0||!std::isfinite(s.idleTimer))return fail(error,"missing or invalid source Walk frame authority");
 State next=s;Output candidate;if(next.collisionTimer)--next.collisionTimer;
 const auto& a=*f.actor;unsigned rng=0;
 if(a.alive){
  if(!f.postControlSceneAnimationTimer||!std::isfinite(*f.postControlSceneAnimationTimer)||*f.postControlSceneAnimationTimer<0)return fail(error,"missing post-control source idle timer");
  control(a,*f.world,candidate);emit(candidate,Kind::FindNextThrowPiki);
  if(!a.hasController&&!a.movieActor){if(next.ai==AI::Control){next.ai=AI::Wait;next.idleTimer=2;}if(!executeAI(params,f,rng,next,candidate,error))return false;}
  else if(a.hasController&&!a.movieActor&&*f.postControlSceneAnimationTimer>9){if(next.ai==AI::Control&&!animation(f,rng,next,candidate,error))return false;if(!executeAI(params,f,rng,next,candidate,error))return false;}
  else if(a.hasController&&next.ai!=AI::Control&&*f.postControlSceneAnimationTimer<=9){next.ai=AI::Control;motion(candidate,Motion::Wait);}
  if(a.hasController&&!a.movieActor&&f.world->demoInactive&&!buttons(f,next,candidate,error))return false;
 }
 s=next;out=candidate;return true;
}
bool resumeActionButton(bool handled,std::optional<bool> throwable,Output& out,std::string& error){
 error.clear();if(out.continuation!=Stage::ActionButton)return fail(error,"wrong source action continuation");
 if(!handled&&!throwable)return fail(error,"missing source throwable query");
 out.continuation=Stage::None;if(!handled&&*throwable)transit(out,StateId::ThrowWait);return true;
}
bool resumeDismiss(const Frame& f,bool released,State& s,Output& out,std::string& error){
 error.clear();if(out.continuation!=Stage::Dismiss||!f.buttons||!f.world)return fail(error,"wrong or missing source dismiss continuation");
 State next=s;Output candidate=out;candidate.continuation=Stage::None;next.dismissTimer=released?1:20;
 if(!afterDismiss(f,next,candidate,error))return false;
 s=next;out=candidate;return true;
}
bool keyEventEnd(State& s,Output& out,std::string& error){error.clear();out={};if(s.ai==AI::Animation)wait(s,out);return true;}
bool jumpKey200(const Frame& f,Output& out,std::string& error){
 error.clear();if(!f.actor||!f.world||f.actor->slot>1)return fail(error,"missing actual jump voice source actor/world");
 out={};if(!f.world->frozen){Command c{Kind::JumpLandVoice};c.scalar=float(f.actor->slot==0?0:f.world->debtPaid?2:1);out.commands.push_back(c);}return true;
}
bool collision(const Frame& f,const Collision& c,State& s,Output& out,std::string& error){
 error.clear();if(!f.actor||!f.world||!c.identity)return fail(error,"missing source collision identity/world");
 State next=s;Output candidate;
 if(f.world->demoInactive&&c.honey&&!c.yellowHoney&&c.absorbable){Command absorb{Kind::TransitSelf};absorb.state=StateId::Absorb;absorb.target=c.identity;candidate.commands.push_back(absorb);}
 if(f.world->demoInactive&&f.world->versusMode&&c.teki&&!c.captured&&c.alive&&c.sourceEnemyBomb&&f.actor->hasController){
  if(!c.controllerStick||!std::isfinite(c.controllerStick->x)||!std::isfinite(c.controllerStick->z)||std::fabs(c.controllerStick->x)>1||std::fabs(c.controllerStick->z)>1)return fail(error,"missing actual source collision stick");
  float x=-c.controllerStick->x,z=c.controllerStick->z;
  if(x*x+z*z>.5f){if(next.collisionTimer<100)next.collisionTimer=std::uint8_t(next.collisionTimer+3);if(next.collisionTimer>60){Command carry{Kind::TransitSelf};carry.state=StateId::CarryBomb;carry.target=c.identity;candidate.commands.push_back(carry);}}
 }
 s=next;out=candidate;return true;
}
void wallHit(State& s){if(s.ai==AI::Escape&&s.escapeCCW&&s.escapeTimer&&!*s.escapeTimer){s.escapeCCW=!*s.escapeCCW;s.escapeTimer=10;}}
bool checkpointValid(const State& s,std::string& error){
 error.clear();if(!std::isfinite(s.idleTimer)||!finite(s.initialPosition)||(!s.dismissTimer||*s.dismissTimer>35)||int(s.ai)<0||int(s.ai)>4||(s.animation&&(int(*s.animation)<0||int(*s.animation)>8))||(s.escapeTimer&&*s.escapeTimer>10)||s.collisionTimer>102)return fail(error,"unresolved or invalid source Walk checkpoint");
 if(s.ai==AI::Animation&&!s.animation)return fail(error,"missing source animation initialization");
 if(s.ai==AI::Escape&&(!s.escapeTimer||!s.escapeCCW))return fail(error,"missing source escape initialization");
 if((s.ai==AI::Escape||s.ai==AI::Attack)&&!s.target)return fail(error,"missing checkpoint target identity");
 return true;
}
}}}
