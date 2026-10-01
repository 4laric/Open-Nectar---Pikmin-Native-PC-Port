// Codec3 independent golden/layout/reassembly checks (#1148); oracle remains active with NDEBUG.
#include "netplay/pc_netplay_randstate.h"
#include "netplay/pc_netplay_gekko_input.h"
#include <cstdio>
#include <cstring>
#include <initializer_list>
using namespace pc_randstate;
int failures=0;
#define CHECK(x) do {if(!(x)){std::printf("FAIL %d: %s\n",__LINE__,#x);++failures;}} while(0)
const uint8_t golden[kStateBytes]={0x03,0x01,0xff,0x03,0x00,0x02,0x09,0x00,0x01,0x19,0xa5,0x03,0x01,0x07,0x01,0x00,0x40,0x30,0x20,0x10,0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x00,0x01,0x02,0x00,0x01,0x02,0x00,0x01,0x02,0x00,0x01,0x02,0x01,0x02,0x00,0x00,0x00,0x01,0x01,0x01,0x02,0x01,0x03,0x01,0x04,0x01,0x05,0x01,0x06,0x01,0x07,0x01,0x08,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x09,0x00,0x00,0x00,0x05,0x26,0xa5,0x45}; // independently packed by Python struct + zlib
void repair(uint8_t* p){uint32_t crc=crc32(p,kPayloadBytes);for(int i=0;i<4;++i)p[164+i]=uint8_t(crc>>(8*i));}
void feed(Reassembler& r,const uint8_t* wire,uint32_t start){for(uint8_t i=0;i<kFragCount;++i)r.feed(true,frag_seq_make(i),wire+4*i,i+1==kFragCount,start+i);}
int main(){
 CHECK(kStateBytes==168 && kFragCount==42 && pc_netplay_gekko::kInputBytes==16);
 PcRandState st;CHECK(decode(golden,sizeof(golden),st));CHECK(st.deathLinks==0x10203040 && st.benefits[8]==264);
 for(unsigned i : {191u,192u,255u,256u,329u,511u})CHECK(st.checks[i/8]&(1u<<(i%8)));
 uint8_t wire[kStateBytes];CHECK(encode(st,wire)==168);CHECK(!std::memcmp(wire,golden,168));
 PcRandState changed=st;changed.gen=55;CHECK(payload_equal(st,changed));changed.maturity[2]=1;CHECK(!payload_equal(st,changed));
 for(unsigned off : {0u,1u,2u,4u,6u,7u,8u,15u,103u,158u,159u,160u}){
  uint8_t bad[168];std::memcpy(bad,golden,168);
  if(off==0)bad[off]=2;else if(off==1)bad[off]=0;else if(off==2)bad[3]|=128;
  else if(off==4){bad[4]=1;bad[5]=2;}else if(off==6)bad[off]=10;else if(off==8)bad[off]=2;
  else if(off==160)std::memset(bad+160,0,4);else bad[off]=1;
  repair(bad);PcRandState sentinel;sentinel.gen=123;CHECK(!decode(bad,168,sentinel));CHECK(sentinel.gen==123);
 }
 PcRandState sentinel;sentinel.gen=123;CHECK(!decode(wire,167,sentinel));CHECK(!decode(wire,169,sentinel));
 wire[24]^=1;CHECK(!decode(wire,168,sentinel));std::memcpy(wire,golden,168);
 for(uint8_t i=0;i<42;++i)CHECK(frag_seq_index(frag_seq_make(i))==i && frag_seq_stream(frag_seq_make(i))==0);
 for(unsigned delay=1;delay<=8;++delay){
  Reassembler a,b;feed(a,wire,delay);feed(b,wire,delay);
  CHECK(a.has_pending()&&b.has_pending()&&a.pending_frame()==64&&b.pending_frame()==64);
  PcRandState x,y;CHECK(a.take_pending(x)&&b.take_pending(y)&&payload_equal(x,y));a.mark_applied(9);b.mark_applied(9);
  feed(a,wire,100);CHECK(!a.has_pending());
 }
 Reassembler r;r.feed(true,42,wire,false,1);r.feed(true,64,wire,false,1);CHECK(!r.has_pending());
 for(uint8_t i=0;i<41;++i){r.feed(true,i,wire+4*i,false,i);r.feed(true,i,wire+4*i,false,i);}CHECK(!r.has_pending());
 r.feed(true,41,wire+164,true,41);CHECK(r.has_pending());CHECK(!r.discard_pending_upto(8));CHECK(r.discard_pending_upto(9));CHECK(!r.has_pending());
 PcRandState tl;tl.mode=2;tl.schema=1;tl.checkCount=330;tl.gen=1;tl.repairs=30;tl.thelynkParts=(1u<<30)-1;tl.thelynkBonuses[17]=330;
 encode(tl,wire);CHECK(decode(wire,168,st));CHECK(st.thelynkBonuses[17]==330&&st.repairs==30);
 tl.checks[329/8]|=1u<<(329%8);encode(tl,wire);CHECK(decode(wire,168,st));
 tl.checks[330/8]|=1u<<(330%8);encode(tl,wire);CHECK(!decode(wire,168,st));
 std::printf("codec3 failures=%d\n",failures);return failures?1:0;
}
