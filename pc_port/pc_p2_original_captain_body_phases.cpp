#include "pc_p2_original_captain_body_phases.h"
#include "Navi.h"
#include "NaviState.h"
#include <array>
#include <cmath>
#include <limits>
namespace p2original {namespace captain {namespace bodyphases {
namespace {
bool fail(std::string& e,const char* s){e=s;return false;}
bool finite(Vec3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
Vec3 get(const Vector3f& v){return {v.x,v.y,v.z};}
void put(Vector3f& v,Vec3 p){v.set(p.x,p.y,p.z);}
Vec3 add(Vec3 a,Vec3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
Vec3 sub(Vec3 a,Vec3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
Vec3 scale(Vec3 a,float f){return {a.x*f,a.y*f,a.z*f};}
float dot(Vec3 a,Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
float length(Vec3 v){return std::sqrt(dot(v,v));}
Vec3 normal(Vec3 v){float d=length(v);return d>0?scale(v,1/d):v;}
// Process-monotonic genuine source birth identity prevents scene-slot ABA
// across composition recreation, independently of allocator pointer reuse.
std::uint64_t nextInitializationSerial=0;
constexpr float pi=3.14159265358979323846f;
float wrap(float a){if(a<0)a+=2*pi;if(a>=2*pi)a-=2*pi;return a;}
float angleDistance(float a,float b){float d=wrap(a-b);return d>=pi?-wrap(2*pi-d):d;}
bool exactState(const Navi* n){
 auto* actor=const_cast<Navi*>(n);auto* current=actor->getCurrState();auto* typed=dynamic_cast<State*>(current);
 if(!typed||typed->nativeState()!=current||!actor->mStateMachine||current->getID()!=48+int(typed->sourceStateId()))return false;
 for(int i=0;i<actor->mStateMachine->mStateCount;++i)if(actor->mStateMachine->mStates[i]==current)return true;
 return false;
}
}
struct Owner::Impl {
 struct Entry {Navi* actor=nullptr;Fields fields;};
 Provider& provider;SourceSceneTrace& trace;SourceBank& bank;Owner* self=nullptr;
 const LoadedScene* scene;std::uint64_t epoch;
 std::string campaign,fingerprint,catalog;std::array<Entry,2> entries{};
 unsigned operationDepth=0,flagEventDepth=0;bool managerChild=false;std::uint64_t mutationRevision=0;
 struct RoomCall {Navi* actor;int room;const void* state;std::uint64_t birth,revision;};
 const RoomCall* roomCall=nullptr;
 struct RoomVisit {
  Impl& owner;RoomCall call;const RoomCall* prior;
  RoomVisit(Impl& o,Navi* n,int room):owner(o),call{n,room,n->getCurrState(),o.entry(n)->fields.initializationSerial,o.mutationRevision},prior(o.roomCall){o.roomCall=&call;}
  ~RoomVisit(){owner.roomCall=prior;}
 };
 struct Operation {
  Impl& owner;bool entered=false;
  Operation(Impl& o,std::string& e,bool childFlag=false):owner(o){
   bool child=o.managerChild;o.managerChild=false;
   if(o.operationDepth&&!child&&!(childFlag&&o.flagEventDepth)){++o.mutationRevision;fail(e,"source body nested mutation refused");return;}
   ++o.operationDepth;entered=true;
  }
  ~Operation(){if(entered)--owner.operationDepth;}
 };
 struct FlagEvent {Impl& owner;FlagEvent(Impl& o):owner(o){++o.flagEventDepth;}~FlagEvent(){--owner.flagEventDepth;}};
 Impl(Provider& p,SourceSceneTrace& t,SourceBank& b):provider(p),trace(t),bank(b),scene(&p.scene()),epoch(scene->incarnation()),campaign(scene->selectedCampaign()),fingerprint(scene->selectedFingerprint()),catalog(scene->sourceCatalog()){}
 Entry* entry(const Navi* n){for(auto& a:entries)if(a.actor==n)return &a;return nullptr;}
 bool auth(const Navi* n,std::string& e,bool loading=false,bool composition=true)const{
  auto* s=pc_p2_original_captain_loaded_scene();auto* w=pc_p2_original_captain_world();
  if(!s||s!=scene||!w||!epoch||campaign.empty()||fingerprint.empty()||catalog.empty()||s->incarnation()!=epoch||s->selectedCampaign()!=campaign||s->selectedFingerprint()!=fingerprint||s->sourceCatalog()!=catalog
   ||&provider.scene()!=s||&trace.scene()!=s||w->incarnation()!=epoch||w->selectedCampaign()!=campaign||w->selectedFingerprint()!=fingerprint||w->sourceCatalog()!=catalog
   ||!s->captainAt(0)||!s->captainAt(1)||s->captainAt(0)==s->captainAt(1)||w->captainAt(0)!=s->captainAt(0)||w->captainAt(1)!=s->captainAt(1)
   ||(w->phase()!=Phase::GameWorldActive&&!(loading&&w->phase()==Phase::Loading))||pc_p2_original_captain_source_bank()!=&bank)
   return fail(e,"source body phase lost canonical scene/world/roster/bank");
  if(!n)return true;
  if((n!=s->captainAt(0)&&n!=s->captainAt(1))||(composition&&pc_p2_original_captain_body_phase_owner(n)!=self))return fail(e,"source body phase composition/actor expired");
  bool alive;MotionState motion;if(!pc_p2_original_captain_actor_lifetime(n,alive)||!bank.state(n,motion,e))return fail(e,"source body phase missing actual CF lifetime/bank binding");
  if(w->phase()==Phase::GameWorldActive&&!exactState(n))return fail(e,"source body phase lacks current registered typed source FSM");
  return true;
 }
 template<class F>bool call(Navi* n,std::string& e,F f,bool stateChange=false){
  auto* a=entry(n);if(!a||!auth(n,e))return fail(e,"source body phase actor not initialized");
  auto serial=a->fields.initializationSerial;auto revision=mutationRevision;auto* current=n->getCurrState();
  if(!f())return false;
  a=entry(n);if(!a||a->fields.initializationSerial!=serial||mutationRevision!=revision||!auth(n,e)||(!stateChange&&n->getCurrState()!=current))return fail(e,"source body callback expired exact actor/scene/state generation");
  return true;
 }
 bool facts(Navi* n,Facts& f,std::string& e){
  if(!call(n,e,[&]{return provider.facts(*n,f,e);}))return false;
  return std::isfinite(f.deltaTime)&&f.deltaTime>=0&&std::isfinite(f.gravity)&&f.gravity>=0&&finite(get(n->mSRT.t))&&finite(get(n->mVelocity))&&finite(get(n->mTargetVelocity))&&std::isfinite(n->mFaceDirection)?true:fail(e,"source body phase lacks finite physical/time/gravity facts");
 }
 bool velocity(Navi* n,const Facts& f,std::string& e){
  auto* a=entry(n);Vec3 old=get(n->mTargetVelocity),extra{};
  if(a->fields.floor.triangle){
   FloorFacts ground;if(!call(n,e,[&]{return trace.floor(a->fields.floor,ground,e);}))return false;
   if(!finite(ground.planeNormal)||ground.slip>2)return fail(e,"invalid actual source floor/slip facts");
   const auto normalFloor=a->fields.floorNormal;float speed=length(old);
   old=scale(normal(sub(old,scale(normalFloor,dot(old,normalFloor)))),speed);
   Vec3 fall{0,-f.gravity*f.deltaTime,0};Vec3 tangent=sub(fall,scale(normalFloor,dot(fall,normalFloor)));
   if(ground.slip==0){if(speed<.1f)extra=scale(tangent,-1);}
   else{float factor=1;if(ground.slip==2){if(!f.rushBoots)return fail(e,"source steep slip lacks actual equipment observation");factor=*f.rushBoots?4:2.5f;}extra=scale(normal(tangent),f.gravity*f.deltaTime*factor);}
  }
  Vec3 velocity=get(n->mVelocity);Vec3 acceleration=sub(add(old,a->fields.simPosition),velocity);
  velocity=add(add(velocity,scale(acceleration,f.deltaTime/.1f)),extra);
  if(!finite(velocity))return fail(e,"source moveVelocity overflow");
  put(n->mVelocity,velocity);return true;
 }
 void rotation(Navi* n,const Facts& f){
  auto* a=entry(n);if(a->fields.fpFlags&1)return;auto v=get(n->mTargetVelocity);
  if(v.x*v.x+v.z*v.z>1)n->mFaceDirection=wrap(n->mFaceDirection+10*(f.deltaTime*(.8f*angleDistance(std::atan2(v.x,v.z),n->mFaceDirection))));
 }
 bool move(Navi* n,float rate,const Facts& f,std::string& e){
  auto* a=entry(n);if(a->fields.fpFlags&16)return fail(e,"source plucked-joint move producer unavailable");
  if(!f.mapPresent||!*f.mapPresent||!f.platformsPresent||!f.hiddenCollision||!f.inWater)return fail(e,"source move lacks actual map/platform/water observations");
  Vec3 position=get(n->mSRT.t);a->fields.acceleration.y=0;a->fields.fakeBounce={};
  TraceInfo info;info.sphere={add(position,{0,a->fields.boundingRadius,0}),a->fields.boundingRadius};info.velocity=add(get(n->mVelocity),a->fields.acceleration);info.traceRadius=a->fields.traceRadius;
  if(!(a->fields.fpFlags&8)){
   if(!call(n,e,[&]{return trace.map(*n,info,rate,e);}))return false;
   if(!finite(info.velocity)||!finite(info.sphere.center)||!finite(info.floorNormal)||!finite(info.wallNormal))return fail(e,"invalid actual map trace output");
   put(n->mVelocity,info.velocity);
   if(info.roomIndex!=-1){a->fields.roomIndex=info.roomIndex;if(!call(n,e,[&]{RoomVisit visit(*this,n,info.roomIndex);return trace.room(*n,info.roomIndex,e);}))return false;}
  }else{info.sphere.center=add(info.sphere.center,scale(get(n->mVelocity),rate));info.floor={};}
  if(!a->fields.floor.triangle&&info.floor.triangle)if(!call(n,e,[&]{FlagEvent event(*this);return provider.bounce(*n,info.floor,e);},true))return false;
  a->fields.floor=info.floor;a->fields.floorNormal=info.floorNormal;
  if(!a->fields.dontUseWallCallback&&info.wall.triangle)if(!call(n,e,[&]{FlagEvent event(*this);return provider.wall(*n,info.wallNormal,e);},true))return false;
  if(*f.platformsPresent){info.velocity=get(n->mVelocity);if(!call(n,e,[&]{return trace.platforms(*n,info,rate,e);}))return false;
   if(!finite(info.velocity)||!finite(info.sphere.center)||!finite(info.floorNormal)||!finite(info.wallNormal))return fail(e,"invalid actual platform trace output");
   put(n->mVelocity,info.velocity);
  }
  if(!a->fields.floor.triangle&&info.floor.triangle){if(!call(n,e,[&]{FlagEvent event(*this);return provider.bounce(*n,info.floor,e);},true))return false;a->fields.floor=info.floor;a->fields.floorNormal=info.floorNormal;}
  if(!a->fields.dontUseWallCallback&&info.wall.triangle)if(!call(n,e,[&]{FlagEvent event(*this);return provider.wall(*n,info.wallNormal,e);},true))return false;
  if(a->fields.floor.triangle){FloorFacts ground;if(!call(n,e,[&]{return trace.floor(a->fields.floor,ground,e);}))return false;if(!finite(ground.planeNormal))return fail(e,"invalid source floor plane");if(ground.planeNormal.y>.6f)a->fields.fakeBounce=a->fields.floor;}
  position=info.sphere.center;position.y-=a->fields.boundingRadius;
  if(*f.hiddenCollision){Sphere sphere{position,a->fields.boundingRadius};if(!call(n,e,[&]{return trace.constrain(*n,sphere,e);}))return false;position=sphere.center;info.sphere.center=position;}
  if(!finite(position)||!finite(get(n->mVelocity)))return fail(e,"source physical trace overflow");
  put(n->mSRT.t,position);a->fields.bounding=info.sphere;
  if(a->fields.floor.triangle&&!*f.inWater){
   if(!a->fields.previous)return fail(e,"source walk effect reads unknown previousPosition");
   if(length(sub(*a->fields.previous,position))>1){float random;if(!call(n,e,[&]{return provider.random(random,e);})||!std::isfinite(random)||random<0||random>1)return fail(e,"source walk effect RNG unavailable");if(random<=.033333335f){FloorFacts ground;if(!call(n,e,[&]{return trace.floor(a->fields.floor,ground,e);})||!call(n,e,[&]{return provider.walkEffect(*n,ground.contents==8,e);}))return false;}}
  }
  return true;
 }
};
Owner::Owner(std::unique_ptr<Impl> p):m(std::move(p)){m->self=this;}Owner::~Owner()=default;
std::unique_ptr<Owner> Owner::create(Provider& p,SourceSceneTrace& t,SourceBank& b,std::string& e){
 auto impl=std::make_unique<Impl>(p,t,b);std::string bytes;control::Params parameters;
 if(!impl->auth(nullptr,e,true)||pc_p2_original_captain_world()->phase()!=Phase::Loading||!b.sourceBytes(SourceResource::Parameters,bytes,e)||!control::parseParameters(bytes,parameters,e))return {};
 return std::unique_ptr<Owner>(new Owner(std::move(impl)));
}
bool Owner::initializeAfterBodyReset(Navi* n,std::string& e){
 Impl::Operation operation(*m,e);if(!operation.entered)return false;
 if(!n||!m->auth(n,e,true)||pc_p2_original_captain_world()->phase()!=Phase::Loading)return fail(e,"source body cold initialization requires actual Loading body/bank reset");
 if(m->entry(n))return fail(e,"source body duplicate cold initialization");
 if(nextInitializationSerial==std::numeric_limits<std::uint64_t>::max())return fail(e,"source body initialization serial exhausted");
 auto slot=n==m->scene->captainAt(0)?0:1;m->entries[slot].actor=n;m->entries[slot].fields=Fields{};m->entries[slot].fields.initializationSerial=++nextInitializationSerial;return true;
}
bool Owner::readFields(const Navi* n,Fields& out,std::string& e)const{auto* a=m->entry(n);if(!a||!m->auth(n,e,true))return false;out=a->fields;return true;}
bool Owner::beginCameraReset(std::uint64_t& revision,std::string& e){
 if(m->operationDepth||!m->auth(nullptr,e,true)||pc_p2_original_captain_world()->phase()!=Phase::Loading||
    !m->entries[0].actor||!m->entries[1].actor||!m->entries[0].fields.initializationSerial||!m->entries[1].fields.initializationSerial)
  return fail(e,"source camera reset lacks idle initialized Loading body owner");
 ++m->operationDepth;revision=m->mutationRevision;return true;
}
bool Owner::cameraResetCurrent(std::uint64_t revision,std::string& e)const{
 if(!m->operationDepth||m->mutationRevision!=revision||!m->auth(nullptr,e,true)||pc_p2_original_captain_world()->phase()!=Phase::Loading)
  return fail(e,"source camera body hold expired");
 return m->mutationRevision==revision;
}
void Owner::endCameraReset()noexcept{--m->operationDepth;}
bool Owner::roomVisitCurrent(const Navi* n,const SourceSceneTrace& trace,int room,std::string& e)const{
 const auto* call=m->roomCall;
 if(!call||!m->operationDepth||call->actor!=n||call->room!=room||room<0||&m->trace!=&trace||!m->auth(n,e))
  return fail(e,"source room visit is outside actual post-map body callback");
 auto* a=m->entry(n);
 return m->roomCall==call&&a&&a->fields.initializationSerial==call->birth&&a->fields.roomIndex==room
  &&m->mutationRevision==call->revision&&const_cast<Navi*>(n)->getCurrState()==call->state?true:
  fail(e,"source room visit callback lost actual body phase generation");
}
bool Owner::canRetire(std::string& e)const{if(m->operationDepth)return fail(e,"source body phase callbacks still retain owner");e.clear();return true;}
void Owner::forget(Navi* n)noexcept{if(m->operationDepth){++m->mutationRevision;return;}auto* a=m->entry(n);if(a)*a=Impl::Entry{};}
bool Owner::setMoveRotation(Navi* n,bool enabled,std::string& e){Impl::Operation operation(*m,e,true);if(!operation.entered)return false;auto* a=m->entry(n);if(!a||!m->auth(n,e,true)||!exactState(n))return fail(e,"source FP moveRotation event lacks current owned typed actor");if(enabled)a->fields.fpFlags&=~1u;else a->fields.fpFlags|=1;return true;}
bool Owner::animation(Navi* n,std::string& e){
 Impl::Operation operation(*m,e);if(!operation.entered)return false;
 Facts f;if(!n||!m->facts(n,f,e))return false;
 if(!f.mapPresent||!f.movieMotion||!f.movieActor||!f.movieExtra||!f.stuck||!f.gameFrozen)return fail(e,"source animation lacks genuine phase/movie/map observations");
 if(*f.movieMotion||!*f.mapPresent)return fail(e,"source movie-motion/mapless animation branch producer unavailable");
 if(!m->call(n,e,[&]{return m->provider.cellLOD(*n,.01f,.009f,e);}))return false;
 if(!m->call(n,e,[&]{Impl::FlagEvent event(*m);return nativecontrol::advanceAnimation(n,[&](Animator channel,Listener listener,int key){return m->provider.animationKey(*n,channel,listener,key,e);},e);},true))return false;
 auto* a=m->entry(n);a->fields.previous=get(n->mSRT.t);
 if((*f.movieExtra||!*f.movieActor)&&a->fields.fakeBounce.triangle){if(!(a->fields.fpFlags&4)||!a->fields.floor.triangle)if(!m->velocity(n,f,e))return false;m->rotation(n,f);}
 if(!a->fields.bounding)return fail(e,"source animation water point reads undefined cached bounding center");
 if(!m->call(n,e,[&]{return m->provider.water(*n,*a->fields.bounding,e);}))return false;
 n->mVelocity.y-=f.deltaTime*f.gravity;
 if(!m->call(n,e,[&]{return m->provider.geometry(*n,!(a->fields.fpFlags&2)&&!*f.stuck,e);}))return false;
 return m->call(n,e,[&]{return m->provider.cursor(*n,e);});
}
bool Owner::simulation(Navi* n,float rate,std::string& e){
 Impl::Operation operation(*m,e);if(!operation.entered)return false;
 Facts f;if(!n||!std::isfinite(rate)||rate<0||!m->facts(n,f,e))return fail(e,"invalid genuine source simulation rate/facts");
 if(!f.movieActive||!f.movieExtra||!f.movieActor||!f.naviManagerFlag1)return fail(e,"source simulation lacks genuine movie/manager facts");
 auto* a=m->entry(n);
 if(*f.movieActive){put(n->mVelocity,{});put(n->mTargetVelocity,{});a->fields.acceleration={};}
 if(!*f.movieExtra){
  if(*f.naviManagerFlag1){put(n->mVelocity,{});a->fields.acceleration={};a->fields.bounding=Sphere{get(n->mSRT.t),a->fields.boundingRadius};return true;}
  if(*f.movieActor){put(n->mVelocity,{});a->fields.acceleration={};}
 }
 if(!f.targetCollision||!f.stuck)return fail(e,"source simulation lacks actual target-collision/stuck facts");
 if(*f.targetCollision||*f.stuck)return fail(e,"source stomach/stick body phase producer unavailable");
 if(!m->move(n,rate,f,e))return false;
 if(*f.movieExtra||!*f.movieActor)if(!m->call(n,e,[&]{return nativecontrol::selectWalkAnimation(n,e);}))return false;
 auto v=get(n->mVelocity);float speed=length(v),acceleration=length(a->fields.acceleration);if(speed>acceleration)speed-=acceleration;
 v=scale(normal(v),speed);if(!finite(v))return fail(e,"source simulation velocity normalization overflow");put(n->mVelocity,v);a->fields.acceleration={};a->fields.bounding=Sphere{get(n->mSRT.t),a->fields.boundingRadius};
 bool below;if(!m->call(n,e,[&]{return m->provider.belowMap(*n,below,e);}))return false;
 return !below||m->call(n,e,[&]{return m->provider.recoverBelowMap(*n,e);},true);
}
bool Owner::update(Navi* n,std::string& e){
 Impl::Operation operation(*m,e);if(!operation.entered)return false;
 if(!n||!m->entry(n)||!m->auth(n,e))return false;
 pc_p2_original_captain_invincibility_update(n);
 for(auto event:{UpdateEvent::SoundExec,UpdateEvent::DemoCheck,UpdateEvent::Look,UpdateEvent::LookCreature})if(!m->call(n,e,[&]{return m->provider.event(*n,event,e);},event==UpdateEvent::DemoCheck))return false;
 pc_p2_original_captain_party_timers_update(n);
 if(!m->call(n,e,[&]{return m->provider.event(*n,UpdateEvent::Effects,e);}))return false;
 bool handled;if(!m->call(n,e,[&]{return m->provider.menus(*n,handled,e);}))return false;if(handled)return true;
 for(auto event:{UpdateEvent::Footmarks,UpdateEvent::VersusCard})if(!m->call(n,e,[&]{return m->provider.event(*n,event,e);}))return false;
 if(!m->call(n,e,[&]{Impl::FlagEvent event(*m);n->getCurrState()->exec(n);return true;},true))return false;
 return m->call(n,e,[&]{return m->provider.plateUpdate(*n,e);});
}
// Retail NaviMgr::doAnimation: update and animate one open slot before the next.
bool Owner::managerAnimation(std::string& e){
 Impl::Operation operation(*m,e);if(!operation.entered||!m->auth(nullptr,e))return false;
 for(auto& entry:m->entries){
  Navi* n=entry.actor;if(!n)return fail(e,"source manager lacks initialized roster slot");
  Facts facts;if(!m->facts(n,facts,e))return false;
  if(!facts.managerSlotOpen||!facts.naviManagerFlag1||!facts.movieActor)return fail(e,"source manager animation lacks actual slot/flag/movie observations");
  if(!*facts.managerSlotOpen||(*facts.naviManagerFlag1&&!*facts.movieActor))continue;
  entry.fields.faceDirectionOffset=n->mFaceDirection;
  m->managerChild=true;if(!update(n,e))return false;
  m->managerChild=true;if(!animation(n,e))return false;
 }
 return true;
}
bool Owner::managerSimulation(float rate,std::string& e){
 Impl::Operation operation(*m,e);if(!operation.entered||!m->auth(nullptr,e)||!std::isfinite(rate)||rate<0)return false;
 for(auto& entry:m->entries){
  Navi* n=entry.actor;if(!n)return fail(e,"source manager lacks initialized roster slot");
  Facts facts;if(!m->facts(n,facts,e))return false;
  if(!facts.managerSlotOpen)return fail(e,"source manager simulation lacks actual slot observation");
  if(*facts.managerSlotOpen){m->managerChild=true;if(!simulation(n,rate,e))return false;}
 }
 auto revision=m->mutationRevision;
 std::array<std::uint64_t,2> serials{};std::array<NaviState*,2> states{};
 for(unsigned i=0;i<2;++i){serials[i]=m->entries[i].fields.initializationSerial;states[i]=m->entries[i].actor->getCurrState();}
 if(!m->provider.managerSimulationEnded(e))return false;
 for(unsigned i=0;i<2;++i)if(m->entries[i].fields.initializationSerial!=serials[i]||!m->auth(m->entries[i].actor,e)||m->entries[i].actor->getCurrState()!=states[i])return fail(e,"source manager simulation suffix expired exact actor/state");
 return revision==m->mutationRevision&&m->auth(nullptr,e)?true:fail(e,"source manager simulation suffix expired authority");
}
bool body_animation(Navi* n,std::string& e){auto* p=pc_p2_original_captain_body_phase_owner(n);return p?p->animation(n,e):fail(e,"canonical source body animation owner absent");}
bool body_simulation(Navi* n,float rate,std::string& e){auto* p=pc_p2_original_captain_body_phase_owner(n);return p?p->simulation(n,rate,e):fail(e,"canonical source body simulation owner absent");}
bool body_update(Navi* n,std::string& e){auto* p=pc_p2_original_captain_body_phase_owner(n);return p?p->update(n,e):fail(e,"canonical source body update owner absent");}
bool setMoveRotation(Navi* n,bool enabled,std::string& e){auto* p=pc_p2_original_captain_body_phase_owner(n);return p?p->setMoveRotation(n,enabled,e):fail(e,"canonical source FP flag owner absent");}
bool readFlag(const Navi* n,unsigned mask,bool& out,std::string& e){auto* p=pc_p2_original_captain_body_phase_owner(n);Fields fields;if(!p||!p->readFields(n,fields,e))return false;out=(fields.fpFlags&mask)!=0;return true;}
}}}
