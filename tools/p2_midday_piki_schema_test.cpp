#include "pc_midday_creature.h"
#include "ObjType.h"
#include "pc_midday_actor_archive.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <cstring>
using namespace pc_midday;
namespace {
int checks=0;
void require(bool ok,const char* why){++checks;if(!ok){std::fprintf(stderr,"FAIL %s\n",why);std::exit(1);}}
struct Resolver final:LogicalResolver {
 bool identify(const char*,RefKind,const void*,LogicalRef&,std::string&)override{return false;}
 bool validate(const char*,RefKind,const LogicalRef& r,std::string& e)const override{if(r.owner==17&&r.resource==0&&r.slot<100)return true;e="wrong stable incarnation or subobject";return false;}
 bool validateTyped(const FieldSchema& field,const LogicalRef& r,std::string& e)const override {return !field.targetType.empty()&&(!r.owner&&!r.resource&&!r.slot?field.nullable:validate(field.key.c_str(),field.reference,r,e));}
 bool resolve(const char*,RefKind,const LogicalRef&,void*&,std::string&)override{return false;}
 bool identifyHandle(const char*,RefKind,u32,LogicalRef&,std::string&)override{return false;}
 bool resolveHandle(const char*,RefKind,const LogicalRef&,u32&,std::string&)override{return false;}
};
void put(ActorFields& f,const std::string& k,ScalarKind kind,u64 bits){ActorField v;v.category=FieldCategory::Scalar;v.scalar=kind;v.bits=bits;f[k]=v;}
void add(ActorFields& f,const std::vector<FieldSchema>& s,const std::string& prefix){for(const auto& x:s){ActorField v;v.category=x.category;v.scalar=x.scalar;v.reference=x.reference;if(x.category!=FieldCategory::Scalar&&!x.nullable)v.target={17,0,1};f[prefix+x.key]=v;}}
void prepareInactive(ActorFields& f){for(auto it=f.begin();it!=f.end();)if(it->first.rfind("piki.inactiveStrong",0)==0)it=f.erase(it);else ++it;std::vector<FieldSchema> schema;std::string error;require(piki_schema(f,schema,error),error.c_str());for(auto&d:schema)if(d.key.rfind("piki.inactiveStrong",0)==0){ActorField v;v.category=d.category;v.scalar=d.scalar;v.reference=d.reference;f[d.key]=v;}}
ActorFields base(int state){
 ActorFields f;std::vector<FieldSchema> s;require(piki_state_schema(state,s),"registered state schema");add(f,s,"piki.fsm.");s.clear();
 put(f,"piki.fsm.current",ScalarKind::S32,state);put(f,"piki.fsm.last",ScalarKind::S32,0xffffffff);
 require(piki_action_schema(1,s),"top schema");add(f,s,"piki.action.");s.clear();
 put(f,"piki.action.type",ScalarKind::S32,1);put(f,"piki.action.child",ScalarKind::S16,0xffff);put(f,"piki.action.count",ScalarKind::S16,31);
 put(f,"piki.action.boredom.count",ScalarKind::S32,0);put(f,"piki.action.boredom.next",ScalarKind::S32,0);
 put(f,"piki.listenerExtras.upper.present",ScalarKind::Bool,0);put(f,"piki.listenerExtras.lower.present",ScalarKind::Bool,0);
 piki_runtime_schema(s);add(f,s,"piki.runtime.");put(f,"piki.runtime.mColor",ScalarKind::U16,1);
 s.clear();std::string error;
 put(f,"creature.search.capacity",ScalarKind::S16,0);put(f,"creature.search.count",ScalarKind::S16,0);put(f,"creature.search.last",ScalarKind::S32,0xffffffff);put(f,"creature.objectType",ScalarKind::S32,OBJTYPE_Piki);
 require(creature_schema(f,s,error),"complete Creature schema");add(f,s,"");put(f,"creature.search.last",ScalarKind::S32,0xffffffff);
 prepareInactive(f);return f;
}
bool valid(ActorFields f){Resolver resolver;ActorBytes bytes;std::string e;return encode_actor_fields(f,bytes,e)&&validate_piki(bytes,resolver,e);}
void node(ActorFields& f,const std::string& p,int type,int count,int child){std::vector<FieldSchema>s;require(piki_action_schema(type,s),"nested schema");add(f,s,p);put(f,p+"type",ScalarKind::S32,type);put(f,p+"count",ScalarKind::S16,count);put(f,p+"child",ScalarKind::S16,static_cast<u16>(child));}
}
int main(){
 for(int state=0;state<37;++state){if(state==32||state==34){std::vector<FieldSchema> s;require(!piki_state_schema(state,s),"unused state refused");continue;}require(valid(base(state)),"every registered state typed roundtrip");}
 for(int type=1;type<=46;++type){std::vector<FieldSchema>s;require(piki_action_schema(type,s),"all concrete action payloads represented");std::set<std::string> keys;for(auto& f:s)require(keys.insert(f.key).second,"no duplicate action fields");}
 auto original=base(0),f=original;
 put(f,"creature.objectType",ScalarKind::S32,1);require(!valid(f),"foreign Creature factory refused");
 f=original;put(f,"creature.search.capacity",ScalarKind::S16,4097);require(!valid(f),"Creature search allocation overflow refused");
 f=original;f.erase("creature.mHealth");require(!valid(f),"missing shared physics health refused");
 f=original;
 put(f,"piki.fsm.current",ScalarKind::S32,32);require(!valid(f),"unregistered current state refused");
 f=original;put(f,"piki.fsm.last",ScalarKind::S32,37);require(!valid(f),"invalid previous state refused");
 f=original;put(f,"piki.action.child",ScalarKind::S16,31);require(!valid(f),"child boundary refused");
 f=original;put(f,"piki.action.type",ScalarKind::S32,44);require(!valid(f),"wrong top dynamic type refused");
 f=original;put(f,"piki.action.boredom.count",ScalarKind::S32,31);require(!valid(f),"boredom allocation bound refused");
 f=original;put(f,"piki.runtime.mNumRoutePoints",ScalarKind::S16,2);put(f,"piki.runtime.mCurrRoutePoint",ScalarKind::S16,2);require(!valid(f),"route one-past-end refused");
 f=original;put(f,"piki.runtime.mNumRoutePoints",ScalarKind::S16,static_cast<u16>(-55));require(!valid(f),"pending route without async refused");put(f,"piki.runtime.mUseAsyncPathfinding",ScalarKind::Bool,1);require(valid(f),"real pending async sentinel accepted");
 f=original;put(f,"piki.runtime.mP2Purple",ScalarKind::Bool,1);put(f,"piki.runtime.mP2White",ScalarKind::Bool,1);require(!valid(f),"conflicting species refused");
 f=original;put(f,"piki.runtime.mColor",ScalarKind::U16,3);require(!valid(f),"legacy color out of bounds refused");
 f=original;f["piki.runtime.mNavi"].target={99,0,1};require(!valid(f),"foreign actor incarnation refused");
 f=base(28);f["piki.fsm.mNectar"].target={};require(!valid(f),"required nectar reference refused");
 f=original;f.erase("piki.runtime.mHappa");require(!valid(f),"missing field refused");
 f=original;put(f,"unexpected",ScalarKind::U32,0);require(!valid(f),"extra field refused");
 f=original;put(f,"piki.runtime.mDeathTimer",ScalarKind::F32,0x7fc00000);require(!valid(f),"NaN state refused");
 f=original;put(f,"piki.listenerExtras.upper.present",ScalarKind::Bool,1);put(f,"piki.listenerExtras.upper.length",ScalarKind::U8,0);require(!valid(f),"duplicate active supplemental action refused");
 f=original;put(f,"piki.action.child",ScalarKind::S16,21);node(f,"piki.action.childPayload.",44,0,0);prepareInactive(f);require(valid(f),"active Transport typed topology accepted");put(f,"piki.action.childPayload.mState",ScalarKind::U16,8);require(!valid(f),"Transport substate overflow refused");
 f=original;put(f,"piki.action.child",ScalarKind::S16,14);node(f,"piki.action.childPayload.",14,0,0);put(f,"piki.action.childPayload.mIsWaiting",ScalarKind::Bool,1);node(f,"piki.action.childPayload.selector.",8,4,-1);prepareInactive(f);require(valid(f),"Crowd standalone selector accepted");put(f,"piki.action.childPayload.selector.type",ScalarKind::S32,22);require(!valid(f),"wrong selector type refused");
 f=original;put(f,"piki.listenerExtras.upper.present",ScalarKind::Bool,1);put(f,"piki.listenerExtras.upper.length",ScalarKind::U8,1);put(f,"piki.listenerExtras.upper.path.0",ScalarKind::U8,0);node(f,"piki.listenerExtras.upper.payload.",38,0,0);
 ActorField listener;listener.category=FieldCategory::Reference;listener.reference=RefKind::AnimListener;listener.target={17,0,2};f["piki.listenerExtras.upper.listener"]=listener;f["piki.runtime.upperAnimation.mListener"]=listener;prepareInactive(f);require(valid(f),"inactive animation listener action payload accepted");
 f["piki.runtime.upperAnimation.mListener"].target.slot=3;require(!valid(f),"supplemental listener alias mismatch refused");
 f=original;put(f,"piki.runtime.path.capacity",ScalarKind::S32,3);put(f,"piki.runtime.mNumRoutePoints",ScalarKind::S16,3);put(f,"piki.runtime.mCurrRoutePoint",ScalarKind::S16,1);f["piki.runtime.mPathBuffers"].target={17,0,3};
 for(int i=0;i<3;++i){put(f,"piki.runtime.path."+std::to_string(i)+".waypoint",ScalarKind::S32,i);put(f,"piki.runtime.path."+std::to_string(i)+".direction",ScalarKind::U8,255);}require(valid(f),"complete typed path buffer accepted");
 put(f,"piki.runtime.path.2.waypoint",ScalarKind::S32,3);require(!valid(f),"waypoint one-past graph refused");
 int strongActions=0;for(int type=1;type<=46;++type){std::vector<FieldSchema> ds;require(piki_action_schema(type,ds),"action strength schema exists");for(auto& d:ds){if(d.strength==ReferenceStrength::StrongCreature){++strongActions;require(d.category==FieldCategory::Reference&&d.reference==RefKind::Creature,"strong action is Creature wrapper");}if(type==1&&d.key=="mTarget")require(d.strength==ReferenceStrength::Weak,"TopAction raw target stays weak");}}
 require(strongActions==20,"all twenty declared action SmartPtr slots classified");std::vector<FieldSchema> rs;piki_runtime_schema(rs);int strongRuntime=0;for(auto&d:rs){if(d.strength==ReferenceStrength::StrongCreature){++strongRuntime;require(d.key=="mLookAtCreature"||d.key=="_500","only runtime SmartPtr slots strong");}if(d.key=="mNavi")require(d.strength==ReferenceStrength::Weak,"raw captain reference weak");}require(strongRuntime==2,"both runtime SmartPtr slots classified");
 for(int state=0;state<37;++state){std::vector<FieldSchema> ds;if(piki_state_schema(state,ds))for(auto&d:ds)require(d.strength==ReferenceStrength::Weak,"Piki state raw references remain weak");}
 f=original;const std::string inactive="piki.inactiveStrong.3.mTarget";require(f.count(inactive),"inactive allocated Chase wrapper serialized");f[inactive].target={17,0,7};require(valid(f),"non-null inactive wrapper preserved");auto missing=f;missing.erase(inactive);require(!valid(missing),"missing inactive wrapper refused");auto foreign=f;foreign[inactive].target.owner=99;require(!valid(foreign),"inactive foreign incarnation refused");auto unknown=f;unknown["piki.inactiveStrong.99.mTarget"]=f[inactive];require(!valid(unknown),"foreign action index path refused");
 put(f,"piki.action.child",ScalarKind::S16,3);node(f,"piki.action.childPayload.",13,0,0);prepareInactive(f);require(!f.count(inactive),"active wrapper excluded from supplemental census");f["piki.action.childPayload.mTarget"].target={17,0,7};require(valid(f),"active wrapper remains represented once");f[inactive]=f["piki.action.childPayload.mTarget"];require(!valid(f),"duplicate active wrapper refused");
 f=original;put(f,"piki.listenerExtras.upper.present",ScalarKind::Bool,1);put(f,"piki.listenerExtras.upper.length",ScalarKind::U8,1);put(f,"piki.listenerExtras.upper.path.0",ScalarKind::U8,8);node(f,"piki.listenerExtras.upper.payload.",3,1,-1);ActorField andTarget;andTarget.category=FieldCategory::Reference;andTarget.reference=RefKind::Creature;f["piki.listenerExtras.upper.payload.andTarget"]=andTarget;f["piki.listenerExtras.upper.listener"]=listener;f["piki.runtime.upperAnimation.mListener"]=listener;prepareInactive(f);require(!f.count("piki.inactiveStrong.8.mOther"),"listener strong payload excluded from supplemental census");require(f.count("piki.inactiveStrong.8.0.mTarget"),"unselected listener child wrapper retained");require(valid(f),"listener strong payload with inactive child accepted");auto duplicate=f;duplicate["piki.inactiveStrong.8.mOther"]=f["piki.listenerExtras.upper.payload.mOther"];require(!valid(duplicate),"duplicate listener strong wrapper refused");
 auto strongCount=[](const ActorFields& x){std::vector<FieldSchema> ds;std::string e;require(piki_schema(x,ds,e),"complete strong census schema");int n=0;for(auto&d:ds)n+=d.strength==ReferenceStrength::StrongCreature;return n;};require(strongCount(original)==strongCount(f),"strong storage census independent of selected/listener paths");
 f=original;std::vector<FieldSchema> inactiveSchema;std::string inactiveError;require(piki_schema(f,inactiveSchema,inactiveError),"all-inactive schema");int inactiveCount=0;for(auto&d:inactiveSchema)if(d.key.rfind("piki.inactiveStrong",0)==0){++inactiveCount;require(d.nullable&&d.strength==ReferenceStrength::StrongCreature&&!d.targetType.empty(),"inactive wrapper nullable with exact strong type");f[d.key].target={};}require(inactiveCount==26,"all allocated action wrappers represented while top idle");require(valid(f),"all inactive wrappers null accepted");f["piki.inactiveStrong.21.mPellet"].target={17,0,8};require(valid(f),"inactive Transport retains nonnull Pellet wrapper");
 std::printf("PASS Piki typed schema controls %d\n",checks);return 0;
}
