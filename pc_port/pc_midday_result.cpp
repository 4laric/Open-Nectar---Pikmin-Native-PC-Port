#include "pc_midday_result.h"
#include <algorithm>
#include <set>
namespace pc_midday {
namespace {
bool valid(const ResultFields&v,std::string&e){
 if(v.descriptors.empty()||v.descriptors.size()>152){e="Result compiled descriptor count invalid";return false;}
 std::set<int32_t>screens;
 for(const auto&d:v.descriptors)if(d.screen<0||d.screen>=148||d.store>2||!screens.insert(d.screen).second){e="Result compiled screen/store topology invalid";return false;}
 return true;
}
void put(Bytes&b,uint64_t n,unsigned w){for(unsigned i=0;i<w;++i)b.push_back(uint8_t(n>>(8*i)));}
uint64_t get(const Bytes&b,size_t at,unsigned w){uint64_t v=0;for(unsigned i=0;i<w;++i)v|=uint64_t(b[at+i])<<(8*i);return v;}
}
bool encodeResult(const ResultFields&v,Bytes&out,std::string&e){
 if(!valid(v,e))return false;
 Bytes b={'P','C','R','E','S','U','0','1'};put(b,1,4);b.insert(b.end(),v.states.begin(),v.states.end());
 for(auto day:v.days)put(b,uint16_t(day),2);
 put(b,v.descriptors.size(),2);for(const auto&d:v.descriptors){put(b,uint32_t(d.screen),4);put(b,uint32_t(d.priority),4);put(b,d.store,4);put(b,d.autoSet?1:0,1);}
 out=std::move(b);e.clear();return true;
}
bool decodeResult(const Bytes&b,ResultFields&out,std::string&e){
 const Bytes magic={'P','C','R','E','S','U','0','1'};
 if(b.size()<112||!std::equal(magic.begin(),magic.end(),b.begin())||get(b,8,4)!=1){e="Result framing/version invalid";return false;}
 const size_t count=size_t(get(b,110,2));if(!count||count>152||b.size()!=112+count*13){e="Result length/count invalid";return false;}
 ResultFields v;std::copy(b.begin()+12,b.begin()+50,v.states.begin());for(size_t i=0;i<30;++i)v.days[i]=int16_t(uint16_t(get(b,50+i*2,2)));
 for(size_t i=0;i<count;++i){size_t at=112+i*13;if(b[at+12]>1){e="Result boolean noncanonical";return false;}
  v.descriptors.push_back({int32_t(uint32_t(get(b,at,4))),int32_t(uint32_t(get(b,at+4,4))),uint32_t(get(b,at+8,4)),b[at+12]!=0});}
 if(!valid(v,e))return false;
 out=std::move(v);e.clear();return true;
}
}
