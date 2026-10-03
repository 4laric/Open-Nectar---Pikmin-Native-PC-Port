#include "pc_p2_original_wisp.h"
#include <cassert>
#include <iostream>
using namespace p2original;using namespace p2original::wisp;
struct Forbidden:Engine {
 unsigned callbacks=0;Resources r;
 Forbidden(){r.parameters={90,1.75f,20,400,.5f,35,180,250,99999};r.model=r.collider=r.waterJoint=r.glowJoint=r.egg=true;r.motions.fill(true);}
 bool called(){++callbacks;return false;}
 bool resources(Resources& out,std::string&)override{out=r;return true;}
 bool reserve(unsigned,unsigned,std::string&)override{return true;}
 bool allocate(Host&,const Position&,float,std::string&)override{return called();}
 bool attachEgg(Host&,Creature*&,std::string&)override{return called();}
 bool setPosition(Host&,const Position&,std::string&)override{return called();}
 bool facing(Host&,float,std::string&)override{return called();}
 bool flags(Host&,bool,bool,bool,bool,std::string&)override{return called();}
 bool motion(Host&,unsigned,bool,std::string&)override{return called();}
 bool effect(Host&,const char*,std::string&)override{return called();}
 bool appear(const Host&,bool&,std::string&)override{return called();}
 bool visible(const Host&,bool&,std::string&)override{return called();}
 bool position(const Host&,Position&,std::string&)override{return called();}
 bool floor(const Position&,float&,std::string&)override{return called();}
 bool velocity(Host&,const Position&,std::string&)override{return called();}
 bool releaseEgg(Host&,Creature*,std::string&)override{return called();}
 bool kill(Host&,std::string&)override{return called();}
 bool cleanup(Host&,std::string&)override{return called();}
};
int main(){
 Forbidden engine;Provider p(engine);CatalogRow row;row.enemy.uid=0x52000001;row.enemy.source=16;row.enemy.count=2;row.enemy.generatorVersion="0000";row.enemy.generatorTail={"200","30"};int gen=0,body[4]{};auto* generator=reinterpret_cast<Generator*>(&gen);std::string e;
 auto host=[&](unsigned ordinal,unsigned bodyIndex){auto h=std::make_unique<Host>();h->row=row;h->generator=generator;h->ordinal=ordinal;h->creature=reinterpret_cast<Creature*>(&body[bodyIndex]);h->staged=true;h->state=State::Move;h->spawn[1]={-30,90,200};h->spawn[0]={0,90,0};h->scale=1;h->timer=.25f;return h;};
 auto a=host(0,0);assert(!p.adoptRestoredHost(a,e)&&a&&engine.callbacks==0);assert(p.preflight({row},e)&&p.reserve({row},e));
 a->staged=false;assert(!p.adoptRestoredHost(a,e)&&a);a->staged=true;a->token=1;assert(!p.adoptRestoredHost(a,e)&&a);a->token=0;a->egg=reinterpret_cast<Creature*>(&body[3]);assert(!p.adoptRestoredHost(a,e)&&a);a->egg=nullptr;
 auto altered=host(0,1);altered->row.enemy.generatorTail[0]="201";assert(!p.adoptRestoredHost(altered,e)&&altered);auto badOrdinal=host(2,1);assert(!p.adoptRestoredHost(badOrdinal,e)&&badOrdinal);
 auto* actor=a->creature;assert(p.adoptRestoredHost(a,e)&&!a);auto* adopted=p.lookup(actor);assert(adopted&&adopted->staged&&adopted->state==State::Move&&adopted->parameters.moveSpeed==35&&adopted->initial.fly==200&&adopted->timer==.25f);
 auto duplicate=host(0,1);assert(!p.adoptRestoredHost(duplicate,e)&&duplicate);duplicate=host(1,0);assert(!p.adoptRestoredHost(duplicate,e)&&duplicate);
 assert(p.tick(actor,100,Event::ReleaseEgg,e));assert(p.flyingCollision(actor,true,e));assert(adopted->state==State::Move&&adopted->timer==.25f&&engine.callbacks==0);assert(p.bind(row,actor,77,e));assert(adopted->staged);
 adopted->state=State::Drop;adopted->egg=reinterpret_cast<Creature*>(&body[3]);assert(p.tick(actor,100,Event::ReleaseEgg,e));assert(!adopted->released&&adopted->egg&&engine.callbacks==0);adopted->state=State::Move;adopted->egg=nullptr;
 auto dead=host(1,1);dead->state=State::Dead;dead->dead=true;dead->released=false;auto* deadActor=dead->creature;assert(p.adoptRestoredHost(dead,e)&&!dead);assert(p.tick(deadActor,100,Event::End,e)&&p.lookup(deadActor)->dead&&engine.callbacks==0);
 p.publishRestoredHosts();assert(!adopted->staged&&!p.lookup(deadActor)->staged&&engine.callbacks==0);assert(!p.flyingCollision(actor,true,e)&&engine.callbacks==1);assert(adopted->state==State::Drop);
 p.retiredNative(actor);p.retiredNative(deadActor);
 std::cout<<"Wisp cold policy PASS admission, adoption, staged callback suppression and publication; no native gameplay claim\n";
}
