#include "pc_p2_original_system_parameters.h"
#include <array>
#include <charconv>
#include <cmath>
#include <vector>
namespace p2original {
static_assert(sizeof(float)==4,"source TimeMgr float parameters require size4");
namespace {
bool fail(std::string& e,const char* text){e=text;return false;}
bool tokens(const std::string& bytes,std::vector<std::string>& out,std::string& e){
 if(bytes.empty()||bytes.size()>65536||bytes.find('\0')!=std::string::npos)return fail(e,"invalid source system parameter byte extent");
 unsigned depth=0;std::string token;
 auto flush=[&](){if(!token.empty()){out.push_back(token);token.clear();}};
 for(std::size_t i=0;i<bytes.size();++i){const auto c=static_cast<unsigned char>(bytes[i]);
  if(c=='#'){flush();while(i<bytes.size()&&bytes[i]!='\r'&&bytes[i]!='\n')++i;continue;}
  if(c=='{'||c=='}'){flush();if(c=='{'){if(++depth>32)return fail(e,"source parameter scope overflow");}
   else{if(!depth)return fail(e,"unbalanced source parameter scope");--depth;}continue;}
  if(c==' '||c=='\t'||c=='\r'||c=='\n'){flush();continue;}
  if(c<0x21||c>0x7e||token.size()>=127)return fail(e,"invalid source parameter token");token.push_back(static_cast<char>(c));
 }
 flush();if(depth||out.empty())return fail(e,"truncated source parameter scope");return true;
}
bool decimal(const std::string& token,bool integral){
 std::size_t i=0;if(i<token.size()&&(token[i]=='+'||token[i]=='-'))++i;
 unsigned digits=0;while(i<token.size()&&token[i]>='0'&&token[i]<='9'){++i;++digits;}
 if(!integral&&i<token.size()&&token[i]=='.'){++i;while(i<token.size()&&token[i]>='0'&&token[i]<='9'){++i;++digits;}}
 if(!digits)return false;
 if(!integral&&i<token.size()&&(token[i]=='e'||token[i]=='E')){++i;if(i<token.size()&&(token[i]=='+'||token[i]=='-'))++i;
  const auto first=i;while(i<token.size()&&token[i]>='0'&&token[i]<='9')++i;if(i==first)return false;}
 return i==token.size();
}
bool floating(const std::string& token,float& out){
 if(!decimal(token,false))return false;const char* first=token.data();if(*first=='+')++first;
 float next=0;auto r=std::from_chars(first,token.data()+token.size(),next,std::chars_format::general);
 if(r.ec!=std::errc{}||r.ptr!=token.data()+token.size()||!std::isfinite(next))return false;out=next;return true;
}
bool integer(const std::string& token,std::int32_t& out){
 if(!decimal(token,true))return false;const char* first=token.data();if(*first=='+')++first;
 std::int32_t next=0;auto r=std::from_chars(first,token.data()+token.size(),next,10);
 if(r.ec!=std::errc{}||r.ptr!=token.data()+token.size())return false;out=next;return true;
}
}
bool parseAIConstantsParameters(const std::string& bytes,AIConstantsParameters& out,std::string& e){
 std::vector<std::string> stream;if(!tokens(bytes,stream,e))return false;
 AIConstantsParameters next;std::array<bool,4> seen{};bool ended=false;
 for(std::size_t i=0;i<stream.size();){const auto& tag=stream[i++];
  if(tag=="end"){ended=true;if(i!=stream.size())return fail(e,"trailing AIConstants tokens");break;}
  unsigned index=4;if(tag=="gravity")index=0;else if(tag=="dopecount")index=1;else if(tag=="debt")index=2;else if(tag=="camera_angle")index=3;
  if(index==4||seen[index]||i==stream.size())return fail(e,"unknown/duplicate/truncated AIConstants tag");seen[index]=true;
  const auto& value=stream[i++];bool valid=false;
  if(index==0)valid=floating(value,next.gravity);else if(index==1)valid=integer(value,next.dopeCount);
  else if(index==2)valid=integer(value,next.debt);else valid=floating(value,next.cameraAngle);
  if(!valid)return fail(e,"invalid AIConstants scalar token");
 }
 // Positive gravity is required by actual Navi throw division. Integer counts
 // admit zero but reject negative resource/count/debt values; angle is any
 // finite source degree value, not a fabricated 0..360 range or clamp.
 if(!ended||!seen[0]||next.gravity<=0||next.dopeCount<0||next.debt<0)return fail(e,"missing/unsafe AIConstants source values");
 out=next;return true;
}
bool parseTimeParameters(const std::string& bytes,TimeParameters& out,std::string& e){
 std::vector<std::string> stream;if(!tokens(bytes,stream,e))return false;
 const std::array<const char*,12> ids{{"fp00","fp01","fp02","fp03","fp04","fp05","fp06","fp07","fp11","fp08","fp09","fp10"}};
 std::array<float,12> values{};std::array<bool,12> seen{};bool ended=false;
 for(std::size_t i=0;i<stream.size();){const auto& id=stream[i++];
  if(id=="_eof"){ended=true;if(i!=stream.size())return fail(e,"trailing TimeMgr tokens");break;}
  unsigned index=12;for(unsigned j=0;j<ids.size();++j)if(id==ids[j])index=j;
  if(index==12||seen[index]||i+1>=stream.size())return fail(e,"unknown/duplicate/truncated TimeMgr parameter");
  std::int32_t size=0;if(!integer(stream[i++],size)||size!=4||!floating(stream[i++],values[index]))return fail(e,"TimeMgr parameter requires exact size4 finite float");
  seen[index]=true;
 }
 if(!ended)return fail(e,"TimeMgr parameter terminator unavailable");
 for(unsigned i=0;i<12;++i){if(!seen[i])return fail(e,"missing selected TimeMgr parameter");
  if(values[i]<0||values[i]>(i==2?3600.0f:24.0f))return fail(e,"TimeMgr parameter outside source declared range");}
 TimeParameters n;n.dayStart=values[0];n.dayEnd=values[1];n.dayLengthSeconds=values[2];n.morningStart=values[3];
 n.midMorning=values[4];n.morningEnd=values[5];n.eveningStart=values[6];n.midEveningStart=values[7];
 n.midEveningEnd=values[8];n.eveningEnd=values[9];n.sundownAlert=values[10];n.countdown=values[11];
 // Refuse zero/negative actual TimeMgr::init/updateSlot denominators rather
 // than silently clamp or alter selected source values. The middle evening
 // interval is a plateau and may be zero; no denominator uses its length.
 if(n.dayLengthSeconds<=0||n.dayEnd<=n.dayStart||n.dayEnd-n.dayStart>=24
 ||n.midMorning<=n.morningStart||n.morningEnd<=n.midMorning||n.eveningStart<=n.morningEnd
 ||n.midEveningStart<=n.eveningStart||n.midEveningEnd<n.midEveningStart||n.eveningEnd<=n.midEveningEnd
 ||24-n.eveningEnd+n.morningStart<=0)return fail(e,"unsafe TimeMgr source interval/divisor");
 out=n;return true;
}
}
