#include "pc_p2_original_piki_brain.h"
#include "Piki.h"
#include <cmath>
namespace p2original { namespace piki {
namespace {
bool fail(std::string& e,const char* why){e=why;return false;}
float speed(const Piki& body,const Parameters& p,unsigned species){
 float base=body.mHappa==2?p.flowerRun:body.mHappa==1?p.budRun:p.run;
 // Source getSpeed(1), with source scaleValue(1) unchanged, no AP multiplier.
 if(species==4)base*=p.whiteMultiplier;else if(species==3)base*=p.purpleMultiplier;
 return base;
}
void face(Piki& p,const Vector3f& navi){
 float angle=std::atan2(navi.x-p.mSRT.t.x,navi.z-p.mSRT.t.z)-p.mFaceDirection;
 while(angle>3.14159265358979323846f)angle-=6.28318530717958647692f;
 while(angle<-3.14159265358979323846f)angle+=6.28318530717958647692f;
 p.mFaceDirection+=0.3f*angle;
}
}
bool brainCleanup(Handle h,RuntimeState& r,Services& s,std::string& e){
 if(r.brain.action==Action::None)return true;
 if(r.brain.action==Action::Free)return s.freeEffects(h,false,e);
 if(r.brain.slot!=-1&&!s.releaseSlot(h,r.brain.navi,r.brain.slot,e))return false;
 r.brain.slot=-1;r.brain.navi=nullptr;return true;
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
 if(!brainCleanup(h,r,s,e)||!s.motion(h,Motion::Wait,e)||!s.freeEffects(h,true,e))return false;
 r.brain=BrainState{};
 h.body->mNavi=nullptr;h.body->mTargetVelocity.set(0,0,0);
 return true;
}
bool brainFormation(Handle h,RuntimeState& r,Services& s,Navi* n,std::string& e){
 if(r.brain.pendingSlot!=-1)return fail(e,"source Formation still owns a pending cleanup slot");
 CaptainFrame frame;if(!n||!s.captainFrame(n,frame,e)||!frame.alive||!frame.formationable)return false;
 if(!s.supports(h,Motion::Run2,e))return false;
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
  if(b.freeState==2){
   int result=1;if(!s.execBore(h,result,e))return false;
   bool available=false;if(!s.invokeFree(h,FreeSearch::Probe,available,e))return false;
   if(available&&!s.finishBore(h,e))return false;
   if(result==0||result==2){b.freeState=0;b.delayTimer=90;}
   return true;
  }
  body->mTargetVelocity.set(0,0,0);
  bool started=false;if(!s.invokeFree(h,FreeSearch::Execute,started,e))return false;
  if(started)return fail(e,"source Free task-action transition not ported in this Brain");
  if(b.delayTimer){--b.delayTimer;return true;}
  float random=0;if(!s.random(random,e)||!std::isfinite(random)||random<0||random>1)return false;
  if(random>0.5f){if(!s.startBore(h,e))return false;b.freeState=2;}
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
