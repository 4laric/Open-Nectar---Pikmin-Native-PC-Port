#include "pc_p2_original_number_animation.h"
#include <cmath>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <utility>
namespace p2originalnumber {
namespace {
constexpr const char* names[]={"sx","rx","tx","sy","ry","ty","sz","rz","tz"};
constexpr const char* hashes[]={"c5041fdfbd8fa31b30f057eeb7db7450be63eba8e90d6329b93e053ade0d9c26","e84fac4a1aa46d78525a558a1ade1834c7199e03fd3f672e2f9516127f2855e1"};
bool fail(std::string& e,const char* text){e=text;return false;}
bool validKey(const AnimationKey& k){return std::isfinite(k.frame)&&std::isfinite(k.value)&&std::isfinite(k.inTangent)&&std::isfinite(k.outTangent)&&k.frame>=0&&k.frame<=41;}
bool value(const AnimationTrack& t,float frame,float& out){
 if(t.keys.empty()||t.keys.size()>64)return false;
 if(t.keys.size()==1||frame<=t.keys.front().frame){out=t.keys.front().value;return true;}
 if(frame>=t.keys.back().frame){out=t.keys.back().value;return true;}
 for(std::size_t i=1;i<t.keys.size();++i)if(frame<t.keys[i].frame){
  const auto& a=t.keys[i-1];const auto& b=t.keys[i];const float width=b.frame-a.frame;
  if(width<=0)return false;
  const float u=(frame-a.frame)/width,u2=u*u,u3=u2*u;
  out=(2*u3-3*u2+1)*a.value+(u3-2*u2+u)*width*a.outTangent+(-2*u3+3*u2)*b.value+(u3-u2)*width*b.inTangent;
  return std::isfinite(out);
 }
 return false;
}
}
bool Animation::read(const std::string& bytes,std::string& e){
 if(bytes.empty()||bytes.size()>32768)return fail(e,"numeric animation descriptor byte bound invalid");
 std::istringstream in(bytes);std::string word,hash;unsigned number=0,duration=0,scale=0,start=0,end=0,count=0;
 if(!(in>>word)||word!="P2_ORIGINAL_NUMBER_ANIMATION_1"||!(in>>word>>number)||word!="number"||(number!=1&&number!=5)||
    !(in>>word>>hash)||word!="source"||hash!=hashes[number==1?0:1]||!(in>>word>>duration)||word!="duration"||duration!=41||
    !(in>>word>>scale)||word!="angle_scale"||scale>15||!(in>>word>>start>>end)||word!="loop"||start!=10||end!=30||
    !(in>>word>>count)||word!="tracks"||count!=9)return fail(e,"numeric animation descriptor header invalid");
 Animation candidate;candidate.size=number==1?Size::One:Size::Five;candidate.angleScale=scale;
 for(unsigned i=0;i<9;++i){std::string name;unsigned keys=0;
  if(!(in>>word>>name>>keys)||word!="track"||name!=names[i]||!keys||keys>64)return fail(e,"numeric animation track invalid");
  auto& track=candidate.tracks[i];track.keys.reserve(keys);
  for(unsigned k=0;k<keys;++k){AnimationKey key;
   if(!(in>>key.frame>>key.value>>key.inTangent>>key.outTangent)||!validKey(key)||(k&&key.frame<=track.keys.back().frame))return fail(e,"numeric animation keys invalid");
   if(i%3==1&&(std::fabs(key.value)>32768||std::fabs(key.inTangent)>32768||std::fabs(key.outTangent)>32768))return fail(e,"numeric rotation source values exceed signed16");
   track.keys.push_back(key);
  }
 }
 if(in>>word)return fail(e,"numeric animation descriptor trailing data");
 std::array<float,12> initial;if(!candidate.sample(0,initial))return fail(e,"numeric animation initial transform invalid");
 *this=std::move(candidate);e.clear();return true;
}
bool Animation::sample(float frame,std::array<float,12>& out)const noexcept{
 if(!std::isfinite(frame)||frame<0||frame>=41||angleScale>15)return false;
 float scale[3],rotation[3],translation[3];
 for(unsigned i=0;i<3;++i){float angle=0;
  if(!value(tracks[i*3],frame,scale[i])||!value(tracks[i*3+1],frame,angle)||!value(tracks[i*3+2],frame,translation[i])||
     angle<=-2147483648.0f||angle>=2147483648.0f)return false;
  // J3D truncates Hermite rotation BEFORE angleScale, then narrows signed16.
  const std::int64_t shifted=std::int64_t(std::int32_t(angle))*(std::int64_t(1)<<angleScale);
  const std::uint16_t bits=std::uint16_t(std::uint64_t(shifted)&65535u);std::int16_t signedAngle=0;
  std::memcpy(&signedAngle,&bits,sizeof(bits));rotation[i]=float(signedAngle)*(3.14159265358979323846f/32768.0f);
 }
 const float sx=std::sin(rotation[0]),cx=std::cos(rotation[0]),sy=std::sin(rotation[1]),cy=std::cos(rotation[1]),sz=std::sin(rotation[2]),cz=std::cos(rotation[2]);
 const std::array<float,12> result{cz*cy*scale[0],(cz*sy*sx-sz*cx)*scale[1],(cz*sy*cx+sz*sx)*scale[2],translation[0],
  sz*cy*scale[0],(sz*sy*sx+cz*cx)*scale[1],(sz*sy*cx-cz*sx)*scale[2],translation[1],
  -sy*scale[0],cy*sx*scale[1],cy*cx*scale[2],translation[2]};
 for(float x:result)if(!std::isfinite(x))return false;
 out=result;return true;
}
bool CarryPlayer::start(float dt)noexcept{
 if(!std::isfinite(dt)||dt<0||!std::isfinite(30*dt))return false;
 mTimer=0;mStep=30*dt;mFinish=false;return true;
}
bool CarryPlayer::advance(float dt,bool picked)noexcept{
 if(!std::isfinite(dt)||dt<0||!std::isfinite(30*dt))return false;
 float candidate=mTimer+mStep;if(!std::isfinite(candidate))return false;
 // Retail key processing uses keyFrame < (int)timer; loop discards overshoot.
 if(!mFinish&&candidate>=31)candidate=10;
 if(candidate>=41){candidate=40;if(mFinish){candidate=0;mStep=picked?30*dt:0;mFinish=false;}}
 mTimer=candidate;return true;
}
}
