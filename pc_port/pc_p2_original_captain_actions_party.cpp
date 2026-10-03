#include "pc_p2_original_captain_actions_party.h"
#include <cmath>
#include <set>
namespace p2original { namespace captain { namespace party {
bool switchAllowed(const WorldFacts& w,const CaptainFacts& c){
 return w.active&&!w.softPaused&&!w.multiplayer&&w.demoInactive&&w.switchUnlocked&&c.alive
  &&c.state!=StateId::Nuku&&c.state!=StateId::NukuAdjust&&c.state!=StateId::Punch;
}
bool whistleAllowed(const WorldFacts& w,const CaptainFacts& c){
 return c.alive&&!c.controller&&(c.state==StateId::Walk||c.state==StateId::Pellet)
  &&(w.mode!=0||w.day!=0||w.reunited);
}
bool needsChange(StateId s){return s==StateId::Walk||s==StateId::Follow;}
namespace {
bool finite(Vec3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
Vec3 difference(Vec3 a,Vec3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
float normalize(Vec3& v){float d=std::sqrt(v.x*v.x+v.y*v.y+v.z*v.z);if(d>0){v.x/=d;v.y/=d;v.z/=d;}return d;}
void move(Vec3& v,Vec3 direction,float d){v.x+=direction.x*d;v.y+=direction.y*d;v.z+=direction.z*d;}
void separate(Group& g,Vec3 captain){Vec3 diff=difference(g.center,captain);float d=normalize(diff)-g.radius-25;if(d<20)move(g.center,diff,20-d);}
}
bool dismissGroups(const std::vector<Member>& members,Vec3 captain,Vec3 other,bool otherAlive,
 std::array<Group,8>& output,std::string& error){
 if(!finite(captain)||!finite(other)){error="nonfinite actual captain position";return false;}
 std::array<Group,8> groups{};std::set<std::pair<Piki*,std::uint64_t>> identities;
 unsigned count=0;
 for(const auto& m:members){
  if(!m.alive||!m.releasable)continue;
  if(!m.handle.actor||!m.handle.lifetime||m.kind>=8||!finite(m.position)
   ||!identities.emplace(m.handle.actor,m.handle.lifetime).second||++count>100){error="invalid source dismissal member";return false;}
  auto& g=groups[m.kind];++g.count;move(g.center,m.position,1);
 }
 for(auto& g:groups)if(g.count){float scale=1.0f/g.count;g.center.x*=scale;g.center.y*=scale;g.center.z*=scale;g.radius=std::sqrt(float(g.count))*6.25f;}
 for(unsigned pass=0;pass<4;++pass)for(unsigned c=0;c<8;++c){
  if(groups[c].count){separate(groups[c],captain);if(otherAlive)separate(groups[c],other);}
  for(unsigned j=c+1;j<8;++j)if(groups[c].count&&groups[j].count){
   auto diff=difference(groups[c].center,groups[j].center);
   float d=normalize(diff)-groups[c].radius-groups[j].radius;
   if(d<20){move(groups[c].center,diff,20-d);move(groups[j].center,diff,-(20-d));}
  }
 }
 output=groups;return true;
}
} } }
#if defined(PIKI_PC_PORT)
#include "pc_p2_original_captain_states.h"
#include "pc_p2_original_captain_motion.h"
#include "Navi.h"
namespace p2original { namespace captain { namespace party {
namespace {
bool fail(std::string& e,const char* s){e=s;return false;}
PartySource* source(Navi* n,std::string& error){
 auto* p=pc_p2_original_captain_party_source(n);auto* scene=pc_p2_original_captain_loaded_scene();
 auto* world=pc_p2_original_captain_world();
 if(!n||!p||!scene||&p->scene()!=scene||!world||world->phase()!=Phase::GameWorldActive
  ||world->incarnation()!=scene->incarnation()||(n!=scene->captainAt(0)&&n!=scene->captainAt(1))){fail(error,"missing actual source party scene/roster");return nullptr;}
 return const_cast<PartySource*>(p);
}
Navi* other(const PartySource& p,Navi* n){return p.scene().captainAt(p.scene().captainAt(0)==n?1:0);}
}
bool whistleCaptain(Navi* n,Navi* caller,bool combine,bool newToParty,std::string& e){
 (void)combine; // Retail actNavi does not inspect mDoCombine; Piki receiver does.
 auto* p=source(n,e);if(!p||caller!=other(*p,n))return fail(e,"whistle caller not other actual source captain");
 WorldFacts w;CaptainFacts c;Vec3 position;
 if(!p->world(w,e)||!p->captain(*n,c,position,e))return false;
 // Already-Follow is NOT callable in retail; do not invent a no-op success.
 if(!whistleAllowed(w,c))return fail(e,"source captain whistle not admitted");
 if(!p->follow(*n,newToParty,e))return false;
 std::vector<Member> members;if(!p->members(*n,members,e))return false;
 // Snapshot BEFORE stimuli mutate the CPlate, exactly as source actNavi.
 for(const auto& m:members)if(!p->whistleMember(*caller,m.handle,true,true,e))return false;
 return true;
}
bool dismissCaptain(Navi* n,std::string& e){
 auto* p=source(n,e);if(!p)return false;CaptainFacts c;Vec3 pos;
 if(!p->captain(*n,c,pos,e))return false;
 if(c.controller||c.state!=StateId::Follow)return fail(e,"source Kaisan recipient not uncontrolled Follow");
 return pc_p2_original_captain_transit(n,StateId::Walk,e);
}
bool releasePikis(Navi* n,bool& released,std::string& e){
 auto* p=source(n,e);if(!p)return false;WorldFacts w;
 if(!p->world(w,e))return false;
 if(!w.active){released=false;return true;}
 Navi* partner=other(*p,n);CaptainFacts self,op;Vec3 pos,opos;
 if(!partner||!p->captain(*n,self,pos,e)||!p->captain(*partner,op,opos,e))return false;
 bool dismissNavi=op.state==StateId::Follow;
 if(!op.controller&&dismissNavi&&!pc_p2_original_captain_transit(partner,StateId::Walk,e))return false;
 std::vector<Member> members;if(!p->members(*n,members,e))return false;
 std::array<Group,8> groups;
 if(!dismissGroups(members,pos,opos,op.alive,groups,e))return false;
 unsigned count=0;for(const auto& g:groups)count+=g.count;
 if((dismissNavi||count)&&!p->dismissSound(*n,e))return false;
 if(!count){released=dismissNavi;return true;}
 for(const auto& m:members)if(m.alive&&m.releasable){const auto& g=groups[m.kind];if(!p->freeMember(*n,m.handle,g.radius,g.center,true,e))return false;}
 if(!p->disbandTimer(*n,60,e))return false;
 released=true;return true;
}
bool switchCaptain(Navi* n,std::string& e){
 auto* p=source(n,e);if(!p)return false;Navi* partner=other(*p,n);
 WorldFacts w;CaptainFacts c;Vec3 pos;
 if(!partner||!p->world(w,e)||!p->captain(*partner,c,pos,e))return false;
 if(!switchAllowed(w,c))return fail(e,"source Y switch not admitted");
 if(!p->togglePlayer(*n,*partner,e)||!p->changeVoice(*partner,e))return false;
 if(c.state==StateId::Follow){
  // Retail stimulate's result is ignored here. A noncallable old captain
  // must not undo the already-completed toggle or skip target Change.
  std::string ignored;whistleCaptain(n,partner,false,false,ignored);
 }
 if(!p->captain(*partner,c,pos,e))return false;
 return !needsChange(c.state)||pc_p2_original_captain_transit(partner,StateId::Change,e);
}
} // party
namespace {
class Change final:public NativeState {
 bool finished_=false;
public:
 Change():NativeState(StateId::Change){}
 bool sourceInvincible()const final{return false;}
 void init(Navi* n)override{
  finished_=false;std::string e;auto* s=pc_p2_original_captain_party_source(n);
  party::CaptainFacts c;actions::Vec3 pos;
  if(!s||!s->captain(*n,c,pos,e))return;
  if(!c.movieActor){auto* bank=pc_p2_original_captain_source_bank();if(bank)bank->start(n,static_cast<Motion>(32),e);}
 }
 void exec(Navi* n)override{
  std::string e;auto* s=pc_p2_original_captain_party_source(n);party::CaptainFacts c;actions::Vec3 pos;
  if(!s||!s->captain(*n,c,pos,e))return;
  if(c.movieActor)pc_p2_original_captain_transit(n,StateId::Walk,e);
  n->mTargetVelocity.set(0,0,0);
  if(finished_)pc_p2_original_captain_transit(n,StateId::Walk,e);
 }
 void sourceKey(Navi* n,int key){if(key==1000){finished_=true;std::string e;auto* b=pc_p2_original_captain_source_bank();if(b)b->start(n,Motion::Walk,e);}}
};
}
void registerPartyStates(NaviStateMachine& fsm){fsm.registerState(new Change);}
bool partyKey(Navi* n,int key){
 if(!n)return false;
 auto* change=dynamic_cast<Change*>(n->getCurrState());
 if(!change)return false;
 change->sourceKey(n,key);return true;
}
} }
#endif
