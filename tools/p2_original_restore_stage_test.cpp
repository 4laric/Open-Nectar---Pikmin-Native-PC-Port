#include "pc_p2_original_gas.h"
#include "pc_p2_original_egg.h"
#include <cassert>
#include <iostream>
#include <functional>
using namespace p2original;
// Source-engine policy fakes only. This does not qualify native gameplay.
struct GasFake : gas::Engine {
 gas::Parameters p;unsigned draws=0,scans=0,finished=0,deaths=0,cleaned=0,actors=0;int stage=0;
 bool gateLiving=true,useBridge=true,useGate=false,failCleanup=false,nullBirth=false;float draw=0.5f;
 std::function<bool(gas::Host&,std::string&)> deathCallback;
 GasFake(){p.waitTime=2;p.activeTime=3;p.attackStartTime=1;p.stopTime=10;p.maxHealth=100;p.attackDamage=5;p.attackRadius=50;p.maxAttackRange=20;p.maxAttackAngle=10;}
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
 bool death(gas::Host& h,std::string& e)override{++deaths;return deathCallback?deathCallback(h,e):true;}
 bool cleanup(gas::Host&,std::string& e)override{if(failCleanup){e="injected cleanup failure";return false;}++cleaned;return true;}
};
struct EggFake:egg::Engine {
 unsigned field=0,dependencies=0,next=10,updates=0,restarts=0,resumes=0,inits=0,captures=0;
 bool drop=false,nullBirth=false,cleanupFail=false,contentsFail=false;
 std::string sequence;
 egg::Provider* retirement=nullptr;
 bool retireInUpdate=false;
 bool resources(egg::Resources& r,std::string&)override{r.parameters.health=50;r.parametersLoaded=r.model=r.collider=r.motion=r.contents=r.breakEffects=r.capture=true;return true;}
 bool commonResources(const CatalogRow&,std::string&)override{return true;}
 bool reserve(unsigned n,std::string&)override{field=n;return true;}
 bool reserveCaptured(unsigned n,std::string&)override{dependencies=n;return true;}
 bool allocate(egg::Host& h,const Position&,float,std::string&)override{h.creature=nullBirth?nullptr:reinterpret_cast<Creature*>(std::uintptr_t(next++));return true;}
 bool initialize(egg::Host& h,std::string&)override{++inits;h.dropGroup=drop;return true;}
 bool flags(egg::Host&,const egg::Flags&,std::string&)override{return true;}
 bool motion(egg::Host&,bool restart,bool stopped,std::string&)override{if(restart&&stopped)++restarts;else if(!restart&&!stopped)++resumes;return true;}
 bool capturedIdentity(Creature* p,std::string& s,std::string&)override{s="source16:epoch3:ordinal"+std::to_string(std::uintptr_t(p))+":cargo0";return true;}
 bool startCapture(egg::Host&,Creature*,void*,std::string&)override{++captures;return true;}
 bool endCapture(egg::Host&,std::string&)override{return true;}
 bool update(egg::Host& h,float,std::string& e)override{++updates;return retireInUpdate&&retirement?retirement->release(h.creature,h.token,e):true;}
 bool contents(egg::Host&,std::string& e)override{if(contentsFail){e="injected contents refusal";return false;}sequence+='C';return true;}
 bool breakEffects(egg::Host&,std::string&)override{sequence+='E';return true;}
 bool kill(egg::Host& h,std::string& e)override{sequence+='K';return retirement?retirement->release(h.creature,h.token,e):true;}
 bool cleanup(egg::Host&,std::string& e)override{if(cleanupFail){e="injected cleanup failure";return false;}return true;}
};
int main(){
 std::string error;CatalogRow gasRow;gasRow.enemy.source=21;gasRow.enemy.uid=123;gasRow.enemy.count=2;
 GasFake gf;gas::Provider gp(gf);auto gh=std::make_unique<gas::Host>();gh->row=gasRow;gh->generator=reinterpret_cast<Generator*>(1);gh->ordinal=0;gh->staged=true;gh->creature=reinterpret_cast<Creature*>(1000);gh->health=0;gh->state=gas::State::Dead;gh->deathReported=true;
 assert(!gp.adoptRestoredHost(gh,error)&&gh);assert(gp.preflight({gasRow},error)&&gp.reserve({gasRow},error));assert(gp.preflightRestoredHost(*gh,error));assert(gp.adoptRestoredHost(gh,error)&&!gh);auto* h=gp.lookup(reinterpret_cast<Creature*>(1000));assert(h&&h->health==0&&h->staged&&h->parameters.maxHealth==100);assert(gf.draws==0&&gf.deaths==0&&gf.scans==0);
 assert(gp.bind(gasRow,h->creature,7,error));assert(gp.tick(h->creature,50,gas::Event::End,error));assert(!gp.damage(h->creature,reinterpret_cast<Creature*>(1),false,{},1,error)&&error.empty());assert(gf.draws==0&&gf.deaths==0&&gf.scans==0);
 auto duplicate=std::make_unique<gas::Host>(*h);duplicate->token=0;duplicate->creature=reinterpret_cast<Creature*>(1001);assert(!gp.adoptRestoredHost(duplicate,error)&&duplicate);assert(gp.release(h->creature,7,error)&&gp.size()==0&&gf.cleaned==1&&gf.deaths==0);
 CatalogRow eggRow;eggRow.enemy.source=37;eggRow.enemy.uid=456;eggRow.enemy.count=2;EggFake ef;egg::Provider ep(ef);assert(ep.preflight({eggRow},error)&&ep.reserve({eggRow},error)&&ep.reserveCaptured(2,error));
 auto eh=std::make_unique<egg::Host>();eh->row=eggRow;eh->generator=reinterpret_cast<Generator*>(2);eh->staged=true;eh->creature=reinterpret_cast<Creature*>(2000);eh->health=0;eh->dropGroup=true;assert(ep.adoptRestoredHost(eh,error)&&!eh);auto* egg=ep.lookup(reinterpret_cast<Creature*>(2000));assert(egg&&egg->parameters.health==50&&egg->health==0);assert(ep.bind(eggRow,egg->creature,8,error));assert(ep.tick(egg->creature,10,egg::Event::End,error));assert(!ep.damage(egg->creature,10,5,error)&&error.empty());assert(ep.bounce(egg->creature,error)&&ep.collision(egg->creature,reinterpret_cast<Creature*>(9),false,error));assert(ef.updates==0&&ef.restarts==0&&ef.resumes==0&&ef.sequence.empty());
 auto cargo=std::make_unique<egg::Host>();cargo->staged=cargo->dependent=true;cargo->dependentIdentity="actual catalog:uid:ordinal:epoch:activation:Egg:0";cargo->creature=reinterpret_cast<Creature*>(2001);cargo->health=0;cargo->flags.invulnerable=true;assert(ep.adoptRestoredHost(cargo,error)&&!cargo);auto* c=ep.lookup(reinterpret_cast<Creature*>(2001));assert(ep.tick(c->creature,10,egg::Event::End,error)&&ef.sequence.empty());assert(!ep.damage(c->creature,1,1,error)&&error.empty());
 auto duplicateCargo=std::make_unique<egg::Host>(*c);duplicateCargo->creature=reinterpret_cast<Creature*>(2002);assert(!ep.adoptRestoredHost(duplicateCargo,error)&&duplicateCargo);assert(ep.release(c->creature,0,error));assert(ep.release(egg->creature,8,error)&&ep.size()==0);assert(ef.sequence.empty()&&ef.inits==0&&ef.captures==0);
 std::cout<<"PASS prepared no-init Host adoption/duplicate frontiers/staged callbacks/retained dead cleanup policy; no gameplay claim\n";
}
