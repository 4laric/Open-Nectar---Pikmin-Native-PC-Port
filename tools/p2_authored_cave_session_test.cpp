#include "pc_p2_authored_cave_session.h"
#include <cassert>
#include <sstream>
#include <iostream>
std::string image(){
    std::string b(P2CaveCacheBanks::imageSize,0);
    auto put=[&](unsigned p,unsigned n){for(int i=3;i>=0;--i){b[p+i]=char(n);n>>=8;}};
    put(4,P2CaveCacheBanks::heapSize);
    for(unsigned i=0;i<5;++i){unsigned p=8+P2CaveCacheBanks::heapSize+i*37;b[p]=char(255);put(p+1,i);}
    assert(P2CaveCacheBanks::imageValid(b));return b;
}
int main(){
    P2AuthoredCaveRoute route;route.present=true;route.seed=42;route.token=std::string(32,'a');route.routeSha=std::string(64,'b');
    route.surface={1,1,"stages/forest.ini",std::string(64,'c'),{1.25f,30,2.5f}};
    route.floor={1,1,"stages/generated-forest.ini",std::string(64,'d'),{3.5f,30,4.75f}};route.exit={10,20,30};assert(route.valid());
    P2CaveSeedBinding binding;binding.seed=42;binding.token=route.token;assert(route.matches(binding));++binding.seed;assert(!route.matches(binding));
    P2CaveCacheBanks banks;assert(banks.enter(image()));assert(banks.captureFloor(image()));
    P2AuthoredCaveSession saved;saved.present=true;saved.route=route;saved.day=0;saved.party.present=true;saved.party.inside=true;saved.party.nextKey=21;
    for(int slot=0;slot<2;++slot){P2CavePartyCaptain c;c.slot=slot;c.health=75;c.maxHealth=100;saved.party.captains.push_back(c);}
    for(int i=0;i<20;++i){P2CavePartyBody b;b.species=i%3;b.growth=i%3;b.owner=i%2;b.player=i%2;b.key=i+1;b.health=5;b.maxHealth=10;saved.party.bodies.push_back(b);}
    saved.surfaceCacheSha=P2AuthoredCaveSession::bankHash(route.routeSha,"surface",banks.surface);
    saved.floorCacheSha=P2AuthoredCaveSession::bankHash(route.routeSha,"floor",banks.floor);saved.activeCacheSha=saved.floorCacheSha;
    assert(saved.matches(route,banks)&&saved.activeCacheMatches(banks.floor));
    std::ostringstream wire;saved.write(wire);P2AuthoredCaveSession loaded;std::istringstream input(wire.str());assert(loaded.read(input)&&loaded.matches(route,banks));
    assert(loaded.party.bodies.size()==20&&loaded.party.captains.size()==2);
    for(unsigned i=0;i<20;++i){const auto& a=saved.party.bodies[i];const auto& b=loaded.party.bodies[i];assert(a.key==b.key&&a.species==b.species&&a.growth==b.growth&&a.health==b.health&&a.owner==b.owner);}
    const auto original=wire.str();
    for(const std::string bad:{original.substr(0,original.size()/2),std::string("AUTHORED_CAVE_SESSION 1 2"),std::string("SURFACE_SESSION 1 0")}){
        std::istringstream in(bad);assert(!loaded.read(in));std::ostringstream after;loaded.write(after);assert(after.str()==original);
    }
    auto other=route;other.token[0]='e';assert(!saved.matches(other,banks));other=route;other.floor.mapSha[0]='e';assert(!saved.matches(other,banks));
    other=route;other.surface.file="stages/../forest.ini";assert(!other.valid());other=route;other.floor.index=2;assert(!other.valid());
    auto corrupt=banks;corrupt.floor[8]='x';assert(corrupt.valid()&&!saved.matches(route,corrupt)&&!saved.activeCacheMatches(corrupt.floor));
    auto cross=saved;cross.activeCacheSha=cross.surfaceCacheSha;assert(!cross.activeCacheMatches(banks.floor));
    auto down=saved;down.party.captains[1].health=0;assert(!down.valid());
    auto wrong=saved;wrong.party.inside=false;assert(!wrong.matches(route,banks));
    P2AuthoredCaveSession empty;std::ostringstream noLiving;empty.write(noLiving);std::istringstream emptyInput(noLiving.str());assert(loaded.read(emptyInput)&&!loaded.present);
    std::cout<<"PASS authored route/session roundtrip20 twoCaptains prospective refusal bank binding; no gameplay\n";
}
