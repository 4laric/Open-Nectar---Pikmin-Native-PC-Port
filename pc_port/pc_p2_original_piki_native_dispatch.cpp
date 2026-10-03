#include "pc_p2_original_piki_native_dispatch.h"
#include "pc_p2_original_piki_host.h"
#include "pc_p2_original_piki_origin.h"
#include "Piki.h"
#include "MapMgr.h"
#include <cmath>
namespace p2original {namespace piki {
namespace {
bool fail(std::string& e,const char* why){e=why;return false;}
bool finite(const Vector3f& p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);}
struct Read {
 NativeHostRead host;
 const captain::World* world=nullptr;
 std::uint64_t incarnation=0;
 captain::Phase phase=captain::Phase::Inactive;
 PhysicalFacts facts;
 bool factsCaptured=false;
 std::string campaign,fingerprint,catalog;
 Navi* captains[2]={};
};
bool sameOwner(const Read& r,std::string& e){
 NativeHostRead n;
 if(nativeHost(r.host.handle.body,n,e)!=NativeHostPhase::Committed||
    n.handle.body!=r.host.handle.body||n.handle.lifetime!=r.host.handle.lifetime||
    n.scene!=r.host.scene||n.stage!=r.host.stage||n.animator!=r.host.animator||
    n.physical!=r.host.physical||n.map!=r.host.map||
    pc_p2_original_captain_loaded_scene()!=r.host.scene||
    pc_p2_original_captain_world()!=r.world)
  return fail(e,"source dispatch borrowed owner changed");
 // Never dereference the old descriptor until canonical pointer equality.
 if(r.host.scene->incarnation()!=r.incarnation||r.world->incarnation()!=r.incarnation||
    r.world->phase()!=r.phase)
  return fail(e,"source dispatch scene phase/incarnation changed");
 if(r.host.scene->selectedCampaign()!=r.campaign||r.world->selectedCampaign()!=r.campaign||
    r.host.scene->selectedFingerprint()!=r.fingerprint||r.world->selectedFingerprint()!=r.fingerprint||
    r.host.scene->sourceCatalog()!=r.catalog||r.world->sourceCatalog()!=r.catalog||
    r.host.scene->captainAt(0)!=r.captains[0]||r.world->captainAt(0)!=r.captains[0]||
    r.host.scene->captainAt(1)!=r.captains[1]||r.world->captainAt(1)!=r.captains[1])
  return fail(e,"source dispatch canonical identity changed");
 if(!pc_p2_original_piki_body_current(r.host.handle.body,r.host.handle.lifetime))return fail(e,"source dispatch physical lifetime changed");
 return true;
}
bool same(const Read& r,std::string& e){
 if(!sameOwner(r,e))return false;
 if(!r.factsCaptured)return true;
  PhysicalFacts f;if(!nativePhysicalFacts(r.host.handle,r.host.physical,f,e))return false;
  if(f.alive!=r.facts.alive||f.frozen!=r.facts.frozen||f.movieExtra!=r.facts.movieExtra||
     f.movieActor!=r.facts.movieActor||f.pikiManagerFlag1!=r.facts.pikiManagerFlag1||
     f.naviManagerFlag1!=r.facts.naviManagerFlag1||f.mapAvailable!=r.facts.mapAvailable||
     f.stuck!=r.facts.stuck||f.targetCollision!=r.facts.targetCollision)
   return fail(e,"source dispatch physical eligibility changed");
 // Repeat all canonical stamps and the actual factory borrowed references
 // after the final PhysicalSource callback; no recursive facts query.
 return sameOwner(r,e);
}

Dispatch read(Piki* p,Read& r,std::string& e){
 const auto phase=nativeHost(p,r.host,e);
 if(phase==NativeHostPhase::Unowned)return Dispatch::Unowned;
 if(phase==NativeHostPhase::Partial)return Dispatch::Handled;
 if(phase!=NativeHostPhase::Committed)return Dispatch::Refused;
 r.world=pc_p2_original_captain_world();
 if(!r.host.scene||!r.host.stage||!r.world||!r.host.animator||!r.host.physical||
    r.host.handle.body!=p||!r.host.handle.lifetime||
    pc_p2_original_captain_loaded_scene()!=r.host.scene)
  {fail(e,"source dispatch missing canonical owner");return Dispatch::Refused;}
 r.incarnation=r.host.scene->incarnation();r.phase=r.world->phase();
 r.campaign=r.host.scene->selectedCampaign();r.fingerprint=r.host.scene->selectedFingerprint();
 r.catalog=r.host.scene->sourceCatalog();r.captains[0]=r.host.scene->captainAt(0);r.captains[1]=r.host.scene->captainAt(1);
 if(!r.incarnation||!same(r,e))return Dispatch::Refused;
 if(r.phase!=captain::Phase::GameWorldActive)return Dispatch::Handled;
 if(!nativePhysicalFacts(r.host.handle,r.host.physical,r.facts,e)||!same(r,e))return Dispatch::Refused;
 r.factsCaptured=true;
 if(!same(r,e))return Dispatch::Refused;
 if(!r.facts.alive)return Dispatch::Handled;
 return Dispatch::Unowned; // Internal operation admission; never returned to hooks.
}
template<class F> Dispatch invoke(Piki* p,std::string& e,F f){
 Read r;const auto result=read(p,r,e);
 if(result!=Dispatch::Unowned)return result;
 // A genuinely Unowned host has no lent handle; only this branch permits P1.
 if(!r.host.handle.body)return Dispatch::Unowned;
 return f(r)&&same(r,e)?Dispatch::Handled:Dispatch::Refused;
}
bool dtValid(float dt,std::string& e){return (std::isfinite(dt)&&dt>=0)||fail(e,"invalid source dispatch delta");}
bool animation(Read& r,float dt,std::string& e){
 if(r.facts.frozen)return true;
 return animate(r.host.handle,dt,e)&&same(r,e);
}
bool ai(Read& r,float dt,std::string& e){
 if(!r.facts.movieExtra&&r.facts.movieActor)return true;
 return update(r.host.handle,dt,e)&&same(r,e);
}
bool velocity(Read& r,float dt,std::string& e){
 if(!r.facts.movieExtra&&r.facts.movieActor)return true;
 if(r.facts.stuck||r.facts.targetCollision)return fail(e,"source stuck/stomach physics producer absent");
 // Airborne Flying retains its actual launched velocity; FakePiki only
 // interpolates target velocity with the floor bounce marker.
 if(!r.host.handle.body->mGroundTriangle)return true;
 return moveVelocity(r.host.handle,dt,e)&&same(r,e);
}
bool move(Read& r,float dt,bool gravity,std::string& e){
 auto* p=r.host.handle.body;
 if(!r.facts.movieExtra&&r.facts.pikiManagerFlag1){
  // Literal source FakePiki::doSimulation suppression. Host volatile velocity
  // and P1 stick/rope/fall-death handlers are never consulted.
  p->mVelocity.set(0,0,0);p->_B0.set(0,0,0);return true;
 }
 if(!r.facts.movieExtra&&r.facts.movieActor){
  // MovieActor suppresses velocity but still performs source collision trace.
  p->mVelocity.set(0,0,0);p->_B0.set(0,0,0);
 }
 if(r.facts.stuck||r.facts.targetCollision)return fail(e,"source stuck/stomach physics producer absent");
 if(!r.facts.mapAvailable||!r.host.map)return fail(e,"source physical map producer absent");
 RuntimeState state;if(!snapshot(r.host.handle,state)||!same(r,e))return false;
 if(state.state==State::Hanged)return true;
 if(gravity&&(!applyGravity(r.host.handle,dt,e)||!same(r,e)))return false;
 if(!finite(p->mSRT.t)||!finite(p->mVelocity))return fail(e,"nonfinite source map trace input");
 // Native MapMgr adapts bottom-origin to centre internally by +radius/-radius.
 // This is ONE real sphere trace; Creature::update's two P1 moveNew passes,
 // AICONST gravity, native air resistance and P1 floor death are bypassed.
 MoveTrace trace(p->mSRT.t,p->mVelocity,8.5f,false);
 trace.mP2WallThreshold=true;
 r.host.map->traceMove(p,trace,dt);
 if(!same(r,e))return false;
 if(!finite(trace.mPosition)||!finite(trace.mVelocity))return fail(e,"nonfinite source map trace output");
 p->mSRT.t=trace.mPosition;p->mVelocity=trace.mVelocity;
 return true;
}
}
Dispatch dispatchAnimation(Piki* p,float dt,std::string& e){return invoke(p,e,[&](Read& r){return dtValid(dt,e)&&animation(r,dt,e);});}
Dispatch dispatchAI(Piki* p,float dt,std::string& e){return invoke(p,e,[&](Read& r){return dtValid(dt,e)&&ai(r,dt,e);});}
Dispatch dispatchVelocity(Piki* p,float dt,std::string& e){return invoke(p,e,[&](Read& r){return dtValid(dt,e)&&velocity(r,dt,e);});}
Dispatch dispatchMove(Piki* p,float dt,bool gravity,std::string& e){return invoke(p,e,[&](Read& r){return dtValid(dt,e)&&move(r,dt,gravity,e);});}
Dispatch dispatchUpdate(Piki* p,float dt,std::string& e){return invoke(p,e,[&](Read& r){
 if(!dtValid(dt,e))return false;
 // Fail unsupported attachment before advancing clocks/Brain/native fields.
 if(r.facts.stuck||r.facts.targetCollision)return fail(e,"source attachment simulation unavailable");
 // FakePiki animation/velocity/gravity precede Piki FSM update; new Brain
 // target velocity is consumed on the following animation step.
 return animation(r,dt,e)&&velocity(r,dt,e)&&
        applyGravity(r.host.handle,dt,e)&&same(r,e)&&ai(r,dt,e)&&move(r,dt,false,e);
});}
Dispatch dispatchPostUpdate(Piki* p,std::string& e){return invoke(p,e,[&](Read& r){
 if(!r.facts.movieExtra&&r.facts.pikiManagerFlag1)return true;
 return nativeDispatchContacts(r.host.handle,e)&&same(r,e);
});}
Dispatch dispatchDraw(Piki* p,Graphics& g,std::string& e){return invoke(p,e,[&](Read& r){return r.host.animator->draw(r.host.handle,g,e)&&same(r,e);});}
Dispatch dispatchBounce(Piki* p,std::string& e){return invoke(p,e,[&](Read& r){return bounce(r.host.handle,e)&&same(r,e);});}
Dispatch dispatchCollision(Piki* p,const CollEvent& c,std::string& e){return invoke(p,e,[&](Read& r){return collision(r.host.handle,c,e)&&same(r,e);});}
} }
