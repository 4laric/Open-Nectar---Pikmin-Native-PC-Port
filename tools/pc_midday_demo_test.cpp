#include "pc_midday_demo.h"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace pc_midday;
int n=0;void check(bool v){++n;if(!v)throw std::runtime_error(std::to_string(n));}
int main(){try{DemoFields v;for(unsigned i=0;i<62;++i)v.descriptors.push_back({uint16_t(i),int16_t(i==15?-1:int(i)),uint16_t(i),i==16});
 for(unsigned i=0;i<32;++i)v.stored[i]=uint8_t(i*7);
 v.timer=-0.0f;Bytes b;std::string e;check(encodeDemo(v,b,e));check(b.size()==494);
 DemoFields parsed;check(decodeDemo(b,parsed,e));check(parsed.stored==v.stored&&parsed.current==-1&&parsed.target==0);check(parsed.descriptors[15].movie==-1);
 v.current=7;v.target=0x123456789abcdef0ULL;v.timer=-12.5f;check(encodeDemo(v,b,e));check(decodeDemo(b,parsed,e));check(parsed.current==7&&parsed.target==v.target&&parsed.timer==v.timer);
 auto refusal=[&](Bytes bad){DemoFields unchanged=parsed;check(!decodeDemo(bad,unchanged,e));check(unchanged.current==parsed.current&&unchanged.target==parsed.target&&unchanged.stored==parsed.stored);};
 for(size_t at:{size_t(0),size_t(8),size_t(58)}){auto bad=b;bad[at]^=1;refusal(bad);}
 auto truncated=b;truncated.pop_back();refusal(truncated);auto appended=b;appended.push_back(0);refusal(appended);
 auto bad=b;bad[44]=62;bad[45]=0;refusal(bad);bad=b;bad[44]=254;bad[45]=255;refusal(bad);
 bad=b;bad[44]=bad[45]=255;refusal(bad);bad=b;bad[66]=2;refusal(bad);bad=b;bad[60]=1;refusal(bad);
 bad=b;bad[46]=0;bad[47]=0;bad[48]=128;bad[49]=127;refusal(bad);
 Bytes output={42};v.current=-1;check(!encodeDemo(v,output,e));check(output==Bytes{42});v.target=0;v.timer=std::numeric_limits<float>::quiet_NaN();check(!encodeDemo(v,output,e));check(output==Bytes{42});
 v.timer=0;v.descriptors.pop_back();check(!encodeDemo(v,output,e));check(output==Bytes{42});
 std::cout<<n<<" Demo codec controls PASS\n";return 0;}catch(const std::exception&x){std::cerr<<x.what()<<"\n";return 1;}}
