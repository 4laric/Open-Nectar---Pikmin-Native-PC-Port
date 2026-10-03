#include "pc_p2_original_gas.h"
#include <cassert>
#include <iostream>
#include <limits>
using namespace p2original;
struct Fake : gas::Engine {
 gas::Parameters p;unsigned draws=0,scans=0,finished=0,deaths=0,cleaned=0,actors=0;int stage=0;
 bool gateLiving=true,useBridge=true,useGate=false,failCleanup=false,nullBirth=false;float draw=0.5f;
 Fake(){p.waitTime=2;p.activeTime=3;p.attackStartTime=1;p.stopTime=10;p.maxHealth=100;p.attackDamage=5;p.attackRadius=50;p.maxAttackRange=20;p.maxAttackAngle=10;}
 bool resources(gas::Resources& r,std::string&)override{r.parameters=p;r.parametersLoaded=r.model=r.collider=r.effects=true;r.clips.fill(true);return true;}
 bool commonResources(const CatalogRow&,std::string&)override{return true;}
 bool reserve(unsigned n,std::string&)override{actors=n;return true;}
 bool allocate(gas::Host& h,const Position&,float,std::string&)override{h.creature=nullBirth?nullptr:reinterpret_cast<Creature*>(std::uintptr_t(100+actors++));return true;}
 bool unitDraw(float& d,std::string&)override{++draws;d=draw;return true;}
 bool flags(gas::Host&,const gas::Flags&,std::string&)override{return true;}
 bool motion(gas::Host&,unsigned,std::string&)override{return true;}
 bool finishMotion(gas::Host&,std::string&)override{++finished;return true;}
 bool gasEffect(gas::Host&,bool,std::string&)override{return true;}
 bool updateEffectLod(gas::Host&,std::string&)override{return true;}
 bool findLivingLinks(gas::Host&,void*& b,void*& g,std::string&)override{b=useBridge?this:nullptr;g=useGate?this:nullptr;return true;}
 bool bridgeStage(void*,int& s,std::string&)override{s=stage;return true;}
 bool gateAlive(void*,bool& alive,std::string&)override{alive=gateLiving;return true;}
 bool gasScan(gas::Host&,std::string&)override{++scans;return true;}
 bool attackSound(gas::Host&,std::string&)override{return true;}
 bool death(gas::Host&,std::string&)override{++deaths;return true;}
 bool cleanup(gas::Host&,std::string& e)override{if(failCleanup){e="injected cleanup failure";return false;}++cleaned;return true;}
};
CatalogRow row(){CatalogRow r;r.enemy.source=21;r.enemy.uid=123;r.enemy.count=2;r.sourceKey="day5:pipe";return r;}
Creature* start(gas::Provider& provider,CatalogRow r,std::string& e,unsigned ordinal=0,unsigned token=1){
 Creature* actor=nullptr;assert(provider.birth(r,reinterpret_cast<Generator*>(1),ordinal,{},0,actor,e));assert(provider.bind(r,actor,token,e));return actor;
}
int main(){
 std::string e;CatalogRow r=row();Fake f;gas::Provider provider(f);
 assert(gas::decode(r,e));auto bad=r;bad.enemy.source=16;assert(!gas::decode(bad,e));bad=r;bad.enemy.generatorTail={"0"};assert(!gas::decode(bad,e));
 assert(provider.preflight({r},e));bad=r;bad.enemy.pelletMaximum++;assert(!provider.reserve({bad},e));assert(provider.reserve({r},e));
 Creature* a=start(provider,r,e);auto* h=provider.lookup(a);assert(h&&h->timer==1&&f.draws==1&&!h->flags.living);
 Creature* duplicate=nullptr;assert(!provider.birth(r,reinterpret_cast<Generator*>(1),0,{},0,duplicate,e));
 assert(provider.tick(a,1,gas::Event::None,e));assert(h->state==gas::State::Wait&&!h->flags.living); // strict timer == threshold
 assert(provider.tick(a,0.01f,gas::Event::None,e));assert(h->state==gas::State::Attack&&h->timer==0&&h->effectActive);
 assert(provider.tick(a,1,gas::Event::None,e)&&f.scans==0); // strict attackStart
 assert(provider.tick(a,0.01f,gas::Event::None,e)&&f.scans==1&&!h->flags.living); // nonliving still emits
 f.stage=1;assert(provider.tick(a,0,gas::Event::None,e)&&h->flags.living);
 f.stage=0;assert(provider.tick(a,0,gas::Event::None,e)&&h->flags.living); // living is monotonic
 h->timer=3;assert(provider.tick(a,0.1f,gas::Event::None,e)&&f.finished==0); // check before increment
 assert(provider.tick(a,0,gas::Event::None,e)&&f.finished==1);
 assert(provider.tick(a,0,gas::Event::End,e)&&h->state==gas::State::Wait&&!h->effectActive&&h->timer==0);
 auto attacker=reinterpret_cast<Creature*>(2);
 assert(!provider.damage(a,attacker,true,{},100,e));assert(!provider.damage(a,nullptr,false,{},100,e));
 assert(!provider.damage(a,attacker,false,{0,20,0},100,e));assert(!provider.damage(a,attacker,false,{0,-10,0},100,e));
 assert(provider.damage(a,attacker,false,{1000,19,1000},100,e)); // source damage has no horizontal bound
 assert(provider.tick(a,0,gas::Event::None,e)&&h->state==gas::State::Dead&&f.deaths==1);
 assert(provider.size()==1&&!h->flags.living&&h->flags.untargetable&&h->flags.invulnerable&&!h->flags.lifeGauge);
 assert(provider.tick(a,10,gas::Event::End,e)&&f.deaths==1);assert(!provider.damage(a,attacker,false,{},1,e));
 assert(!provider.release(a,2,e));f.failCleanup=true;assert(!provider.release(a,1,e)&&provider.size()==1);f.failCleanup=false;
 assert(provider.release(a,1,e)&&provider.size()==0);assert(provider.preflight({r},e)&&provider.reserve({r},e));
 a=start(provider,r,e);assert(f.draws==2);assert(provider.release(a,1,e));
 assert(gas::nearbyLink({},{74.9f,24.9f,74.9f}));assert(!gas::nearbyLink({},{75,0,0}));assert(!gas::nearbyLink({},{0,25,0}));assert(!gas::nearbyLink({},{0,0,-75}));
 assert(gas::gasContains({},{49.9f,19.9f,0},f.p));assert(!gas::gasContains({},{50,0,0},f.p));assert(!gas::gasContains({},{0,20,0},f.p));assert(!gas::gasContains({},{0,-10,0},f.p));
 // Continuous discharge waitTime==0 still consumes init draw and never times out.
 Fake continuous;continuous.p.waitTime=0;gas::Provider cp(continuous);assert(cp.preflight({r},e)&&cp.reserve({r},e));a=start(cp,r,e);
 assert(cp.tick(a,0.1f,gas::Event::None,e));assert(cp.tick(a,100,gas::Event::None,e)&&continuous.finished==0&&continuous.draws==1);
 assert(cp.damage(a,attacker,false,{},100,e));assert(cp.tick(a,0,gas::Event::None,e)&&continuous.finished==1&&continuous.deaths==0); // attack death waits authored END
 assert(cp.tick(a,0,gas::Event::End,e)&&continuous.deaths==1);assert(cp.release(a,1,e));
 Fake gate;gate.useBridge=false;gate.useGate=true;gas::Provider gp(gate);assert(gp.preflight({r},e)&&gp.reserve({r},e));a=start(gp,r,e);
 assert(gp.tick(a,0,gas::Event::None,e)&&!gp.lookup(a)->flags.living);gate.gateLiving=false;assert(gp.tick(a,0,gas::Event::None,e)&&gp.lookup(a)->flags.living);assert(gp.release(a,1,e));
 Fake unresolved;unresolved.p.maxHealth=0;gas::Provider no(unresolved);assert(!no.preflight({r},e));
 Fake null;null.nullBirth=true;gas::Provider np(null);r.enemy.deathCount=1;assert(np.preflight({r},e)&&np.reserve({r},e)&&null.actors==2);
 a=reinterpret_cast<Creature*>(3);assert(np.birth(r,reinterpret_cast<Generator*>(1),0,{},0,a,e)&&!a&&null.draws==0);
 assert(!np.birth(r,reinterpret_cast<Generator*>(1),0,{},0,a,e));
 std::cout<<"Original GasHiba source policy checks passed (fake engine; no gameplay claim)\n";
}
