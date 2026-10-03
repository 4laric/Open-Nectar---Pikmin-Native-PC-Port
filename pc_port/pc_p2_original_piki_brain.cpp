#include "pc_p2_original_piki_brain.h"
#include "Piki.h"
#include <cmath>
namespace p2original { namespace piki {
namespace {
bool fail(std::string& e,const char* why){e=why;return false;}
float speed(const Piki& body,const Parameters& p,unsigned species,float multiplier=1){
 float base=body.mHappa==2?p.flowerRun:body.mHappa==1?p.budRun:p.run;
 // Source getSpeed(1), with source scaleValue(1) unchanged, no AP multiplier.
 base=multiplier*(base-p.walk)+p.walk;
 if(species==4)base*=p.whiteMultiplier;else if(species==3)base*=p.purpleMultiplier;
 return base;
}
void face(Piki& p,const Vector3f& navi){
 float angle=std::atan2(navi.x-p.mSRT.t.x,navi.z-p.mSRT.t.z)-p.mFaceDirection;
 while(angle>3.14159265358979323846f)angle-=6.28318530717958647692f;
 while(angle<-3.14159265358979323846f)angle+=6.28318530717958647692f;
 p.mFaceDirection+=0.3f*angle;
}
bool random(Services& s,float& value,std::string& e){
 return s.random(value,e)&&std::isfinite(value)&&value>=0&&value<1;
}
bool sourceSpecies(Handle h,unsigned& species,std::string& e){
 PcP2SourceBody source;if(pc_p2_source_body_query(h.body,source)!=PcP2SourceBodyKind::GenPiki)return fail(e,"source action needs actual GenPiki body");
 OriginalPikiBodyHandle current;if(!pc_p2_original_piki_body_handle(h.body,current)||current.nativeLifetime!=h.lifetime)return fail(e,"source action lifetime changed");
 species=source.state.species;return true;
}
bool sitDown(Handle h,BoreState& b,Services& s,std::string& e){
 if(b.restState==0){b.restState=1;return s.motion(h,Motion::Sit,e);}
 if(b.restState==1){b.restState=3;return s.motion(h,Motion::Sleep,e);}
 return true;
}
bool standUp(Handle h,BoreState& b,Services& s,std::string& e){
 if(b.restState>=1){b.idle=true;return s.finishMotion(h,e);}return true;
}
bool startBoreAction(Handle h,BoreState& b,Services& s,std::string& e){
 float v=0;
 if(b.oneshotTimer<2)b.behavior=1;
 b.forced=false;b.animFinished=false;b.idle=false;b.interruptible=false;
 if(b.behavior==0){
  b.restState=0;if(!sitDown(h,b,s,e)||!random(s,v,e))return false;
  b.restTimer=v*4+5;if(!s.animationSpeed(h,30,e))return false;
 }else{
  if(!random(s,v,e))return false;
  // Actual KandoLib cumulative weights; the final choice owns remainder.
  b.oneshot=v<.05f?Motion::Yawn:v<.4f?Motion::Chat:v<.8f?Motion::Search:Motion::Irritated;
  if(!s.motion(h,b.oneshot,e))return false;
 }
 if(!random(s,v,e))return false;
 b.forceTimer=v*6+6;return true;
}
bool startBore(Handle h,BoreState& b,Services& s,std::string& e){
 float v=0;if(!random(s,v,e))return false;b=BoreState{};b.behavior=static_cast<unsigned>(v*2);
 return startBoreAction(h,b,s,e);
}
bool finishBoreAction(Handle h,BoreState& b,Services& s,std::string& e){
 b.forced=true;if(!s.animationSpeed(h,60,e))return false;
 return b.behavior==0||s.finishMotion(h,e);
}
bool execRest(Handle h,BoreState& b,Services& s,float dt,int& result,std::string& e){
 result=1;if(b.interruptible){result=0;return true;}
 h.body->mTargetVelocity.set(0,0,0);
 Motion motion;float rate=0;bool completed=false;
 if(!s.animationStatus(h,motion,rate,completed,e))return false;
 if(rate==0&&!s.animationSpeed(h,30,e))return false;
 if(motion!=Motion::Sit&&motion!=Motion::Sleep){result=2;return true;}
 if(b.forced){
  if(!b.idle){if(b.restState>=1)return standUp(h,b,s,e);result=0;return true;}
  if(completed){b.idle=false;if(b.restState==3){b.restState=1;if(!s.motion(h,Motion::Sit,e)||!s.loopStart(h,e))return false;}}
  return true;
 }
 b.restTimer-=dt;
 if(!b.idle&&b.restTimer<0){
  float v=0;if(b.restState<=1){if(!random(s,v,e))return false;}
  if(b.restState<=1&&v>.5f){if(!sitDown(h,b,s,e))return false;}
  else if(b.restState>=1&&!standUp(h,b,s,e))return false;
  if(!random(s,v,e))return false;
  b.restTimer=v*2+3;
 }
 return true;
}
bool execBore(Handle h,BoreState& b,Services& s,float dt,int& result,std::string& e){
 if(b.oneshotTimer<2)b.oneshotTimer+=dt;
 int sub=1;
 if(b.behavior==0){if(!execRest(h,b,s,dt,sub,e))return false;}
 else{
  h.body->mTargetVelocity.set(0,0,0);
  if(b.forced||b.animFinished)sub=0;
  else{Motion motion;if(!s.currentMotion(h,motion,e))return false;if(motion!=b.oneshot)sub=2;}
 }
 result=1;
 if(sub==0||sub==2){
  if(b.finished){result=0;return true;}
  float v=0;if(!random(s,v,e))return false;b.behavior=static_cast<unsigned>(v*2);
  return startBoreAction(h,b,s,e);
 }
 b.forceTimer-=dt;if(b.forceTimer<=0)return finishBoreAction(h,b,s,e);
 return true;
}

}
bool brainCleanup(Handle h,RuntimeState& r,Services& s,std::string& e){
 bool complete=true;
 if(r.brain.freeEffectsOwned||r.brain.action==Action::Free){
  if(s.freeEffects(h,false,e))r.brain.freeEffectsOwned=false;else complete=false;
 }
 if(r.brain.action==Action::Formation&&r.brain.slot!=-1){
  std::string slotError;
  if(s.releaseSlot(h,r.brain.navi,r.brain.slot,slotError)){r.brain.slot=-1;r.brain.navi=nullptr;}
  else{if(complete)e=slotError;else e+="; "+slotError;complete=false;}
 }
 return complete;
}
bool brainCleanupAll(Handle h,RuntimeState& r,Services& s,std::string& e){
 bool clean=brainCleanup(h,r,s,e);
 if(r.brain.pendingSlot!=-1){
  std::string pendingError;
  if(s.releaseSlot(h,r.brain.pendingNavi,r.brain.pendingSlot,pendingError)){
   r.brain.pendingSlot=-1;r.brain.pendingNavi=nullptr;
  }else{if(clean)e=pendingError;clean=false;}
 }
 return clean;
}
bool brainFree(Handle h,RuntimeState& r,Services& s,std::string& e){
 if(r.brain.pendingSlot!=-1)return fail(e,"source Free transition cannot discard pending Formation slot");
 if(!brainCleanup(h,r,s,e)||!s.motion(h,Motion::Wait,e))return false;
 r.brain.freeEffectsOwned=true; // Own callback attempts before native writes.
 if(!s.freeEffects(h,true,e))return false;
 r.brain=BrainState{};r.brain.freeEffectsOwned=true;
 h.body->mNavi=nullptr;h.body->mTargetVelocity.set(0,0,0);
 return true;
}
bool brainGather(Handle h,RuntimeState& r,Services& s,const Vector3f& goal,float radius,std::string& e){
 if(!std::isfinite(radius)||radius<0||!std::isfinite(goal.x)||!std::isfinite(goal.y)||!std::isfinite(goal.z))return fail(e,"invalid actual source Gather geometry");
 if(!s.supports(h,Motion::Walk,e)||!brainFree(h,r,s,e))return false;
 r.brain.freeState=1;r.brain.gatherGoal=goal;r.brain.gatherRadius=radius*.6f;r.brain.gatherTimer=5;
 return s.motion(h,Motion::Walk,e);
}
bool brainAnimationKey(Handle h,RuntimeState& r,Services& s,unsigned key,std::string& e){
 auto& b=r.brain.bore;
 if(r.brain.action!=Action::Free||r.brain.freeState!=2)return true;
 if(b.behavior==1){
  if(key==1000)b.animFinished=true;
  if(key==200&&b.oneshot==Motion::Yawn)return s.boreVoice(h,false,e);
 }else{
  if(key==200)return s.boreVoice(h,true,e);
  if(key==1000&&b.idle){
   if(b.restState==1){b.idle=false;b.restState=0;b.interruptible=true;}
   else if(b.restState==3){b.idle=false;b.restState=1;if(!s.motion(h,Motion::Sit,e)||!s.loopStart(h,e))return false;}
  }
 }
 return true;
}
bool brainFormation(Handle h,RuntimeState& r,Services& s,Navi* n,std::string& e){
 if(r.brain.pendingSlot!=-1)return fail(e,"source Formation still owns a pending cleanup slot");
 CaptainFrame frame;if(!n||!s.captainFrame(n,frame,e)||!frame.alive||!frame.formationable)return false;
 if(!s.supports(h,Motion::Run2,e))return false;
 // Retail Brain::start cleans the old action before Formation::init. The
 // whistle path already does so before LookAt. Do not duplicate one body in
 // a single source plate through a direct same-captain reinit request.
 if(r.brain.action==Action::Formation&&r.brain.navi==n&&r.brain.slot>=0)
  return fail(e,"source Formation reinit requires old Brain cleanup first");
 int slot=-1;bool allocated=s.allocateSlot(h,n,slot,e);
 if(slot>=0){r.brain.pendingSlot=slot;r.brain.pendingNavi=n;}
 if(!allocated||slot<0)return fail(e,"source Formation has no committed actual CPlate slot");
 if(!brainCleanup(h,r,s,e)){
  std::string cleanupError;
  if(s.releaseSlot(h,n,slot,cleanupError)){r.brain.pendingSlot=-1;r.brain.pendingNavi=nullptr;}
  else e+="; newly allocated slot cleanup also refused: "+cleanupError;
  return false;
 }
 r.brain=BrainState{};r.brain.action=Action::Formation;r.brain.navi=n;r.brain.slot=slot;
 // Retail InteractFue(false,true) ActFormationInitArg uses touch cooldown.
 r.brain.touchCooldown=45;
 h.body->mNavi=n;
 return s.motion(h,Motion::Run2,e);
}
bool brainExec(Handle h,RuntimeState& r,Services& s,const Parameters& p,float dt,std::string& e){
 auto& b=r.brain;auto* body=h.body;
 if(b.action==Action::None)return fail(e,"source Walk has no current Brain action");
 if(b.action==Action::Free){
  if(b.freeState==1){
   Vector3f direction=b.gatherGoal-body->mSRT.t;float distance=direction.normalise();
   if(!std::isfinite(distance))return fail(e,"nonfinite actual source Gather distance");
   b.gatherTimer-=dt;
   if(distance<b.gatherRadius||b.gatherTimer<=0){
    body->mTargetVelocity.set(0,0,0);float v=0;if(!random(s,v,e))return false;
    b.freeState=0;b.delayTimer=150+static_cast<unsigned>(30*v);
   }else{unsigned species=0;if(!sourceSpecies(h,species,e))return false;body->mTargetVelocity=direction*speed(*body,p,species,.6f);}
   return true;
  }
  if(b.freeState==2){
   int result=1;if(!execBore(h,b.bore,s,dt,result,e))return false;
   bool available=false;if(!s.freeTaskAvailable(h,FreeSearch::Probe,available,e))return false;
   if(available){if(!finishBoreAction(h,b.bore,s,e))return false;b.bore.finished=true;}
   if(result==0||result==2){b.freeState=0;b.delayTimer=90;}
   return true;
  }
  body->mTargetVelocity.set(0,0,0);
  bool started=false;if(!s.freeTaskAvailable(h,FreeSearch::Execute,started,e))return false;
  if(started)return fail(e,"source Free task-action transition not ported in this Brain");
  if(b.delayTimer){--b.delayTimer;return true;}
  float random=0;if(!s.random(random,e)||!std::isfinite(random)||random<0||random>1)return false;
  if(random>0.5f){b.freeState=2;if(!startBore(h,b.bore,s,e))return false;}
  return true;
 }
 if(b.touchCooldown)--b.touchCooldown;
 if(b.slot==-1||!b.navi||body->mNavi!=b.navi)return brainFree(h,r,s,e);
 CaptainFrame n;if(!s.captainFrame(b.navi,n,e))return false;
 if(!n.alive||n.carryingPellet||(!n.controller&&n.follow))return brainFree(h,r,s,e);
 // The command/numbness/footmark/trip states are deliberately not mapped onto
 // P1 ActCrowd. Missing source subactions stop this consumer with a diagnosis.
 if(n.command)return fail(e,"source Formation command/numbness branch not ported");
 Vector3f slot;if(!s.slotPosition(h,b.navi,b.slot,slot,e))return false;
 Vector3f separation=slot-body->mSRT.t;float distance=separation.normalise();
 if(!std::isfinite(distance))return fail(e,"nonfinite source Formation slot");
 PcP2SourceBody source;auto kind=pc_p2_source_body_query(body,source);
 if(kind==PcP2SourceBodyKind::GenPiki){OriginalPikiBodyHandle current;if(!pc_p2_original_piki_body_handle(body,current))return false;source.nativeLifetime=current.nativeLifetime;}
 if(kind==PcP2SourceBodyKind::None||kind==PcP2SourceBodyKind::Unavailable||source.nativeLifetime!=h.lifetime)return fail(e,"source Formation lifetime changed");
 b.oldDistanceType=b.distanceType;b.distanceType=5;
 if(distance<=7)b.distanceCounter=0;
 else if(distance<15){
  if(b.distanceCounter<6)++b.distanceCounter;
  if(b.oldDistanceType==2&&n.sceneAnimationTimer>0.1f)b.distanceCounter=0;
 }else b.distanceCounter=0;
 float run=speed(*body,p,source.state.species);
 if(distance<=7||(b.distanceCounter<6&&distance<=15)){
  b.distanceType=2;body->mTargetVelocity.set(0,0,0);face(*body,n.position);
  if(b.sortState!=1){if(!s.formed(h,b.navi,e))return false;b.sortState=1;}
 }else{
  if(distance<15){
   b.distanceType=3;float factor=10/p.acceleration,sim=body->mVelocity.length();
   float stopping=0.5f*(sim/factor)*sim,full=0.5f*(run/factor)*run;
   if(distance<stopping){body->mTargetVelocity.set(0,0,0);face(*body,n.position);}
   else if(distance<full)body->mTargetVelocity=separation*(0.5f*std::sqrt(sim*sim+8*factor*distance)+sim);
   else body->mTargetVelocity=separation*run;
  }else{b.distanceType=4;body->mTargetVelocity=separation*run;}
  Vector3f naviPiki=body->mSRT.t-n.position,plate=n.position-n.plateOffset;plate.normalise();
  if(plate.dot(naviPiki)>0){
   Vector3f impulse(-naviPiki.z,0,naviPiki.x);if(!(b.slot&1))impulse=impulse*-1;
   impulse.normalise();if(!n.cstickNeutral)impulse.set(0,0,0);
   float length=body->mTargetVelocity.length();body->mTargetVelocity=body->mTargetVelocity+impulse*run;
   body->mTargetVelocity.normalise();body->mTargetVelocity=body->mTargetVelocity*length;
  }
 }
 if(distance<p.whiteDistance){b.lostTimer=0;b.releasedSlot=false;}
 else if(distance<p.grayDistance){
  b.lostTimer+=dt;
  if(!b.releasedSlot){
   if(!s.releaseSlot(h,b.navi,b.slot,e))return false;
   b.slot=-1;
   if(!s.allocateSlot(h,b.navi,b.slot,e))return false;
   b.releasedSlot=true;
  }
  if(b.slot==-1||b.lostTimer>p.lostTime)return brainFree(h,r,s,e);
 }else return brainFree(h,r,s,e);
 return true;
}
} }
