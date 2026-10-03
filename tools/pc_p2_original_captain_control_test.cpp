#include "pc_p2_original_captain_control.h"
#include <fstream>
#include <iterator>
#include <iostream>
#include <cmath>
#include <limits>
#include <cstdlib>
using namespace p2original::captain::control;
static void require(bool condition,const char* name){if(!condition){std::cerr<<"FAIL "<<name<<'\n';std::exit(1);}}
static bool near(float a,float b){return std::fabs(a-b)<.0002f;}
int main(int argc,char** argv){
 require(argc==2,"private authored resource argument");std::ifstream file(argv[1],std::ios::binary);std::string bytes((std::istreambuf_iterator<char>(file)),{}),error;
 Params p;require(parseParameters(bytes,p,error),error.c_str());const auto& v=p.values();
 require(v.maxHealth==50&&v.moveSpeed==160&&v.rushBootSpeed==205&&v.justiceReduction==.5f&&v.wideWhistleRadius==130,"authored combat/equipment values");
 require(v.escapeStart==60&&v.walkMin==60&&v.cursorLookTime==.85f&&v.clampStick==.85f,"both duplicate parameter IDs");
 Params bad;require(!parseParameters("",bad,error)&&!bad.authenticated(),"missing resource refuses");
 std::string altered=bytes;altered[0]^=1;require(!parseParameters(altered,bad,error),"tampered source refuses");
 Vec2 result;require(!reviseStick(bad,{},result,error),"no constructor fallback");
 require(reviseStick(p,{0,.09f},result,error)&&near(result.z,0),"strict deadzone below threshold");
 require(reviseStick(p,{0,.1f},result,error)&&near(result.z,.1f),"threshold remains nonzero");
 require(reviseStick(p,{0,.85f},result,error)&&near(result.z,1),"clamp equality");
 require(reviseStick(p,{.8f,.8f},result,error)&&near(std::sqrt(result.x*result.x+result.z*result.z),1),"octagonal correction reaches clamp");
 require(reviseStick(p,{.1f,.8f},result,error)&&near(std::atan2(result.x,result.z),10*3.14159265358979323846f/180),"authored ten degree bins");
 CameraBasis camera{{1,0,0},{0,1,0},{0,-.5f,1}};Input input;input.hasController=true;input.deltaTime=.1f;input.stickY=1;input.cursorOffset={1,0};RuntimeControl state;VelocityOutput out;
 require(makeVelocity(p,input,camera,state,out,error)&&near(out.targetVelocity.z,160)&&state.moveRotation&&state.sceneAnimationTimer==0,"world camera basis + move resets idle");
 input.stickY=.5f;require(makeVelocity(p,input,camera,state,out,error)&&near(out.targetVelocity.z,0)&&!state.moveRotation&&near(state.faceDirection,3.14159265358979323846f*.1f),"cursor face smoothing .2 and velocity suppression");
 input.stickY=0;state.sceneAnimationTimer=.8f;require(makeVelocity(p,input,camera,state,out,error)&&!state.moveRotation,"idle look boundary");
 input.activityButton=true;require(makeVelocity(p,input,camera,state,out,error)&&state.sceneAnimationTimer==0&&state.moveRotation,"source activity buttons reset timer");
 camera.up={0,-1,-1};camera.view={0,.5f,1};input.stickY=1;require(makeVelocity(p,input,camera,state,out,error)&&near(out.targetVelocity.z,-160),"source camera up fallback");
 auto before=state;camera.side={0,1,0};require(!makeVelocity(p,input,camera,state,out,error)&&state.faceDirection==before.faceDirection,"invalid projection refuses atomically");
 camera.side={1,0,0};input.stickY=std::numeric_limits<float>::quiet_NaN();require(!makeVelocity(p,input,camera,state,out,error),"NaN stick refuses");
 AnimationState anim;AnimationOutput animation;
 require(updateWalkAnimation(p,{10,0},1,0,0,false,anim,animation,error)&&!animation.transition,"source first WAIT candidate resets pending counter");
 require(updateWalkAnimation(p,{10,0},1,0,0,false,anim,animation,error)&&animation.transition&&animation.motion==Motion::Walk&&near(animation.playbackSpeed,65),"WAIT to moving uses source expedited second candidate");
 require(updateWalkAnimation(p,{60,0},1,0,0,false,anim,animation,error),"escape pending");
 for(unsigned i=0;i<4;++i)require(updateWalkAnimation(p,{60,0},1,0,0,false,anim,animation,error),"escape hysteresis");
 require(animation.motion==Motion::Escape&&animation.preserveFrame&&animation.playbackSpeed==60&&animation.listener,"escape boundary and frame preservation");
 require(!updateWalkAnimation(p,{},0,0,0,false,anim,animation,error),"zero dt refuses");
 require(!updateWalkAnimation(p,{},1,0,0,true,anim,animation,error),"JKOKE refuses");
 require(std::string(authoredClip(Motion::Step))=="asibumi.bca"&&std::string(authoredClip(Motion::Run))=="run2.bca"&&!authoredClip(Motion::Unsupported),"authored source clip mapping");
 anim={};require(updateWalkAnimation(p,{},1,.02f,0,false,anim,animation,error),"turn step pending");
 for(unsigned i=0;i<4;++i)require(updateWalkAnimation(p,{},1,.02f,0,false,anim,animation,error),"turn step hysteresis");
 require(animation.motion==Motion::Step&&animation.playbackSpeed==60,"source stationary turning step");
 std::cout<<"PASS verified GPVE01 parameter hash, duplicate IDs, retail stick/camera/cursor and displacement animation controls\n";
}


