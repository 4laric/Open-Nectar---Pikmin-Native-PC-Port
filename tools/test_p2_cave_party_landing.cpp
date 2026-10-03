#include "pc_p2_cave_party_landing.h"
#include <cstdlib>
#include <iostream>
#include <sstream>
static std::string image(const P2CaveCampaignParty& party){std::ostringstream out;party.write(out);return out.str();}
int main(){
 unsigned checks=0;auto check=[&](bool yes){++checks;if(!yes){std::cerr<<"failed "<<checks<<'\n';std::exit(1);}};
 P2CaveCampaignParty source;source.present=true;source.active=0;source.nextKey=101;
 source.captains={{0,100,100,0,{-50,0,0}},{1,80,100,1,{-20,0,0}}};
 for(unsigned i=0;i<100;++i){P2CavePartyBody b;b.key=i+1;b.species=i%5;b.growth=i%3;
  b.owner=i%2;b.player=i%2;b.mode=i%2;b.health=30;b.maxHealth=100;b.face=.25f;
  b.position={-1350.f+float(i),20,2000};b.originPosition={float(i),0,0};b.originGenerator=17;
  source.bodies.push_back(b);source.origins.push_back(b);
 }
 check(source.valid());std::array<P2CavePartyPoint,2> to{{{800,0,0},{830,0,0}}};
 auto landed=source;check(p2CavePlaceLandingParty(landed,to));
 for(std::size_t i=0;i<landed.bodies.size();++i){const auto& body=landed.bodies[i];
  check(std::hypot(body.position.x-to[body.owner].x,body.position.z-to[body.owner].z)<65);
  auto restored=body;restored.position=source.bodies[i].position;
  auto comparison=source;comparison.bodies[i]=restored;check(image(comparison)==image(source));
 }
 auto reverted=landed;for(std::size_t i=0;i<reverted.bodies.size();++i)reverted.bodies[i].position=source.bodies[i].position;
 for(std::size_t i=0;i<reverted.captains.size();++i)reverted.captains[i].position=source.captains[i].position;
 check(image(reverted)==image(source));
 auto shuffled=source;std::reverse(shuffled.bodies.begin(),shuffled.bodies.end());
 check(p2CavePlaceLandingParty(shuffled,to));
 for(const auto& b:shuffled.bodies){const auto& expected=landed.bodies[b.key-1];
  check(b.position.x==expected.position.x&&b.position.y==expected.position.y&&b.position.z==expected.position.z);
 }
 auto single=source;for(auto& b:single.bodies){b.owner=-1;b.mode=0;}single.active=1;
 check(p2CavePlaceLandingParty(single,to));
 for(const auto& b:single.bodies)check(std::hypot(b.position.x-to[1].x,b.position.z-to[1].z)<65&&b.owner==-1);
 auto bad=source;bad.bodies[1].key=bad.bodies[0].key;auto before=image(bad);
 check(!p2CavePlaceLandingParty(bad,to)&&image(bad)==before);
 auto invalid=to;invalid[0].x=std::numeric_limits<float>::infinity();bad=source;before=image(bad);
 check(!p2CavePlaceLandingParty(bad,invalid)&&image(bad)==before);
 P2CaveCampaignParty absent;check(!p2CavePlaceLandingParty(absent,to));
 std::cout<<checks<<" transported landing controls PASS (geometry/gameplay separate)\n";
}
