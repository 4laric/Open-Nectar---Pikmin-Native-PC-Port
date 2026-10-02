#include "pc_midday_creature.h"
#if defined(PIKI_PC_PORT)
#include "pc_midday_actor_states.h"
#include "pc_midday_actor_archive.h"
#include "pc_midday_piki_storage.h"
#include "PikiAI.h"
#include <string>
#include <type_traits>
#include <set>
#include <vector>
using namespace pc_midday;

namespace {
template<class T, size_t N> bool arrayFields(ActorArchive& ar,const char* key,T (&values)[N]) {
 PrefixArchive a(ar,key);
 for(size_t i=0;i<N;++i) if(!a.field(std::to_string(i).c_str(),values[i])) return false;
 return true;
}
bool odometer(ActorArchive& ar,const char* key,OdoMeter& o) {
 PrefixArchive a(ar,key);
 return a.field("distance",o.mTotalDistance)&&a.field("remaining",o.mRemainingTime)&&a.field("minimum",o.mMinAllowedDistance)&&a.field("reset",o.mResetTimeValue);
}
}
struct PcMiddayPikiActionAccess {
 static bool visit(Action&,ActorArchive&,unsigned);
 static thread_local std::set<Action*> visited;
 static int typeOf(Action& action){
 int type=0;
 if(dynamic_cast<TopAction*>(&action))type=1;
 else if(dynamic_cast<ActAdjust*>(&action))type=2;
 else if(dynamic_cast<ActAttack*>(&action))type=3;
 else if(dynamic_cast<ActBoMake*>(&action))type=4;
 else if(dynamic_cast<ActBoreListen*>(&action))type=5;
 else if(dynamic_cast<ActBoreOneshot*>(&action))type=6;
 else if(dynamic_cast<ActBoreRest*>(&action))type=7;
 else if(dynamic_cast<ActBoreSelect*>(&action))type=8;
 else if(dynamic_cast<ActBoreTalk*>(&action))type=9;
 else if(dynamic_cast<ActBou*>(&action))type=10;
 else if(dynamic_cast<ActBreakWall*>(&action))type=11;
 else if(dynamic_cast<ActBridge*>(&action))type=12;
 else if(dynamic_cast<ActChase*>(&action))type=13;
 else if(dynamic_cast<ActCrowd*>(&action))type=14;
 else if(dynamic_cast<ActDecoy*>(&action))type=15;
 else if(dynamic_cast<ActDeliver*>(&action))type=16;
 else if(dynamic_cast<ActEnter*>(&action))type=17;
 else if(dynamic_cast<ActEscape*>(&action))type=18;
 else if(dynamic_cast<ActExit*>(&action))type=19;
 else if(dynamic_cast<ActFlower*>(&action))type=20;
 else if(dynamic_cast<ActFormation*>(&action))type=21;
 else if(dynamic_cast<ActFreeSelect*>(&action))type=22;
 else if(dynamic_cast<ActFree*>(&action))type=23;
 else if(dynamic_cast<ActGoto*>(&action))type=24;
 else if(dynamic_cast<ActGuard*>(&action))type=25;
 else if(dynamic_cast<ActJumpAttack*>(&action))type=26;
 else if(dynamic_cast<ActKinoko*>(&action))type=27;
 else if(dynamic_cast<ActMine*>(&action))type=28;
 else if(dynamic_cast<ActPick*>(&action))type=29;
 else if(dynamic_cast<ActPickCreature*>(&action))type=30;
 else if(dynamic_cast<ActPickItem*>(&action))type=31;
 else if(dynamic_cast<ActPullout*>(&action))type=32;
 else if(dynamic_cast<ActPulloutCreature*>(&action))type=33;
 else if(dynamic_cast<ActPush*>(&action))type=34;
 else if(dynamic_cast<ActPut*>(&action))type=35;
 else if(dynamic_cast<ActPutBomb*>(&action))type=36;
 else if(dynamic_cast<ActPutItem*>(&action))type=37;
 else if(dynamic_cast<ActRandomBoid*>(&action))type=38;
 else if(dynamic_cast<ActRescue*>(&action))type=39;
 else if(dynamic_cast<ActRope*>(&action))type=40;
 else if(dynamic_cast<ActShoot*>(&action))type=41;
 else if(dynamic_cast<ActShootCreature*>(&action))type=42;
 else if(dynamic_cast<ActStone*>(&action))type=43;
 else if(dynamic_cast<ActTransport*>(&action))type=44;
 else if(dynamic_cast<ActWatch*>(&action))type=45;
 else if(dynamic_cast<ActWeed*>(&action))type=46;
 return type;
 }
 static std::vector<int> allocatedChildren(int type){
 switch(type){
 case 1:return {38,45,18,13,24,30,37,21,3,41,25,32,31,15,14,23,40,17,19,11,28,44,27,12,34,36,39,46,43,4,10};
 case 3:return {26};case 8:case 22:return {45,9,6,7};case 16:return {30,24,35};
 case 30:case 31:return {24,29};case 32:return {24,2,33};case 41:return {24,42};default:return {};
 }
}
 static bool inactiveStrong(Action& action,ActorArchive& outer,const std::string& path,int expected,Piki* owner,std::set<Action*>& all,unsigned depth){
  if(depth>16||action.mPiki!=owner||!all.insert(&action).second||typeOf(action)!=expected)return outer.fail("inactive strong action allocation mismatch");
  auto graph=allocatedChildren(expected);if(action.mChildCount!=int(graph.size())||(!graph.empty()&&!action.mChildActions))return outer.fail("inactive strong child topology mismatch");
  if(!visited.count(&action)){PrefixArchive ar(outer,path.c_str());switch(expected){
 case 3:{auto& s=static_cast<ActAttack&>(action);if(!ar.strongRef("mOther",s.mOther,&s,"ActAttack","mOther"))return false;break;}
 case 13:{auto& s=static_cast<ActChase&>(action);if(!ar.strongRef("mTarget",s.mTarget,&s,"ActChase","mTarget"))return false;break;}
 case 16:{auto& s=static_cast<ActDeliver&>(action);if(!ar.strongRef("mObject",s.mObject,&s,"ActDeliver","mObject"))return false;break;}
 case 18:{auto& s=static_cast<ActEscape&>(action);if(!ar.strongRef("mTarget",s.mTarget,&s,"ActEscape","mTarget"))return false;break;}
 case 24:{auto& s=static_cast<ActGoto&>(action);if(!ar.strongRef("mTarget",s.mTarget,&s,"ActGoto","mTarget"))return false;break;}
 case 25:{auto& s=static_cast<ActGuard&>(action);if(!ar.strongRef("mTarget",s.mTarget,&s,"ActGuard","mTarget"))return false;if(!ar.strongRef("mLeftGuard",s.mLeftGuard,&s,"ActGuard","mLeftGuard"))return false;if(!ar.strongRef("mRightGuard",s.mRightGuard,&s,"ActGuard","mRightGuard"))return false;break;}
 case 26:{auto& s=static_cast<ActJumpAttack&>(action);if(!ar.strongRef("mTarget",s.mTarget,&s,"ActJumpAttack","mTarget"))return false;break;}
 case 27:{auto& s=static_cast<ActKinoko&>(action);if(!ar.strongRef("mTarget",s.mTarget,&s,"ActKinoko","mTarget"))return false;break;}
 case 29:{auto& s=static_cast<ActPick&>(action);if(!ar.strongRef("mObject",s.mObject,&s,"ActPick","mObject"))return false;break;}
 case 30:{auto& s=static_cast<ActPickCreature&>(action);if(!ar.strongRef("_18",s._18,&s,"ActPickCreature","_18"))return false;break;}
 case 31:{auto& s=static_cast<ActPickItem&>(action);if(!ar.strongRef("mTargetItem",s.mTargetItem,&s,"ActPickItem","mTargetItem"))return false;break;}
 case 32:{auto& s=static_cast<ActPullout&>(action);if(!ar.strongRef("mTarget",s.mTarget,&s,"ActPullout","mTarget"))return false;break;}
 case 33:{auto& s=static_cast<ActPulloutCreature&>(action);if(!ar.strongRef("mTarget",s.mTarget,&s,"ActPulloutCreature","mTarget"))return false;break;}
 case 37:{auto& s=static_cast<ActPutItem&>(action);if(!ar.strongRef("mItem",s.mItem,&s,"ActPutItem","mItem"))return false;break;}
 case 41:{auto& s=static_cast<ActShoot&>(action);if(!ar.strongRef("mTarget",s.mTarget,&s,"ActShoot","mTarget"))return false;break;}
 case 42:{auto& s=static_cast<ActShootCreature&>(action);if(!ar.strongRef("mTarget",s.mTarget,&s,"ActShootCreature","mTarget"))return false;break;}
 case 44:{auto& s=static_cast<ActTransport&>(action);if(!ar.strongRef("mPellet",s.mPellet,&s,"ActTransport","mPellet"))return false;break;}
 case 45:{auto& s=static_cast<ActWatch&>(action);if(!ar.strongRef("mTarget",s.mTarget,&s,"ActWatch","mTarget"))return false;break;}
 default:break;}}
  for(size_t i=0;i<graph.size();++i){auto* child=action.mChildActions[i].mAction;if(!child||!inactiveStrong(*child,outer,path+"."+std::to_string(i),graph[i],owner,all,depth+1))return false;}
  Action* selector=nullptr;if(expected==14)selector=static_cast<ActCrowd&>(action).mSelectAction;if(expected==23)selector=static_cast<ActFree&>(action).mSelectAction;
  if(expected==14||expected==23){if(!selector||!inactiveStrong(*selector,outer,path+".31",expected==14?8:22,owner,all,depth+1))return outer.fail("inactive strong selector allocation missing");}
  return true;
 }
 static bool inactiveStrong(Piki& piki,ActorArchive& ar){std::set<Action*> all;return inactiveStrong(*piki.mActiveAction,ar,"piki.inactiveStrong",1,&piki,all,0);}
 static bool storage(Piki& piki,const ActorFields& fields,StrongStorageVisitor& visitor,std::string& error){
 std::vector<PikiStrongPath> paths;if(!piki_strong_paths(fields,paths,error)||!piki.mActiveAction)return false;
 // Check the whole fresh allocation topology without consulting current child or listeners.
 struct Ignore:StrongStorageVisitor{bool visit(const char*,const StrongStorageSlot&,std::string&)override{return true;}} ignore;
 StrongStorageArchive check(ignore,error);std::set<Action*> all;auto savedVisited=visited;visited.clear();bool topology=inactiveStrong(*piki.mActiveAction,check,"allocation",1,&piki,all,0);visited=std::move(savedVisited);if(!topology)return false;
 StrongStorageArchive ar(visitor,error);
 for(auto& entry:paths){std::vector<u8> path(entry.path.begin(),entry.path.end());Action* action=follow(*piki.mActiveAction,path);if(!action||typeOf(*action)!=entry.type)return ar.fail("strong storage canonical action path");bool emitted=false;switch(entry.type){
 case 3:{auto& s=static_cast<ActAttack&>(*action);if(entry.member=="mOther"){if(!ar.strongRef(entry.key.c_str(),s.mOther,&s,"ActAttack","mOther"))return false;emitted=true;}break;}
 case 13:{auto& s=static_cast<ActChase&>(*action);if(entry.member=="mTarget"){if(!ar.strongRef(entry.key.c_str(),s.mTarget,&s,"ActChase","mTarget"))return false;emitted=true;}break;}
 case 16:{auto& s=static_cast<ActDeliver&>(*action);if(entry.member=="mObject"){if(!ar.strongRef(entry.key.c_str(),s.mObject,&s,"ActDeliver","mObject"))return false;emitted=true;}break;}
 case 18:{auto& s=static_cast<ActEscape&>(*action);if(entry.member=="mTarget"){if(!ar.strongRef(entry.key.c_str(),s.mTarget,&s,"ActEscape","mTarget"))return false;emitted=true;}break;}
 case 24:{auto& s=static_cast<ActGoto&>(*action);if(entry.member=="mTarget"){if(!ar.strongRef(entry.key.c_str(),s.mTarget,&s,"ActGoto","mTarget"))return false;emitted=true;}break;}
 case 25:{auto& s=static_cast<ActGuard&>(*action);if(entry.member=="mTarget"){if(!ar.strongRef(entry.key.c_str(),s.mTarget,&s,"ActGuard","mTarget"))return false;emitted=true;}if(entry.member=="mLeftGuard"){if(!ar.strongRef(entry.key.c_str(),s.mLeftGuard,&s,"ActGuard","mLeftGuard"))return false;emitted=true;}if(entry.member=="mRightGuard"){if(!ar.strongRef(entry.key.c_str(),s.mRightGuard,&s,"ActGuard","mRightGuard"))return false;emitted=true;}break;}
 case 26:{auto& s=static_cast<ActJumpAttack&>(*action);if(entry.member=="mTarget"){if(!ar.strongRef(entry.key.c_str(),s.mTarget,&s,"ActJumpAttack","mTarget"))return false;emitted=true;}break;}
 case 27:{auto& s=static_cast<ActKinoko&>(*action);if(entry.member=="mTarget"){if(!ar.strongRef(entry.key.c_str(),s.mTarget,&s,"ActKinoko","mTarget"))return false;emitted=true;}break;}
 case 29:{auto& s=static_cast<ActPick&>(*action);if(entry.member=="mObject"){if(!ar.strongRef(entry.key.c_str(),s.mObject,&s,"ActPick","mObject"))return false;emitted=true;}break;}
 case 30:{auto& s=static_cast<ActPickCreature&>(*action);if(entry.member=="_18"){if(!ar.strongRef(entry.key.c_str(),s._18,&s,"ActPickCreature","_18"))return false;emitted=true;}break;}
 case 31:{auto& s=static_cast<ActPickItem&>(*action);if(entry.member=="mTargetItem"){if(!ar.strongRef(entry.key.c_str(),s.mTargetItem,&s,"ActPickItem","mTargetItem"))return false;emitted=true;}break;}
 case 32:{auto& s=static_cast<ActPullout&>(*action);if(entry.member=="mTarget"){if(!ar.strongRef(entry.key.c_str(),s.mTarget,&s,"ActPullout","mTarget"))return false;emitted=true;}break;}
 case 33:{auto& s=static_cast<ActPulloutCreature&>(*action);if(entry.member=="mTarget"){if(!ar.strongRef(entry.key.c_str(),s.mTarget,&s,"ActPulloutCreature","mTarget"))return false;emitted=true;}break;}
 case 37:{auto& s=static_cast<ActPutItem&>(*action);if(entry.member=="mItem"){if(!ar.strongRef(entry.key.c_str(),s.mItem,&s,"ActPutItem","mItem"))return false;emitted=true;}break;}
 case 41:{auto& s=static_cast<ActShoot&>(*action);if(entry.member=="mTarget"){if(!ar.strongRef(entry.key.c_str(),s.mTarget,&s,"ActShoot","mTarget"))return false;emitted=true;}break;}
 case 42:{auto& s=static_cast<ActShootCreature&>(*action);if(entry.member=="mTarget"){if(!ar.strongRef(entry.key.c_str(),s.mTarget,&s,"ActShootCreature","mTarget"))return false;emitted=true;}break;}
 case 44:{auto& s=static_cast<ActTransport&>(*action);if(entry.member=="mPellet"){if(!ar.strongRef(entry.key.c_str(),s.mPellet,&s,"ActTransport","mPellet"))return false;emitted=true;}break;}
 case 45:{auto& s=static_cast<ActWatch&>(*action);if(entry.member=="mTarget"){if(!ar.strongRef(entry.key.c_str(),s.mTarget,&s,"ActWatch","mTarget"))return false;emitted=true;}break;}
 default:break;}if(!emitted)return ar.fail("strong storage member not compiled for action");}
 if(!ar.strongRef("piki.runtime.mLookAtCreature",piki.mLookAtCreature,&piki,"Piki","mLookAtCreature")||!ar.strongRef("piki.runtime._500",piki._500,&piki,"Piki","_500"))return false;
 return visit_creature_strong_storage(piki,fields,visitor,error);
 }
 static bool extras(Piki&,ActorArchive&);
 static bool payload(TopAction& s,ActorArchive& ar,unsigned depth) {
  if(!ar.ref("mListener",RefKind::AnimListener,s.mListener))return false;
  if(!ar.field("mIsDebugDraw",s.mIsDebugDraw))return false;
  if(!ar.field("mIsSuspended",s.mIsSuspended))return false;
  if(!ar.field("mIsAnimating",s.mIsAnimating))return false;
  if(!ar.field("_1C",s._1C))return false;
  if(!ar.ref("mTarget",RefKind::Creature,s.mTarget))return false;
  if(!ar.field("_24",s._24))return false;
  if(!ar.field("_28",s._28))return false;
  if(!ar.field("_2C",s._2C))return false;
  if(!boredom(s.mBoredom,ar))return false;
  return true;
 }
 static bool payload(ActAdjust& s,ActorArchive& ar,unsigned depth) {
  if(!ar.field("mAdjustDistance",s.mAdjustDistance))return false;
  if(!ar.field("mAdjustTimeLimit",s.mAdjustTimeLimit))return false;
  if(!ar.field("mTargetPosition",s.mTargetPosition))return false;
  if(!ar.field("mAdjustTimer",s.mAdjustTimer))return false;
  if(!ar.field("mTurnSpeed",s.mTurnSpeed))return false;
  if(!ar.field("mVelocity",s.mVelocity))return false;
  if(!ar.field("mForceFail",s.mForceFail))return false;
  return true;
 }
 static bool payload(ActAttack& s,ActorArchive& ar,unsigned depth) {
  if(!ar.field("mHasLost",s.mHasLost))return false;
  if(!ar.field("mIsAttackFinished",s.mIsAttackFinished))return false;
  if(!ar.field("mIsCriticalHit",s.mIsCriticalHit))return false;
  if(!ar.field("mTargetIsPlayer",s.mTargetIsPlayer))return false;
  if(!ar.ref("mTargetObjectPool",RefKind::Traversable,s.mTargetObjectPool))return false;
  if(!ar.strongRef("mOther",s.mOther,&s,"ActAttack","mOther"))return false;
  if(!ar.ref("mPlayerObject",RefKind::Creature,s.mPlayerObject))return false;
  return true;
 }
 static bool payload(ActBoMake& s,ActorArchive& ar,unsigned depth) {
  if(!ar.field("mState",s.mState))return false;
  if(!ar.ref("mBuildObject",RefKind::Creature,s.mBuildObject))return false;
  return true;
 }
 static bool payload(ActBoreListen& s,ActorArchive& ar,unsigned depth) {
  return true;
 }
 static bool payload(ActBoreOneshot& s,ActorArchive& ar,unsigned depth) {
  if(!ar.field("mIsAnimFinished",s.mIsAnimFinished))return false;
  return true;
 }
 static bool payload(ActBoreRest& s,ActorArchive& ar,unsigned depth) {
  if(!ar.field("mIsFinished",s.mIsFinished))return false;
  if(!ar.field("mRestState",s.mRestState))return false;
  if(!ar.field("mRestTimer",s.mRestTimer))return false;
  if(!ar.field("mIsAnimFinished",s.mIsAnimFinished))return false;
  if(!ar.field("mForceComplete",s.mForceComplete))return false;
  return true;
 }
 static bool payload(ActBoreSelect& s,ActorArchive& ar,unsigned depth) {
  if(!ar.field("mActionTimer",s.mActionTimer))return false;
  if(!ar.field("mIsTimerActive",s.mIsTimerActive))return false;
  if(!ar.field("mIsChildActionActive",s.mIsChildActionActive))return false;
  if(!ar.field("mStop",s.mStop))return false;
  if(!ar.field("_1C",s._1C))return false;
  return true;
 }
 static bool payload(ActBoreTalk& s,ActorArchive& ar,unsigned depth) {
  if(!ar.field("mIsLookHandledElsewhere",s.mIsLookHandledElsewhere))return false;
  if(!ar.ref("mTarget",RefKind::Creature,s.mTarget))return false;
  if(!ar.field("mTalkTimer",s.mTalkTimer))return false;
  if(!ar.field("mIsAnimFinished",s.mIsAnimFinished))return false;
  return true;
 }
 static bool payload(ActBou& s,ActorArchive& ar,unsigned depth) {
  if(!ar.field("mState",s.mState))return false;
  if(!ar.field("mTimeoutCounter",s.mTimeoutCounter))return false;
  if(!ar.field("mClimbDirection",s.mClimbDirection))return false;
  if(!ar.ref("mTargetStick",RefKind::Creature,s.mTargetStick))return false;
  if(!ar.field("mLastPosition",s.mLastPosition))return false;
  return true;
 }
 static bool payload(ActBreakWall& s,ActorArchive& ar,unsigned depth) {
  if(!ar.ref("mWall",RefKind::Creature,s.mWall))return false;
  if(!ar.field("mState",s.mState))return false;
  if(!ar.field("mHitPikminPosition",s.mHitPikminPosition))return false;
  if(!ar.field("mStartAttackTime",s.mStartAttackTime))return false;
  if(!ar.field("mWorkTimer",s.mWorkTimer))return false;
  if(!ar.field("mFailAttackCounter",s.mFailAttackCounter))return false;
  if(!ar.field("mIsAttackReady",s.mIsAttackReady))return false;
  return true;
 }
 static bool payload(ActBridge& s,ActorArchive& ar,unsigned depth) {
  if(!ar.ref("mBridge",RefKind::Creature,s.mBridge))return false;
  if(!ar.field("mState",s.mState))return false;
  if(!ar.field("mStartWorkTime",s.mStartWorkTime))return false;
  if(!ar.field("mIsAttackReady",s.mIsAttackReady))return false;
  if(!ar.field("mCollisionCount",s.mCollisionCount))return false;
  if(!ar.field("_2A",s._2A))return false;
  if(!ar.field("mRandomBridgeWidth",s.mRandomBridgeWidth))return false;
  if(!ar.field("mStageID",s.mStageID))return false;
  if(!ar.field("mClimbingBridge",s.mClimbingBridge))return false;
  if(!ar.field("_33",s._33))return false;
  if(!ar.field("mBridgeWallNormal",s.mBridgeWallNormal))return false;
  if(!ar.field("mClimbingVelocity",s.mClimbingVelocity))return false;
  if(!ar.field("mActionCounter",s.mActionCounter))return false;
  if(!ar.field("mAnimationFinished",s.mAnimationFinished))return false;
  return true;
 }
 static bool payload(ActChase& s,ActorArchive& ar,unsigned depth) {
  if(!ar.strongRef("mTarget",s.mTarget,&s,"ActChase","mTarget"))return false;
  if(!ar.field("mChaseTimer",s.mChaseTimer))return false;
  return true;
 }
 static bool payload(ActCrowd& s,ActorArchive& ar,unsigned depth) {
  if(!odometer(ar,"mOdometer",s.mOdometer))return false;
  if(!ar.field("mState",s.mState))return false;
  if(!ar.field("mMode",s.mMode))return false;
  if(!ar.field("mPrevMode",s.mPrevMode))return false;
  if(!ar.field("mNearSlotCounter",s.mNearSlotCounter))return false;
  if(!ar.field("mHasRequestedNewSlot",s.mHasRequestedNewSlot))return false;
  if(!ar.field("_35",s._35))return false;
  if(!ar.field("_36",s._36))return false;
  if(!ar.field("mCPlateSlotID",s.mCPlateSlotID))return false;
  if(!ar.field("mTripLoopCounter",s.mTripLoopCounter))return false;
  if(!ar.field("mTravelDistance",s.mTravelDistance))return false;
  if(!ar.field("mIsTripping",s.mIsTripping))return false;
  if(!ar.field("mLostChildTimer",s.mLostChildTimer))return false;
  if(!ar.ref("mPlateMgr",RefKind::CPlate,s.mPlateMgr))return false;
  bool waiting=ar.mode()==Mode::Capture?s.mIsWaiting:false;
  if(!ar.scalar("mIsWaiting",ScalarKind::Bool,&waiting))return false;
  if(ar.mode()==Mode::Apply)s.mIsWaiting=waiting;
  if(!ar.field("mWasWaiting",s.mWasWaiting))return false;
  if(!ar.field("mHasRoute",s.mHasRoute))return false;
  if(!ar.field("mPcLastPos",s.mPcLastPos))return false;
  if(!ar.field("mPcWindowTimer",s.mPcWindowTimer))return false;
  if(!ar.field("mPcWindowProgress",s.mPcWindowProgress))return false;
  if(!ar.field("mPcRouteTimer",s.mPcRouteTimer))return false;
  if(!ar.field("mPcRouteCooldown",s.mPcRouteCooldown))return false;
  if(!ar.field("mPcRepathTimer",s.mPcRepathTimer))return false;
  if(!ar.field("mPcRouteGoal",s.mPcRouteGoal))return false;
  if(!ar.field("mPcOwnsRoute",s.mPcOwnsRoute))return false;
  if(waiting){if(!s.mSelectAction)return ar.fail("missing Crowd selector"); PrefixArchive select(ar,"selector");if(!visit(*s.mSelectAction,select,depth+1))return false;}
  return true;
 }
 static bool payload(ActDecoy& s,ActorArchive& ar,unsigned depth) {
  if(!ar.field("mState",s.mState))return false;
  if(!ar.field("mDecoyTimer",s.mDecoyTimer))return false;
  return true;
 }
 static bool payload(ActDeliver& s,ActorArchive& ar,unsigned depth) {
  if(!ar.strongRef("mObject",s.mObject,&s,"ActDeliver","mObject"))return false;
  return true;
 }
 static bool payload(ActEnter& s,ActorArchive& ar,unsigned depth) {
  if(!ar.field("mState",s.mState))return false;
  if(!ar.ref("mOnyon",RefKind::Creature,s.mOnyon))return false;
  if(!ar.ref("mLeg",RefKind::CollPart,s.mLeg))return false;
  if(!ar.field("mLastPosition",s.mLastPosition))return false;
  if(!ar.field("mHasCollided",s.mHasCollided))return false;
  return true;
 }
 static bool payload(ActEscape& s,ActorArchive& ar,unsigned depth) {
  if(!ar.strongRef("mTarget",s.mTarget,&s,"ActEscape","mTarget"))return false;
  if(!ar.field("mEscapeTimer",s.mEscapeTimer))return false;
  if(!ar.field("mState",s.mState))return false;
  if(!ar.field("mAvoidDirection",s.mAvoidDirection))return false;
  return true;
 }
 static bool payload(ActExit& s,ActorArchive& ar,unsigned depth) {
  if(!ar.field("mPrevPosition",s.mPrevPosition))return false;
  if(!ar.field("mHasCollided",s.mHasCollided))return false;
  return true;
 }
 static bool payload(ActFlower& s,ActorArchive& ar,unsigned depth) {
  if(!ar.field("mElapsedTime",s.mElapsedTime))return false;
  if(!ar.field("mIsAnimationComplete",s.mIsAnimationComplete))return false;
  if(!ar.field("mIsCarryEmpty",s.mIsCarryEmpty))return false;
  return true;
 }
 static bool payload(ActFormation& s,ActorArchive& ar,unsigned depth) {
  if(!ar.field("mInFormation",s.mInFormation))return false;
  if(!ar.field("mIdleTimer",s.mIdleTimer))return false;
  if(!ar.ref("mFormMgr",RefKind::FormationMgr,s.mFormMgr))return false;
  if(!ar.field("mDistanceToTarget",s.mDistanceToTarget))return false;
  if(!ar.field("mUseLastFormationPosition",s.mUseLastFormationPosition))return false;
  if(!ar.field("mIsIdling",s.mIsIdling))return false;
  if(!ar.field("mHasStartedIdleAnim",s.mHasStartedIdleAnim))return false;
  if(!ar.field("mIsOnFloorTripped",s.mIsOnFloorTripped))return false;
  if(!ar.field("mHasStartedRunAnim",s.mHasStartedRunAnim))return false;
  if(!ar.field("mIsTripping",s.mIsTripping))return false;
  return true;
 }
 static bool payload(ActFreeSelect& s,ActorArchive& ar,unsigned depth) {
  if(!ar.field("mActionTimer",s.mActionTimer))return false;
  if(!ar.field("mIsTimerActive",s.mIsTimerActive))return false;
  if(!ar.field("mIsChildActionActive",s.mIsChildActionActive))return false;
  if(!ar.field("mIsFinished",s.mIsFinished))return false;
  if(!ar.field("_1C",s._1C))return false;
  return true;
 }
 static bool payload(ActFree& s,ActorArchive& ar,unsigned depth) {
  if(!ar.field("_1C",s._1C))return false;
  if(!ar.field("_20",s._20))return false;
  if(!ar.field("_24",s._24))return false;
  if(!ar.field("mBoidTimer",s.mBoidTimer))return false;
  if(!ar.field("mFixedPositionTimer",s.mFixedPositionTimer))return false;
  if(!ar.field("mTargetPosition",s.mTargetPosition))return false;
  if(!ar.field("mArrivalRadius",s.mArrivalRadius))return false;
  if(!ar.field("mCollisionCooldownTimer",s.mCollisionCooldownTimer))return false;
  if(!ar.field("mTouchedPlayer",s.mTouchedPlayer))return false;
  if(!ar.field("mIsBoidActive",s.mIsBoidActive))return false;
  if(!s.mSelectAction)return ar.fail("missing Free selector");
  PrefixArchive select(ar,"selector"); if(!visit(*s.mSelectAction,select,depth+1))return false;
  return true;
 }
 static bool payload(ActGoto& s,ActorArchive& ar,unsigned depth) {
  if(!ar.field("mMaxDistance",s.mMaxDistance))return false;
  if(!ar.field("mMinDistance",s.mMinDistance))return false;
  if(!ar.strongRef("mTarget",s.mTarget,&s,"ActGoto","mTarget"))return false;
  if(!ar.field("mTimeoutDuration",s.mTimeoutDuration))return false;
  return true;
 }
 static bool payload(ActGuard& s,ActorArchive& ar,unsigned depth) {
  if(!ar.strongRef("mTarget",s.mTarget,&s,"ActGuard","mTarget"))return false;
  if(!ar.strongRef("mLeftGuard",s.mLeftGuard,&s,"ActGuard","mLeftGuard"))return false;
  if(!ar.strongRef("mRightGuard",s.mRightGuard,&s,"ActGuard","mRightGuard"))return false;
  if(!ar.field("mGoalPosition",s.mGoalPosition))return false;
  if(!ar.field("mFormationAngle",s.mFormationAngle))return false;
  if(!ar.field("mLandPosition",s.mLandPosition))return false;
  if(!ar.field("mFormationSpacing",s.mFormationSpacing))return false;
  if(!ar.field("mTimer",s.mTimer))return false;
  if(!ar.value("mFormationSide",ScalarKind::S32,s.mFormationSide))return false;
  if(!ar.field("mIsWaiting",s.mIsWaiting))return false;
  if(!ar.field("mIsGuardable",s.mIsGuardable))return false;
  return true;
 }
 static bool payload(ActJumpAttack& s,ActorArchive& ar,unsigned depth) {
  if(!ar.field("mState",s.mState))return false;
  if(!ar.field("mAttackState",s.mAttackState))return false;
  if(!ar.strongRef("mTarget",s.mTarget,&s,"ActJumpAttack","mTarget"))return false;
  if(!ar.ref("mTargetCollider",RefKind::CollPart,s.mTargetCollider))return false;
  if(!ar.field("_2C",s._2C))return false;
  if(!ar.field("mIsCriticalHit",s.mIsCriticalHit))return false;
  return true;
 }
 static bool payload(ActKinoko& s,ActorArchive& ar,unsigned depth) {
  if(!ar.strongRef("mTarget",s.mTarget,&s,"ActKinoko","mTarget"))return false;
  if(!ar.field("mState",s.mState))return false;
  if(!ar.field("mStateTimer",s.mStateTimer))return false;
  if(!ar.field("mTargetDirection",s.mTargetDirection))return false;
  return true;
 }
 static bool payload(ActMine& s,ActorArchive& ar,unsigned depth) {
  if(!ar.ref("mBombGen",RefKind::Creature,s.mBombGen))return false;
  if(!ar.field("mState",s.mState))return false;
  if(!ar.field("mIsMineActionReady",s.mIsMineActionReady))return false;
  return true;
 }
 static bool payload(ActPick& s,ActorArchive& ar,unsigned depth) {
  if(!ar.strongRef("mObject",s.mObject,&s,"ActPick","mObject"))return false;
  if(!ar.field("mIsAnimationFinished",s.mIsAnimationFinished))return false;
  return true;
 }
 static bool payload(ActPickCreature& s,ActorArchive& ar,unsigned depth) {
  if(!ar.strongRef("_18",s._18,&s,"ActPickCreature","_18"))return false;
  return true;
 }
 static bool payload(ActPickItem& s,ActorArchive& ar,unsigned depth) {
  if(!ar.strongRef("mTargetItem",s.mTargetItem,&s,"ActPickItem","mTargetItem"))return false;
  return true;
 }
 static bool payload(ActPullout& s,ActorArchive& ar,unsigned depth) {
  if(!ar.strongRef("mTarget",s.mTarget,&s,"ActPullout","mTarget"))return false;
  return true;
 }
 static bool payload(ActPulloutCreature& s,ActorArchive& ar,unsigned depth) {
  if(!ar.field("mState",s.mState))return false;
  if(!ar.field("mPulloutTimer",s.mPulloutTimer))return false;
  if(!ar.strongRef("mTarget",s.mTarget,&s,"ActPulloutCreature","mTarget"))return false;
  if(!ar.field("mPulloutSuccess",s.mPulloutSuccess))return false;
  return true;
 }
 static bool payload(ActPush& s,ActorArchive& ar,unsigned depth) {
  if(!ar.ref("mHinderRock",RefKind::Creature,s.mHinderRock))return false;
  if(!ar.field("mPushAnimationState",s.mPushAnimationState))return false;
  if(!ar.field("mState",s.mState))return false;
  if(!ar.field("_24",s._24))return false;
  if(!ar.field("_38",s._38))return false;
  if(!ar.field("mPushObjectStopped",s.mPushObjectStopped))return false;
  if(!ar.field("_40",s._40))return false;
  if(!ar.field("_44",s._44))return false;
  if(!ar.field("mPushCount",s.mPushCount))return false;
  if(!ar.field("mIsPushReady",s.mIsPushReady))return false;
  return true;
 }
 static bool payload(ActPut& s,ActorArchive& ar,unsigned depth) {
  if(!ar.field("mFailCountdownTimer",s.mFailCountdownTimer))return false;
  return true;
 }
 static bool payload(ActPutBomb& s,ActorArchive& ar,unsigned depth) {
  if(!ar.field("mState",s.mState))return false;
  if(!ar.field("mAnimationFinished",s.mAnimationFinished))return false;
  if(!ar.field("mTouchedPlayer",s.mTouchedPlayer))return false;
  if(!ar.field("mAimTimer",s.mAimTimer))return false;
  if(!ar.field("mPlaceTimer",s.mPlaceTimer))return false;
  if(!ar.ref("mTarget",RefKind::Creature,s.mTarget))return false;
  return true;
 }
 static bool payload(ActPutItem& s,ActorArchive& ar,unsigned depth) {
  if(!ar.field("mItemPosition",s.mItemPosition))return false;
  if(!ar.strongRef("mItem",s.mItem,&s,"ActPutItem","mItem"))return false;
  return true;
 }
 static bool payload(ActRandomBoid& s,ActorArchive& ar,unsigned depth) {
  if(!ar.field("mState",s.mState))return false;
  if(!ar.field("mStateTimer",s.mStateTimer))return false;
  if(!ar.field("mIsAnimFinishing",s.mIsAnimFinishing))return false;
  if(!ar.ref("mListener",RefKind::AnimListener,s.mListener))return false;
  return true;
 }
 static bool payload(ActRescue& s,ActorArchive& ar,unsigned depth) {
  if(!ar.field("mState",s.mState))return false;
  if(!ar.ref("mDrowningPiki",RefKind::Creature,s.mDrowningPiki))return false;
  if(!ar.field("mTargetSurviveTimer",s.mTargetSurviveTimer))return false;
  if(!ar.field("mRescueTargetPosition",s.mRescueTargetPosition))return false;
  if(!ar.field("mGotAnimationAction",s.mGotAnimationAction))return false;
  if(!ar.field("mAnimationFinished",s.mAnimationFinished))return false;
  if(!ar.field("mThrowReady",s.mThrowReady))return false;
  return true;
 }
 static bool payload(ActRope& s,ActorArchive& ar,unsigned depth) {
  if(!ar.field("mSpeed",s.mSpeed))return false;
  if(!ar.field("mRopeDirection",s.mRopeDirection))return false;
  return true;
 }
 static bool payload(ActShoot& s,ActorArchive& ar,unsigned depth) {
  if(!ar.field("mTargetIsPlayer",s.mTargetIsPlayer))return false;
  if(!ar.ref("mTargetObjectPool",RefKind::Traversable,s.mTargetObjectPool))return false;
  if(!ar.strongRef("mTarget",s.mTarget,&s,"ActShoot","mTarget"))return false;
  if(!ar.ref("mNavi",RefKind::Creature,s.mNavi))return false;
  return true;
 }
 static bool payload(ActShootCreature& s,ActorArchive& ar,unsigned depth) {
  if(!ar.field("mState",s.mState))return false;
  if(!ar.field("mChaseTimer",s.mChaseTimer))return false;
  if(!ar.strongRef("mTarget",s.mTarget,&s,"ActShootCreature","mTarget"))return false;
  return true;
 }
 static bool payload(ActStone& s,ActorArchive& ar,unsigned depth) {
  if(!ar.field("mState",s.mState))return false;
  if(!ar.ref("mCurrPebble",RefKind::Pebble,s.mCurrPebble))return false;
  if(!ar.ref("mRockGen",RefKind::RockGen,s.mRockGen))return false;
  if(!ar.field("mIsAttackReady",s.mIsAttackReady))return false;
  return true;
 }
 static bool payload(ActTransport& s,ActorArchive& ar,unsigned depth) {
  if(!ar.strongRef("mPellet",s.mPellet,&s,"ActTransport","mPellet"))return false;
  if(!ar.field("mState",s.mState))return false;
  if(!odometer(ar,"mOdometer",s.mOdometer))return false;
  if(!ar.field("mMoveDir",s.mMoveDir))return false;
  if(!ar.field("mNumRoutePoints",s.mNumRoutePoints))return false;
  if(!ar.field("mJumpRetryTimer",s.mJumpRetryTimer))return false;
  if(!ar.field("mStateProgress",s.mStateProgress))return false;
  if(!ar.field("mNextPathIndex",s.mNextPathIndex))return false;
  if(!ar.field("_48",s._48))return false;
  if(!arrayFields(ar,"mSplineControlPts",s.mSplineControlPts))return false;
  if(!ar.field("mRouteStartPos",s.mRouteStartPos))return false;
  if(!ar.field("mPathType",s.mPathType))return false;
  if(!ar.field("mSlotIndex",s.mSlotIndex))return false;
  if(!ar.field("mSpinStartPosition",s.mSpinStartPosition))return false;
  if(!ar.field("mFinishPutting",s.mFinishPutting))return false;
  if(!ar.field("mIsLiftActionDone",s.mIsLiftActionDone))return false;
  if(!ar.field("mLiftRetryCount",s.mLiftRetryCount))return false;
  if(!ar.field("mWaitTimer",s.mWaitTimer))return false;
  if(!ar.field("mPathIndex",s.mPathIndex))return false;
  if(!ar.field("mGoalWPIndex",s.mGoalWPIndex))return false;
  if(!ar.ref("mGoal",RefKind::Creature,s.mGoal))return false;
  if(!ar.field("mCanCarry",s.mCanCarry))return false;
  if(!ar.field("mPcStallCheckPos",s.mPcStallCheckPos))return false;
  if(!ar.field("mPcStallTimer",s.mPcStallTimer))return false;
  if(!ar.field("mPcStallArmed",s.mPcStallArmed))return false;
  return true;
 }
 static bool payload(ActWatch& s,ActorArchive& ar,unsigned depth) {
  if(!ar.strongRef("mTarget",s.mTarget,&s,"ActWatch","mTarget"))return false;
  if(!ar.field("mWatchRetryTimer",s.mWatchRetryTimer))return false;
  if(!ar.ref("mListener",RefKind::AnimListener,s.mListener))return false;
  if(!ar.field("mTargetPosition",s.mTargetPosition))return false;
  return true;
 }
 static bool payload(ActWeed& s,ActorArchive& ar,unsigned depth) {
  if(!ar.field("mState",s.mState))return false;
  if(!ar.ref("mCurrGrass",RefKind::Grass,s.mCurrGrass))return false;
  if(!ar.ref("mGrassGen",RefKind::GrassGen,s.mGrassGen))return false;
  if(!ar.field("_28",s._28))return false;
  if(!ar.field("mAnimationFinished",s.mAnimationFinished))return false;
  return true;
 }
 static bool listenerMatches(Action& a,PaniAnimKeyListener* listener){
  if(!listener)return false;
  if(auto* p=dynamic_cast<ActAttack*>(&a))if(static_cast<PaniAnimKeyListener*>(p)==listener)return true;
  if(auto* p=dynamic_cast<ActBoMake*>(&a))if(static_cast<PaniAnimKeyListener*>(p)==listener)return true;
  if(auto* p=dynamic_cast<ActBoreOneshot*>(&a))if(static_cast<PaniAnimKeyListener*>(p)==listener)return true;
  if(auto* p=dynamic_cast<ActBoreRest*>(&a))if(static_cast<PaniAnimKeyListener*>(p)==listener)return true;
  if(auto* p=dynamic_cast<ActBoreTalk*>(&a))if(static_cast<PaniAnimKeyListener*>(p)==listener)return true;
  if(auto* p=dynamic_cast<ActBreakWall*>(&a))if(static_cast<PaniAnimKeyListener*>(p)==listener)return true;
  if(auto* p=dynamic_cast<ActBridge*>(&a))if(static_cast<PaniAnimKeyListener*>(p)==listener)return true;
  if(auto* p=dynamic_cast<ActDecoy*>(&a))if(static_cast<PaniAnimKeyListener*>(p)==listener)return true;
  if(auto* p=dynamic_cast<ActFlower*>(&a))if(static_cast<PaniAnimKeyListener*>(p)==listener)return true;
  if(auto* p=dynamic_cast<ActFormation*>(&a))if(static_cast<PaniAnimKeyListener*>(p)==listener)return true;
  if(auto* p=dynamic_cast<ActFree*>(&a))if(static_cast<PaniAnimKeyListener*>(p)==listener)return true;
  if(auto* p=dynamic_cast<ActJumpAttack*>(&a))if(static_cast<PaniAnimKeyListener*>(p)==listener)return true;
  if(auto* p=dynamic_cast<ActKinoko*>(&a))if(static_cast<PaniAnimKeyListener*>(p)==listener)return true;
  if(auto* p=dynamic_cast<ActMine*>(&a))if(static_cast<PaniAnimKeyListener*>(p)==listener)return true;
  if(auto* p=dynamic_cast<ActPick*>(&a))if(static_cast<PaniAnimKeyListener*>(p)==listener)return true;
  if(auto* p=dynamic_cast<ActPulloutCreature*>(&a))if(static_cast<PaniAnimKeyListener*>(p)==listener)return true;
  if(auto* p=dynamic_cast<ActPush*>(&a))if(static_cast<PaniAnimKeyListener*>(p)==listener)return true;
  if(auto* p=dynamic_cast<ActPutBomb*>(&a))if(static_cast<PaniAnimKeyListener*>(p)==listener)return true;
  if(auto* p=dynamic_cast<ActRescue*>(&a))if(static_cast<PaniAnimKeyListener*>(p)==listener)return true;
  if(auto* p=dynamic_cast<ActShootCreature*>(&a))if(static_cast<PaniAnimKeyListener*>(p)==listener)return true;
  if(auto* p=dynamic_cast<ActStone*>(&a))if(static_cast<PaniAnimKeyListener*>(p)==listener)return true;
  if(auto* p=dynamic_cast<ActTransport*>(&a))if(static_cast<PaniAnimKeyListener*>(p)==listener)return true;
  if(auto* p=dynamic_cast<ActWeed*>(&a))if(static_cast<PaniAnimKeyListener*>(p)==listener)return true;
  if(auto* p=dynamic_cast<ActRandomBoid*>(&a))if(p->mListener==listener)return true;
  if(auto* p=dynamic_cast<ActWatch*>(&a))if(p->mListener==listener)return true;
  return false;
 }
 static bool findListener(Action& a,PaniAnimKeyListener* listener,std::vector<u8>& path,unsigned depth){
  if(depth>16)return false;
  if(listenerMatches(a,listener))return true;
  if(a.mChildCount<0||a.mChildCount>31||(a.mChildCount&&!a.mChildActions))return false;
  for(int i=0;i<a.mChildCount;++i){auto* p=a.mChildActions[i].mAction;if(!p||p->mPiki!=a.mPiki)continue;path.push_back(u8(i));if(findListener(*p,listener,path,depth+1))return true;path.pop_back();}
  Action* selector=nullptr;
  if(auto* p=dynamic_cast<ActFree*>(&a))selector=p->mSelectAction;
  if(auto* p=dynamic_cast<ActCrowd*>(&a))selector=p->mSelectAction;
  if(selector){path.push_back(31);if(findListener(*selector,listener,path,depth+1))return true;path.pop_back();}
  return false;
 }
 static Action* follow(Action& root,const std::vector<u8>& path){
  Action* a=&root;
  for(u8 edge:path){
   if(edge==31){
    if(auto* p=dynamic_cast<ActFree*>(a))a=p->mSelectAction;
    else if(auto* p=dynamic_cast<ActCrowd*>(a))a=p->mSelectAction;
    else return nullptr;
   }else{if(!a->mChildActions||edge>=a->mChildCount)return nullptr;a=a->mChildActions[edge].mAction;}
   if(!a||a->mPiki!=root.mPiki)return nullptr;
  }
  return a;
 }
 static bool boredom(TopAction::Boredom& b,ActorArchive& ar) {
  PrefixArchive a(ar,"boredom");
  int count=ar.mode()==Mode::Capture?b.mCurrentBoredomCount:0;
  int next=ar.mode()==Mode::Capture?b.mNextAvailableIndex:0;
  if(!a.scalar("count",ScalarKind::S32,&count)||!a.scalar("next",ScalarKind::S32,&next))return false;
  if(count<0||count>b.mMaxBoredomCollectors||next<0||next>=b.mMaxBoredomCollectors||b.mMaxBoredomCollectors!=30||!b.mBoredomCollectors||!b.mBoredomIds)return a.fail("invalid boredom bounds");
  for(int i=0;i<count;++i){
   PrefixArchive item(a,std::to_string(i).c_str());auto& o=b.mBoredomCollectors[i];
   u32 used=ar.mode()==Mode::Capture?o.mCurrentCount:0;
   if(!item.field("id",b.mBoredomIds[i])||!item.scalar("used",ScalarKind::U32,&used))return false;
   if(o.mMaxCount!=5||used>5||!o.mBoredomLevels||!o.mObjectIds||!o.mIsMaxBored)return item.fail("invalid boredom object bounds");
   for(unsigned j=0;j<used;++j){PrefixArchive entry(item,std::to_string(j).c_str());if(!entry.field("level",o.mBoredomLevels[j])||!entry.field("object",o.mObjectIds[j])||!entry.field("max",o.mIsMaxBored[j]))return false;}
   if(ar.mode()==Mode::Apply)o.mCurrentCount=used;
  }
  if(ar.mode()==Mode::Apply){b.mCurrentBoredomCount=count;b.mNextAvailableIndex=next;}
  return true;
 }
};
thread_local std::set<Action*> PcMiddayPikiActionAccess::visited;
bool PcMiddayPikiActionAccess::extras(Piki& piki,ActorArchive& outer){
 PrefixArchive ar(outer,"piki.listenerExtras");
 PaniAnimKeyListener* listeners[]={piki.mPikiAnimMgr.mUpperAnimator.mListener,piki.mPikiAnimMgr.mLowerAnimator.mListener};
 for(unsigned index=0;index<2;++index){
  PrefixArchive extra(ar,index==0?"upper":"lower");std::vector<u8> path;bool present=false;
  if(ar.mode()==Mode::Capture && findListener(*piki.mActiveAction,listeners[index],path,0)){
   auto* target=follow(*piki.mActiveAction,path);present=target&&!visited.count(target);
  }
  if(!extra.scalar("present",ScalarKind::Bool,&present))return false;
  if(!present)continue;
  u8 length=ar.mode()==Mode::Capture?u8(path.size()):0;
  if(!extra.scalar("length",ScalarKind::U8,&length)||length>16)return extra.fail("listener action path exceeds bound");
  if(ar.mode()!=Mode::Capture)path.resize(length);
  for(unsigned i=0;i<length;++i)if(!extra.scalar(("path."+std::to_string(i)).c_str(),ScalarKind::U8,&path[i]))return false;
  Action* target=follow(*piki.mActiveAction,path);
  if(!target||visited.count(target))return extra.fail("duplicate/invalid listener action path");
  PaniAnimKeyListener* listener=ar.mode()==Mode::Capture?listeners[index]:nullptr;
  if(!extra.ref("listener",RefKind::AnimListener,listener))return false;
  if(ar.mode()==Mode::Apply && !listenerMatches(*target,listener))return extra.fail("listener does not belong to declared action topology");
  PrefixArchive payload(extra,"payload");if(!visit(*target,payload,0))return false;
 }
 return true;
}
bool PcMiddayPikiActionAccess::visit(Action& action,ActorArchive& ar,unsigned depth) {
 if(depth>16)return ar.fail("action graph exceeds depth bound");
 if(!visited.insert(&action).second)return ar.fail("overlapping action payload subtree");
 if(action.mChildCount<0||action.mChildCount>PikiAction::COUNT)return ar.fail("invalid allocated child count");
 s16 child=ar.mode()==Mode::Capture?action.mCurrActionIdx:0;
 s16 count=action.mChildCount;
 if(!ar.scalar("child",ScalarKind::S16,&child)||!ar.scalar("count",ScalarKind::S16,&count))return false;
 if(count!=action.mChildCount||child < -1||(count>0&&child>=count)||(count==0&&child>0))return ar.fail("action child discriminator corrupt");
 if(count && !action.mChildActions)return ar.fail("missing action child allocation");
 int type=typeOf(action);
 int saved=ar.mode()==Mode::Capture?type:0;
 if(!ar.scalar("type",ScalarKind::S32,&saved)||!type||saved!=type)return ar.fail("action type mismatch");
 if(auto* andAction=dynamic_cast<AndAction*>(&action)){if(!ar.ref("andTarget",RefKind::Creature,andAction->mOtherCreature))return false;}
 if(auto* orAction=dynamic_cast<OrAction*>(&action)){if(!ar.ref("orTarget",RefKind::Creature,orAction->mOtherCreature))return false;}
 switch(type){
 case 1:if(!payload(static_cast<TopAction&>(action),ar,depth))return false;break;
 case 2:if(!payload(static_cast<ActAdjust&>(action),ar,depth))return false;break;
 case 3:if(!payload(static_cast<ActAttack&>(action),ar,depth))return false;break;
 case 4:if(!payload(static_cast<ActBoMake&>(action),ar,depth))return false;break;
 case 5:if(!payload(static_cast<ActBoreListen&>(action),ar,depth))return false;break;
 case 6:if(!payload(static_cast<ActBoreOneshot&>(action),ar,depth))return false;break;
 case 7:if(!payload(static_cast<ActBoreRest&>(action),ar,depth))return false;break;
 case 8:if(!payload(static_cast<ActBoreSelect&>(action),ar,depth))return false;break;
 case 9:if(!payload(static_cast<ActBoreTalk&>(action),ar,depth))return false;break;
 case 10:if(!payload(static_cast<ActBou&>(action),ar,depth))return false;break;
 case 11:if(!payload(static_cast<ActBreakWall&>(action),ar,depth))return false;break;
 case 12:if(!payload(static_cast<ActBridge&>(action),ar,depth))return false;break;
 case 13:if(!payload(static_cast<ActChase&>(action),ar,depth))return false;break;
 case 14:if(!payload(static_cast<ActCrowd&>(action),ar,depth))return false;break;
 case 15:if(!payload(static_cast<ActDecoy&>(action),ar,depth))return false;break;
 case 16:if(!payload(static_cast<ActDeliver&>(action),ar,depth))return false;break;
 case 17:if(!payload(static_cast<ActEnter&>(action),ar,depth))return false;break;
 case 18:if(!payload(static_cast<ActEscape&>(action),ar,depth))return false;break;
 case 19:if(!payload(static_cast<ActExit&>(action),ar,depth))return false;break;
 case 20:if(!payload(static_cast<ActFlower&>(action),ar,depth))return false;break;
 case 21:if(!payload(static_cast<ActFormation&>(action),ar,depth))return false;break;
 case 22:if(!payload(static_cast<ActFreeSelect&>(action),ar,depth))return false;break;
 case 23:if(!payload(static_cast<ActFree&>(action),ar,depth))return false;break;
 case 24:if(!payload(static_cast<ActGoto&>(action),ar,depth))return false;break;
 case 25:if(!payload(static_cast<ActGuard&>(action),ar,depth))return false;break;
 case 26:if(!payload(static_cast<ActJumpAttack&>(action),ar,depth))return false;break;
 case 27:if(!payload(static_cast<ActKinoko&>(action),ar,depth))return false;break;
 case 28:if(!payload(static_cast<ActMine&>(action),ar,depth))return false;break;
 case 29:if(!payload(static_cast<ActPick&>(action),ar,depth))return false;break;
 case 30:if(!payload(static_cast<ActPickCreature&>(action),ar,depth))return false;break;
 case 31:if(!payload(static_cast<ActPickItem&>(action),ar,depth))return false;break;
 case 32:if(!payload(static_cast<ActPullout&>(action),ar,depth))return false;break;
 case 33:if(!payload(static_cast<ActPulloutCreature&>(action),ar,depth))return false;break;
 case 34:if(!payload(static_cast<ActPush&>(action),ar,depth))return false;break;
 case 35:if(!payload(static_cast<ActPut&>(action),ar,depth))return false;break;
 case 36:if(!payload(static_cast<ActPutBomb&>(action),ar,depth))return false;break;
 case 37:if(!payload(static_cast<ActPutItem&>(action),ar,depth))return false;break;
 case 38:if(!payload(static_cast<ActRandomBoid&>(action),ar,depth))return false;break;
 case 39:if(!payload(static_cast<ActRescue&>(action),ar,depth))return false;break;
 case 40:if(!payload(static_cast<ActRope&>(action),ar,depth))return false;break;
 case 41:if(!payload(static_cast<ActShoot&>(action),ar,depth))return false;break;
 case 42:if(!payload(static_cast<ActShootCreature&>(action),ar,depth))return false;break;
 case 43:if(!payload(static_cast<ActStone&>(action),ar,depth))return false;break;
 case 44:if(!payload(static_cast<ActTransport&>(action),ar,depth))return false;break;
 case 45:if(!payload(static_cast<ActWatch&>(action),ar,depth))return false;break;
 case 46:if(!payload(static_cast<ActWeed&>(action),ar,depth))return false;break;
 default:return ar.fail("unknown action type");
 }
 if(count>0 && child>=0){
  auto* nested=action.mChildActions[child].mAction;
  if(!nested||nested->mPiki!=action.mPiki)return ar.fail("foreign/missing action owner");
  PrefixArchive next(ar,"childPayload");if(!visit(*nested,next,depth+1))return false;
 }
 if(ar.mode()==Mode::Apply)action.mCurrActionIdx=child;
 return true;
}
namespace pc_midday {
bool piki_runtime_fields(Piki&,ActorArchive&);
bool piki_actions(Piki& piki,ActorArchive& ar){
 if(!piki.mActiveAction||piki.mActiveAction->mPiki!=&piki)return ar.fail("missing/foreign TopAction");
 PcMiddayPikiActionAccess::visited.clear();
 PrefixArchive top(ar,"piki.action");return PcMiddayPikiActionAccess::visit(*piki.mActiveAction,top,0) && PcMiddayPikiActionAccess::extras(piki,ar) && PcMiddayPikiActionAccess::inactiveStrong(piki,ar) && piki_runtime_fields(piki,ar);
}
bool visit_piki_strong_storage(Piki& piki,const ActorFields& fields,StrongStorageVisitor& visitor,std::string& error){return PcMiddayPikiActionAccess::storage(piki,fields,visitor,error);}
bool capture_piki(Piki& piki,LogicalResolver& resolver,double now,ActorBytes& output,std::string& error){
 ActorFields fields;FieldArchive capture(Mode::Capture,fields,resolver,error,now);
 if(!creature_fields(piki,capture)||!piki_states(piki,capture)||!piki_actions(piki,capture)||!capture.finish())return false;
 std::vector<FieldSchema> schema;
 if(!piki_schema(fields,schema,error)||!validate_actor_fields(fields,schema,resolver,error))return false;
 return encode_actor_fields(fields,output,error);
}
bool bind_piki(Piki& piki,const ActorBytes& bytes,LogicalResolver& resolver,double now,std::string& error){
 if(!validate_piki(bytes,resolver,error))return false;
 ActorFields fields;if(!decode_actor_fields(bytes,fields,error))return false;
 FieldArchive validation(Mode::Validate,fields,resolver,error,now);
 if(!creature_fields(piki,validation)||!piki_states(piki,validation)||!piki_actions(piki,validation)||!validation.finish())return false;
 FieldArchive apply(Mode::Apply,fields,resolver,error,now);
 return creature_fields(piki,apply)&&piki_states(piki,apply)&&piki_actions(piki,apply)&&apply.finish();
}
}
#endif
