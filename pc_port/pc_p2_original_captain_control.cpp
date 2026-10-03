#include "pc_p2_original_captain_control.h"
#include "netplay/pc_netplay_sha256.h"
#include <cmath>
#include <sstream>
#include <vector>
#include <map>
namespace p2original { namespace captain { namespace control {
namespace {
constexpr float pi=3.14159265358979323846f;
float roundAngle(float a) { if(a<0)a+=2*pi; if(a>=2*pi)a-=2*pi; return a; }
float angleDistance(float a,float b) { float d=roundAngle(a-b); if(d>=pi)d=-roundAngle(2*pi-d); return d; }
bool finite(Vec2 v) { return std::isfinite(v.x)&&std::isfinite(v.z); }
bool finite(Vec3 v) { return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z); }
Vec2 normalize(Vec2 v) { float n=std::sqrt(v.x*v.x+v.z*v.z); return n>0?Vec2{v.x/n,v.z/n}:Vec2{}; }
bool fail(std::string& e,const char* reason) { e=reason; return false; }
}
const char* parameterSha256() { return "dfcc8e0cf89195f06ea78fdc1a342631da4e5d2495d85380212af7d72eb2eba0"; }
bool parseParameters(const std::string& bytes,Params& output,std::string& error) {
 output=Params{}; error.clear(); unsigned char digest[32];
 pc_netplay_sha::sha256(bytes.data(),bytes.size(),digest);
 if(pc_netplay_sha::hex(digest,32)!=parameterSha256())return fail(error,"captain parameter source hash mismatch");
 std::map<std::string,std::vector<float>> entries; std::istringstream lines(bytes); std::string line;
 while(std::getline(lines,line)) {
  auto start=line.find('{'), end=line.find('}'); if(start==std::string::npos||end==std::string::npos||end<=start)continue;
  std::string key=line.substr(start+1,end-start-1); std::istringstream fields(line.substr(end+1)); int type;float value;
  if(fields>>type>>value) { if(type!=4||!std::isfinite(value))return fail(error,"invalid authored parameter"); entries[key].push_back(value); }
 }
 Values v{};
 auto get=[&](const char* key,unsigned index,float& value){auto it=entries.find(key);if(it==entries.end()||index>=it->second.size())return false;value=it->second[index];return true;};
 // Both fp04 and p048 are intentionally duplicated by the source schema.
 if(!get("p004",0,v.moveSpeed)||!get("q006",0,v.rushBootSpeed)||!get("p050",0,v.maxHealth)||!get("q008",0,v.justiceReduction)||!get("q007",0,v.wideWhistleRadius)
 ||!get("p035",0,v.angleDegrees)||!get("p043",0,v.neutralStick)||!get("p044",0,v.cursorMovementStick)||!get("p048",0,v.cursorLookTime)||!get("p048",1,v.clampStick)
 ||!get("fp01",0,v.stepStart)||!get("fp02",0,v.walkStart)||!get("fp03",0,v.runStart)||!get("fp04",0,v.escapeStart)||!get("fp04",1,v.walkMin)||!get("fp05",0,v.walkMax)
 ||!get("fp06",0,v.runMin)||!get("fp07",0,v.runMax)||!get("fp08",0,v.escapeMin)||!get("fp09",0,v.escapeMax))return fail(error,"missing authored parameter");
 if(!(v.angleDegrees>0&&v.moveSpeed>0&&v.stepStart<v.walkStart&&v.walkStart<v.runStart&&v.runStart<v.escapeStart))return fail(error,"invalid authored parameter order");
 output.data_=v;output.valid_=true;return true;
}
bool reviseStick(const Params& p,Vec2 input,Vec2& output,std::string& error) {
 error.clear();if(!p.authenticated()||!finite(input)||std::fabs(input.x)>1||std::fabs(input.z)>1)return fail(error,"invalid source stick input or parameters");
 const auto& v=p.values();float magnitude=std::sqrt(input.x*input.x+input.z*input.z);
 float theta=roundAngle(std::atan2(input.x,input.z)),bin=v.angleDegrees*pi/180;
 float binned=bin*int((bin*.5f+theta)/bin);
 float boost=-(int(binned/(pi/4))*(pi/4)-binned);
 float factor=std::sin(pi/4-boost)+std::sin(boost);
 magnitude*=1/(std::sin(pi/4)/factor);
 if(magnitude>=v.clampStick)magnitude=1;
 if(magnitude<v.neutralStick)magnitude=0;
 output={std::sin(binned)*magnitude,std::cos(binned)*magnitude};return true;
}
bool makeVelocity(const Params& p,const Input& in,const CameraBasis& camera,RuntimeControl& state,VelocityOutput& out,std::string& error) {
 error.clear();if(!p.authenticated()||!std::isfinite(in.deltaTime)||in.deltaTime<0||!std::isfinite(state.sceneAnimationTimer)||state.sceneAnimationTimer<0||(!std::isfinite(state.faceDirection)||state.faceDirection<0||state.faceDirection>=2*pi)||!finite(in.cursorOffset)||!finite(camera.side)||!finite(camera.up)||!finite(camera.view))return fail(error,"invalid source velocity inputs");
 Vec2 stick;if(!reviseStick(p,in.hasController?Vec2{-in.stickX,in.stickY}:Vec2{},stick,error))return false;
 Vec2 side=normalize({camera.side.x,camera.side.z});
 Vec3 forward=camera.up.y>camera.view.y?camera.view:camera.up;
 Vec2 view=normalize({forward.x,forward.z});
 if((side.x==0&&side.z==0)||(view.x==0&&view.z==0))return fail(error,"degenerate source camera projection");
 Vec2 result{side.x*stick.x+view.x*stick.z,side.z*stick.x+view.z*stick.z};
 RuntimeControl next=state;next.sceneAnimationTimer=in.hasController&&in.activityButton?0:state.sceneAnimationTimer+in.deltaTime;
 float distance=std::sqrt(result.x*result.x+result.z*result.z);const auto& v=p.values();
 if(distance>v.neutralStick)next.sceneAnimationTimer=0;
 VelocityOutput candidate{{result.x*v.moveSpeed,result.z*v.moveSpeed},result};
 if(in.hasController) {
  bool turn=next.sceneAnimationTimer>=v.cursorLookTime;
  if((turn||(!turn&&distance>v.neutralStick))&&distance<=v.cursorMovementStick) {
   candidate.targetVelocity={};float rad=roundAngle(std::atan2(in.cursorOffset.x,in.cursorOffset.z));
   next.faceDirection=roundAngle(next.faceDirection+angleDistance(rad,next.faceDirection)*.2f);next.moveRotation=false;
  }else next.moveRotation=true;
 }
 if(!std::isfinite(next.sceneAnimationTimer))return fail(error,"source idle timer overflow");
 state=next;out=candidate;return true;
}
const char* authoredClip(Motion m) {
 switch(m){case Motion::Wait:return "wait.bca";case Motion::Step:return "asibumi.bca";case Motion::Walk:return "walk.bca";case Motion::Run:return "run2.bca";case Motion::Escape:return "nigeru.bca";default:return nullptr;}
}
bool updateWalkAnimation(const Params& p,Vec2 displacement,float dt,float face,float offset,bool jkoke,AnimationState& state,AnimationOutput& out,std::string& error) {
 error.clear();if(!p.authenticated()||!finite(displacement)||!std::isfinite(dt)||dt<=0||!std::isfinite(face)||!std::isfinite(offset))return fail(error,"invalid source animation inputs");
 if(state.bound==Motion::Unsupported){state.playbackSpeed=30;out={Motion::Unsupported,30,false,false,false};return true;}
 if(jkoke)return fail(error,"retail walk animator cannot own JKOKE self animation");
 const auto& v=p.values();float speed=std::sqrt(displacement.x*displacement.x+displacement.z*displacement.z)/dt;
 if(!std::isfinite(speed))return fail(error,"source displacement speed overflow");
 Motion other;bool listener=false;
 if(speed<v.stepStart){other=Motion::Wait;speed=30;if(std::fabs(face-offset)>.01f){speed=60;other=Motion::Step;}}
 else if(speed<v.walkStart){speed=30;other=Motion::Step;}
 else if(speed<v.runStart){speed=v.walkMin+(speed-v.walkStart)/(v.runStart-v.walkStart)*(v.walkMax-v.walkMin);other=Motion::Walk;listener=true;}
 else if(speed<v.escapeStart){speed=v.runMin+(speed-v.runStart)/(v.escapeStart-v.runStart)*(v.runMax-v.runMin);other=Motion::Run;listener=true;}
 else {speed=v.escapeMin;other=Motion::Escape;listener=true;}
 if(other!=state.bound){
  if(state.bound==Motion::Wait&&other!=Motion::Step)state.pendingUpdates=4;
  if(state.bound!=Motion::Step&&other==Motion::Wait)state.pendingUpdates=4;
  if(other!=state.pending){state.pendingUpdates=0;state.pending=other;}else ++state.pendingUpdates;
  if(state.pendingUpdates<4){out={state.bound,state.playbackSpeed,false,false,false};return true;}
 }
 bool change=state.bound!=other;
 bool preserve=change&&state.bound!=Motion::Wait&&state.bound!=Motion::Step&&other!=Motion::Wait&&other!=Motion::Step;
 state.bound=other;state.playbackSpeed=speed;out={other,speed,change,preserve,listener};return true;
}
}}}

