#include "pc_p2_original_captain_cstick.h"
#include <cmath>
#include <limits>
#include <set>
#include <sstream>
namespace p2original {namespace captain {namespace cstick {
namespace {
constexpr float pi=3.14159265358979323846f;
bool fail(std::string& e,const char* s){e=s;return false;}
bool finite(Vec3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
Vec3 vec(control::Vec3 v){return {v.x,v.y,v.z};}
float length(Vec3 v){return std::sqrt(v.x*v.x+v.y*v.y+v.z*v.z);}
Vec3 normalize(Vec3 v){float n=length(v);if(n>0){v.x/=n;v.y/=n;v.z/=n;}return v;}
float round(float a){if(a<0)a+=2*pi;if(a>=2*pi)a-=2*pi;return a;}
float distance(float a,float b){float d=round(a-b);if(d>=pi)d=-round(2*pi-d);return d;}
Command pose(CommandKind kind,Vec3 p,float a,Vec3 v,float scale=1){Command c;c.kind=kind;c.position=p;c.angle=a;c.velocity=v;c.scale=scale;return c;}
Command refresh(unsigned count,float strength){Command c;c.kind=CommandKind::Refresh;c.count=count;c.strength=strength;return c;}
}
bool parseParameters(const std::string& bytes,Parameters& out,std::string& e){
 control::Params authenticated;if(!control::parseParameters(bytes,authenticated,e))return false;
 Parameters next;std::istringstream lines(bytes);std::string line;unsigned foundWait=0,foundChange=0;
 while(std::getline(lines,line)){
  auto begin=line.find('{'),end=line.find('}');if(begin==std::string::npos||end==std::string::npos||end<=begin)continue;
  auto key=line.substr(begin+1,end-begin-1);if(key!="p039"&&key!="p040")continue;
  std::istringstream fields(line.substr(end+1));int type;float value;
  if(!(fields>>type>>value)||type!=4||!std::isfinite(value))return fail(e,"invalid authored C-stick distance parameter");
  if(key=="p039"){next.wait_=value;++foundWait;}else{next.change_=value;++foundChange;}
 }
 if(foundWait!=1||foundChange!=1||next.wait_<0||next.change_<=next.wait_)return fail(e,"missing/invalid authored C-stick distance ranges");
 next.authenticated_=true;out=next;e.clear();return true;
}
void onInit(State& state){state.scaleTimer=0;state.angle=0;}
bool prepare(const Parameters& p,const Input& in,const State& old,Plan& out,std::string& e){
 e.clear();if(!p.authenticated())return fail(e,"missing authenticated C-stick parameters");
 if(!finite(vec(in.camera.side))||!finite(vec(in.camera.up))||!finite(vec(in.camera.view))||in.slotCount>100)
  return fail(e,"invalid actual source C-stick camera/plate census");
 float x=0,z=0;
 if(in.controller){
  if(!in.demoInactive)return fail(e,"missing actual MoviePlayer demo state for C-stick");
  if(*in.demoInactive){if(!std::isfinite(in.stickX)||!std::isfinite(in.stickY)||std::fabs(in.stickX)>1||std::fabs(in.stickY)>1)return fail(e,"invalid actual hardware sub-stick");x=-in.stickX;z=in.stickY;}
 }
 Vec3 side=vec(in.camera.side);side.y=0;side=normalize(side);
 auto view=in.camera.up.y>in.camera.view.y?in.camera.view:in.camera.up;
 Vec3 forward=normalize({view.x,0,view.z});Vec3 transformed{side.x*x+forward.x*z,0,side.z*x+forward.z*z};
 float magnitude=length(transformed);if(!std::isfinite(magnitude))return fail(e,"source C-stick projection overflow");
 Plan next;next.state=old;next.state.position=Vec3{};next.state.commandOn2=false;
 if(magnitude>.05f){
  if(!old.scaleTimer||!in.plateAngle||!std::isfinite(*in.plateAngle)||!finite(in.position)||!finite(in.velocity))return fail(e,"missing actual active C-stick scale/plate pose");
  float angle=std::atan2(transformed.x,transformed.z),plate=*in.plateAngle;
  const float alignment=std::sin(angle)*std::sin(plate)+std::cos(angle)*std::cos(plate);
  angle=round(alignment>std::cos(2*pi/3)?distance(angle,plate)*.4f+plate:angle);
  next.state.angle=angle;next.state.commandOn2=true;next.state.position=transformed;
  float strength=(magnitude-.05f)/.95f;
  strength=strength>=.9f?1:(strength/.9f)*.6f;
  next.commands.push_back(refresh(in.slotCount,strength));
  auto timer=*old.scaleTimer;if(timer<40)++timer;next.state.scaleTimer=timer;
  next.commands.push_back(pose(CommandKind::SetPos,in.position,angle,in.velocity,timer>=40?3:1));
  next.state.neutralTurn=0;next.state.commandOn1=false;next.state.targetVector=transformed;next.resetSceneAnimationTimer=true;
 }else{
  if(!old.neutralTurn)return fail(e,"source neutral C-stick reads unknown _2FC");
  if(!std::isfinite(in.face))return fail(e,"invalid actual source C-stick face");
  next.state.scaleTimer=0;
  if(!*old.neutralTurn){
   next.state.commandOn1=true;
   if(!in.targetVelocity||!finite(*in.targetVelocity))return fail(e,"missing whole actual target velocity for neutral C-stick");
   const float speed=length(*in.targetVelocity);if(!std::isfinite(speed))return fail(e,"source target velocity length overflow");
   if(speed<50&&!in.sourceState)return fail(e,"missing actual current source state for neutral C-stick");
   if(speed<50&&*in.sourceState!=StateId::ThrowWait){
    if(!old.angle||!std::isfinite(*old.angle)||!finite(in.position)||!finite(in.velocity))return fail(e,"missing neutral retained C-stick angle/pose");
    next.commands.push_back(pose(CommandKind::SetPos,in.position,*old.angle,in.velocity));
   }else next.state.neutralTurn=1;
  }
  if(!finite(in.position)||!finite(in.velocity))return fail(e,"missing actual post-refresh source pose");
  next.commands.push_back(refresh(in.slotCount,0));
  NeutralContinuation continuation;continuation.state_=next.state;continuation.position_=in.position;continuation.velocity_=in.velocity;continuation.face_=in.face;continuation.issued_=true;
  // TargetVector is assigned at the source function's end, after the suffix.
  continuation.state_.targetVector=transformed;next.neutral=continuation;
 }
 out=std::move(next);return true;
}
bool completeNeutral(const Parameters& p,const NeutralContinuation& continuation,const AfterRefresh& after,Plan& out,std::string& e){
 e.clear();if(!p.authenticated()||!continuation.issued_)return fail(e,"missing genuine prepared neutral C-stick continuation");
 Plan next;next.state=continuation.state_;
 if(!next.state.distanceState)return fail(e,"source neutral C-stick reads unknown mCStickState");
 if(after.members.size()>100)return fail(e,"source full CPlate census exceeds source capacity");
 float minimum=12800;std::set<std::pair<Piki*,std::uint64_t>> identities;
 for(const auto& member:after.members){
  if(!member.handle.actor||!member.handle.lifetime||!finite(member.position)||!identities.emplace(member.handle.actor,member.handle.lifetime).second)return fail(e,"missing exact post-refresh CPlate member");
  Vec3 diff{member.position.x-continuation.position_.x,member.position.y-continuation.position_.y,member.position.z-continuation.position_.z};float d=length(diff);
  if(!std::isfinite(d))return fail(e,"source whole CPlate distance overflow");
  if(d<minimum)minimum=d;
 }
 const int state=minimum<p.waitRange()?0:minimum<p.changeRange()?1:2;
 if(*next.state.distanceState==state){
  if(!next.state.increment)return fail(e,"source C-stick reads unknown matching-state increment");
  if(*next.state.increment==std::numeric_limits<int>::max())return fail(e,"source C-stick signed increment overflow");
  ++*next.state.increment;
 }else{next.state.increment=0;next.state.distanceState=state;}
 if(state==0)next.state.needRearrange=1;
 else if(state==1){
  if(!after.maxPositionOffset||!finite(*after.maxPositionOffset))return fail(e,"missing actual post-refresh maxPositionOffset");
  next.state.needRearrange=1;Vec3 diff{after.maxPositionOffset->x-continuation.position_.x,after.maxPositionOffset->y-continuation.position_.y,after.maxPositionOffset->z-continuation.position_.z};diff=normalize(diff);
  next.commands.push_back(pose(CommandKind::SetPosGray,continuation.position_,std::atan2(diff.x,diff.z),continuation.velocity_));
 }else{
  next.state.commandOn1=false;if(!next.state.needRearrange)return fail(e,"source C-stick reads unknown _2FD");
  float angle=continuation.face_+pi;
  if(*next.state.needRearrange){next.commands.push_back(pose(CommandKind::Rearrange,continuation.position_,angle,continuation.velocity_));next.state.needRearrange=0;}
  next.commands.push_back(pose(CommandKind::SetPos,continuation.position_,angle,continuation.velocity_));
 }
 out=std::move(next);return true;
}
}}}
