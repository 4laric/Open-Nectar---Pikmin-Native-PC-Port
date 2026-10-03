#include "pc_p2_original_captain_whistle.h"
#include <cmath>
#include <map>
#include <sstream>
namespace p2original { namespace captain { namespace whistle { namespace {
bool fail(std::string& e,const char* s){e=s;return false;}
bool finite(Vec3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
float length(Vec3 v){return std::sqrt(v.x*v.x+v.y*v.y+v.z*v.z);}
Vec3 normalize(Vec3 v){const float n=length(v);return n>0?Vec3{v.x/n,v.y/n,v.z/n}:v;}
bool position(Vec3 actor,const Terrain& terrain,State& state,std::string& e){
 Vec3 p{actor.x+state.offset.x,actor.y+state.offset.y,actor.z+state.offset.z};
 if(!finite(p))return fail(e,"nonfinite actual whistle terrain position");
 float height;Vec3 normal;
 if(!terrain.currentTriangle(p,height,normal,e))return false;
 if(!std::isfinite(height)||!finite(normal))return fail(e,"nonfinite actual whistle terrain query");
 Vec3 result{p.x+normal.x,height+normal.y,p.z+normal.z};
 if(!finite(result))return fail(e,"nonfinite actual whistle final position");
 state.normal=normal;state.position=result;return true;
}
void advance(const Parameters::Values& p,float delta,bool wide,State& s){
 switch(s.mode){
 case Mode::Idle:s.color={255,120,0,120};break;
 case Mode::Blowing:{
  const float t=s.time/p.callTime;
  s.color={static_cast<unsigned char>(255-175*t),static_cast<unsigned char>(120-110*t),static_cast<unsigned char>(255*t),120};
  s.time+=delta;
  if(s.time>p.callTime){s.time=0;s.mode=Mode::Ended;break;}
  s.radius=s.time/p.callTime*((wide?p.wide:p.maximum)-p.minimum)+p.minimum;break;
 }
 case Mode::Ended:
  s.color[3]=static_cast<unsigned char>((1-s.time/p.fadeTime)*120);
  s.time+=delta;
  if(s.time>p.fadeTime){s.time=0;s.mode=Mode::Idle;s.radius=10;}break;
 }
}
}
bool parseParameters(const std::string& bytes,Parameters& out,std::string& e){
 out=Parameters{};control::Params authored;
 if(!control::parseParameters(bytes,authored,e))return false;
 std::map<std::string,float> values;std::istringstream input(bytes);std::string line;
 while(std::getline(input,line)){
  auto a=line.find('{'),b=line.find('}');if(a==std::string::npos||b==std::string::npos||b<=a)continue;
  std::istringstream field(line.substr(b+1));int type;float value;
  if(field>>type>>value)values.emplace(line.substr(a+1,b-a-1),value);
 }
 auto read=[&](const char* key,float& value){auto i=values.find(key);if(i==values.end()||!std::isfinite(i->second))return false;value=i->second;return true;};
 auto& p=out.data;
 if(!read("p053",p.minimum)||!read("p001",p.maximum)||!read("q007",p.wide)||!read("p002",p.callTime)||!read("p003",p.fadeTime)||!read("p046",p.cursorRadius)||!read("p047",p.cursorSpeed))return fail(e,"missing authored whistle parameters");
 if(p.minimum<0||p.maximum<p.minimum||p.wide<p.minimum||p.callTime<=0||p.fadeTime<=0||p.cursorRadius<=0||p.cursorSpeed<0)return fail(e,"invalid authored whistle parameters");
 out.valid=true;return true;
}
bool initialize(const Parameters& p,Vec3 actor,float face,const Terrain& terrain,State& state,std::string& e){
 e.clear();if(!p.authenticated()||!finite(actor)||!std::isfinite(face))return fail(e,"missing actual whistle initialization inputs");
 State next=state;next.mode=Mode::Idle;next.radius=10;next.time=0;next.color={255,150,0,120};
 const float r=p.values().cursorRadius*.5f;next.offset={std::sin(face)*r,0,std::cos(face)*r};
 if(!position(actor,terrain,next,e))return false;
 state=next;return true;
}
bool update(const Parameters& p,Vec3 actor,Vec3 stick,bool automatic,bool wide,float delta,const Terrain& terrain,State& state,std::string& e){
 e.clear();if(!p.authenticated()||!finite(actor)||!finite(stick)||!finite(state.offset)||!std::isfinite(delta)||delta<0||!std::isfinite(state.time)||state.time<0||!std::isfinite(state.time+delta))return fail(e,"invalid actual whistle update inputs");
 const auto& v=p.values();State next=state;
 if((state.mode!=Mode::Idle&&state.mode!=Mode::Blowing&&state.mode!=Mode::Ended)
 ||(state.mode==Mode::Blowing&&state.time>v.callTime)
 ||(state.mode==Mode::Ended&&state.time>v.fadeTime))return fail(e,"invalid retained source whistle phase/time");
 if(automatic)next.offset={};
 else {
  Vec3 dir=normalize(stick);dir={dir.x*v.cursorSpeed,dir.y*v.cursorSpeed,dir.z*v.cursorSpeed};
  Vec3 offset{state.offset.x+dir.x*delta,state.offset.y+dir.y*delta,state.offset.z+dir.z*delta};
  if(length(offset)>=v.cursorRadius){
   offset=normalize(offset);const float dot=offset.x*dir.x+offset.y*dir.y+offset.z*dir.z;
   offset={state.offset.x+(dir.x-offset.x*dot)*delta,state.offset.y+(dir.y-offset.y*dot)*delta,state.offset.z+(dir.z-offset.z*dot)*delta};
  }
  if(!finite(offset))return fail(e,"nonfinite source whistle offset");
  next.offset=offset;
 }
 if(!position(actor,terrain,next,e))return false;
 advance(v,delta,wide,next);state=next;return true;
}
void start(const Parameters& p,State& s){if(!p.authenticated())return;s.isWhistleActive=false;if(s.mode==Mode::Idle||s.mode==Mode::Ended){s.mode=Mode::Blowing;s.time=0;s.radius=p.values().minimum;}}
void stop(State& s){if(s.mode==Mode::Blowing){s.mode=Mode::Ended;s.time=0;s.isWhistleActive=true;}}
bool timeout(const State& s){return s.mode==Mode::Idle;}
bool setFace(float face,State& s,std::string& e){e.clear();if(!std::isfinite(face)||!finite(s.offset))return fail(e,"invalid source whistle face");const float n=length(s.offset);if(!std::isfinite(n))return fail(e,"nonfinite source whistle distance");s.offset={n*std::sin(face),0,n*std::cos(face)};return true;}
}}}
