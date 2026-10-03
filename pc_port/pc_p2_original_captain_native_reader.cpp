#include "pc_p2_original_captain_native_reader.h"
#include "Navi.h"
#include "NaviState.h"
#include <cmath>
#include <cstdlib>
// Exact strong Body composer borrow API. Its implementation belongs to Body;
// absence never substitutes an independently constructed Plate.
namespace p2original {namespace piki {Plate* nativePlate(const captain::LoadedScene&,std::string&);}}
namespace p2original {namespace captain {namespace nativereader {
namespace {
bool fail(std::string& e,const char* s){e=s;return false;}
bool finite(const Vector3f& v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
actions::Vec3 vec(const Vector3f& v){return {v.x,v.y,v.z};}
}
struct Reader::Impl {
 struct Actor {Navi* body=nullptr;cstick::State stick;std::optional<bool> pellet;std::optional<piki::PlatePose> pose;std::optional<float> refreshStrength;std::uint64_t revision=0;};
 const LoadedScene* owner=nullptr;std::uint64_t epoch=0;SourceBank* bank=nullptr;
 std::string campaign,fingerprint,catalog;
 std::array<Actor,2> actors;std::string bytes;SourceParameters parameters;cstick::Parameters cstickParameters;
 PlateDriver* plate=nullptr;bool busy=false,reentered=false;
 bool check(const Navi* n,std::string& e,bool cleanup=false)const {
  const auto* s=pc_p2_original_captain_loaded_scene();const auto* w=pc_p2_original_captain_world();
  if(!owner||s!=owner||!w||s->incarnation()!=epoch||w->incarnation()!=epoch||!epoch
   ||(!cleanup&&w->phase()==Phase::Inactive)||s->selectedCampaign()!=campaign||s->selectedFingerprint()!=fingerprint||s->sourceCatalog()!=catalog
   ||w->selectedCampaign()!=campaign||w->selectedFingerprint()!=fingerprint||w->sourceCatalog()!=catalog
   ||s->captainAt(0)!=actors[0].body||s->captainAt(1)!=actors[1].body||w->captainAt(0)!=actors[0].body||w->captainAt(1)!=actors[1].body
   ||!bank||(!cleanup&&pc_p2_original_captain_source_bank()!=bank))return fail(e,"Concrete captain reader lost canonical source scene/roster/bank");
  if(plate&&!cleanup&&piki::nativePlate(*owner,e)!=&plate->storage())return fail(e,"Concrete captain reader Body Plate owner changed");
  if(!n)return true;
  if(n!=actors[0].body&&n!=actors[1].body)return fail(e,"Captain reader body outside exact source roster");
  if(cleanup)return true;
  bool alive;MotionState motion;
  return pc_p2_original_captain_actor_lifetime(n,alive)&&bank->state(n,motion,e)?true:fail(e,"Captain reader actual body/bank lifetime is unavailable");
 }
 Actor* actor(const Navi* n){for(auto& a:actors)if(a.body==n)return &a;return nullptr;}
 const Actor* actor(const Navi* n)const{for(const auto& a:actors)if(a.body==n)return &a;return nullptr;}
 bool matches(const Plan& p,std::string& e)const {
  if(!check(p.actor_,e))return false;
  const auto* a=actor(p.actor_);
  MotionState motion;if(!bank->state(p.actor_,motion,e))return false;
  return a&&a->revision==p.revision_&&owner==p.scene_&&epoch==p.incarnation_&&bank==p.bank_&&plate==p.plate_
   &&motion.generation==p.motionGeneration_&&pc_p2_original_captain_world()==p.world_&&p.actor_->getCurrState()==p.state_&&!reentered?true:fail(e,"Source C-stick plan expired during actual callbacks");
 }
};
Reader::Reader():m(std::make_unique<Impl>()){}Reader::~Reader()=default;
bool Reader::currentBody(const Navi* n,std::string& e)const{return n&&m->check(n,e);}
const LoadedScene& Reader::scene()const{if(!m->owner)std::abort();return *m->owner;}
const std::string& Reader::naviParameterBytes()const{return m->bytes;}
piki::PlateSource& Reader::plateSource()const{return *const_cast<Reader*>(this);}
bool Reader::readParameters(const Navi* n,piki::PlateParameters& out,std::string& e)const{
 if(!n||!m->check(n,e,true))return false;
 out={17.5f,130.0f,6.0f};e.clear();return true; // Literal CPlate::Parms object.
}
bool Reader::readPose(const Navi* n,piki::PlatePose& out,std::string& e)const{
 if(!n||!m->check(n,e))return false;
 const auto* a=m->actor(n);if(!a->pose)return fail(e,"Actual source SetPos/SetPosGray pose is not initialized");
 out=*a->pose;e.clear();return true;
}
bool Reader::setFormed(piki::Handle,Navi* n,std::string& e){
 if(!m->check(n,e))return false;
 return fail(e,"Actual ActFormation tutorial/effect event owner is unavailable");
}
bool Reader::initializeAfterBodyReset(std::string& e){
 if(m->busy){m->reentered=true;return fail(e,"Source reader initialization reentered actual C-stick mutation");}
 const auto* s=pc_p2_original_captain_loaded_scene();const auto* w=pc_p2_original_captain_world();auto* bank=pc_p2_original_captain_source_bank();
 if(!s||!w||!bank||!s->incarnation()||w->phase()!=Phase::Loading||s->incarnation()!=w->incarnation()
  ||w->selectedCampaign()!=s->selectedCampaign()||w->selectedFingerprint()!=s->selectedFingerprint()||w->sourceCatalog()!=s->sourceCatalog()
  ||!s->captainAt(0)||!s->captainAt(1)||s->captainAt(0)==s->captainAt(1)||w->captainAt(0)!=s->captainAt(0)||w->captainAt(1)!=s->captainAt(1))return fail(e,"Source reader initialization lacks actual Loading two-body binding");
 if(m->owner==s&&m->epoch==s->incarnation())return m->check(nullptr,e); // No repeated constructor/onInit.
 if(m->owner)return fail(e,"Retained source reader requires checked Body-consumer retirement before replacement");
 const auto campaign=s->selectedCampaign(),fingerprint=s->selectedFingerprint(),catalog=s->sourceCatalog();
 const auto epoch=s->incarnation();std::string bytes;SourceParameters parameters;cstick::Parameters parsed;control::Params controlParameters;
 if(!bank->sourceBytes(SourceResource::Parameters,bytes,e)||!bank->parameters(parameters,e)||!cstick::parseParameters(bytes,parsed,e)
  ||!control::parseParameters(bytes,controlParameters,e)||parameters.neutralStick!=controlParameters.values().neutralStick
  ||!std::isfinite(parameters.neutralStick)||parameters.neutralStick<0)return fail(e,"Captain reader selected source parameters are unavailable");
 std::array<Impl::Actor,2> next;
 for(unsigned slot=0;slot<2;++slot){auto* n=s->captainAt(slot);bool alive;MotionState motion;PcOriginalCaptainTimers timers;
  if(!pc_p2_original_captain_actor_lifetime(n,alive)||!pc_p2_original_captain_actor_timers(n,timers)||!bank->state(n,motion,e))return fail(e,"Captain reader initialization lacks actual source body reset/bank");
  next[slot].body=n;next[slot].revision=m->actors[slot].revision+1;
  next[slot].pellet=false; // New source field object: literal PelletView ctor, not P1 reset.
  cstick::onInit(next[slot].stick); // Only angle/scaleTimer; command/vector remain unknown.
 }
 if(pc_p2_original_captain_loaded_scene()!=s||pc_p2_original_captain_world()!=w||s->incarnation()!=epoch||w->incarnation()!=epoch||w->phase()!=Phase::Loading||pc_p2_original_captain_source_bank()!=bank
  ||s->selectedCampaign()!=campaign||s->selectedFingerprint()!=fingerprint||s->sourceCatalog()!=catalog)return fail(e,"Captain reader Loading initialization expired");
 m->owner=s;m->epoch=s->incarnation();m->campaign=campaign;m->fingerprint=fingerprint;m->catalog=catalog;m->bank=bank;m->actors=next;m->bytes=std::move(bytes);m->parameters=parameters;m->cstickParameters=parsed;m->plate=nullptr;e.clear();return true;
}
bool Reader::bindPlate(PlateDriver& p,std::string& e){
 if(m->busy){m->reentered=true;return fail(e,"Source Plate binding reentered C-stick mutation");}
 if(!m->check(nullptr,e)||piki::nativePlate(*m->owner,e)!=&p.storage())return fail(e,"Source reader Plate binding has wrong actual Body owner");
 if(m->plate&&m->plate!=&p)return fail(e,"Source reader already borrows another actual Plate");
 m->plate=&p;e.clear();return true;
}
bool Reader::frame(const Navi* n,piki::CaptainFrame& out,std::string& e)const {
 if(!n||!m->check(n,e)||!m->plate)return fail(e,"Captain reader has no actual body/borrowed Plate");
 const auto* a=m->actor(n);auto* current=const_cast<Navi*>(n)->getCurrState();const auto* typed=dynamic_cast<const State*>(current);
 if(!typed||typed->nativeState()!=current||!a->stick.commandOn2||!a->stick.position||!a->pellet)return fail(e,"Captain reader source FSM/command/C-stick/PelletView field is unknown");
 if(typed->sourceStateId()==StateId::Pellet)return fail(e,"Actual source PelletView becomePellet lifecycle owner is unavailable");
 const auto revision=a->revision;const auto* world=pc_p2_original_captain_world();MotionState beforeMotion;
 if(!m->bank->state(n,beforeMotion,e))return false;
 // Retail getPosition chooses model translation during MVP_IsActive. This
 // ordinary reader cannot substitute body position for missing movie authority.
 if(world->demo()!=Demo::Inactive&&world->demo()!=Demo::Absent)return fail(e,"Captain reader movie-active position requires genuine source movie owner");
 PcOriginalCaptainTimers timers;bool alive;std::array<float,12> joint;piki::PlateState plate;
 const auto timer=nativecontrol::sceneAnimationTimer(n);
 if((!timer&&world->phase()!=Phase::Loading)||!pc_p2_original_captain_actor_timers(n,timers)||!pc_p2_original_captain_actor_lifetime(n,alive)
  ||!m->bank->jointWorld(const_cast<Navi*>(n),10,joint,e)||!m->plate->storage().state(const_cast<Navi*>(n),plate,e))return false;
 if(!plate.maxPositionKnown)return fail(e,"Actual source CPlate maxPositionOffset is not initialized");
 piki::CaptainFrame f;f.position=n->mSRT.t;f.velocity=n->mVelocity;f.face=n->mFaceDirection;f.sceneAnimationTimer=timer?*timer:0;
 f.rhnd={joint[0]*3+joint[3],joint[4]*3+joint[7],joint[8]*3+joint[11]};f.plateOffset=plate.maxPositionOffset;
 f.throwWait=typed->sourceStateId()==StateId::ThrowWait;f.throwing=typed->sourceStateId()==StateId::Throw;f.follow=typed->sourceStateId()==StateId::Follow;
 f.controller=n->mKontroller!=nullptr;f.formationable=timers.disbandDisable==0;f.alive=alive;f.command=*a->stick.commandOn2;f.carryingPellet=*a->pellet;
 const auto v=*a->stick.position;const float magnitude=std::sqrt(v.x*v.x+v.y*v.y+v.z*v.z);f.cstickNeutral=magnitude<=m->parameters.neutralStick;
 if(!finite(f.position)||!finite(f.velocity)||!finite(f.rhnd)||!finite(f.plateOffset)||!std::isfinite(f.face)||!std::isfinite(f.sceneAnimationTimer)||!std::isfinite(magnitude))return fail(e,"Captain reader actual source fields are nonfinite");
 MotionState afterMotion;
 if(!m->check(n,e)||!m->bank->state(n,afterMotion,e)||afterMotion.generation!=beforeMotion.generation||m->actor(n)->revision!=revision||const_cast<Navi*>(n)->getCurrState()!=current||pc_p2_original_captain_world()!=world)return fail(e,"Captain reader observation expired during actual queries");
 out=f;e.clear();return true;
}
bool Reader::prepareCStick(Navi* n,const nativecontrol::Request& request,Plan& out,std::string& e)const {
 if(!n||!m->check(n,e)||!m->plate)return fail(e,"Source C-stick lacks actual body/borrowed Plate");
 const auto* a=m->actor(n);auto* current=n->getCurrState();auto* typed=dynamic_cast<State*>(current);const auto* world=pc_p2_original_captain_world();
 if(!typed||typed->nativeState()!=current||request.hasController!=(n->mKontroller!=nullptr)||request.demo!=world->demo())return fail(e,"Source C-stick request lost actual controller/FSM/movie observation");
 piki::PlateState plate;if(!m->plate->storage().state(n,plate,e))return false;
 cstick::Input in;in.controller=request.hasController;if(request.demo==Demo::Inactive)in.demoInactive=true;else if(request.demo==Demo::Playing)in.demoInactive=false;
 in.stickX=request.subStick.x;in.stickY=request.subStick.z;in.camera=request.camera;in.position=vec(n->mSRT.t);in.velocity=vec(n->mVelocity);
 in.targetVelocity={request.proposedTargetVelocity.x,request.proposedTargetVelocity.y,request.proposedTargetVelocity.z};in.face=request.proposedFaceDirection;in.sourceState=typed->sourceStateId();in.plateAngle=plate.angle;in.slotCount=plate.count;
 Plan next;next.actor_=n;next.state_=current;next.world_=world;next.scene_=m->owner;next.bank_=m->bank;next.plate_=m->plate;next.incarnation_=m->epoch;next.revision_=a->revision;
 MotionState motion;if(!m->bank->state(n,motion,e))return false;next.motionGeneration_=motion.generation;
 if(!cstick::prepare(m->cstickParameters,in,a->stick,next.source_,e)||!m->matches(next,e))return false;
 out=std::move(next);e.clear();return true;
}
bool Reader::commitCStick(const Plan& plan,std::string& e){
 if(m->busy){m->reentered=true;return fail(e,"Source C-stick actual mutation reentered");}
 if(!m->matches(plan,e)||!nativecontrol::cStickControlTransaction(plan.actor_,e)||!m->matches(plan,e))return false;
 m->busy=true;m->reentered=false;
 struct Scope {Impl& m;~Scope(){m.busy=false;m.reentered=false;}} scope{*m};
 Plan live=plan;
 auto write=[&](const std::function<void(Impl::Actor&)>& f){if(!m->matches(live,e))return false;auto* a=m->actor(live.actor_);f(*a);live.revision_=++a->revision;return m->matches(live,e);};
 auto execute=[&](const cstick::Command& command){
  if(command.kind==cstick::CommandKind::SetPos||command.kind==cstick::CommandKind::SetPosGray){
   if(!m->actor(live.actor_)->refreshStrength)return fail(e,"Actual source Refresh strength is unknown before SetPos");
   if(!write([&](Impl::Actor& a){piki::PlatePose p;p.position={command.position.x,command.position.y,command.position.z};p.velocity={command.velocity.x,command.velocity.y,command.velocity.z};p.angle=command.angle;p.scale=command.scale;p.gray=command.kind==cstick::CommandKind::SetPosGray;p.moveStrength=*a.refreshStrength;a.pose=p;}))return false;
  }
  if(command.kind==cstick::CommandKind::Refresh&&!write([&](Impl::Actor& a){a.refreshStrength=command.strength;if(a.pose)a.pose->moveStrength=command.strength;}))return false;
  // Refresh's literal strength is supplied directly to the genuine driver.
  // It MUST NOT call the legacy combined refresh+setPos storage convenience.
  return m->matches(live,e)&&m->plate->execute(live.actor_,command,e)&&m->matches(live,e);
 };
 // navi.cpp4864-4872: clear position/command, assign active command, reset
 // the source clock, THEN publish transformed position and calculate angle.
 if(!write([&](Impl::Actor& a){a.stick.position=actions::Vec3{};a.stick.commandOn2=false;}))return false;
 if(!plan.source_.neutral){
  if(!write([&](Impl::Actor& a){a.stick.commandOn2=true;}))return false;
  if(!m->matches(live,e)||!nativecontrol::resetCStickSceneAnimationTimer(live.actor_,e)||!m->matches(live,e))return false;
  if(!write([&](Impl::Actor& a){a.stick.position=plan.source_.state.position;a.stick.angle=plan.source_.state.angle;}))return false;
 }else if(!write([&](Impl::Actor& a){a.stick.scaleTimer=plan.source_.state.scaleTimer;a.stick.commandOn1=plan.source_.state.commandOn1;a.stick.neutralTurn=plan.source_.state.neutralTurn;}))return false;
 for(const auto& command:plan.source_.commands){
  if(command.kind==cstick::CommandKind::SetPos&&!plan.source_.neutral&&!write([&](Impl::Actor& a){a.stick.scaleTimer=plan.source_.state.scaleTimer;}))return false;
  if(!execute(command))return false;
 }
 cstick::Plan final=plan.source_;
 if(plan.source_.neutral){
  cstick::AfterRefresh after;if(!m->plate->afterRefresh(live.actor_,after,e)||!m->matches(live,e)||!cstick::completeNeutral(m->cstickParameters,*plan.source_.neutral,after,final,e))return false;
  if(!write([&](Impl::Actor& a){a.stick.distanceState=final.state.distanceState;a.stick.increment=final.state.increment;a.stick.commandOn1=final.state.commandOn1;
    if(final.state.distanceState&&*final.state.distanceState!=2)a.stick.needRearrange=final.state.needRearrange;}))return false;
  for(const auto& command:final.commands){if(!execute(command))return false;
   if(command.kind==cstick::CommandKind::Rearrange&&!write([&](Impl::Actor& a){a.stick.needRearrange=0;}))return false;
  }
 }
 if(!write([&](Impl::Actor& a){a.stick=final.state;}))return false;
 e.clear();return true;
}
Reader& instance(){static Reader reader;return reader;}
bool Reader::canRetire(std::string& e)const{
 if(m->busy)return fail(e,"Actual source C-stick operation retains Reader");
 if(!m->owner){e.clear();return true;}
 if(!m->check(nullptr,e,true)||pc_p2_original_captain_world()->phase()!=Phase::Inactive)return fail(e,"Reader retirement requires retained canonical inactive scene");
 auto* plate=piki::nativePlate(*m->owner,e);
 if(plate&&(plate->retainedSlots()||plate->retainedListeners()))return fail(e,"Actual Body CPlate retains source references");
 piki::Ownership ownership;
 if(!piki::readOwnership(ownership,e))return false;
 if(ownership.entries||ownership.committed||ownership.pendingInitializations||ownership.inFlightOwnerOperations||ownership.formationSlots||ownership.pendingSlots||ownership.freeEffectOwners||ownership.throwEffectOwners)return fail(e,"Actual source Body Services retain Reader consumers");
 return m->check(nullptr,e,true)&&pc_p2_original_captain_world()->phase()==Phase::Inactive;
}
bool Reader::retireAfterBodyConsumers(std::string& e){
 if(m->busy){m->reentered=true;return fail(e,"Reader retirement reentered source C-stick mutation");}
 if(!canRetire(e))return false;
 auto next=std::make_unique<Impl>();for(unsigned i=0;i<2;++i)next->actors[i].revision=m->actors[i].revision+1;
 m=std::move(next);e.clear();return true;
}
piki::NativeCaptainReader* borrow(){auto& reader=instance();std::string e;return reader.m->check(nullptr,e)?&reader:nullptr;}
}}}
p2original::piki::NativeCaptainReader* pc_p2_original_captain_native_reader(){return p2original::captain::nativereader::borrow();}
