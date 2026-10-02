#include "pc_midday_player_core.h"
#include <cstdlib>
#include <iostream>
#include <limits>
using namespace pc_midday;
static int checks=0;
void check(bool ok){++checks;if(!ok){std::cerr<<"player core check failed "<<checks<<"\n";std::exit(1);}}
int main(){
 PlayerCoreFields v;v.sprouted=std::numeric_limits<int32_t>::min();v.living=std::numeric_limits<int32_t>::max();v.totalDead=-8;v.unused186=0xfe;v.container=0xa5;v.dayEnd=true;v.lastUpdated=65535;
 for(size_t i=0;i<30;++i){v.collectedByDay[i]=uint8_t(i);v.partsToNext[i]=uint8_t(30-i);}v.stageParts={1,2,3,4,5};
 v.hour.start=7;v.hour.end=8;v.hour.entries={{{-1,-1,-1}},{{1,2,3}}};v.day.start=0;v.day.end=0;v.day.entries={{{42,21,9}}};
 v.courses[0]={8,{0xab,0xcd}};v.courses[4]={1,{0x81}};
 std::string e;Bytes b;check(encodePlayerCore(v,b,e));PlayerCoreFields restored;check(decodePlayerCore(b,restored,e));Bytes roundtrip;check(encodePlayerCore(restored,roundtrip,e)&&roundtrip==b);
 check(restored.unused186==0xfe&&restored.lastUpdated==65535&&restored.sprouted==v.sprouted&&restored.hour.entries[0][0]==-1&&restored.courses[1].bits.empty());
 auto refuses=[&](Bytes bad){PlayerCoreFields untouched;untouched.living=6543;check(!decodePlayerCore(bad,untouched,e)&&untouched.living==6543);};
 auto bad=b;bad[0]=0;refuses(bad);bad=b;bad[8]=2;refuses(bad);bad=b;bad.push_back(0);refuses(bad);
 // All truncation boundaries, not just the final byte; the decoder must reject
 // before allocating arrays from a truncated or attacker-controlled header.
 for(size_t i=0;i<b.size();++i){bad.assign(b.begin(),b.begin()+i);refuses(bad);}
 bad=b;bad[12+44+5]=2;refuses(bad); // noncanonical native bool
 bad=b;const size_t graphAt=12+44+5+5+2+65;bad[graphAt+2]=0;bad[graphAt+3]=0;refuses(bad); // reversed range
 bad=b;bad[graphAt+2]=0xff;bad[graphAt+3]=0xff;refuses(bad); // huge range
 auto invalid=v;invalid.hour.entries.pop_back();Bytes sentinel={7};check(!encodePlayerCore(invalid,sentinel,e)&&sentinel==Bytes{7});
 invalid=v;invalid.courses[0].bits.pop_back();check(!encodePlayerCore(invalid,sentinel,e)&&sentinel==Bytes{7});invalid=v;invalid.courses[0].entries=4097;check(!encodePlayerCore(invalid,sentinel,e));
 PlayerCoreTopology topology{7,8,0,0,{8,0,0,0,1}};check(validatePlayerCoreTopology(v,topology,e));
 RestoreGate gate{true,true,true,true,true,true,true};check(preparePlayerCore(restored,b,topology,gate,e));
 for(unsigned i=0;i<7;++i){auto g=gate;bool* f[]={&g.freshProcess,&g.paused,&g.zeroInput,&g.birthEffectsSuppressed,&g.rewardsSuppressed,&g.rngDrawsSuppressed,&g.audioVoicesSuppressed};*f[i]=false;restored.living=987;check(!preparePlayerCore(restored,b,topology,g,e)&&restored.living==987);}
 auto wrong=topology;wrong.courseEntries[0]=7;restored.living=879;check(!preparePlayerCore(restored,b,wrong,gate,e)&&restored.living==879);
 wrong=topology;wrong.hourStart=6;check(!preparePlayerCore(restored,b,wrong,gate,e)&&restored.living==879);
 wrong=topology;wrong.dayEnd=1;check(!preparePlayerCore(restored,b,wrong,gate,e)&&restored.living==879);
 std::cout<<checks<<" player core controls PASS\n";
}
