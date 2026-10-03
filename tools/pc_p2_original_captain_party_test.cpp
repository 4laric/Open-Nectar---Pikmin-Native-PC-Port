#include "pc_p2_original_captain_actions_party.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
using namespace p2original::captain;
namespace p=party;
void check(bool b,const char* name){if(!b){std::cerr<<name<<'\n';std::exit(1);}}
int main(){
 p::WorldFacts w;w.active=true;w.demoInactive=true;w.switchUnlocked=true;
 p::CaptainFacts c;c.alive=true;c.state=StateId::Walk;
 check(p::switchAllowed(w,c),"ordinary switch");
 for(auto state:{StateId::Nuku,StateId::NukuAdjust,StateId::Punch}){c.state=state;check(!p::switchAllowed(w,c),"excluded partner state");}
 c.state=StateId::Walk;
 auto a=w;a.softPaused=true;check(!p::switchAllowed(a,c),"softpause");
 a=w;a.multiplayer=true;check(!p::switchAllowed(a,c),"multiplayer");
 a=w;a.demoInactive=false;check(!p::switchAllowed(a,c),"demo");
 a=w;a.switchUnlocked=false;check(!p::switchAllowed(a,c),"switch unlock distinct from reunion");
 check(!p::whistleAllowed(w,c),"day0 not reunited");
 w.reunited=true;check(p::whistleAllowed(w,c),"reunited callable");
 c.controller=true;check(!p::whistleAllowed(w,c),"controlled captain refuses whistle");
 c.controller=false;c.state=StateId::Follow;check(!p::whistleAllowed(w,c),"Follow not callable");
 c.state=StateId::Pellet;check(p::whistleAllowed(w,c),"Pellet callable");
 check(p::needsChange(StateId::Follow)&&p::needsChange(StateId::Walk)&&!p::needsChange(StateId::Gather),"source change capability");
 std::vector<p::Member> members;
 for(unsigned i=0;i<100;++i){p::Member m;m.handle={reinterpret_cast<Piki*>(std::uintptr_t(i+1)),1};m.position={100,float(i%2)*10,0};m.kind=i%8;m.alive=true;m.releasable=true;members.push_back(m);}
 std::array<p::Group,8> groups{};std::string error;
 check(p::dismissGroups(members,{0,0,0},{500,0,0},true,groups,error),"full100 pool all kinds");
 unsigned total=0;for(auto& g:groups){total+=g.count;check(g.radius==std::sqrt(float(g.count))*6.25f,"source cluster radius");check(std::isfinite(g.center.x)&&std::isfinite(g.center.y),"finite clusters");}
 check(total==100,"all100 retained");
 auto old=groups;members.push_back(members[0]);check(!p::dismissGroups(members,{0,0,0},{500,0,0},true,groups,error),"duplicate refuses");check(groups[0].center.x==old[0].center.x,"failure preserves output");
 members.resize(1);members[0].position={0,60,0};members[0].kind=0;
 check(p::dismissGroups(members,{0,0,0},{0,0,0},false,groups,error),"3D source distance");check(groups[0].center.y==60,"vertical distance not horizontal approximation");
 members[0].alive=false;check(p::dismissGroups(members,{0,0,0},{0,0,0},false,groups,error)&&groups[0].count==0,"dead not dismissed");
 members[0].alive=true;members[0].releasable=false;check(p::dismissGroups(members,{0,0,0},{0,0,0},false,groups,error)&&groups[0].count==0,"unreleasable preserved");
 members[0].releasable=true;members[0].position.x=std::numeric_limits<float>::quiet_NaN();check(!p::dismissGroups(members,{0,0,0},{0,0,0},false,groups,error),"bad member refuses");
 std::cout<<"source captain party controls PASS\n";
}
