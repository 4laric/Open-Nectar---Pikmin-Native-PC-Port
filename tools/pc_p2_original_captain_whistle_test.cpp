#include "pc_p2_original_captain_whistle.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>
using namespace p2original::captain::whistle;
namespace {
int checks=0;
void check(bool b){if(!b)throw std::runtime_error("whistle check "+std::to_string(checks+1));++checks;}
bool near(float a,float b){return std::fabs(a-b)<.0001f;}
struct TerrainControl:Terrain {
 mutable Vec3 input;mutable unsigned queries=0;bool refuse=false;float y=7;Vec3 normal{.1f,.9f,.2f};
 bool currentTriangle(Vec3 p,float& height,Vec3& n,std::string& e)const override{
  ++queries;input=p;if(refuse){e="actual terrain unavailable";return false;}height=y;n=normal;return true;
 }
};
}
int main(int argc,char** argv){
 check(argc==2);std::ifstream input(argv[1],std::ios::binary);std::string bytes{std::istreambuf_iterator<char>(input),{}};
 Parameters p;std::string e;check(parseParameters(bytes,p,e)&&p.authenticated());
 check(near(p.values().minimum,5)&&near(p.values().maximum,90)&&near(p.values().wide,130));
 check(near(p.values().callTime,.45f)&&near(p.values().fadeTime,1)&&near(p.values().cursorRadius,100)&&near(p.values().cursorSpeed,300));
 TerrainControl terrain;State s;check(initialize(p,{1,2,3},0,terrain,s,e));
 check(near(terrain.input.x,1)&&near(terrain.input.y,2)&&near(terrain.input.z,53));
 check(near(s.position.x,1.1f)&&near(s.position.y,7.9f)&&near(s.position.z,53.2f));
 check(!s.isWhistleActive&&s.mode==Mode::Idle&&near(s.radius,10));
 stop(s);check(!s.isWhistleActive&&s.mode==Mode::Idle);
 start(p,s);check(s.mode==Mode::Blowing&&near(s.radius,5)&&s.isWhistleActive==false);
 check(update(p,{1,2,3},{0,0,0},false,false,.225f,terrain,s,e));
 check(near(s.radius,47.5f)&&near(s.time,.225f)&&s.color[0]==255&&s.color[1]==120&&s.color[2]==0);
 start(p,s);check(near(s.time,.225f)); // repeated blowing does not restart timer
 check(update(p,{1,2,3},{0,0,0},false,true,.225f,terrain,s,e));
 check(near(s.radius,130)&&s.mode==Mode::Blowing&&s.color[0]==167&&s.color[1]==65&&s.color[2]==127);
 check(update(p,{1,2,3},{0,0,0},false,true,.001f,terrain,s,e));
 check(s.mode==Mode::Ended&&s.time==0&&near(s.radius,130)&&s.isWhistleActive==false); // timeout doesn't call stop
 check(update(p,{1,2,3},{0,0,0},false,false,.5f,terrain,s,e));
 check(s.color[3]==120&&!timeout(s));
 State restart=s;start(p,restart);check(restart.mode==Mode::Blowing&&restart.time==0&&restart.radius==5&&restart.color==s.color);
 check(update(p,{1,2,3},{0,0,0},false,false,.5f,terrain,s,e));check(s.mode==Mode::Ended&&s.color[3]==60);
 check(update(p,{1,2,3},{0,0,0},false,false,.001f,terrain,s,e));check(timeout(s)&&s.radius==10&&s.time==0);
 start(p,s);stop(s);check(s.mode==Mode::Ended&&s.isWhistleActive==true&&s.time==0);stop(s);check(s.isWhistleActive==true);
 check(initialize(p,{1,2,3},0,terrain,s,e)&&s.isWhistleActive==true); // init preserves unassigned source flag
 check(update(p,{1,2,3},{0,0,1},false,false,1.f/6,terrain,s,e));check(near(s.offset.z,50)); // exact >= equality
 check(update(p,{1,2,3},{0,0,1},false,false,.2f,terrain,s,e));check(near(s.offset.z,50)); // outward radial motion cancels at boundary
 check(update(p,{1,2,3},{1,0,0},false,false,.3f,terrain,s,e));check(near(s.offset.x,21.226415f)&&near(s.offset.z,11.792454f)); // tangent projection, not radial clamp
 check(initialize(p,{1,2,3},0,terrain,s,e));
 check(update(p,{1,2,3},{1,0,0},false,false,.1f,terrain,s,e));check(near(s.offset.x,30)&&near(s.offset.z,50));
 check(update(p,{1,2,3},{1,0,0},true,false,.01f,terrain,s,e));check(s.offset.x==0&&s.offset.y==0&&s.offset.z==0);
 check(initialize(p,{1,2,3},0,terrain,s,e));check(setFace(3.14159265358979323846f/2,s,e)&&near(s.offset.x,50)&&near(s.offset.z,0));
 const auto old=s;terrain.refuse=true;check(!update(p,{1,2,3},{1,0,0},false,false,.1f,terrain,s,e));
 check(s.offset.x==old.offset.x&&s.offset.z==old.offset.z&&s.time==old.time&&s.position.y==old.position.y);
 check(!initialize(p,{4,5,6},0,terrain,s,e)&&s.offset.x==old.offset.x);
 terrain.refuse=false;terrain.y=std::numeric_limits<float>::infinity();check(!update(p,{1,2,3},{},false,false,.1f,terrain,s,e));
 check(!update(p,{1,2,3},{},false,false,-1,terrain,s,e));check(!setFace(std::numeric_limits<float>::quiet_NaN(),s,e));
 terrain.y=7;State invalid=s;invalid.mode=Mode::Blowing;invalid.time=.46f;
 check(!update(p,{1,2,3},{},false,false,0,terrain,invalid,e)&&invalid.time==.46f);
 invalid.mode=Mode::Ended;invalid.time=1.01f;check(!update(p,{1,2,3},{},false,false,0,terrain,invalid,e));
 invalid.mode=static_cast<Mode>(100);invalid.time=0;check(!update(p,{1,2,3},{},false,false,0,terrain,invalid,e));
 invalid=s;invalid.offset={std::numeric_limits<float>::max(),0,std::numeric_limits<float>::max()};
 check(!setFace(0,invalid,e));
 check(!update(p,{std::numeric_limits<float>::max(),2,3},{},false,false,0,terrain,invalid,e));
 Parameters rejected;bytes[0]^=1;check(!parseParameters(bytes,rejected,e)&&!rejected.authenticated());
 check(!initialize(rejected,{1,2,3},0,terrain,s,e));
 std::cout<<"P2_ORIGINAL_WHISTLE_ACTUAL_POLICY_PASS checks="<<checks<<" terrain=CONTROL gameplay=UNTESTED\n";
}
