// Engineering doubles for the actual policy TU; no scene/gameplay evidence.
#include "pc_p2_original_captain_water.h"
#include <cstdlib>
#include <functional>
#include <iostream>
#include <limits>
#include <vector>
class Navi {};
namespace Game {struct WaterBox {};}
namespace p2original {namespace captain {struct LoadedScene {};}}
using namespace p2original::captain::water;
namespace {
int checks=0;void require(bool b){++checks;if(!b){std::cerr<<"Failed check "<<checks<<'\n';std::exit(1);}}
struct Double final:Provider {
 p2original::captain::LoadedScene scene;Navi actor;Game::WaterBox a,b;
 Owner owner{&scene,&actor,1,1};ColdInitialization cold{owner,ColdEvent::Constructor,1};
 Sphere sphere{1,2,3,8.5f};std::optional<bool> map=true;std::optional<Handle> found;
 bool inside=false,live=true,accept=true,available=true;mutable std::vector<std::string> calls;
 std::function<void()> callback;std::function<void()> query;
 Handle first(){return {&a,1};}Handle second(){return {&b,2};}
 bool currentOwner(Owner& v,std::string&)const override{v=owner;return available;}
 bool coldInitialization(ColdInitialization& v,std::string&)const override{v=cold;return available;}
 bool boundingSphere(Sphere& v,std::string&)const override{calls.push_back("sphere");v=sphere;if(query)query();return available;}
 bool mapPresent(std::optional<bool>& v,std::string&)const override{calls.push_back("map");v=map;return available;}
 bool liveWater(Handle h,std::string&)const override{return live&&h.pointer&&h.lifetime;}
 bool containsSphere(Handle,const Sphere& s,bool& v,std::string&)const override{calls.push_back("contains");require(s.radius==sphere.radius);v=inside;return available;}
 bool findWater(const Sphere&,std::optional<Handle>& v,std::string&)const override{calls.push_back("find");v=found;return available;}
 bool inWaterCallback(Handle h,std::string&) override{calls.push_back("in");require(found&&h.pointer==found->pointer);if(callback)callback();return accept;}
 bool outWaterCallback(std::string&) override{calls.push_back("out");if(callback)callback();return accept;}
 void retire(){++owner.actorIncarnation;}
 void reinitialize(){++cold.serial;cold.event=ColdEvent::InitFakePiki;}
};
void is(Cache& c,Double& p,std::optional<Handle> expected){std::string e;std::optional<Handle> got;require(cachedWater(c,p,got,e));require(bool(got)==bool(expected));if(got)require(got->pointer==expected->pointer&&got->lifetime==expected->lifetime);}
void enter(Cache& c,Double& p){std::string e;p.found=p.first();require(checkWater(c,p,e));is(c,p,p.first());p.calls.clear();}
}
int main(){
 std::string e;
 {Cache c;Double p;std::optional<Handle> out=p.first();require(!cachedWater(c,p,out,e));require(out->pointer==p.first().pointer);require(!checkWater(c,p,e));require(p.calls.empty());require(initialize(c,p,e));is(c,p,{});require(checkWater(c,p,e));require((p.calls==std::vector<std::string>{"sphere","map","find"}));}
 {Cache c;Double p;require(initialize(c,p,e));p.map.reset();require(!checkWater(c,p,e));is(c,p,{});p.map=false;p.calls.clear();require(checkWater(c,p,e));require((p.calls==std::vector<std::string>{"sphere","map"}));is(c,p,{});}
 {Cache c;Double p;require(initialize(c,p,e));p.found=p.first();p.callback=[&]{is(c,p,{});};require(checkWater(c,p,e));require((p.calls==std::vector<std::string>{"sphere","map","find","in"}));is(c,p,p.first());require(initialize(c,p,e));is(c,p,p.first());p.reinitialize();require(initialize(c,p,e));is(c,p,{});}
 {Cache c;Double p;require(initialize(c,p,e));enter(c,p);p.inside=true;p.map.reset();require(checkWater(c,p,e));require((p.calls==std::vector<std::string>{"sphere","contains"}));is(c,p,p.first());p.inside=false;p.map=false;p.calls.clear();require(checkWater(c,p,e));require((p.calls==std::vector<std::string>{"sphere","contains","map"}));is(c,p,p.first());}
 {Cache c;Double p;require(initialize(c,p,e));enter(c,p);p.found=p.second();require(checkWater(c,p,e));require((p.calls==std::vector<std::string>{"sphere","contains","map","find"}));is(c,p,p.second());p.found.reset();p.calls.clear();p.callback=[&]{is(c,p,p.second());};require(checkWater(c,p,e));require((p.calls==std::vector<std::string>{"sphere","contains","map","find","out"}));is(c,p,{});}
 {Cache c;Double p;require(initialize(c,p,e));enter(c,p);p.map.reset();require(!checkWater(c,p,e));is(c,p,p.first());p.map=true;p.calls.clear();require(checkWater(c,p,e));require((p.calls==std::vector<std::string>{"sphere","contains","map","find"}));is(c,p,p.first());}
 {Cache c;Double p;require(initialize(c,p,e));p.found=p.first();p.accept=false;require(!checkWater(c,p,e));is(c,p,{});p.accept=true;enter(c,p);p.found.reset();p.accept=false;require(!checkWater(c,p,e));is(c,p,p.first());}
 {Cache c;Double p;require(initialize(c,p,e));p.found=p.first();p.callback=[&]{p.retire();};require(!checkWater(c,p,e));std::optional<Handle> out=p.second();require(!cachedWater(c,p,out,e));require(out->pointer==p.second().pointer);p.cold.owner=p.owner;p.reinitialize();require(initialize(c,p,e));is(c,p,{});}
 {Cache c;Double p;require(initialize(c,p,e));enter(c,p);p.found.reset();p.callback=[&]{p.available=false;};require(!checkWater(c,p,e));p.available=true;is(c,p,p.first());}
 {Cache c;Double p;require(initialize(c,p,e));enter(c,p);p.found.reset();p.callback=[&]{p.reinitialize();require(initialize(c,p,e));};require(!checkWater(c,p,e));is(c,p,{});}
 {Cache c;Double p;require(initialize(c,p,e));p.found=p.first();p.callback=[&]{p.live=false;};require(!checkWater(c,p,e));is(c,p,{});}
 {Cache c;Double p;require(initialize(c,p,e));p.query=[&]{p.retire();};require(!checkWater(c,p,e));require((p.calls==std::vector<std::string>{"sphere"}));}
 {Cache c;Double p;require(initialize(c,p,e));p.sphere.radius=std::numeric_limits<float>::quiet_NaN();require(!checkWater(c,p,e));is(c,p,{});p.sphere.radius=-1;require(!checkWater(c,p,e));p.sphere.radius=8.5f;p.sphere.x=std::numeric_limits<float>::infinity();require(!checkWater(c,p,e));is(c,p,{});}
 {Cache c;Double p,q;require(initialize(c,p,e));require(!checkWater(c,q,e));std::optional<Handle> out=q.first();require(!cachedWater(c,q,out,e));require(out->pointer==q.first().pointer);}
 {Cache c;Double p;p.cold.serial=0;require(!initialize(c,p,e));p.cold.serial=1;p.cold.event=static_cast<ColdEvent>(99);require(!initialize(c,p,e));p.cold.event=ColdEvent::Constructor;p.owner.sceneIncarnation=0;require(!initialize(c,p,e));}
 {Cache c;Double p;require(initialize(c,p,e));p.found=Handle{&p.a,0};require(!checkWater(c,p,e));is(c,p,{});require(p.calls.back()=="find");}
 std::cout<<checks<<" source water policy engineering checks PASS\n";
}
