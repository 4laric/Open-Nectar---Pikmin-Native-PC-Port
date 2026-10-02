#if defined(PIKI_PC_PORT)
#include "pc_midday_view_piki_prepare.h"
#include "pc_midday_view_piki.h"
#include "pc_midday_collision.h"
#include "ViewPiki.h"
#include "Collision.h"
#include <typeinfo>
namespace pc_midday {
namespace {
class EmptyCollider final:public ActorArchive {
 std::string& error;
public:
 explicit EmptyCollider(std::string&e):error(e){}
 Mode mode()const override{return Mode::Capture;}double clock_now()const override{return 0;}
 bool scalar(const char*k,ScalarKind t,void*v)override{
  const std::string key(k);
  if(key=="count")return t==ScalarKind::U16&&*static_cast<u16*>(v)==0 ? true:fail("prepared collider must be empty");
  if(key=="capacity")return t==ScalarKind::U16&&*static_cast<u16*>(v)==4 ? true:fail("ViewPiki collider capacity must be four");
  if(key=="defaultStorage")return t==ScalarKind::Bool&&!*static_cast<bool*>(v) ? true:fail("ViewPiki requires allocated collider");
  return fail("unexpected collider field");
 }
 bool reference(const char*k,RefKind,void*&v)override{return std::string(k)=="identity"?v!=nullptr:std::string(k)=="shape"&&!v;}
 bool handle(const char*,RefKind,u32&)override{return fail("unexpected collider handle");}
 bool fail(const char*m)override{if(error.empty())error=m;return false;}
};
}
bool prepare_view_piki(ViewPiki& actor,const ActorBytes& base,const ActorBytes& subtype,
 LogicalResolver& resolver,RouteMgr* constructorRoute,CollInfo& collider,std::string&e){
 auto refuse=[&](const char*m){if(e.empty())e=m;return false;};
 if(typeid(actor)!=typeid(ViewPiki)||actor.mPikiShape||actor.mCollInfo||actor.mCurrentState)return refuse("ViewPiki must be bare constructed staged object");
 if(!validate_piki(base,resolver,e)||!validate_view_piki(subtype,resolver,e))return false;
 ActorFields b,s;if(!decode_actor_fields(base,b,e)||!decode_actor_fields(subtype,s,e))return false;
 int capacity=0;if(!actor_i32(b,"piki.runtime.path.capacity",capacity,e)||capacity<0||capacity>32767)return false;
 if(constructorRoute!=routeMgr)return refuse("constructor route global changed");
 const int actual=constructorRoute?constructorRoute->getNumWayPoints('test'):0;
 if(actual!=capacity||(capacity>0&&!actor.mPathBuffers)||(capacity==0&&actor.mPathBuffers))return refuse("ViewPiki constructor path allocation mismatch");
 EmptyCollider probe(e);if(!collision_fields(collider,probe))return refuse("canonical collider not fresh constructor topology");
 void* shape=nullptr;void* leaf=nullptr;void* collision=nullptr;
 if(!resolver.resolve("viewPiki.pikiShape",RefKind::Shape,s.at("viewPiki.pikiShape").target,shape,e)||!shape)return false;
 const auto& leafRef=s.at("viewPiki.happaModel").target;
 if(leafRef.owner||leafRef.resource||leafRef.slot){
  if(!resolver.resolve("viewPiki.happaModel",RefKind::Shape,leafRef,leaf,e))return false;
  if(!leaf)return refuse("non-null ViewPiki leaf resolved to null");
 }
 if(!resolver.resolve("creature.mCollInfo",RefKind::CollInfo,b.at("creature.mCollInfo").target,collision,e)||collision!=&collider)return refuse("canonical collider identity mismatch");
 auto* selected=static_cast<PikiShapeObject*>(shape);
 auto matches=[&](const char* key,const void* expected){auto i=b.find(key);void* actualPointer=nullptr;return i!=b.end()&&resolver.resolve(key,RefKind::Animation,i->second.target,actualPointer,e)&&actualPointer==expected;};
 if(!selected->mShape||!selected->mAnimMgr||
    !matches("piki.runtime.upperAnimation.mContext",&selected->mAnimatorB)||
    !matches("piki.runtime.lowerAnimation.mContext",&selected->mAnimatorA)||
    !matches("piki.runtime.upperAnimation.mMgr",selected->mAnimMgr)||
    !matches("piki.runtime.lowerAnimation.mMgr",selected->mAnimMgr))return refuse("ViewPiki selected shape/animation resource mismatch");
 // All fallible checks complete; only resource pointers are prepared. Actor
 // scalar/FSM/animation/collision payloads remain for their validated binders.
 actor.mPikiShape=static_cast<PikiShapeObject*>(shape);
 actor.mHappaModel=static_cast<Shape*>(leaf);actor.mCollInfo=&collider;
 return true;
}
}
#endif
