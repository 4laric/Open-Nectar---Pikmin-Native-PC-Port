#include "pc_midday_demo.h"
#include <algorithm>
#include <cmath>
#include <cstring>
namespace pc_midday {
namespace {
bool valid(const DemoFields&v,std::string&e){
 if(v.descriptors.size()!=62||v.current<-1||v.current>=62||!std::isfinite(v.timer)||(v.current==-1&&v.target)){e="DemoFlags framing/current/target invalid";return false;}
 for(size_t i=0;i<v.descriptors.size();++i)if(v.descriptors[i].index!=i){e="DemoFlag compiled registration order invalid";return false;}
 return true;
}
void put(Bytes&b,uint64_t n,unsigned w){for(unsigned i=0;i<w;++i)b.push_back(uint8_t(n>>(8*i)));}
uint64_t get(const Bytes&b,size_t at,unsigned w){uint64_t v=0;for(unsigned i=0;i<w;++i)v|=uint64_t(b[at+i])<<(8*i);return v;}
}
bool encodeDemo(const DemoFields&v,Bytes&out,std::string&e){
 if(!valid(v,e))return false;
 Bytes b={'P','C','D','E','M','O','0','1'};put(b,1,4);
 b.insert(b.end(),v.stored.begin(),v.stored.end());put(b,uint16_t(v.current),2);
 uint32_t timer=0;static_assert(sizeof(timer)==sizeof(v.timer),"float representation changed");std::memcpy(&timer,&v.timer,4);
 put(b,timer,4);put(b,v.target,8);put(b,v.descriptors.size(),2);
 for(const auto&d:v.descriptors){put(b,d.index,2);put(b,uint16_t(d.movie),2);put(b,d.part,2);put(b,d.text?1:0,1);}
 out=std::move(b);e.clear();return true;
}
bool decodeDemo(const Bytes&b,DemoFields&out,std::string&e){
 const Bytes magic={'P','C','D','E','M','O','0','1'};
 if(b.size()!=60+62*7||!std::equal(magic.begin(),magic.end(),b.begin())||get(b,8,4)!=1||get(b,58,2)!=62){e="DemoFlags version/length mismatch";return false;}
 DemoFields v;std::copy(b.begin()+12,b.begin()+44,v.stored.begin());
 const uint16_t index=uint16_t(get(b,44,2));v.current=index==0xffff?-1:int16_t(index);
 uint32_t timer=uint32_t(get(b,46,4));std::memcpy(&v.timer,&timer,4);v.target=get(b,50,8);
 for(size_t i=0;i<62;++i){size_t at=60+i*7;uint8_t flag=b[at+6];if(flag>1){e="DemoFlag boolean noncanonical";return false;}
  v.descriptors.push_back({uint16_t(get(b,at,2)),int16_t(uint16_t(get(b,at+2,2))),uint16_t(get(b,at+4,2)),flag!=0});}
 if(!valid(v,e))return false;
 out=std::move(v);e.clear();return true;
}
}
