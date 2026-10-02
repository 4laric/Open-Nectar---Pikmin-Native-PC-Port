#include "pc_midday_enemy.h"
#include "pc_p2_chappy_mouth.h"
#include "pc_p2_sampled_clock.h"
#include <cmath>
#include <cstring>
namespace pc_midday {
namespace {
const EnemyRegistration registrations[]={
{0,"TEKI_Frog","TaiOtimotiStrategy"},
{1,"TEKI_Iwagen","TaiIwagenStrategy"},
{2,"TEKI_Iwagon","TaiIwagonStrategy"},
{3,"TEKI_Chappy","TaiChappyStrategy"},
{4,"TEKI_Swallow","TaiSwallowStrategy"},
{5,"TEKI_Mizigen","TaiMizigenStrategy"},
{6,"TEKI_Qurione","TaiMizinkoStrategy"},
{7,"TEKI_Palm","TaiPalmStrategy"},
{8,"TEKI_Collec","TaiCollecStrategy"},
{9,"TEKI_Kinoko","TaiKinokoStrategy"},
{10,"TEKI_Shell","TaiShellStrategy"},
{11,"TEKI_Napkid","TaiNapkidStrategy"},
{12,"TEKI_Hollec","TaiHollecStrategy"},
{13,"TEKI_Pearl","TaiPearlStrategy"},
{14,"TEKI_Rocpe","TaiPearlStrategy"},
{31,"TEKI_Chappb","TaiChappyStrategy"},
{32,"TEKI_Swallob","TaiSwallowStrategy"},
{33,"TEKI_Frow","TaiOtimotiStrategy"},
{30,"TEKI_Namazu","TaiChappyStrategy"},
{35,"TEKI_P2Demon","TaiChappyStrategy"},
{15,"TEKI_Tank","TAItankStrategy"},
{16,"TEKI_Mar","TAImarStrategy"},
{17,"TEKI_Beatle","TAIbeatleStrategy"},
{18,"TEKI_KabekuiA","TAIkabekuiAStrategy"},
{19,"TEKI_KabekuiB","TAIkabekuiBStrategy"},
{20,"TEKI_KabekuiC","TAIkabekuiCStrategy"},
{21,"TEKI_Tamago","TAItamagoStrategy"},
{22,"TEKI_Dororo","TAIdororoStrategy"},
{23,"TEKI_HibaA","TAIhibaAStrategy"},
{24,"TEKI_Miurin","TAImiurinStrategy"},
{25,"TEKI_Otama","TAIotamaStrategy"}
};
bool number(const ActorFields& f,const char* key,float& value,std::string& e){auto i=f.find(key);if(i==f.end()||i->second.category!=FieldCategory::Scalar||i->second.scalar!=ScalarKind::F32){e="missing float";return false;}u32 bits=static_cast<u32>(i->second.bits);std::memcpy(&value,&bits,4);return std::isfinite(value);}
void scalar(std::vector<FieldSchema>& o,const std::string& k,ScalarKind t){o.push_back(FieldSchema::value(k.c_str(),t));}
void vector(std::vector<FieldSchema>& o,const std::string& k){for(const char* c:{".x",".y",".z"})scalar(o,k+c,ScalarKind::F32);}
void ref(std::vector<FieldSchema>& o,const std::string& k,RefKind t,bool n,const char* type,ReferenceOwnership owner=ReferenceOwnership::AnyLive){o.push_back(FieldSchema::ref(k.c_str(),t,n,type,owner));}
}
const EnemyRegistration* enemy_registration(int id){for(const auto& r:registrations)if(r.id==id)return &r;return nullptr;}
void enemy_animation_schema(const std::string& p,std::vector<FieldSchema>& out) {
 for(const char* k:{"mPlayState","mCurrentAnimID","mStartKeyIndex","mEndKeyIndex","mCurrentKeyIndex","mMotionIdx"})scalar(out,p+k,ScalarKind::S32);
 scalar(out,p+"mPreviousKeyIndex",ScalarKind::U32);scalar(out,p+"mAnimationCounter",ScalarKind::F32);scalar(out,p+"mIsFinished",ScalarKind::Bool);
 for(const auto& entry:{std::pair<const char*,const char*>{"mMgr","AnimMgr"},{"mContext","AnimContext"},{"mAnimInfo","AnimInfo"},{"mMotionTable","PaniMotionTable"}})ref(out,p+entry.first,RefKind::Animation,false,entry.second,ReferenceOwnership::Content);
 ref(out,p+"mListener",RefKind::AnimListener,true,"PaniAnimKeyListener",ReferenceOwnership::ActorSubobject);
}
bool enemy_base_schema(const ActorFields& f,std::vector<FieldSchema>& out,std::string& error) {
 const std::string p="enemy.base.";int type=0,count=0,capacity=0,state=0;
 if(!actor_i32(f,(p+"type").c_str(),type,error)||!enemy_registration(type)){error="unregistered enemy family";return false;}
 if(!actor_i32(f,(p+"state").c_str(),state,error)||state<0){error="invalid enemy state";return false;}
 if(!actor_i32(f,(p+"mRouteWayPointMax").c_str(),capacity,error)||capacity<0||capacity>4096||!actor_i32(f,(p+"mRouteWayPointCount").c_str(),count,error)||count<0||count>capacity){error="invalid enemy route topology";return false;}
 auto stateRef=f.find(p+"strategyState");
 if(stateRef!=f.end()&&(stateRef->second.target.owner!=0||stateRef->second.target.resource==0||stateRef->second.target.slot!=static_cast<u32>(state))){error="enemy state/catalog identity mismatch";return false;}
 scalar(out,p+"type",ScalarKind::S32);scalar(out,p+"state",ScalarKind::S32);ref(out,p+"strategyState",RefKind::Action,false,"TaiState",ReferenceOwnership::Content);
#define FIELD(k,x) scalar(out,p+#x,ScalarKind::k);
#define VECTOR(x) vector(out,p+#x);
#include "pc_midday_enemy_base_fields.inc"
#undef FIELD
#undef VECTOR
 scalar(out,p+"mRouteWayPointMax",ScalarKind::S32);scalar(out,p+"mRouteWayPointCount",ScalarKind::S32);
 for(int i=0;i<count;++i)ref(out,p+"route."+std::to_string(i),RefKind::WayPoint,false,"WayPoint",ReferenceOwnership::Content);
 scalar(out,p+"routeGraph",ScalarKind::U32);
 for(int i=0;i<5;++i)scalar(out,p+"timer."+std::to_string(i),ScalarKind::F32);
 for(int i=0;i<4;++i){out.push_back(FieldSchema::ref((p+"target."+std::to_string(i)).c_str(),RefKind::Creature,true,"Creature",ReferenceOwnership::AnyLive,"",ReferenceStrength::StrongCreature));ref(out,p+"particle."+std::to_string(i),RefKind::ParticleGenerator,true,"zen::particleGenerator",ReferenceOwnership::ActorSubobject);}
 for(int i=0;i<8;++i)scalar(out,p+"corpseJoint."+std::to_string(i),ScalarKind::S32);
 ref(out,p+"pellet",RefKind::Creature,true,"Pellet");enemy_animation_schema(p+"animation.",out);
 for(const char* k:{"phase","frequency","amplitude"})scalar(out,p+"vibration."+k,ScalarKind::F32);
 vector(out,p+"personality.position");vector(out,p+"personality.nest");scalar(out,p+"personality.facing",ScalarKind::F32);
 scalar(out,p+"personality.pelletKind",ScalarKind::S32);scalar(out,p+"personality.pelletColor",ScalarKind::S32);scalar(out,p+"personality.id",ScalarKind::U32);
 for(int i=0;i<5;++i){scalar(out,p+"personality.int."+std::to_string(i),ScalarKind::S32);scalar(out,p+"personality.float."+std::to_string(i),ScalarKind::F32);}
 return true;
}
bool enemy_frog_schema(const ActorFields& f,std::vector<FieldSchema>& out,std::string& error) {
 const std::string p="enemy.p2.frog.";bool present=false;
 auto it=f.find(p+"present");if(it==f.end()||it->second.category!=FieldCategory::Scalar||it->second.scalar!=ScalarKind::Bool||it->second.bits>1){error="invalid frog presence";return false;}present=it->second.bits!=0;
 scalar(out,p+"present",ScalarKind::Bool);if(!present)return true;
 int state=0,clip=0,kind=0;
 if(!actor_i32(f,(p+"state").c_str(),state,error)||state<0||state>9||!actor_i32(f,(p+"clip").c_str(),clip,error)||clip<0||clip>8||!actor_i32(f,(p+"kind").c_str(),kind,error)||kind<0||kind>1){error="invalid frog discriminator";return false;}
 for(const char* key:{"stateTime","phase","flight.elapsed"}){float value=0;if(!number(f,(p+key).c_str(),value,error)||value<0){error="invalid frog clock";return false;}}
 scalar(out,p+"state",ScalarKind::S32);scalar(out,p+"clip",ScalarKind::S32);
#define FIELD(k,x) scalar(out,p+#x,ScalarKind::k);
#define VECTOR(x) vector(out,p+#x);
#include "pc_midday_enemy_frog_fields.inc"
#undef FIELD
#undef VECTOR
 out.push_back(FieldSchema::handle((p+"generator").c_str(),RefKind::Generator,false,"Generator",ReferenceOwnership::Content));scalar(out,p+"bitter",ScalarKind::Bool);scalar(out,p+"pressing",ScalarKind::Bool);return true;
}

bool enemy_kochappy_schema(const ActorFields& f,std::vector<FieldSchema>& out,std::string& error) {
 const std::string p="enemy.p2.kochappy.";
 auto it=f.find(p+"present");if(it==f.end()||it->second.category!=FieldCategory::Scalar||it->second.scalar!=ScalarKind::Bool||it->second.bits>1){error="invalid Kochappy presence";return false;}
 scalar(out,p+"present",ScalarKind::Bool);if(!it->second.bits)return true;
 int state=0,last=0,source=0;
 if(!actor_i32(f,(p+"state").c_str(),state,error)||state<0||state>=9||!actor_i32(f,(p+"returnState").c_str(),last,error)||last<0||last>=9||!actor_i32(f,(p+"sourceId").c_str(),source,error)||(source!=1&&source!=44)){error="invalid Kochappy discriminator";return false;}
 float time=0;if(!number(f,(p+"stateTime").c_str(),time,error)||time<0){error="invalid Kochappy state clock";return false;}
 scalar(out,p+"state",ScalarKind::S32);scalar(out,p+"returnState",ScalarKind::S32);
#define FIELD(k,x) scalar(out,p+#x,ScalarKind::k);
#define VECTOR(x) vector(out,p+#x);
#include "pc_midday_enemy_kochappy_fields.inc"
#undef FIELD
#undef VECTOR
 for(int i=0;i<p2chappymouth::MaxSlots;++i)ref(out,p+"mouth."+std::to_string(i),RefKind::Creature,true,"Creature");
 return true;
}

void enemy_subobject_schema(std::vector<FieldSchema>& out) {
 const std::string p="enemy.subobjects.";
 for(const char* k:{"status","tableIndex","mapCode"})scalar(out,p+k,ScalarKind::S32);
 for(const char* k:{"frame","frameMax","speed","turnAngle","dororoGravity","dororoBark","attack.time","attack.duration","attack.radius","attack.range","attack.damage","cone.angle"})scalar(out,p+k,ScalarKind::F32);
 for(const char* k:{"bite","runAway","stay","flying","timer","choke","footEffect","attack.moving"})scalar(out,p+k,ScalarKind::U32);
 ref(out,p+"work",RefKind::Creature,true,"WorkObject");
 for(int i=0;i<4;++i)scalar(out,p+"foot."+std::to_string(i),ScalarKind::F32);
 for(int i=0;i<8;++i)ref(out,p+"particle."+std::to_string(i),RefKind::ParticleGenerator,true,"zen::particleGenerator",ReferenceOwnership::ActorSubobject);
 for(const char* k:{"attack.position","attack.velocity","attack.direction"})vector(out,p+k);
 ref(out,p+"attack.owner",RefKind::Creature,true,"Teki",ReferenceOwnership::Self);
 for(const char* k:{"attack.emitter1","attack.emitter2"})ref(out,p+k,RefKind::ParticleGenerator,true,"zen::particleGenerator",ReferenceOwnership::ActorSubobject);
 ref(out,p+"attack.callback",RefKind::Action,false,"TAIeffectAttackEventCallBack",ReferenceOwnership::ActorSubobject);
 for(const char* k:{"cone.param","cylinder.param","event.param"})ref(out,p+k,RefKind::Action,true,"TAIeffectAttackParam",ReferenceOwnership::ActorSubobject);
 for(const char* k:{"accel","parabola","circle","wave"}){scalar(out,p+k+".options",ScalarKind::S32);ref(out,p+k+".condition",RefKind::Action,true,"PeveCondition",ReferenceOwnership::ActorSubobject);ref(out,p+k+".position",RefKind::Action,true,"NVector3fIO",ReferenceOwnership::ActorSubobject);}
 for(const char* k:{"accel","parabola"})for(const char* role:{"velocity","acceleration"})ref(out,p+k+"."+role,RefKind::Action,true,"NVector3fIO",ReferenceOwnership::ActorSubobject);
 vector(out,p+"parabola.gravity");vector(out,p+"parabola.velocityValue");scalar(out,p+"parabola.velocityLimit",ScalarKind::F32);
 for(const char* k:{"mAngle","mPositionLerpFactor","mRadius","mHeightOffset","mAngularSpeed","mTimeCondition.mCurrTime","mTimeCondition.mLimit"})scalar(out,p+"circle."+k,ScalarKind::F32);
 ref(out,p+"circle.center",RefKind::Action,true,"NVector3fIO",ReferenceOwnership::ActorSubobject);
 vector(out,p+"wave.velocity");for(const char* k:{"mOffset","mAmplitude","mStartingTheta","mAngularVelocity","mTheta"})scalar(out,p+"wave."+k,ScalarKind::F32);
}

bool enemy_stun_schema(const ActorFields& f,std::vector<FieldSchema>& out,std::string& error) {
 const std::string p="enemy.p2.purpleStun.";auto it=f.find(p+"present");
 if(it==f.end()||it->second.category!=FieldCategory::Scalar||it->second.scalar!=ScalarKind::Bool||it->second.bits>1){error="invalid stun presence";return false;}scalar(out,p+"present",ScalarKind::Bool);if(!it->second.bits)return true;
 int phase=0;float duration=0,elapsed=0;
 if(!actor_i32(f,(p+"phase").c_str(),phase,error)||phase<0||phase>2||!number(f,(p+"fitDuration").c_str(),duration,error)||duration<=0||!number(f,(p+"fitElapsed").c_str(),elapsed,error)||elapsed<0){error="invalid stun state or timer";return false;}
 out.push_back(FieldSchema::token64((p+"lifetime").c_str(),RefKind::Creature,false,"BTeki",ReferenceOwnership::Self));
 out.push_back(FieldSchema::token64((p+"targetLifetime").c_str(),RefKind::Creature,phase==0,"BTeki",ReferenceOwnership::Self));
 scalar(out,p+"phase",ScalarKind::S32);scalar(out,p+"fitDuration",ScalarKind::F32);scalar(out,p+"fitElapsed",ScalarKind::F32);scalar(out,p+"bounceUpdates",ScalarKind::U32);return true;
}

bool enemy_catfish_clock_valid(const ActorFields& f,const p2sampled::Clip& clip,std::string& error) {
 const std::string p="enemy.p2.catfish.clock.";p2sampled::Clock::SavedState s{};bool receiver=false;
 auto bits=[&](const char* key,ScalarKind kind,u64& value){auto it=f.find(p+key);if(it==f.end()||it->second.category!=FieldCategory::Scalar||it->second.scalar!=kind||(kind==ScalarKind::Bool&&it->second.bits>1)){error="invalid Catfish clock field";return false;}value=it->second.bits;return true;};
 u64 value=0;if(!bits("clock.frame",ScalarKind::F64,value))return false;std::memcpy(&s.frame,&value,8);
 if(!bits("clock.generation",ScalarKind::U64,value))return false;s.generation=value;
 if(!bits("clock.cycle",ScalarKind::U64,value))return false;s.cycle=value;
 if(!bits("clock.active",ScalarKind::Bool,value))return false;s.active=value;
 if(!bits("clock.entry",ScalarKind::Bool,value))return false;s.entry=value;
 if(!bits("clock.paused",ScalarKind::Bool,value))return false;s.paused=value;
 if(!bits("active",ScalarKind::Bool,value))return false;receiver=value;
 if((receiver&&!s.active)||!p2sampled::Clock::validState(clip,s)){error="Catfish clock does not match resolved content";return false;}
 return true;
}

bool enemy_catfish_schema(const ActorFields& f,std::vector<FieldSchema>& out,std::string& error) {
 const std::string p="enemy.p2.catfish.";auto present=f.find(p+"present");
 if(present==f.end()||present->second.category!=FieldCategory::Scalar||present->second.scalar!=ScalarKind::Bool||present->second.bits>1){error="invalid Catfish presence";return false;}scalar(out,p+"present",ScalarKind::Bool);if(!present->second.bits)return true;
 int state=0,clip=0;u32 consumed=0;
 if(!actor_i32(f,(p+"state").c_str(),state,error)||state<0||state>7||!actor_i32(f,(p+"clip").c_str(),clip,error)||clip<0||clip>6||!actor_u32(f,(p+"consumed.count").c_str(),consumed,error)||consumed>2){error="invalid Catfish discriminator";return false;}
 scalar(out,p+"state",ScalarKind::S32);scalar(out,p+"clip",ScalarKind::S32);
 for(const char* key:{"stateTime","heading","phase","logTimer"})scalar(out,p+key,ScalarKind::F32);
 for(const char* key:{"wanderValid","deadLogged","escaped","clock.active","clock.clock.active","clock.clock.entry","clock.clock.paused"})scalar(out,p+key,ScalarKind::Bool);
 vector(out,p+"home");vector(out,p+"wanderTarget");scalar(out,p+"rng",ScalarKind::U32);scalar(out,p+"sfxState",ScalarKind::S32);
 for(int i=0;i<2;++i)ref(out,p+"mouth."+std::to_string(i),RefKind::Creature,true,"Piki");
 scalar(out,p+"consumed.count",ScalarKind::U32);for(u32 i=0;i<consumed;++i)ref(out,p+"consumed."+std::to_string(i),RefKind::Creature,false,"Piki");
 if(consumed==2){auto a=f.find(p+"consumed.0"),b=f.find(p+"consumed.1");if(a!=f.end()&&b!=f.end()&&a->second.target.owner==b->second.target.owner&&a->second.target.resource==b->second.target.resource&&a->second.target.slot==b->second.target.slot){error="duplicate consumed actor";return false;}}
 auto frameIt=f.find(p+"clock.clock.frame");double frame=0;
 if(frameIt==f.end()||frameIt->second.scalar!=ScalarKind::F64){error="missing Catfish clock frame";return false;}
 auto frameBits=frameIt->second.bits;std::memcpy(&frame,&frameBits,8);
 if(!std::isfinite(frame)||frame<0){error="invalid Catfish source frame";return false;}
 for(const char* key:{"stateTime","phase"}){float v=0;if(!number(f,(p+key).c_str(),v,error)||v<0){error="invalid Catfish state clock";return false;}}
 scalar(out,p+"clock.clock.frame",ScalarKind::F64);scalar(out,p+"clock.clock.generation",ScalarKind::U64);scalar(out,p+"clock.clock.cycle",ScalarKind::U64);
 auto active=f.find(p+"clock.clock.active");bool running=active!=f.end()&&active->second.bits!=0;
 auto receiver=f.find(p+"clock.active");
 if(receiver!=f.end()&&receiver->second.bits&&!running){error="active Catfish receiver lacks active clock";return false;}
 if(running){auto generation=f.find(p+"clock.clock.generation");if(generation==f.end()||generation->second.scalar!=ScalarKind::U64||!generation->second.bits){error="active Catfish clock has no generation";return false;}}
 ref(out,p+"clock.content",RefKind::Animation,!running,"p2sampled::Clip",ReferenceOwnership::Content);
 return true;
}

}
