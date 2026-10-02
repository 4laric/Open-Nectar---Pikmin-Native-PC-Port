#if defined(PIKI_PC_PORT)
#include "pc_midday_world.h"
#include "ItemMgr.h"
#include "PaniAnimator.h"
#include "ObjType.h"
#include "Pellet.h"
#include "pc_midday_collision.h"
namespace pc_midday {
bool world_animation_fields(PaniAnimator& s,ActorArchive& ar){
 bool active=ar.mode()==Mode::Capture&&s.mAnimInfo;
 if(!ar.scalar("active",ScalarKind::Bool,&active))return false;
 if(!ar.ref("mMgr",RefKind::Animation,s.mMgr)||!ar.ref("mContext",RefKind::Animation,s.mContext)||!ar.ref("mMotionTable",RefKind::Animation,s.mMotionTable)||!ar.ref("mAnimInfo",RefKind::Animation,s.mAnimInfo))return false;
 if(!active)return true;
 if(ar.mode()!=Mode::Validate&&(!s.mMgr||!s.mContext||!s.mMotionTable||!s.mAnimInfo))return ar.fail("active world animation has missing resources");
 return ar.field("mPlayState",s.mPlayState)&&ar.field("mCurrentAnimID",s.mCurrentAnimID)&&ar.field("mStartKeyIndex",s.mStartKeyIndex)&&ar.field("mEndKeyIndex",s.mEndKeyIndex)&&ar.field("mAnimationCounter",s.mAnimationCounter)&&ar.field("mCurrentKeyIndex",s.mCurrentKeyIndex)&&ar.field("mPreviousKeyIndex",s.mPreviousKeyIndex)&&ar.field("mMotionIdx",s.mMotionIdx)&&ar.field("mIsFinished",s.mIsFinished)&&ar.ref("mListener",RefKind::AnimListener,s.mListener);
}
bool world_ai_fields(AICreature& s,ActorArchive& outer){
 PrefixArchive ar(outer,"world.ai");auto& c=s.mSAICtx;
 auto* machine=c.mStateMachine;
 if(!ar.ref("machine",RefKind::SAIStateMachine,machine))return false;
 int current=ar.mode()==Mode::Capture&&c.mCurrentState?c.mCurrentState->getID():-1;
 int last=ar.mode()==Mode::Capture&&machine?machine->mLastStateID:-1;
 if(!ar.scalar("current",ScalarKind::S32,&current)||!ar.scalar("last",ScalarKind::S32,&last))return false;
 // During Validate resource resolution is deliberately staged, not assigned.
 // The prepared object already carries its factory's immutable table.
 auto* table=ar.mode()==Mode::Apply?machine:c.mStateMachine;
 AState<AICreature>* selected=nullptr;bool lastFound=last==-1;
 if(table){if(table->mStateCount<0||table->mStateCount>64||!table->mStates)return ar.fail("invalid SAI table allocation");for(int i=0;i<table->mStateCount;++i){auto* state=table->mStates[i];if(!state)return ar.fail("null SAI table entry");if(state->getID()==current)selected=state;if(state->getID()==last)lastFound=true;}}
 if((current!=-1&&!selected)||!lastFound||(current==-1&&last!=-1))return ar.fail("unregistered world SAI state");
 if(!ar.ref("collision",RefKind::Creature,c.mCollidingCreature)||!ar.field("vector",c._08)||!ar.field("animation",c.mCurrAnimId)||!ar.field("counter",c.mCounter)||!ar.field("health",c.mCurrentItemHealth)||!ar.field("events",c.mCurrentEventCount))return false;
 for(int i=0;i<16;++i)if(!ar.field(("event."+std::to_string(i)).c_str(),c.mEventFlags[i]))return false;
 if(s.mObjType==OBJTYPE_Bomb&&!ar.field("maxHealth",c.mMaxItemHealth))return false;
 if(ar.mode()==Mode::Apply){c.mStateMachine=machine;c.mCurrentState=selected;if(machine)machine->mLastStateID=last;}
 return true;
}
bool world_item_fields(ItemCreature& s,ActorArchive& outer){
 if(!world_ai_fields(s,outer))return false;
 PrefixArchive ar(outer,"world.item");
 if(!ar.field("motionSpeed",s.mMotionSpeed)||!ar.field("setup",s._3C4)||!ar.ref("shape",RefKind::Shape,s.mItemShape)||!ar.ref("shapeObject",RefKind::ItemShape,s.mItemShapeObject))return false;
 PrefixArchive anim(ar,"animation");return world_animation_fields(s.mItemAnimator,anim);
}
}
namespace pc_midday {
bool world_structure_fields(Creature&,WorldKind,ActorArchive&);
bool world_item_derived_fields(ItemCreature&,ActorArchive&);
bool world_pellet_fields(Pellet&,ActorArchive&);
bool world_fields(Creature& s,WorldKind kind,ActorArchive& ar){
 int discriminator=ar.mode()==Mode::Capture?static_cast<int>(kind):0;
 if(!ar.scalar("world.kind",ScalarKind::S32,&discriminator)||discriminator!=static_cast<int>(kind))return ar.fail("world factory kind mismatch");
 if(!creature_fields(s,ar))return false;
 bool collision=ar.mode()==Mode::Capture&&s.mCollInfo;
 if(!ar.scalar("world.collision.present",ScalarKind::Bool,&collision)||collision!=(s.mCollInfo!=nullptr))return ar.fail("world collider allocation/presence mismatch");
 if(collision){PrefixArchive coll(ar,"world.collision");if(!collision_fields(*s.mCollInfo,coll))return false;}
 if(kind==WorldKind::Pellet){auto* p=dynamic_cast<Pellet*>(&s);return p&&world_pellet_fields(*p,ar);}
 if(kind==WorldKind::Item||kind==WorldKind::Bridge||kind==WorldKind::HinderRock){auto* p=dynamic_cast<ItemCreature*>(&s);if(!p||!world_item_fields(*p,ar))return false;if(kind==WorldKind::Item)return world_item_derived_fields(*p,ar);}
 return world_structure_fields(s,kind,ar);
}
bool capture_world(Creature& s,WorldKind kind,LogicalResolver& resolver,double now,ActorBytes& bytes,std::string& error){
 ActorFields f;FieldArchive ar(Mode::Capture,f,resolver,error,now);std::vector<FieldSchema> schema;
 return world_fields(s,kind,ar)&&ar.finish()&&world_schema(f,s.mObjType,kind,schema,error)&&validate_actor_fields(f,schema,resolver,error)&&encode_actor_fields(f,bytes,error);
}
bool bind_world(Creature& s,WorldKind kind,const ActorBytes& bytes,LogicalResolver& resolver,double now,std::string& error){
 if(!validate_world(bytes,resolver,s.mObjType,kind,error))return false;ActorFields f;if(!decode_actor_fields(bytes,f,error))return false;
 FieldArchive check(Mode::Validate,f,resolver,error,now);if(!world_fields(s,kind,check)||!check.finish())return false;
 FieldArchive apply(Mode::Apply,f,resolver,error,now);return world_fields(s,kind,apply)&&apply.finish();
}
}
#endif
