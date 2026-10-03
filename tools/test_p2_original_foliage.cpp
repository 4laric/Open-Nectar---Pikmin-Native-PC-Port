#include "pc_p2_original_foliage.h"
#include "pc_p2_original_foliage_lod.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
// Controlled ownership and animation tests; these do not claim native gameplay.
class Creature {};
class Generator {};
using namespace p2original;
using namespace p2original::foliage;
#define CHECK(condition) do {if(!(condition)) throw std::runtime_error(std::string("line ")+std::to_string(__LINE__)+": " #condition);} while(0)
struct ControlledEngine final:Engine {
 Resources bank{true,true,true,100,31};
 std::map<unsigned,unsigned> resourceCalls;
 std::vector<std::unique_ptr<Creature>> actors;
 unsigned reserved=999,allocations=0,cleanups=0,sounds=0;
 bool resourceFails=false,reserveFails=false,allocateFails=false,cleanupFails=false,soundFails=false;
 bool resources(unsigned source,Resources& out,std::string& e)override{++resourceCalls[source];out=bank;if(resourceFails)e="resources failed";return !resourceFails;}
 bool reserve(unsigned count,std::string& e)override{reserved=count;if(reserveFails)e="reserve failed";return !reserveFails;}
 bool allocate(Host& h,const Position&,float,std::string& e)override{++allocations;actors.emplace_back(new Creature);h.creature=actors.back().get();if(allocateFails)e="allocation failed";return !allocateFails;}
 bool cleanup(Host&,std::string& e)override{++cleanups;if(cleanupFails)e="cleanup failed";return !cleanupFails;}
 bool touchSound(Host&,Creature*,std::string& e)override{++sounds;if(soundFails)e="sound failed";return !soundFails;}
};
CatalogRow row(unsigned uid,unsigned source=91,unsigned count=1){CatalogRow r;r.course="tutorial";r.member="plantsgen.txt";r.sourceKey="literal-source:"+std::to_string(uid);r.index=uid;r.enemy.uid=uid;r.enemy.source=source;r.enemy.count=count;return r;}
void identityAndResources(){
 std::string e;for(auto source:{46u,47u,49u,51u,52u,80u,88u,90u,91u,92u})CHECK(supported(source));CHECK(!supported(50));
 auto literal=row(1);CHECK(decode(literal,e));
 for(auto source:{0u,48u,50u,53u,81u,89u,93u}){auto bad=literal;bad.enemy.source=source;CHECK(!decode(bad,e));}
 for(auto version:{"0000","0001","?????"}){auto bad=literal;bad.enemy.generatorVersion=version;CHECK(!decode(bad,e));}
 auto bad=literal;bad.enemy.generatorTail={"0"};CHECK(!decode(bad,e));bad=literal;bad.enemy.uid=0;CHECK(!decode(bad,e));
 bad=literal;bad.enemy.pelletColor=0;bad.enemy.pelletSize=1;CHECK(!decode(bad,e));
 ControlledEngine engine;Provider p(engine);auto zero=row(1,91,0),dead=row(2,88,2);dead.enemy.deathCount=2;
 std::vector<CatalogRow> rows{zero,dead,row(3,91,0)};
 engine.bank.collider=false;CHECK(!p.preflight(rows,e)&&engine.allocations==0);engine.bank.collider=true;
 engine.bank.duration=1;CHECK(!p.preflight(rows,e));engine.bank.duration=31;
 engine.bank.health=std::numeric_limits<float>::quiet_NaN();CHECK(!p.preflight(rows,e));engine.bank.health=100;
 engine.resourceCalls.clear();CHECK(p.preflight(rows,e));CHECK(engine.resourceCalls[91]==1&&engine.resourceCalls[88]==1);
 CHECK(p.reserve(rows,e)&&engine.reserved==0&&engine.allocations==0);
 Generator g;Creature* out=nullptr;CHECK(!p.birth(zero,&g,0,{},0,out,e)&&!out&&engine.allocations==0);
 CHECK(p.preflight(rows,e));auto duplicate=rows;duplicate.push_back(zero);CHECK(!p.preflight(duplicate,e));CHECK(!p.reserve(rows,e));
}
void reservationAndCleanup(){
 ControlledEngine engine;Provider p(engine);std::string e;std::vector<CatalogRow> rows{row(1),row(2,88,2),row(3,91,0)};
 CHECK(!p.reserve(rows,e));CHECK(p.preflight(rows,e));
 auto altered=rows;altered[0].sourceKey+="x";CHECK(!p.reserve(altered,e));altered=rows;altered[0].enemy.offset.z=1;CHECK(!p.reserve(altered,e));
 altered=rows;altered[0].enemy.treasureCode=1;CHECK(!p.reserve(altered,e));altered=rows;altered.pop_back();CHECK(!p.reserve(altered,e));
 altered=rows;altered.push_back(rows[0]);CHECK(!p.reserve(altered,e));engine.reserveFails=true;CHECK(!p.reserve(rows,e));engine.reserveFails=false;
 CHECK(p.reserve(rows,e)&&engine.reserved==3);CHECK(!p.reserve(rows,e));
 Generator a,b;Creature* actor=nullptr;CHECK(!p.birth(rows[0],nullptr,0,{},0,actor,e));CHECK(!p.birth(rows[0],&a,1,{},0,actor,e));
 CHECK(p.birth(rows[0],&a,0,{0,10,0},0,actor,e));CHECK(!p.preflight(rows,e));CHECK(!p.bind(rows[1],actor,7,e));CHECK(!p.bind(rows[0],actor,0,e));CHECK(p.bind(rows[0],actor,7,e));CHECK(!p.bind(rows[0],actor,8,e));
 Creature* second=nullptr;CHECK(p.birth(rows[1],&b,0,{},0,second,e));CHECK(!p.bind(rows[1],second,7,e));CHECK(p.bind(rows[1],second,8,e));
 Creature* duplicate=nullptr;CHECK(!p.birth(rows[1],&b,0,{},0,duplicate,e)&&!duplicate);
 CHECK(!p.release(actor,8,e));engine.cleanupFails=true;CHECK(!p.release(actor,7,e)&&p.lookup(actor));engine.cleanupFails=false;CHECK(p.release(actor,7,e)&&!p.lookup(actor));CHECK(p.release(second,8,e));
 engine.allocateFails=true;CHECK(!p.birth(rows[1],&b,1,{},0,actor,e)&&!actor&&p.size()==0);
 engine.cleanupFails=true;CHECK(!p.birth(rows[1],&b,1,{},0,actor,e)&&actor&&p.lookup(actor));CHECK(!p.release(actor,0,e));engine.cleanupFails=false;CHECK(p.release(actor,0,e));
 engine.allocateFails=false;CHECK(p.birth(rows[1],&b,1,{},0,actor,e));CHECK(p.release(actor,0,e));CHECK(p.preflight(rows,e)&&p.reserve(rows,e));
}
void forestIdentityAndResources(){
 ControlledEngine engine;Provider p(engine);std::string e;
 auto large=row(47,47),small=row(49,49),zero=row(147,47,0),dead=row(149,49,2);dead.enemy.deathCount=2;
 CHECK(decode(large,e)&&decode(small,e));
 auto bad=large;bad.enemy.generatorTail={"0"};CHECK(!decode(bad,e));bad=small;bad.enemy.generatorVersion="0001";CHECK(!decode(bad,e));
 std::vector<CatalogRow> rows{large,small,zero,dead,row(191,91,0),row(188,88,0)};
 CHECK(p.preflight(rows,e));for(auto source:{47u,49u,91u,88u})CHECK(engine.resourceCalls[source]==1);
 auto alias=rows;alias[0].enemy.source=91;CHECK(!p.reserve(alias,e));alias=rows;alias[1].enemy.source=47;CHECK(!p.reserve(alias,e));
 CHECK(p.reserve(rows,e)&&engine.reserved==2);Generator largeGen,smallGen;Creature* largeActor=nullptr;Creature* smallActor=nullptr;
 CHECK(p.birth(large,&largeGen,0,{},0,largeActor,e)&&p.birth(small,&smallGen,0,{},0,smallActor,e));
 CHECK(p.lookup(largeActor)->row.enemy.source==47&&p.lookup(smallActor)->row.enemy.source==49);
 CHECK(!p.bind(small,largeActor,47,e)&&!p.bind(large,smallActor,49,e));
 CHECK(p.bind(large,largeActor,47,e)&&p.bind(small,smallActor,49,e));
 Creature* noActor=nullptr;CHECK(!p.birth(zero,&largeGen,0,{},0,noActor,e)&&!noActor);CHECK(!p.birth(dead,&smallGen,1,{},0,noActor,e)&&!noActor);
 CHECK(p.release(largeActor,47,e)&&p.release(smallActor,49,e));CHECK(p.preflight(rows,e)&&p.reserve(rows,e));
}
void allNineOwnership(){
 ControlledEngine engine;Provider p(engine);std::string e;const unsigned sources[]={46,47,49,51,52,80,88,90,91};
 std::vector<CatalogRow> zeroRows,liveRows;for(auto source:sources){zeroRows.push_back(row(100+source,source,0));liveRows.push_back(row(100+source,source));}
 CHECK(p.preflight(zeroRows,e));for(auto source:sources)CHECK(engine.resourceCalls[source]==1);CHECK(p.reserve(zeroRows,e)&&engine.reserved==0&&engine.allocations==0);
 CHECK(p.preflight(liveRows,e));for(unsigned i=0;i<liveRows.size();++i){auto altered=liveRows;altered[i].enemy.source=sources[(i+1)%9];CHECK(!p.reserve(altered,e));}
 CHECK(p.reserve(liveRows,e)&&engine.reserved==9);Generator generators[9];Creature* actors[9]{};
 for(unsigned i=0;i<liveRows.size();++i){CHECK(p.birth(liveRows[i],&generators[i],0,{float(i),0,0},0,actors[i],e));CHECK(p.bind(liveRows[i],actors[i],1000+i,e));CHECK(p.lookup(actors[i])->row.enemy.source==sources[i]);}
 CHECK(p.size()==9);for(unsigned i=0;i<liveRows.size();++i)CHECK(!p.release(actors[i],1000+(i+1)%9,e));
 engine.cleanupFails=true;CHECK(!p.release(actors[8],1008,e)&&p.size()==9);engine.cleanupFails=false;
 for(unsigned i=0;i<liveRows.size();++i){CHECK(p.release(actors[i],1000+i,e));}
 CHECK(p.size()==0&&p.preflight(zeroRows,e)&&p.reserve(zeroRows,e));
}
void literalCylinderPlanes(){
 for(auto source:{51u,52u,80u,88u,90u}){CHECK(cylinderSource(source));}
 for(auto source:{46u,47u,49u,91u}){CHECK(!cylinderSource(source));}
 Cylinder c{{0,0,0},10,2};
 CHECK(!cylinderPlaneVisible(c,1,0,0,2));CHECK(cylinderPlaneVisible(c,1,0,0,1.99f));CHECK(!cylinderPlaneVisible(c,1,0,0,2.01f));
 CHECK(!cylinderPlaneVisible(c,0,1,0,10));CHECK(cylinderPlaneVisible(c,0,1,0,9.99f));CHECK(!cylinderPlaneVisible(c,0,1,0,10.01f));
 CHECK(!cylinderPlaneVisible(c,0,-1,0,0));CHECK(cylinderPlaneVisible(c,0,-1,0,-.01f));
 // Radial support on this tilted unit plane is1.6; the upper endpoint adds6.
 CHECK(cylinderPlaneVisible(c,0,.6f,.8f,7.5f));CHECK(!cylinderPlaneVisible(c,0,.6f,.8f,7.7f));
 // An enclosing sphere would incorrectly admit a tall cylinder behind x=3.
 CHECK(!cylinderPlaneVisible(c,1,0,0,3));
 auto ordinary=sourceCylinder(90,{10,20,30},1.57079632679f,4,8);CHECK(ordinary.bottom.x==10&&ordinary.bottom.y==20&&ordinary.bottom.z==30&&ordinary.radius==4&&ordinary.height==8);
 auto foxtail=sourceCylinder(88,{10,20,30},0,4,8);CHECK(foxtail.bottom.x==10&&foxtail.bottom.y==20&&foxtail.bottom.z==-20);
 foxtail=sourceCylinder(88,{10,20,30},1.57079632679f,4,8);CHECK(std::fabs(foxtail.bottom.x+40)<.001f&&std::fabs(foxtail.bottom.z-30)<.001f);
}
void brownLargeIdentityAndEnd(){
 ControlledEngine engine;Provider p(engine);std::string e;const unsigned sources[]={47,49,88,91,92};std::vector<CatalogRow> zeros;
 for(auto source:sources){auto literal=row(100+source,source,0);CHECK(decode(literal,e));auto sentinel=literal;sentinel.enemy.pelletColor=0;sentinel.enemy.pelletSize=1;CHECK(!decode(sentinel,e));zeros.push_back(literal);}
 CHECK(p.preflight(zeros,e));for(auto source:sources){CHECK(engine.resourceCalls[source]==1);}
 CHECK(p.reserve(zeros,e)&&engine.reserved==0&&engine.allocations==0);
 auto large=row(92,92),small=row(91,91);CHECK(p.preflight({large,small},e));auto alias=large;alias.enemy.source=91;CHECK(!p.reserve({alias,small},e));
 auto tail=large;tail.enemy.generatorTail={"0"};CHECK(!decode(tail,e));auto version=large;version.enemy.generatorVersion="0004";CHECK(!decode(version,e));
 engine.bank.duration=60;CHECK(p.preflight({large,small},e)&&p.reserve({large,small},e));Generator generator;Creature collider;Creature* actor=nullptr;
 CHECK(p.birth(large,&generator,0,{},0,actor,e)&&p.lookup(actor)->row.enemy.source==92);CHECK(!p.bind(small,actor,92,e));CHECK(p.bind(large,actor,92,e));
 CHECK(p.collision(actor,&collider,true,false,0,2,0,true,e));auto* h=p.lookup(actor);CHECK(h->active&&h->touched&&engine.sounds==1);
 CHECK(p.tick(actor,1.9f,true,e)&&h->active&&std::fabs(h->frame-57)<.001f);CHECK(p.tick(actor,.1f,true,e)&&!h->active&&!h->touched&&h->frame==59);
 CHECK(p.release(actor,92,e)&&p.size()==0);CHECK(p.preflight(zeros,e)&&p.reserve(zeros,e));
}
void touchAndTiming(unsigned source){
 ControlledEngine engine;Provider p(engine);std::string e;auto r=row(1,source);CHECK(p.preflight({r},e)&&p.reserve({r},e));Generator g;Creature collider;Creature* actor=nullptr;
 CHECK(p.birth(r,&g,0,{0,10,0},0,actor,e));Host* h=p.lookup(actor);CHECK(h&&!h->active&&!h->touched&&h->frame==0);
 CHECK(p.tick(actor,1,true,e)&&h->frame==0);
 CHECK(p.collision(actor,nullptr,true,false,10,2,0,true,e)&&!h->active);
 CHECK(p.collision(actor,&collider,true,true,10,2,0,true,e)&&!h->active);
 CHECK(p.collision(actor,&collider,true,false,10,2,0,false,e)&&!h->active);
 CHECK(p.collision(actor,&collider,true,false,4.99f,2,0,true,e)&&!h->active);
 CHECK(p.collision(actor,&collider,true,false,5,1,-1,true,e)&&!h->active);
 CHECK(p.collision(actor,&collider,false,false,5,0,-1.01f,true,e)&&h->active&&!h->touched&&engine.sounds==0);
 CHECK(p.tick(actor,0.1f,true,e)&&std::fabs(h->frame-3)<.0001f);
 engine.soundFails=true;CHECK(!p.collision(actor,&collider,true,false,5,1.01f,0,true,e)&&!h->touched&&h->active&&h->frame==3);
 engine.soundFails=false;
 CHECK(p.collision(actor,&collider,true,false,5,1.01f,0,true,e)&&h->touched&&engine.sounds==2&&h->frame==3);
 CHECK(p.collision(actor,&collider,true,false,5,2,0,true,e)&&engine.sounds==2&&h->frame==3);
 // Retail active animator advances even when visible model recalculation is skipped.
 CHECK(p.tick(actor,0.1f,false,e)&&std::fabs(h->frame-6)<.0001f);
 CHECK(!p.tick(actor,-1,true,e));CHECK(!p.tick(actor,std::numeric_limits<float>::quiet_NaN(),true,e));
 CHECK(p.tick(actor,1,true,e)&&!h->active&&!h->touched&&h->frame==30);
 CHECK(p.earthquake(actor,e)&&h->active&&h->frame==0&&engine.sounds==2);
 CHECK(p.tick(actor,0.1f,true,e));CHECK(p.earthquake(actor,e)&&h->frame==3);
 engine.cleanupFails=true;CHECK(!p.release(actor,0,e)&&p.lookup(actor)==h&&h->active);
 CHECK(p.tick(actor,0.1f,false,e)&&h->frame==6);engine.cleanupFails=false;
 CHECK(p.release(actor,0,e));CHECK(!p.tick(actor,0,true,e)&&!p.earthquake(actor,e));
}
int main(){try{identityAndResources();reservationAndCleanup();forestIdentityAndResources();allNineOwnership();brownLargeIdentityAndEnd();literalCylinderPlanes();for(auto source:{46u,47u,49u,51u,52u,80u,88u,90u,91u,92u})touchAndTiming(source);std::cout<<"Foliage sources46,47,49,51,52,80,88,90,91 literal identities, distinct banks, zero resource rows, lifecycle, cylinder planes and touch timing controls PASS; native gameplay not claimed\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
