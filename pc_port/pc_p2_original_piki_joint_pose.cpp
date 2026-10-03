#include "pc_p2_original_piki_joint_pose.h"
#include <cstdint>
#include <cstring>
#include <cmath>
#include <vector>
#include <limits>
namespace p2original {namespace piki {
namespace {
bool fail(std::string& e,const char* why){e=why;return false;}
unsigned u16(const std::string& s,std::size_t n){return (unsigned(static_cast<unsigned char>(s[n]))<<8)|static_cast<unsigned char>(s[n+1]);}
unsigned u32(const std::string& s,std::size_t n){return (unsigned(static_cast<unsigned char>(s[n]))<<24)|(unsigned(static_cast<unsigned char>(s[n+1]))<<16)|(unsigned(static_cast<unsigned char>(s[n+2]))<<8)|static_cast<unsigned char>(s[n+3]);}
float f32(const std::string& s,std::size_t n){std::uint32_t bits=u32(s,n);float out;std::memcpy(&out,&bits,4);return out;}
bool range(std::size_t size,std::size_t at,std::size_t count,std::size_t stride){return at<=size&&count<=(size-at)/stride;}
bool modelRoot(const std::string& model,unsigned& joints){
 if(model.size()<32||model.compare(0,8,"J3D2bmd3")||u32(model,8)!=model.size())return false;
 const unsigned blocks=u32(model,12);if(!blocks||blocks>128)return false;
 std::string inf,jnt;std::size_t at=32;
 for(unsigned i=0;i<blocks;++i){if(!range(model.size(),at,1,8))return false;const auto size=u32(model,at+4);if(size<8||!range(model.size(),at,size,1))return false;
  const auto tag=model.substr(at,4);if(tag=="INF1"){if(!inf.empty())return false;inf=model.substr(at,size);}if(tag=="JNT1"){if(!jnt.empty())return false;jnt=model.substr(at,size);}at+=size;
 }
 if(at!=model.size()||inf.size()<24||jnt.size()<24)return false;
 joints=u16(jnt,8);const auto records=u32(jnt,12),remap=u32(jnt,16);
 if(!joints||records<24||!range(jnt.size(),records,joints,64)||remap<24||!range(jnt.size(),remap,joints,2))return false;
 for(unsigned j=0;j<joints;++j)if(u16(jnt,remap+2*j)>=joints)return false;
 const auto hierarchy=u32(inf,20);if(hierarchy<24||!range(inf.size(),hierarchy,1,4))return false;
 std::vector<int> parents(joints,-2),stack;int current=-1;bool ended=false;
 for(std::size_t i=hierarchy;range(inf.size(),i,1,4);i+=4){const auto kind=u16(inf,i),index=u16(inf,i+2);
  if(kind==0){ended=true;break;}
  if(kind==1){stack.push_back(current);if(stack.size()>joints+1)return false;}
  else if(kind==2){if(stack.empty())return false;current=stack.back();stack.pop_back();}
  else if(kind==0x10){if(index>=joints||parents[index]!=-2)return false;parents[index]=stack.empty()?-1:stack.back();current=int(index);}
  else if(kind!=0x11&&kind!=0x12)return false;
 }
 if(!ended||!stack.empty()||parents[0]!=-1)return false;
 for(int parent:parents)if(parent==-2)return false;
 return true;
}
}
bool sourceRootTranslation(const std::string& model,const std::string& raw,float frame,Vector3f& out,std::string& e){
 unsigned joints=0;if(!modelRoot(model,joints))return fail(e,"source BMD has no complete unparented joint0");
 if(raw.size()<68||raw.compare(0,8,"J3D1bca1")||u32(raw,8)!=(raw.size()+31)/32*32||u32(raw,12)!=1||raw.compare(32,4,"ANF1")||u32(raw,36)+32!=u32(raw,8)||u16(raw,44)!=joints||!u16(raw,42))return fail(e,"source full animation skeleton/header mismatch");
 if(!std::isfinite(frame)||frame>float(std::numeric_limits<int>::max()-1024))return fail(e,"invalid source full animation frame");
 const unsigned table=u32(raw,52),counts[]={u16(raw,46),u16(raw,48),u16(raw,50)};
 const unsigned offsets[]={u32(raw,56),u32(raw,60),u32(raw,64)},stride[]={4,2,4};
 if(table<36||!range(raw.size()-32,table,joints,36))return fail(e,"source ANF1 joint table truncated");
 for(unsigned c=0;c<3;++c)if(offsets[c]<36||!range(raw.size()-32,offsets[c],counts[c],stride[c]))return fail(e,"source ANF1 channel table truncated");
 const unsigned sample=frame<0?0:unsigned(int(frame+0.5f));Vector3f result;
 // Joint0 has no source joint parent. Its own scale/rotation do not alter the
 // translation column of T*R*S; nevertheless validate every root SRT channel.
 for(unsigned axis=0;axis<3;++axis)for(unsigned c=0;c<3;++c){
  const auto channel=32+table+axis*12+c*4;const unsigned count=u16(raw,channel),index=u16(raw,channel+2);
  if(!count||index>counts[c]||count>counts[c]-index)return fail(e,"source root has absent/out-of-bounds full channel");
  const auto valueAt=32+offsets[c]+stride[c]*(index+(sample>=count?count-1:sample));
  if(c!=1){const float value=f32(raw,valueAt);if(!std::isfinite(value))return fail(e,"nonfinite source root SRT");if(c==2){if(axis==0)result.x=value;else if(axis==1)result.y=value;else result.z=value;}}
 }
 out=result;e.clear();return true;
}
} }
