#include "pc_midday_projectile.h"
#include <tuple>
namespace pc_midday {namespace {
bool bad(std::string& e,const char*m){if(e.empty())e=m;return false;}
void scalar(std::vector<FieldSchema>& s,const std::string& k,ScalarKind t){s.push_back(FieldSchema::value(k.c_str(),t));}
bool range(const ActorFields& f,const std::string& k,int max,std::string& e){int n=0;return actor_i32(f,k.c_str(),n,e)&&n>=0&&n<=max;}
bool policy(const ActorFields& f,std::vector<FieldSchema>& s,const std::string& p,bool rock,std::string& e){
 if(!range(f,p+"phase",rock?6:3,e))return bad(e,"projectile phase");
 scalar(s,p+"phase",ScalarKind::S32);
 if(!rock){
 if(!range(f,p+"config.variant",1,e))return bad(e,"projectile variant/motion");scalar(s,p+"config.variant",ScalarKind::S32);
 scalar(s,p+"config.moveSpeed",ScalarKind::F32);
 scalar(s,p+"config.searchRumbleSpeed",ScalarKind::F32);
 scalar(s,p+"config.turnSpeed",ScalarKind::F32);
 scalar(s,p+"config.maxTurnAngle",ScalarKind::F32);
 scalar(s,p+"config.attackDamage",ScalarKind::F32);
 scalar(s,p+"config.sightRadius",ScalarKind::F32);
 scalar(s,p+"config.collisionRadius",ScalarKind::F32);
 scalar(s,p+"config.health",ScalarKind::F32);
 scalar(s,p+"mFaceDir",ScalarKind::F32);
 scalar(s,p+"mTimer",ScalarKind::F32);
 scalar(s,p+"mScale",ScalarKind::F32);
 scalar(s,p+"mHealth",ScalarKind::F32);
 scalar(s,p+"mHoming",ScalarKind::Bool);
 scalar(s,p+"mHealthZeroed",ScalarKind::Bool);
 }else{
 if(!range(f,p+"motion",2,e))return bad(e,"projectile variant/motion");scalar(s,p+"motion",ScalarKind::S32);
 scalar(s,p+"config.fallSpeed",ScalarKind::F32);
 scalar(s,p+"config.fallOffset",ScalarKind::F32);
 scalar(s,p+"config.scaleUpRate",ScalarKind::F32);
 scalar(s,p+"config.sightRadius",ScalarKind::F32);
 scalar(s,p+"config.attackDamage",ScalarKind::F32);
 scalar(s,p+"config.collisionRadius",ScalarKind::F32);
 scalar(s,p+"config.health",ScalarKind::F32);
 scalar(s,p+"mScale",ScalarKind::F32);
 scalar(s,p+"mTimer",ScalarKind::F32);
 scalar(s,p+"mHealth",ScalarKind::F32);
 scalar(s,p+"mTimedAppear",ScalarKind::Bool);
 scalar(s,p+"mAtari",ScalarKind::Bool);
 scalar(s,p+"mUntargetable",ScalarKind::Bool);
 scalar(s,p+"mHardConstrained",ScalarKind::Bool);
 scalar(s,p+"mAnimating",ScalarKind::Bool);
 scalar(s,p+"mModelHidden",ScalarKind::Bool);
 scalar(s,p+"mCullable",ScalarKind::Bool);
 scalar(s,p+"mCullSound",ScalarKind::Bool);
 scalar(s,p+"mShadow",ScalarKind::Bool);
 scalar(s,p+"mShadowForced",ScalarKind::Bool);
 scalar(s,p+"mFallEffect",ScalarKind::Bool);
 scalar(s,p+"mDeadEffect",ScalarKind::Bool);
 scalar(s,p+"mAnimationCullingOff",ScalarKind::Bool);
 scalar(s,p+"mColliding",ScalarKind::Bool);
 scalar(s,p+"mMotionStopped",ScalarKind::Bool);
 }
 for(auto v:{"mPosition.","mVelocity.","mTargetVelocity."})for(auto x:{"x","y","z"})scalar(s,p+v+x,ScalarKind::F32);
 int phase=0;if(!actor_i32(f,(p+"phase").c_str(),phase,e))return false;
 s.push_back(FieldSchema::token64((p+"source").c_str(),RefKind::ProjectileToken,true,"Creature",ReferenceOwnership::AnyLive));
 s.push_back(FieldSchema::token64((p+"self").c_str(),RefKind::ProjectileToken,phase==0,rock?"P2RockHazard":"P2CannonStone",p=="projectile.body."?ReferenceOwnership::Self:ReferenceOwnership::ActorSubobject));
 return true;
}
void vec(std::vector<FieldSchema>&s,const std::string&p){for(auto x:{"x","y","z"})scalar(s,p+x,ScalarKind::F32);}
bool count(const ActorFields&f,std::vector<FieldSchema>&s,const std::string&k,u32 limit,u32&n,std::string&e){scalar(s,k,ScalarKind::U32);return actor_u32(f,k.c_str(),n,e)&&n<=limit;}
bool terminal(const ActorFields&f,std::vector<FieldSchema>&s,const std::string&p,std::string&e){scalar(s,p+"valid",ScalarKind::Bool);scalar(s,p+"reason",ScalarKind::S32);vec(s,p+"start.");vec(s,p+"end.");return range(f,p+"reason",4,e);}
bool shell(const ActorFields&f,std::vector<FieldSchema>&s,const std::string&p,std::string&e){scalar(s,p+"active",ScalarKind::Bool);scalar(s,p+"primary",ScalarKind::Bool);vec(s,p+"position.");vec(s,p+"velocity.");return terminal(f,s,p+"terminal.",e);}
bool volley(const ActorFields&f,std::vector<FieldSchema>&s,const std::string&p,std::string&e){
 u32 active=0,inactive=0,term=0,segments=0;
 if(!count(f,s,p+"activeCount",6,active,e)||!count(f,s,p+"inactiveCount",6,inactive,e)||active+inactive!=6||!count(f,s,p+"terminalCount",6,term,e)||!count(f,s,p+"segmentCount",6,segments,e))return bad(e,"volley list counts");
 std::set<u32> partition;
 for(u32 i=0;i<6;++i){auto q=p+"slot."+std::to_string(i)+".";u32 ai=0,ii=0,t=0,g=0;
 if(!shell(f,s,q+"node.",e)||!count(f,s,q+"activeIndex",5,ai,e)||!count(f,s,q+"inactiveIndex",5,ii,e)||!count(f,s,q+"terminal.slot",5,t,e)||!count(f,s,q+"segment.slot",5,g,e))return false;
 scalar(s,q+"primary",ScalarKind::Bool);scalar(s,q+"terminal.primary",ScalarKind::Bool);
 if(!terminal(f,s,q+"terminal.step.",e))return false;
 scalar(s,q+"segment.primary",ScalarKind::Bool);scalar(s,q+"segment.terminal",ScalarKind::Bool);vec(s,q+"segment.start.");vec(s,q+"segment.end.");
 for(int list=0;list<2;++list){if(i>=(list?inactive:active))continue;u32 slot=list?ii:ai;if(!partition.insert(slot).second)return bad(e,"volley duplicate pool membership");auto it=f.find(p+"slot."+std::to_string(slot)+".node.active");if(it==f.end()||it->second.bits!=u64(!list))return bad(e,"volley node/list activity mismatch");}
 }return true;
}
bool fleet(const ActorFields&f,std::vector<FieldSchema>&s,const std::string&p,std::string&e){
 scalar(s,p+"nextId",ScalarKind::U32);scalar(s,p+"graceIgnored",ScalarKind::U64);scalar(s,p+"strikesDeferred",ScalarKind::U64);std::set<u32>ids;
 for(int i=0;i<16;++i){auto q=p+"slot."+std::to_string(i)+".";scalar(s,q+"used",ScalarKind::Bool);scalar(s,q+"id",ScalarKind::U32);scalar(s,q+"hits",ScalarKind::S32);vec(s,q+"birth.");
 for(auto k:{"dirX","dirZ","vy","deadHold","travel","maxLateral","closest"})scalar(s,q+k,ScalarKind::F32);
 if(!policy(f,s,q+"stone.",false,e))return false;
 auto used=f.find(q+"used");u32 id=0;if(used==f.end()||!actor_u32(f,(q+"id").c_str(),id,e))return false;
 if(used->second.bits&&(!id||!ids.insert(id).second))return bad(e,"fleet live id collision");
 s.push_back(FieldSchema::token64((q+"owner").c_str(),RefKind::ProjectileToken,true,"Creature",ReferenceOwnership::AnyLive));u32 n=0;if(!count(f,s,q+"ledgerCount",4096,n,e))return bad(e,"fleet ledger bound");
 std::set<std::tuple<u64,u64,u32>> seen;
 for(u32 j=0;j<n;++j){auto key=q+"ledger."+std::to_string(j);s.push_back(FieldSchema::token64(key.c_str(),RefKind::ProjectileToken,false,"Creature",ReferenceOwnership::AnyLive));auto it=f.find(key);if(it==f.end()||!seen.emplace(it->second.target.owner,it->second.target.resource,it->second.target.slot).second)return bad(e,"fleet duplicate logical contact");}
 }return true;
}

bool bomb(const ActorFields&f,std::vector<FieldSchema>&s,const std::string&p,std::string&e){
 scalar(s,p+"phase",ScalarKind::S32);if(!range(f,p+"phase",5,e))return bad(e,"bomb phase");
 scalar(s,p+"config.gravityPerTick",ScalarKind::F32);
 scalar(s,p+"config.fuseHealth",ScalarKind::F32);
 scalar(s,p+"config.armLoopTicks",ScalarKind::S32);
 scalar(s,p+"config.bombRadius",ScalarKind::F32);
 scalar(s,p+"config.blastRadius",ScalarKind::F32);
 scalar(s,p+"config.blastHalfHeight",ScalarKind::F32);
 scalar(s,p+"config.tekiDamage",ScalarKind::F32);
 scalar(s,p+"config.naviPikiDamage",ScalarKind::F32);
 scalar(s,p+"config.ip02TriggerLimit",ScalarKind::S32);
 scalar(s,p+"mEscapeTicks",ScalarKind::S32);
 scalar(s,p+"mArmTicksRemaining",ScalarKind::S32);
 scalar(s,p+"mFuseHealthRemaining",ScalarKind::F32);
 scalar(s,p+"mDetonateDelayTicks",ScalarKind::S32);
 scalar(s,p+"mInductionCounter",ScalarKind::S32);
 scalar(s,p+"mHasBlast",ScalarKind::Bool);
 vec(s,p+"position.");vec(s,p+"velocity.");vec(s,p+"blast.center.");
 scalar(s,p+"blast.radius",ScalarKind::F32);
 scalar(s,p+"blast.halfHeight",ScalarKind::F32);
 scalar(s,p+"blast.tekiDamage",ScalarKind::F32);
 scalar(s,p+"blast.naviPikiDamage",ScalarKind::F32);
 scalar(s,p+"blast.knockbackNavi",ScalarKind::F32);
 scalar(s,p+"blast.knockbackPiki",ScalarKind::F32);
 scalar(s,p+"blast.hasCarrier",ScalarKind::Bool);
 scalar(s,p+"blast.carrierValid",ScalarKind::Bool);
 for(auto key:{"carrier","blast.carrier"})s.push_back(FieldSchema::token64((p+key).c_str(),RefKind::ProjectileToken,true,"Creature",ReferenceOwnership::AnyLive));return true;
}

}
bool projectile_schema(const ActorFields& f,ProjectileType expected,std::vector<FieldSchema>& out,std::string& e){
 int version=0,kind=0;if(!actor_i32(f,"projectile.version",version,e)||!actor_i32(f,"projectile.kind",kind,e)||version!=1||kind!=int(expected)||kind<1||kind>9)return bad(e,"projectile factory/version mismatch");
 std::vector<FieldSchema>s;scalar(s,"projectile.version",ScalarKind::S32);scalar(s,"projectile.kind",ScalarKind::S32);
 bool rock=kind==2||kind==4;
 if(kind<=2){if(!policy(f,s,"projectile.body.",rock,e))return false;}
 else if(kind<=4) {int capacity=0;if(!actor_i32(f,"projectile.body.capacity",capacity,e)||capacity<0||capacity>16)return bad(e,"projectile capacity");scalar(s,"projectile.body.capacity",ScalarKind::S32);
 for(int i=0;i<16;++i){auto p=std::string("projectile.body.slot.")+std::to_string(i)+".";scalar(s,p+"used",ScalarKind::Bool);auto it=f.find(p+"used");if(it==f.end()||(i>=capacity&&it->second.bits))return bad(e,"projectile outside capacity");if(!policy(f,s,p,rock,e))return false;}}
 if(kind==5&&!fleet(f,s,"projectile.body.",e))return false;
 if(kind==6&&!shell(f,s,"projectile.body.",e))return false;
 if(kind==7&&!volley(f,s,"projectile.body.",e))return false;
 if(kind==8&&!bomb(f,s,"projectile.body.",e))return false;
 if(kind==9){int cap=0;if(!actor_i32(f,"projectile.body.capacity",cap,e)||cap<0||cap>16)return bad(e,"bomb capacity");scalar(s,"projectile.body.capacity",ScalarKind::S32);
 for(int i=0;i<16;++i){auto p=std::string("projectile.body.slot.")+std::to_string(i)+".";scalar(s,p+"used",ScalarKind::Bool);auto it=f.find(p+"used");if(it==f.end()||(i>=cap&&it->second.bits))return bad(e,"bomb outside capacity");if(!bomb(f,s,p,e))return false;}}
 out.swap(s);return true;
}
bool projectile_nested_schema(const ActorFields& f,ProjectileType type,const std::string& prefix,std::vector<FieldSchema>& out,std::string& e){
 if(prefix.empty()||prefix.back()=='.')return bad(e,"invalid nested projectile prefix");
 ActorFields nested;const auto start=prefix+".";
 for(const auto& entry:f)if(entry.first.compare(0,start.size(),start)==0)nested.emplace(entry.first.substr(start.size()),entry.second);
 std::vector<FieldSchema> fields;if(!projectile_schema(nested,type,fields,e))return false;
 for(auto& field:fields){field.key=start+field.key;if(field.ownership==ReferenceOwnership::Self)field.ownership=ReferenceOwnership::ActorSubobject;out.push_back(field);}return true;
}
bool validate_projectile(const ActorBytes& b,ProjectileType t,const LogicalResolver& r,std::string& e){ActorFields f;std::vector<FieldSchema>s;return decode_actor_fields(b,f,e)&&projectile_schema(f,t,s,e)&&validate_actor_fields(f,s,r,e);}
}
