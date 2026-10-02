#include "pc_midday_player_root.h"
#include <cstdio>
using namespace pc_midday;
int checks=0,failures=0;
void check(bool b,const char* why){++checks;if(!b){++failures;std::printf("FAIL %s\n",why);}}
int main(){PlayerCoreFields v;v.totalParts=30;v.totalRegisteredParts=30;v.hour={6,7,{{1,2,3},{4,5,6}}};v.day={0,1,{{7,8,9},{10,11,12}}};PlayerCoreTopology t{6,7,0,1,{0,8,1,0,0}};v.courses[1]={8,{0xab,0xcd}};v.courses[2]={1,{0xff}};std::string e;
 check(validatePlayerRootCore(v,t,e),"valid native core topology");
 for(int n:{-1,0,29,31}){auto bad=v;bad.totalParts=n;check(!validatePlayerRootCore(bad,t,e),"wrong ship allocation count");}
 for(int n:{-1,31}){auto bad=v;bad.totalRegisteredParts=n;check(!validatePlayerRootCore(bad,t,e),"registration bounds");}
 for(int n:{0,1,15,30}){auto good=v;good.totalRegisteredParts=n;check(validatePlayerRootCore(good,t,e),"partial/native registration allowed");}
 for(int n:{0,1}){auto good=v;good.unused186=uint8_t(n);check(validatePlayerRootCore(good,t,e),"canonical native bool byte");}
 for(int n:{2,127,128,255}){auto bad=v;bad.unused186=uint8_t(n);Bytes b;check(encodePlayerCore(bad,b,e),"generic codec preserves raw byte");PlayerCoreFields read;check(decodePlayerCore(b,read,e)&&read.unused186==n,"raw byte decode unchanged");check(!validatePlayerRootCore(read,t,e),"native invalid bool refused without normalization");}
 {auto bad=v;bad.hour.entries.pop_back();check(!validatePlayerRootCore(bad,t,e),"incomplete graph refused");bad=v;bad.courses[1].bits.pop_back();check(!validatePlayerRootCore(bad,t,e),"native ceil-plus-one allocation retained");auto wrong=t;wrong.courseEntries[2]=0;check(!validatePlayerRootCore(v,wrong,e),"compiled factory count mismatch");}
 {auto raw=v;raw.living=-9;raw.currentParts=-123;raw.hour.entries[0][0]=-2147483647;check(validatePlayerRootCore(raw,t,e),"signed source counters preserved without gameplay recomputation");}
 std::printf("%d checks, %d failures\n",checks,failures);return failures?1:0;
}
