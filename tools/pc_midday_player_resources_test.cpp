#include "pc_midday_player_resources.h"
#include <cstdio>
using namespace pc_midday;
namespace {
int checks=0,failures=0;
void check(bool v,const char* what){++checks;if(!v){++failures;std::printf("FAIL %s\n",what);}}
void value(ActorFields& f,const std::string& k,ScalarKind t,u64 bits=0){ActorField v;v.category=FieldCategory::Scalar;v.scalar=t;v.bits=bits;f[k]=v;}
void ref(ActorFields& f,const std::string& k,RefKind t,u64 id=0){ActorField v;v.category=FieldCategory::Reference;v.reference=t;v.target.resource=id;f[k]=v;}
void vector(ActorFields& f,const std::string& p){for(auto a:{".x",".y",".z"})value(f,p+a,ScalarKind::F32);}
void animation(ActorFields& f,const std::string& p){value(f,p+".active",ScalarKind::Bool);ref(f,p+".mMgr",RefKind::Animation,1);ref(f,p+".mContext",RefKind::Animation,2);ref(f,p+".mMotionTable",RefKind::Animation,3);ref(f,p+".mAnimInfo",RefKind::Animation);}
ActorFields fixture(){ActorFields f;value(f,"player.resources.version",ScalarKind::S32,1);value(f,"player.resources.total",ScalarKind::S32,30);value(f,"player.resources.registered",ScalarKind::S32,1);value(f,"player.resources.repair",ScalarKind::S32,0xffffffffu);value(f,"player.resources.preload",ScalarKind::Bool,1);
 for(int i=0;i<30;++i){value(f,"player.resources.replay."+std::to_string(i),ScalarKind::Bool,i%2);value(f,"player.resources.part."+std::to_string(i)+".visibility",ScalarKind::U8,i%3);}
 const std::string p="player.resources.part.0.";value(f,p+"joint",ScalarKind::S32,0xffffffffu);value(f,p+"model",ScalarKind::U32,0x75663031);value(f,p+"pellet",ScalarKind::U32,0x70303031);value(f,p+"shaped",ScalarKind::Bool);ref(f,p+"shape",RefKind::ItemShape);vector(f,p+"repairPosition");value(f,p+"materials.count",ScalarKind::S32);ref(f,p+"materials.next",RefKind::ShapeDynMaterials);ref(f,p+"materials.model",RefKind::Shape);
 ref(f,p+"listenerIdentity",RefKind::AnimListener,99);f[p+"listenerIdentity"].target.slot=1;
 ref(f,"player.resources.olimarShape",RefKind::Shape,4);value(f,"player.resources.olimarSpeed",ScalarKind::F32,0x41f00000);animation(f,"player.resources.olimarLower");animation(f,"player.resources.olimarUpper");ref(f,"player.resources.light",RefKind::Effect,5);ref(f,"player.resources.lightGlow",RefKind::Effect,6);vector(f,"player.resources.lightPosition");return f;}
struct Resolver:LogicalResolver{
 mutable int typed=0;bool deny=false;
 bool identify(const char*,RefKind,const void*,LogicalRef&,std::string&)override{return false;}
 bool validate(const char*,RefKind,const LogicalRef&,std::string&)const override{return true;}
 bool validateTyped(const FieldSchema& s,const LogicalRef& r,std::string&)const override{++typed;return !deny&&!s.targetType.empty()&&!r.owner&&(s.ownership!=ReferenceOwnership::ResourceSubobject||!r.resource||(r.resource==99&&r.slot==1));}
 bool resolve(const char*,RefKind,const LogicalRef&,void*&,std::string&)override{return false;}
 bool identifyHandle(const char*,RefKind,u32,LogicalRef&,std::string&)override{return false;}
 bool resolveHandle(const char*,RefKind,const LogicalRef&,u32&,std::string&)override{return false;}
};
bool valid(const ActorFields& f,Resolver& r){std::string e;std::vector<FieldSchema>s;return player_resources_schema(f,s,e)&&validate_actor_fields(f,s,r,e);}
}
int main(){Resolver r;auto f=fixture();std::string e;check(valid(f,r),"unshaped registration with all30 visibility/replay slots");check(r.typed>=10,"typed nullable resource contracts checked");ActorBytes b;check(encode_actor_fields(f,b,e),"encode real actor format");check(validatePlayerResources(b,r,e),"decode/schema/typed validation");ActorFields decoded;check(decode_actor_fields(b,decoded,e)&&decoded.at("player.resources.replay.29").bits==1,"last replay bit preserved");
 for(auto field:{"player.resources.version","player.resources.total","player.resources.registered","player.resources.repair"}){auto bad=f;bad[field].bits=31;check(!valid(bad,r),field);}
 {auto bad=f;bad["player.resources.repair"].bits=0;check(valid(bad,r),"registered repair index");bad["player.resources.repair"].bits=1;check(!valid(bad,r),"unregistered repair refused");}
 {auto bad=f;bad["player.resources.part.29.visibility"].bits=3;check(!valid(bad,r),"invalid invisible slot29 visibility");bad=f;bad["player.resources.replay.29"].bits=2;check(!valid(bad,r),"noncanonical replay boolean");}
 {auto bad=f;bad["player.resources.part.0.joint"].bits=0xfffffffeu;check(!valid(bad,r),"negative invalid joint");bad=f;bad["player.resources.part.0.shaped"].bits=1;check(!valid(bad,r),"shaped-with-null resource refused");bad=f;bad["player.resources.part.0.shape"].target.resource=10;check(!valid(bad,r),"unshaped-with-shape resource refused");}
 {auto bad=f;value(bad,"player.resources.part.0.motionSpeed",ScalarKind::F32);check(!valid(bad,r),"uninitialized alternate animator storage prohibited");bad=f;bad["player.resources.part.0.materials.count"].bits=257;check(!valid(bad,r),"material allocation bound");}
 {auto bad=f;bad["player.resources.light"].target={};check(!valid(bad,r),"missing initialized light");bad=f;bad["player.resources.light"].reference=RefKind::Creature;check(!valid(bad,r),"wrong concrete resource role");bad=f;bad["player.resources.olimarSpeed"].bits=0x7fc00000;check(!valid(bad,r),"NaN rejected");}
 {auto bad=f;bad["player.resources.olimarLower.mAnimInfo"].target.resource=7;check(!valid(bad,r),"inactive animator with active info rejected");bad=f;bad["player.resources.lightPosition.x"].bits=0x80000000;check(valid(bad,r),"negative zero retained");}
 {Resolver refuse;refuse.deny=true;check(!valid(f,refuse),"typed resolver refusal");}
 {auto shaped=f;const std::string p="player.resources.part.0.";shaped[p+"shaped"].bits=1;shaped[p+"shape"].target.resource=10;value(shaped,p+"motionSpeed",ScalarKind::F32,0x41f00000);
  for(auto name:{"lower","upper"}){const auto a=p+name;animation(shaped,a);shaped[a+".active"].bits=1;shaped[a+".mAnimInfo"].target.resource=7;for(auto key:{"mPlayState","mCurrentAnimID","mStartKeyIndex","mEndKeyIndex","mCurrentKeyIndex","mMotionIdx"})value(shaped,a+"."+key,ScalarKind::S32);value(shaped,a+".mPreviousKeyIndex",ScalarKind::U32);value(shaped,a+".mAnimationCounter",ScalarKind::F32);value(shaped,a+".mIsFinished",ScalarKind::Bool);ref(shaped,a+".mListener",RefKind::AnimListener);}
  shaped[p+"lower.mListener"].target=shaped[p+"listenerIdentity"].target;check(valid(shaped,r),"active shaped animations and exact part listener");
  std::vector<FieldSchema>s;check(player_resources_schema(shaped,s,e),"shaped compiled schema");bool contract=false;for(const auto& field:s)if(field.key==p+"lower.mListener")contract=field.ownership==ReferenceOwnership::ResourceSubobject&&field.targetType=="PlayerState::UfoParts";check(contract,"listener concrete resource ownership");
  auto bad=shaped;bad[p+"lower.mListener"].target.slot=2;check(!valid(bad,r),"wrong part listener slot refused before allocation");bad=shaped;bad[p+"lower.mListener"].target.resource=98;check(!valid(bad,r),"foreign array listener refused");bad=shaped;bad[p+"listenerIdentity"].target={};check(!valid(bad,r),"null canonical part listener identity refused");
 }
 {std::vector<FieldSchema> out{FieldSchema::value("sentinel",ScalarKind::U8)};auto bad=f;bad["player.resources.total"].bits=29;check(!player_resources_schema(bad,out,e)&&out.size()==1&&out[0].key=="sentinel","schema output atomic on failure");}
 {auto bad=b;bad.pop_back();check(!validatePlayerResources(bad,r,e),"truncation rejected");bad=b;bad.push_back(0);check(!validatePlayerResources(bad,r,e),"trailing bytes rejected");}
 std::printf("%d checks, %d failures\n",checks,failures);return failures?1:0;
}
