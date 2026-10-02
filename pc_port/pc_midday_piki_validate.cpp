#include "pc_midday_creature.h"
#include "ObjType.h"
#include "pc_midday_actor_archive.h"
#include <cstring>
namespace pc_midday {
namespace {
bool bad(std::string& e,const char* s){if(e.empty())e=s;return false;}
bool number(const ActorFields& f,const std::string& key,ScalarKind kind,int& n,std::string& e){
 auto i=f.find(key);if(i==f.end()||i->second.category!=FieldCategory::Scalar||i->second.scalar!=kind)return bad(e,"missing Piki discriminator");
 auto bits=i->second.bits;
 switch(kind){
 case ScalarKind::Bool: if(bits>1)return bad(e,"invalid boolean");n=int(bits);break;
 case ScalarKind::S16: {u16 u=static_cast<u16>(bits);s16 v;std::memcpy(&v,&u,2);n=v;break;}
 case ScalarKind::S32: {u32 u=static_cast<u32>(bits);s32 v;std::memcpy(&v,&u,4);n=v;break;}
 default: if(bits>0x7fffffff)return bad(e,"Piki value exceeds bound");n=int(bits);break;
 }
 return true;
}
void append(std::vector<FieldSchema>& out,std::vector<FieldSchema> part,const std::string& prefix){for(auto& x:part){x.key=prefix+x.key;out.push_back(x);}}
void scalar(std::vector<FieldSchema>& out,const std::string& key,ScalarKind type){out.push_back(FieldSchema::value(key.c_str(),type));}
std::vector<int> children(int type){
 switch(type){
 case 1:return {38,45,18,13,24,30,37,21,3,41,25,32,31,15,14,23,40,17,19,11,28,44,27,12,34,36,39,46,43,4,10};
 case 3:return {26};case 8:case 22:return {45,9,6,7};case 16:return {30,24,35};
 case 30:case 31:return {24,29};case 32:return {24,2,33};case 41:return {24,42};default:return {};
 }
}
bool action(const ActorFields& f,std::vector<FieldSchema>& out,const std::string& p,int expected,unsigned depth,std::string& e){
 if(depth>16)return bad(e,"Piki action depth exceeded");
 int type=0,count=0,child=0;
 if(!number(f,p+"type",ScalarKind::S32,type,e)||type!=expected||type<1||type>46)return bad(e,"Piki action topology mismatch");
 auto graph=children(type);
 if(!number(f,p+"count",ScalarKind::S16,count,e)||count!=int(graph.size())||!number(f,p+"child",ScalarKind::S16,child,e))return bad(e,"Piki child allocation mismatch");
 if(child< -1||(count&&child>=count)||(!count&&child>0))return bad(e,"Piki action child out of bounds");
 scalar(out,p+"type",ScalarKind::S32);scalar(out,p+"count",ScalarKind::S16);scalar(out,p+"child",ScalarKind::S16);
 std::vector<FieldSchema> payload;if(!piki_action_schema(type,payload))return bad(e,"unknown Piki action schema");append(out,payload,p);
 if(type==44||type==17||type==4){int sub=0;int maximum=type==44?7:type==17?2:1;if(!number(f,p+"mState",ScalarKind::U16,sub,e)||sub<0||sub>maximum)return bad(e,"Piki action substate invalid");}
 if(type==3||type==16||type==30||type==31||type==32||type==41)out.push_back(FieldSchema::ref((p+"andTarget").c_str(),RefKind::Creature,true,"Creature",ReferenceOwnership::AnyLive));
 if(type==1){
  int used=0,next=0;if(!number(f,p+"boredom.count",ScalarKind::S32,used,e)||!number(f,p+"boredom.next",ScalarKind::S32,next,e)||used<0||used>30||next<0||next>=30)return bad(e,"Piki boredom bounds invalid");
  scalar(out,p+"boredom.count",ScalarKind::S32);scalar(out,p+"boredom.next",ScalarKind::S32);
  for(int i=0;i<used;++i){
   auto k=p+"boredom."+std::to_string(i)+".";int n=0;if(!number(f,k+"used",ScalarKind::U32,n,e)||n<0||n>5)return bad(e,"Piki boredom object bounds invalid");
   scalar(out,k+"id",ScalarKind::S32);scalar(out,k+"used",ScalarKind::U32);
   for(int j=0;j<n;++j){auto q=k+std::to_string(j)+".";scalar(out,q+"level",ScalarKind::F32);scalar(out,q+"object",ScalarKind::S32);scalar(out,q+"max",ScalarKind::Bool);}
  }
 }
 if(type==23 && !action(f,out,p+"selector.",22,depth+1,e))return false;
 if(type==14){int waiting=0;if(!number(f,p+"mIsWaiting",ScalarKind::Bool,waiting,e))return false;if(waiting&&!action(f,out,p+"selector.",8,depth+1,e))return false;}
 if(count>0 && child>=0 && !action(f,out,p+"childPayload.",graph[child],depth+1,e))return false;
 return true;
}
bool range(const ActorFields& f,const char* name,ScalarKind kind,int low,int high,std::string& e){int n=0;return number(f,std::string("piki.runtime.")+name,kind,n,e)&&(n>=low&&n<=high||bad(e,"Piki runtime enum out of range"));}
using Path=std::vector<int>;
void activePaths(const ActorFields& f,const std::string& p,int type,Path path,std::set<Path>& seen){
 seen.insert(path);std::string ignored;int child=-1;number(f,p+"child",ScalarKind::S16,child,ignored);auto graph=children(type);
 if(child>=0&&child<int(graph.size())){auto next=path;next.push_back(child);activePaths(f,p+"childPayload.",graph[child],next,seen);}
 int waiting=0;if(type==14)number(f,p+"mIsWaiting",ScalarKind::Bool,waiting,ignored);
 if(type==23||(type==14&&waiting)){path.push_back(31);activePaths(f,p+"selector.",type==23?22:8,path,seen);}
}
bool extras(const ActorFields& f,std::vector<FieldSchema>& schema,std::string& e){
 std::set<Path> seen;activePaths(f,"piki.action.",1,{},seen);
 for(const char* layer:{"upper","lower"}){
  std::string p=std::string("piki.listenerExtras.")+layer+".";int present=0;
  if(!number(f,p+"present",ScalarKind::Bool,present,e))return false;scalar(schema,p+"present",ScalarKind::Bool);
  if(!present)continue;
  int length=0;if(!number(f,p+"length",ScalarKind::U8,length,e)||length<0||length>16)return bad(e,"invalid listener action path length");
  scalar(schema,p+"length",ScalarKind::U8);Path path;int type=1;
  for(int i=0;i<length;++i){
   int edge=0;auto key=p+"path."+std::to_string(i);if(!number(f,key,ScalarKind::U8,edge,e))return false;
   scalar(schema,key,ScalarKind::U8);auto graph=children(type);
   if(edge==31){if(type!=14&&type!=23)return bad(e,"listener selector path invalid");type=type==14?8:22;}
   else {if(edge<0||edge>=int(graph.size()))return bad(e,"listener child path invalid");type=graph[edge];}
   path.push_back(edge);
  }
  if(seen.count(path))return bad(e,"listener extra duplicates active action");
  auto ref=f.find(p+"listener"),runtime=f.find(std::string("piki.runtime.")+layer+"Animation.mListener");
  if(ref==f.end()||runtime==f.end()||ref->second.category!=FieldCategory::Reference||runtime->second.category!=FieldCategory::Reference||ref->second.reference!=RefKind::AnimListener||runtime->second.reference!=RefKind::AnimListener)return bad(e,"missing listener reference");
  const auto& x=ref->second.target;const auto& y=runtime->second.target;
  if(x.owner!=y.owner||x.resource!=y.resource||x.slot!=y.slot)return bad(e,"listener supplemental reference mismatch");
  schema.push_back(FieldSchema::ref((p+"listener").c_str(),RefKind::AnimListener,false,"PaniAnimKeyListener",ReferenceOwnership::ActorSubobject));
  if(!action(f,schema,p+"payload.",type,0,e))return false;
  activePaths(f,p+"payload.",type,path,seen);
 }
 return true;
}
}
bool piki_schema(const ActorFields& f,std::vector<FieldSchema>& out,std::string& e){
 std::vector<FieldSchema> schema,state;
 int objectType=-1;
 if(!actor_i32(f,"creature.objectType",objectType,e)||objectType!=OBJTYPE_Piki)return bad(e,"Piki factory Creature type mismatch");
 if(!creature_schema(f,schema,e))return false;
 int current=0,last=0;if(!actor_i32(f,"piki.fsm.current",current,e)||!actor_i32(f,"piki.fsm.last",last,e)||!piki_state_schema(current,state))return bad(e,"invalid Piki FSM discriminator");
 std::vector<FieldSchema> unused;if(last!=-1&&!piki_state_schema(last,unused))return bad(e,"invalid last Piki state");
 int maximum=-1;ScalarKind subKind=ScalarKind::S32;
 switch(current){case 16:maximum=3;subKind=ScalarKind::U32;break;case 17:maximum=2;break;case 22:case 25:maximum=3;subKind=ScalarKind::U16;break;case 23:maximum=1;break;case 24:maximum=2;subKind=ScalarKind::U16;break;case 26:maximum=4;break;case 28:maximum=2;break;default:break;}
 if(maximum>=0){int sub=0;if(!number(f,"piki.fsm.mState",subKind,sub,e)||sub<0||sub>maximum)return bad(e,"Piki FSM substate invalid");}
 scalar(schema,"piki.fsm.current",ScalarKind::S32);scalar(schema,"piki.fsm.last",ScalarKind::S32);append(schema,state,"piki.fsm.");
 if(!action(f,schema,"piki.action.",1,0,e))return false;
 if(!extras(f,schema,e))return false;
 std::vector<FieldSchema> runtime;piki_runtime_schema(runtime);append(schema,runtime,"piki.runtime.");
 int capacity=0;if(!number(f,"piki.runtime.path.capacity",ScalarKind::S32,capacity,e)||capacity<0||capacity>32767)return bad(e,"Piki path capacity invalid");
 if(capacity)for(auto& field:schema)if(field.key=="piki.runtime.mPathBuffers")field.nullable=false;
 for(int i=0;i<capacity;++i){auto p=std::string("piki.runtime.path.")+std::to_string(i)+".";int waypoint=0;if(!number(f,p+"waypoint",ScalarKind::S32,waypoint,e)||waypoint< -1||waypoint>=capacity)return bad(e,"Piki path waypoint invalid");scalar(schema,p+"waypoint",ScalarKind::S32);scalar(schema,p+"direction",ScalarKind::U8);}
 if(!range(f,"mColor",ScalarKind::U16,0,2,e)||!range(f,"mHappa",ScalarKind::S32,0,2,e)||!range(f,"mMode",ScalarKind::U16,0,24,e)||!range(f,"mEmotion",ScalarKind::U8,0,10,e)||!range(f,"mActionState",ScalarKind::U8,0,2,e)||!range(f,"mFiredState",ScalarKind::U16,0,2,e))return false;
 int purple=0,white=0,bulbmin=0,color=0;
 if(!number(f,"piki.runtime.mP2Purple",ScalarKind::Bool,purple,e)||!number(f,"piki.runtime.mP2White",ScalarKind::Bool,white,e)||!number(f,"piki.runtime.mP2Bulbmin",ScalarKind::Bool,bulbmin,e)||!number(f,"piki.runtime.mColor",ScalarKind::U16,color,e))return false;
 // Experimental identity is separate from legacy mColor; pc_p2_make_purple
 // does not rewrite that backing color. Only incompatible identities conflict.
 if(purple+white+bulbmin>1)return bad(e,"inconsistent Pikmin species flags");
 int points=0,cursor=0,async=0;
 if(!number(f,"piki.runtime.mNumRoutePoints",ScalarKind::S16,points,e)||!number(f,"piki.runtime.mCurrRoutePoint",ScalarKind::S16,cursor,e)||!number(f,"piki.runtime.mUseAsyncPathfinding",ScalarKind::Bool,async,e))return false;
 // -55 is the native pending async request sentinel. Route resource resolver
 // validates actual waypoint inventory and content identity independently.
 if((points<0 && !(points==-55&&async))||points>capacity||cursor< -1||(points>0&&cursor>=points))return bad(e,"invalid Piki route cursor/count");
 for(const char* layer:{"upperAnimation.","lowerAnimation."}){
  int motion=0,play=0;
  if(!number(f,std::string("piki.runtime.")+layer+"mMotionIdx",ScalarKind::S32,motion,e)||motion< -1||motion>=90||!number(f,std::string("piki.runtime.")+layer+"mPlayState",ScalarKind::S32,play,e)||play<0||play>2)return bad(e,"invalid Piki animation state");
 }
 out.swap(schema);return true;
}
bool validate_piki(const ActorBytes& bytes,const LogicalResolver& resolver,std::string& e){ActorFields fields;std::vector<FieldSchema> schema;return decode_actor_fields(bytes,fields,e)&&piki_schema(fields,schema,e)&&validate_actor_fields(fields,schema,resolver,e);}
}
