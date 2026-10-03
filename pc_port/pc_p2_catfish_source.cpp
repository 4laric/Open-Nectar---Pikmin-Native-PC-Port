#include "pc_p2_catfish_source.h"
#include "pc_p2_catfish_source_policy.h"
#include "pc_p2_original_catfish_bank.h"
#include "pc_p2_original_actor.h"
#include "pc_p2_original_drop_engine.h"
#include "teki.h"
#include "Piki.h"
#include "PikiState.h"
#include "pc_p2_catfish_mouth.h"
#include "PikiMgr.h"
#include "Navi.h"
#include "pc_p2_navi_select.h"
#include "Interactions.h"
#include <map>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <cstdlib>
namespace {
using namespace p2catfishsource;
using CatVec=p2catfishsource::Vec;
struct Actor {
 unsigned uid=0,ordinal=0,token=0;State state=Wait,previous=Wait,next=Wait;int flickNext=-1;
 CatVec home;float heading=0,alert=0,speed=30;Creature* target=nullptr;
 Motion motion;CorpseMotion corpseMotion;bool escaped=false;
};
std::map<const BTeki*,Actor> actors;
P2CatfishSourceFlick receiver=nullptr;std::string residentBank;bool ready=false;
[[noreturn]] void fail(const char* s){std::fprintf(stderr,"P2_ORIGINAL_CATFISH_SOURCE %s\n",s);std::abort();}
bool refuse(std::string& e,const char* s){e=s;return false;}
CatVec vec(const Vector3f& v){return {v.x,v.y,v.z};}
bool searchable(Piki* p){
 if(!p||!p->isAlive()||p->isStickToMouth())return false;
 switch(p->getState()){
 case PIKISTATE_Grow:case PIKISTATE_Bury:case PIKISTATE_NukareWait:return false;
 default:return true;}
}
std::vector<Creature*> scene(){
 std::vector<Creature*> out;for(Navi* n:pc_p2_navis())if(n&&n->isAlive())out.push_back(n);
 if(pikiMgr){Iterator it(pikiMgr);CI_LOOP(it){Piki* p=static_cast<Piki*>(*it);if(searchable(p))out.push_back(p);}}
 return out;
}
Creature* nearest(const std::vector<Creature*>& candidates,CatVec pos,float heading,float searchAngle,float radius){
 Creature* found=nullptr;float best=radius*radius;
 // Navi pass first; Pikmin win only a strict smaller distance, exactly as
 // EnemyFunc::getNearestPikminOrNavi passes the first pass's minimum onward.
 for(Creature* c:candidates){const CatVec p=vec(c->getPosition());const float d=flatSeparation(pos,p);
  if(d<best&&std::fabs(facing(pos,p,heading))<=rad(searchAngle)){best=d;found=c;}}
 return found;
}
int stuck(BTeki* a){int n=0;for(Creature* p=a->mStickListHead;p;p=p->mNextSticker)if(p->isPiki())++n;return n;}
bool flickWanted(BTeki* a){if(!startFlick(a->mDamageCount,stuck(a)))return false;a->mDamageCount=0;return true;}
void stop(BTeki* a){a->inputDrive(Vector3f(0,0,0));a->mVelocity.set(0,0,0);a->mTargetVelocity.set(0,0,0);}
float turn(BTeki* a,Actor& s,CatVec target){const float difference=facing(vec(a->getPosition()),target,s.heading);
 s.heading=angle(s.heading+turnStep(difference));a->mFaceDirection=s.heading;a->mSRT.r.y=s.heading;return difference;}
void walk(BTeki* a,Actor& s,CatVec target){turn(a,s,target);Vector3f drive(std::sin(s.heading)*60,0,std::cos(s.heading)*60);a->inputDrive(drive);a->mVelocity.set(drive);}
void flick(BTeki* a,bool stickOnly,float logicalAngle){if(!receiver||!receiver(a,stickOnly,logicalAngle,25,80,1))fail("source flick receiver failed");}
void enter(BTeki* a,Actor& s,State next,int flickNext=-1){
 if(s.state==Flick)a->setTekiOption(TEKIOPT_DamageCountable);
 s.previous=s.state;s.state=next;s.next=next;s.flickNext=flickNext;
 int anim=WaitAnim;s.speed=30;
 switch(next){
 case Wait:s.target=nullptr;stop(a);break;
 case Turn:anim=TurnAnim;stop(a);break;
 case Walk:anim=MoveAnim;s.speed=40*(60.0f/50);break;
 case Attack:anim=AttackAnim;stop(a);break;
 case Flick:anim=FlickAnim;stop(a);a->clearTekiOption(TEKIOPT_DamageCountable);break;
 case TurnToHome:stop(a);if(separation(vec(a->getPosition()),s.home)<80*80){enter(a,s,Wait);return;}anim=TurnAnim;break;
 case GoHome:anim=MoveAnim;s.speed=40;break;
 case Dead:anim=DeadAnim;stop(a);pc_p2_catfish_mouth_release(a);
  if(!pc_p2_original_spawn_items(a))fail("death lost original drop identity");break;
 }
 s.motion.start(anim);
 std::printf("P2_ORIGINAL_CATFISH_STATE token=%u uid=%u ordinal=%u state=%d anim=%d\n",s.token,s.uid,s.ordinal,int(next),anim);
}
bool ended(const std::vector<Key>& events){for(const auto& e:events)if(e.type==1000)return true;return false;}
void finish(Actor& s,State next,float speed=-1){s.next=next;s.motion.finish();if(speed>=0)s.speed=speed;}
void attackNavi(BTeki* a,Actor& s){const CatVec origin=vec(a->getPosition());for(Navi* n:pc_p2_navis())if(n&&n->isAlive()){
 const CatVec p=vec(n->getPosition());if(separation(origin,p)<50*50&&std::fabs(facing(origin,p,s.heading))<rad(25))n->stimulate(InteractAttack(a,nullptr,10,false));}}
} // namespace
bool pc_p2_catfish_source_set_flick_receiver(P2CatfishSourceFlick hook,std::string& error){
 if(!actors.empty())return refuse(error,"cannot replace Catfish source flick receiver with live actors");
 receiver=hook;if(!hook)ready=false;error.clear();return true;
}
bool pc_p2_catfish_source_resources(std::string& error){
 if(!actors.empty())return refuse(error,"cannot replace original Catfish source resources with live actors");
 if(!receiver)return refuse(error,"original Catfish source flick receiver is not installed");
 std::ifstream file("p2-aquatic-bank.txt");std::ostringstream data;data<<file.rdbuf();std::string staged=data.str();
 std::istringstream checked(staged);if(!p2original::catfish::validateCatfishBank(checked,error))return false;
 // Other aquatic species share this file. Only this family's authored rows
 // are immutable inputs to this FSM.
 std::istringstream lines(staged);std::string line,identity;
 while(std::getline(lines,line)){std::istringstream row(line);std::string kind,species;row>>kind>>species;
  if(species=="Catfish")identity+=line+"\n";}
 if(!residentBank.empty()&&residentBank!=identity)return refuse(error,"resident original Catfish authored bank changed");
 if(!pc_p2_catfish_mouth_resources(error))return false;
 residentBank=std::move(identity);ready=true;error.clear();return true;
}
bool pc_p2_catfish_source_birth(BTeki* a,unsigned uid,unsigned ordinal,std::string& error){
 if(!a||a->mTekiType!=TEKI_Namazu||!uid||!ready||!receiver||actors.count(a))return refuse(error,"invalid or reused original Catfish source actor");
 if(!pc_p2_catfish_mouth_birth(a,error))return false;
 Actor s;s.uid=uid;s.ordinal=ordinal;s.home=vec(a->getPosition());s.heading=a->getDirection();s.motion.start(WaitAnim);
 // Birth passes WaitArg 'rand'; later Wait transitions pass nullptr and
 // consume no draw. The first indexed key is0, so rand*firstKeyFrame is0.
 (void)gsys->getRand(1.0f);
 a->mHealth=a->mMaxHealth=200;a->mDamageCount=0;a->mStoredDamage=0;a->setTekiOption(TEKIOPT_DamageCountable);
 a->clearTekiOption(TEKIOPT_Invincible);actors.emplace(a,s);stop(a);error.clear();return true;
}
bool pc_p2_catfish_source_registry(BTeki* a,unsigned token,std::string& error){
 auto at=actors.find(a);unsigned source=0,actual=0;p2original::InstanceIdentity id;
 if(at==actors.end()||!token||(at->second.token&&at->second.token!=token)||!p2original::originalActors().query(a,source,actual,&id)||source!=26||actual!=token||id.generator!=at->second.uid||id.ordinal!=at->second.ordinal)return refuse(error,"original Catfish source identity mismatch");
 at->second.token=token;error.clear();return true;
}
bool pc_p2_catfish_source_clip(const BTeki* a,const char*& clip,float& phase){auto at=actors.find(a);if(at==actors.end())return false;
 const auto& s=at->second;clip=s.motion.clip();phase=s.motion.frame()/float(registration(s.motion.id()).duration-1);return true;
}
bool pc_p2_catfish_source_corpse_clip(const BTeki* a,const char*& clip,float& phase){
 auto at=actors.find(a);if(at==actors.end()||!at->second.escaped||!a->mPellet||a->mDeadState!=2)return false;
 const auto& motion=at->second.corpseMotion.motion();clip=motion.clip();phase=motion.frame()/float(registration(CarryAnim).duration-1);return true;
}
bool pc_p2_catfish_source_carry_start(BTeki* a,bool restart){
 // Pellet::init invokes its view before becomePellet assigns mPellet and
 // dieSoon sets dead-state2. The escaped marker owns that pending lifecycle.
 auto at=actors.find(a);if(at==actors.end()||!at->second.escaped)return false;
 at->second.corpseMotion.start(restart);return true;
}
bool pc_p2_catfish_source_carry_stop(BTeki* a){
 auto at=actors.find(a);if(at==actors.end()||!at->second.escaped)return false;
 at->second.corpseMotion.stop();return true;
}
bool pc_p2_catfish_source_carry_finish(BTeki* a){
 auto at=actors.find(a);if(at==actors.end()||!at->second.escaped)return false;
 at->second.corpseMotion.finish();return true;
}
void pc_p2_catfish_source_forget(BTeki* a){if(actors.erase(a))pc_p2_catfish_mouth_forget(a);}
bool pc_p2_catfish_source_press(BTeki* a,Creature*,float damage){
 auto at=actors.find(a);if(at!=actors.end()&&std::isfinite(damage)&&damage>=0&&!a->getTekiOption(TEKIOPT_Invincible)&&at->second.state!=Dead){
  a->mStoredDamage+=damage;if(a->getTekiOption(TEKIOPT_DamageCountable))a->mDamageCount+=1;}
 // The retail callback returns false after addDamage (no Press state).
 // This host hook reports ownership: suppress the borrowed P1 Pressed
 // event even when the source refuses damage (invulnerable/dead).
 return at!=actors.end();
}
float pc_p2_catfish_source_param(const BTeki* a,int index,float fallback){if(!actors.count(a))return fallback;
 switch(index){case TPF_Life:return 200;case TPF_WalkVelocity:case TPF_RunVelocity:return 60;
 case TPF_Scale:return 1;case TPF_LifeRecoverRate:return 0;case TPF_CollisionRadius:return 20;default:return fallback;}}
void pc_p2_catfish_source_update(BTeki* a){
 auto at=actors.find(a);if(at==actors.end())return;Actor& s=at->second;
 const float dt=gsys->getFrameTime();if(!std::isfinite(dt)||dt<=0||dt>.5f)return;
 if(!s.token||pc_p2_original_actor_token(a)!=s.token)fail("update lost original registry token");
 // The host family update precedes BTeki's dead-state gate, so retained
 // actor-backed pellets still tick. Never replay death events or drops.
 if(s.escaped){if(a->mPellet&&a->mDeadState==2)s.corpseMotion.advance(dt*30);return;}
 if(a->mStoredDamage>0)a->makeDamaged();
 auto events=s.motion.advance(dt*s.speed);
 if(!pc_p2_catfish_mouth_follow(a,s.motion.clip(),s.motion.frame()))fail("actual mouth follow failed");
 const CatVec pos=vec(a->getPosition());auto candidates=scene();
 if(s.target&&std::find(candidates.begin(),candidates.end(),s.target)==candidates.end())s.target=nullptr;
 auto search=[&](float degrees=180.0f){return nearest(candidates,pos,s.heading,degrees,200);};
 auto attackableTarget=[&](Creature* t){return t&&attackable(pos,vec(t->getPosition()),facing(pos,vec(t->getPosition()),s.heading));};
 const bool end=ended(events);
 switch(s.state){
 case Wait:
  if(flickWanted(a))enter(a,s,Flick,Turn);
  else{if(!s.target){s.target=search();if(s.target)s.motion.finish();}
   if(end){if(s.target){const float difference=turn(a,s,vec(s.target->getPosition()));enter(a,s,std::fabs(difference)<=rad(90)?Walk:Turn);}else fail("Wait END without source target");}}
  break;
 case Turn:case Walk:{
  const State state=s.state;
  if(flickWanted(a)){enter(a,s,Flick);break;}
  if(a->mHealth<30||nearest(candidates,pos,s.heading,180,70))s.alert=0;
  const float searchAngle=s.alert<15?180.0f:Params{}.searchAngle;
  const float viewAngle=state==Turn&&s.alert<15?180.0f:Params{}.view;
  if(s.alert<15)s.alert+=dt;
  Creature* target=search(searchAngle);s.target=target;
  if(target){const CatVec prey=vec(target->getPosition());const float difference=facing(pos,prey,s.heading);
   if(attackableTarget(target)){finish(s,Attack,60);if(state==Walk)stop(a);}
   else if(outOfRange(pos,prey,difference,viewAngle)){finish(s,TurnToHome);if(state==Walk)stop(a);}
   else if(state==Turn){const float before=turn(a,s,prey);if(std::fabs(before)<=rad(90))finish(s,Walk,60);}
   else if(std::fabs(difference)<=rad(Params{}.view))walk(a,s,prey);
   else{finish(s,Turn);stop(a);}
  }else{finish(s,state==Walk&&a->mDamageCount!=0?Flick:TurnToHome);}
  if(state==Walk&&separation(pos,s.home)>280*280){finish(s,TurnToHome);stop(a);}
  if(end)enter(a,s,s.next);
  break;}
 case TurnToHome:
  if(flickWanted(a)){enter(a,s,Flick);break;}
  if(std::fabs(turn(a,s,s.home))<=rad(25))s.motion.finish();
  if(end)enter(a,s,GoHome);
  if(Creature* target=search()){s.target=target;if(attackableTarget(target))enter(a,s,Attack);}
  break;
 case GoHome:
  if(flickWanted(a)){enter(a,s,Flick);break;}
  walk(a,s,s.home);
  if(separation(pos,s.home)<80*80){finish(s,Wait);stop(a);}
  if(Creature* target=search()){s.target=target;if(attackableTarget(target))finish(s,Attack,60);else finish(s,Walk);}
  if(end)enter(a,s,s.next);
  break;
 case Attack:
  for(const auto& e:events){
   if(e.type==2){attackNavi(a,s);const int captured=pc_p2_catfish_mouth_eat(a);
    if(!captured)s.motion.start(EatAnim);
    flick(a,true,s.heading);break;}
   if(e.type==3)pc_p2_catfish_mouth_swallow(a);
  }
  if(end){Creature* target=search();s.target=target;enter(a,s,attackEnd(target!=nullptr,attackableTarget(target)));}
  break;
 case Flick:
  for(const auto& e:events)if(e.type==2){flick(a,false,FLICK_BACKWARDS_ANGLE);a->mDamageCount=0;}
  // Catfish::setEnemyNonStone enables EB_NoInterrupt, and KEY3 reset clears
  // it and updates bitter bounce state. That source reaction remains open
  // until the distinct original bitter receiver/state is implemented.
  if(end)enter(a,s,flickReturn(s.previous,s.flickNext));
  break;
 case Dead:
  stop(a);if(end&&!s.escaped){s.escaped=true;s.corpseMotion.prepare();a->pcEscapeNow();return;}break;
 }
 if(a->mHealth<=0&&s.state!=Dead)enter(a,s,Dead);
 // KEY2 and enter() can replace the animation, and walking can rotate the
 // actor. Keep the authored collision/mouth pose aligned with the clip that
 // the visual bridge will draw this same frame; the earlier follow remains
 // necessary for capture at the source attack event.
 if(!pc_p2_catfish_mouth_follow(a,s.motion.clip(),s.motion.frame()))fail("final actual mouth follow failed");
}
