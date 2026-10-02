#include "pc_midday_result.h"
#include <iostream>
#include <stdexcept>
using namespace pc_midday;
int n=0;void check(bool v){++n;if(!v)throw std::runtime_error(std::to_string(n));}
int main(){try{ResultFields v;v.descriptors={{0,1,0,false},{3,-27,1,true},{147,2147483647,2,false}};
 for(size_t i=0;i<38;++i)v.states[i]=uint8_t(i*11);
 for(size_t i=0;i<30;++i)v.days[i]=int16_t(i==0?-32768:i==29?32767:int(i)-1);
 Bytes b;std::string e;check(encodeResult(v,b,e));check(b.size()==151);ResultFields parsed;check(decodeResult(b,parsed,e));check(parsed.states==v.states&&parsed.days==v.days);check(parsed.descriptors[1].priority==-27);
 auto refuse=[&](Bytes bad){ResultFields unchanged=parsed;check(!decodeResult(bad,unchanged,e));check(unchanged.states==parsed.states&&unchanged.days==parsed.days&&unchanged.descriptors.size()==3);};
 for(size_t at:{size_t(0),size_t(8),size_t(110)}){auto bad=b;bad[at]^=1;refuse(bad);}
 auto bad=b;bad.pop_back();refuse(bad);bad=b;bad.push_back(1);refuse(bad);
 bad=b;bad[112]=148;refuse(bad);bad=b;bad[125]=0;refuse(bad);bad=b;bad[124]=2;refuse(bad);bad=b;bad[120]=3;refuse(bad);
 Bytes out={17};auto old=v;v.descriptors.clear();check(!encodeResult(v,out,e)&&out==Bytes{17});v=old;v.descriptors[2].screen=-1;check(!encodeResult(v,out,e)&&out==Bytes{17});
 v=old;v.descriptors[1].screen=0;check(!encodeResult(v,out,e)&&out==Bytes{17});
 std::cout<<n<<" Result codec controls PASS\n";return 0;}catch(const std::exception&x){std::cerr<<x.what()<<"\n";return 1;}}
