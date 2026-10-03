#include "pc_p2_original_snagret_clock.h"
#include "pc_p2_original_snagret_death.h"
#include <cassert>
#include <cstdio>
using namespace p2original::bulblax_snagret;
int main(){
 for(auto registration:std::vector<std::pair<int,int>>{{34,80},{42,70},{12,36},{131,165}}){
  MotionClock c;unsigned count=0,end=0;
  for(int frame=1;frame<=registration.second+5;++frame){
   auto events=c.advance(1,{{registration.first,3}},registration.second,false);
   assert(delivered(events,3)==(frame==registration.first+1));
   assert(delivered(events,1000)==(frame==registration.second));
   count+=delivered(events,3);end+=delivered(events,1000);
  }
  assert(count==1&&end==1&&c.frame()==registration.second-1);
  assert(c.phase(registration.second)==1.0f); // bank draw reconstructs phase*(duration-1)
 }
 MotionClock crossed;const std::vector<std::pair<int,int>> death={{67,2},{75,2},{110,5},{131,3},{143,4},{149,4}};
 assert(crossed.advance(131.99f,death,165,false)==(std::vector<int>{2,2,5}));
 assert(crossed.advance(.01f,death,165,false)==(std::vector<int>{3}));
 auto final=crossed.advance(40,death,165,false);
 assert(final==(std::vector<int>{4,4,1000}));
 DeathItems latch;assert(latch.key(true,true));assert(!latch.key(true,true));
 DeathItems ap;assert(!ap.key(false,true)&&!ap.emitted);
 MotionClock leap;auto all=leap.advance(200,death,165,false);
 assert(all==(std::vector<int>{2,2,5,3,4,4,1000}));
 MotionClock loop;assert(loop.advance(60,{{0,0},{49,1}},50,true)==(std::vector<int>{0,1}));
 assert(loop.frame()==0);assert(loop.advance(1,{{0,0},{49,1}},50,true)==(std::vector<int>{0}));
 loop.reset();assert(loop.frame()==0);
 MotionClock pose;pose.advance(35,{{34,3}},80,false);
 assert(std::fabs(pose.phase(80)*79-35)<.00001f);
 assert(pose.phase(1)==0);
 std::puts("original SnakeCrow strict key clock PASS");
}
