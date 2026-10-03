#include "pc_p2_retail_cave_native.h"
#include "pc_p2_retail_cave_registry.h"
#include "pc_p2_original_snow_native.h"
#include "Generator.h"
#include "MapMgr.h"
#include "Vector.h"
#include <cstdio>
#include <cstdlib>
#include <map>
#include <utility>
namespace p2retail {namespace {
std::set<NativeFloor*>& owners(){static std::set<NativeFloor*> value;return value;}
bool refuse(std::string& error,const char* text){error=text;return false;}
}
struct NativeFloor::Impl {
 FloorPlan plan;SceneOps& scene;std::map<unsigned,std::shared_ptr<FamilyOps>> families;
 std::set<std::shared_ptr<FamilyOps>> preparedFamilies;
 Snapshot context;bool prepared=false,scenePreparationOwned=false,begun=false,committed=false,cleaning=false;
 struct Source {p2original::CatalogRow row;std::unique_ptr<Generator> generator;std::uint64_t handle=0;};
 struct Actor {Creature* actor=nullptr;unsigned source=0,token=0;std::uint64_t handle=0;
               BirthIdentity origin;bool registered=false;};
 std::vector<Source> sources;std::vector<Actor> actors;
 Impl(FloorPlan value,SceneOps& owner):plan(std::move(value)),scene(owner){}
};
NativeFloor::NativeFloor(FloorPlan plan,SceneOps& scene):m(new Impl(std::move(plan),scene)){
 FamilyOps snow;
 snow.prepare=[](const std::vector<p2original::CatalogRow>& rows,std::string& e){unsigned count=0;for(const auto& row:rows)count+=row.enemy.count;return pc_p2_snow_prepare_cave(e)&&pc_p2_snow_reserve_cave(count,e);};
 snow.birth=[](const p2original::CatalogRow&,const Snapshot&,Generator* gen,unsigned,const Vector3f& p,float yaw,Creature*& out,bool& suppressed,std::string& e){suppressed=false;return pc_p2_snow_birth_cave(gen,p,yaw,out,e);};
 snow.bind=[](const p2original::CatalogRow&,Creature* actor,unsigned token,std::string& e){return pc_p2_snow_bind_cave(actor,token,e);};snow.release=pc_p2_snow_release_cave;
 snow.retired=[](Creature*,std::string&){return true;}; // Snow native forget clears its own leaf ownership.
 snow.cancel=[](std::string&){pc_p2_original_snow_resources_reset();return true;};
 m->families.emplace(45,std::make_shared<FamilyOps>(std::move(snow)));owners().insert(this);
}
NativeFloor::~NativeFloor(){
 if(!m->actors.empty()||!m->sources.empty()||m->begun||m->prepared||m->scenePreparationOwned){std::fputs("P2_RETAIL_FLOOR live owner destroyed\n",stderr);std::abort();}
 owners().erase(this);
}
bool NativeFloor::family(unsigned source,FamilyOps ops,std::string& error){
 return familyGroup({source},std::move(ops),error);
}
bool NativeFloor::familyGroup(const std::vector<unsigned>& sources,FamilyOps ops,std::string& error){
 if(m->prepared||sources.empty()||!ops.prepare||!ops.birth||!ops.bind||!ops.release||!ops.cancel||!ops.retired)
  return refuse(error,"retail family registration invalid/already prepared");
 std::set<unsigned> unique;
 for(unsigned source:sources)if(m->families.count(source)||!unique.insert(source).second)
  return refuse(error,"retail duplicate physical family");
 auto shared=std::make_shared<FamilyOps>(std::move(ops));
 for(unsigned source:sources)m->families.emplace(source,shared);
 error.clear();return true;
}
bool NativeFloor::preflight(const CaveDescriptor& cave,const FloorDefinition& floor,unsigned number,const SceneIdentity& identity,std::string& error){
 FloorPlan verified;
 if(!parseFloorPlan(m->plan.authenticatedBytes,m->plan.layoutSha256,verified,error))return false;
 m->plan=std::move(verified);
 if(m->prepared||m->scenePreparationOwned||m->begun||!m->sources.empty()||!m->actors.empty()||!mapMgr||
    m->plan.cave!=cave.cave||m->plan.floor!=number||identity.layoutSha256!=m->plan.layoutSha256||
    m->plan.sourceSha256!=cave.sourceSha256||m->plan.catalogSha256!=cave.catalogSha256)
  return refuse(error,"retail floor native scene/source mismatch");
 std::vector<p2original::CatalogRow> rows;
 if(!catalogRows(cave.cave,number,rows,error))return false;
 std::map<std::shared_ptr<FamilyOps>,std::vector<p2original::CatalogRow>> groups;
 for(const auto& row:floor.rows)if(row.kind!="loose_treasure"){
  if(row.sourceId<0||!m->families.count(unsigned(row.sourceId)))return refuse(error,"retail literal physical family unsupported");

 }
 for(const auto& row:rows)groups[m->families.at(row.enemy.source)].push_back(row);
 bool story=false,inCave=false;
 if(!m->scene.mode(identity,story,inCave,error)||!inCave)return refuse(error,"retail floor actual cave mode unavailable");
 m->context={cave.cave,cave.source,cave.sourceSha256,cave.catalogSha256,number,cave.maxFloor,identity,story,inCave};
 // The scene owner verifies actual selected native mode, card/session, installed
 // geometry/routes, source anchors, cargo assets/profiles and receiver capacity.
 m->scenePreparationOwned=true; // Pod/resources may stage state even if preflight later fails.
 if(!m->scene.preflight(m->plan,m->context,error))return false;
 m->prepared=true;
 for(const auto& group:groups){
  if(!group.first->prepare(group.second,error)){
   // Include the failed leaf: a partial resource reservation belongs to it.
   m->preparedFamilies.insert(group.first);
   return false; // FloorSession invokes release and retains a failed cleanup.
  }
  m->preparedFamilies.insert(group.first);
 }
 error.clear();return true;
}
bool NativeFloor::install(const CaveDescriptor& cave,const FloorDefinition& floor,unsigned number,const SceneIdentity& identity,
                          const std::vector<BirthIdentity>& expected,std::vector<LiveBinding>& out,std::string& error){
 if(!m->prepared||!(m->context.scene==identity)||number!=m->context.floor)return refuse(error,"retail floor install without preflight");
 std::vector<p2original::CatalogRow> rows;
 if(!catalogRows(cave.cave,number,rows,error))return false;
 auto capability=[&](const p2original::CatalogRow& row,std::string& e){
  if(row.sourceForm!=p2original::SourceForm::CaveTekiInfo||!m->families.count(row.enemy.source))return refuse(e,"retail source registry family mismatch");
  return true;
 };
 auto& registry=p2original::originalActors();
 if(!registry.install(m->plan.layoutSha256,rows,capability,error))return false;
 for(const auto& row:rows){
  Impl::Source source;source.row=row;source.generator.reset(new Generator);
  source.generator->_70=row.enemy.uid;source.generator->mAliveCount=0;
  source.generator->mCarryOverFlags=0; // P1 compatibility fields are never SAVE authority.
  if(!registry.generator(source.generator.get(),row.enemy.uid,source.handle,error))return false;
  m->sources.push_back(std::move(source));
 }
 m->begun=true; // begin may allocate partial Pod/exit state requiring rollback
 if(!m->scene.begin(m->plan,m->context,error))return false;
 for(const auto& p:m->plan.actors){
  const auto& row=floor.rows[p.row];const BirthIdentity* origin=nullptr;
  for(const auto& candidate:expected)if(candidate.row==p.row&&candidate.ordinal==p.ordinal){origin=&candidate;break;}
  if(!origin||origin->instance!=p.instance)return refuse(error,"retail selected placement has no expected source origin");
  LiveBinding binding;binding.identity=*origin;
  if(row.kind=="loose_treasure"){
   if(!m->scene.cargo(p,row,*origin,m->context,binding,error))return false;
  }else{
   bool absent=false;
   if(!m->scene.prior(row,*origin,m->context,binding,absent,error))return false;
   if(!(binding.identity==*origin)||binding.actor)return refuse(error,"retail prior disposition changed identity/actor");
   if(absent){
    if((binding.state!=BindingState::RetiredNative&&
        !(binding.state==BindingState::SourceSuppressed&&(row.sourceId==6||row.sourceId==7)))||
       !m->scene.absent(row,*origin,m->context,binding))
     return refuse(error,"retail prior absence has no native-cache/population receipt");
    out.push_back(std::move(binding));continue;
   }
   if(binding.state!=BindingState::Live||!binding.receipt.empty())return refuse(error,"retail live prior disposition differs");
   Impl::Source* source=nullptr;for(auto& candidate:m->sources)if(candidate.row.caveRow==p.row){source=&candidate;break;}
   if(!source)return refuse(error,"retail physical actor source generator missing");
   Vector3f position(p.x,p.y,p.z);position.y=mapMgr->getMinY(position.x,position.z,true);
   if(!std::isfinite(position.y))return refuse(error,"retail native map has no source ground");
   Impl::Actor actor;actor.source=unsigned(row.sourceId);actor.origin=*origin;bool suppressed=false;
   const float yaw=p.yawDegrees*0.01745329251994329577f;
   auto& family=*m->families.at(actor.source);
   bool born=family.birth(source->row,m->context,source->generator.get(),origin->ordinal,position,yaw,actor.actor,suppressed,error);
   // Preserve every partial native allocation for bounded rollback.
   if(actor.actor)m->actors.push_back(actor);
   if(!born)return false;
   if(suppressed){
    if(actor.actor)return refuse(error,"retail suppressed source unexpectedly born");
    if((row.sourceId!=6&&row.sourceId!=7)||!m->scene.suppressed(row,*origin,m->context,binding,error))return false;
    if(!(binding.identity==*origin)||binding.actor||binding.state!=BindingState::SourceSuppressed||
       binding.receipt.empty()||!m->scene.absent(row,*origin,m->context,binding))
     return refuse(error,"retail actual suppression has no selected source receipt");
   }else{
    if(!actor.actor)return refuse(error,"retail family returned no actual actor");
    auto& owned=m->actors.back();
    if(!registry.actorActivation(owned.actor,source->row.enemy.uid,origin->ordinal,origin->epoch,origin->activation,owned.token,owned.handle,error))return false;
    owned.registered=true;++source->generator->mAliveCount;
    if(!family.bind(source->row,owned.actor,owned.token,error))return false;
    binding.actor=owned.actor;
   }
  }
  out.push_back(std::move(binding));
 }
 error.clear();return true;
}
bool NativeFloor::verifyAbsent(const CaveDescriptor&,unsigned,const ContentRow& row,const SceneIdentity& identity,
                              const BirthIdentity& expected,const LiveBinding& binding)const{
 return m->context.scene==identity&&binding.identity==expected&&m->scene.absent(row,expected,m->context,binding);
}
bool NativeFloor::placement(const SceneIdentity& identity,unsigned row,unsigned ordinal,Placement& out,std::string& error)const{
 if(!m->prepared||!(m->context.scene==identity))return refuse(error,"retail placement outside prepared selected scene");
 FloorPlan verified;
 if(!parseFloorPlan(m->plan.authenticatedBytes,identity.layoutSha256,verified,error))return false;
 for(const auto& candidate:verified.actors)if(candidate.row==row&&candidate.ordinal==ordinal){
  out=candidate;error.clear();return true;
 }
 return refuse(error,"retail selected placement absent");
}
bool NativeFloor::commit(const Snapshot& context,std::string& error){
 if(!m->begun||m->committed||!(context.scene==m->context.scene)||context.cave!=m->context.cave||context.floor!=m->context.floor||
    context.story!=m->context.story||context.inCave!=m->context.inCave)
  return refuse(error,"retail commit scene differs");
 if(!m->scene.commit(context,error))return false;
 m->context=context;m->committed=true;error.clear();return true;
}
bool NativeFloor::retired(Creature* pointer,std::string& error){
 for(auto& actor:m->actors)if(actor.actor==pointer){
  if(!m->cleaning&&m->committed&&!m->scene.retired(actor.origin,m->context,error))return false;
  if(actor.registered&&!p2original::originalActors().retire(pointer,actor.handle))return refuse(error,"retail native association retirement failed");
  actor.registered=false;
  if(!m->families.at(actor.source)->retired(pointer,error))return false;
  actor.actor=nullptr;error.clear();return true;
 }
 error.clear();return true;
}
bool NativeFloor::release(std::string& error){
 // Cargo/Pod reject an observable incomplete suction transaction. No actor is
 // destroyed until that owner agrees the scene can release or roll back.
 if((m->scenePreparationOwned||m->begun)&&!m->scene.release(error))return false;
 m->scenePreparationOwned=false;m->begun=false;m->committed=false;m->cleaning=true;
 auto& registry=p2original::originalActors();
 for(auto& actor:m->actors)if(actor.actor){
  Creature* pointer=actor.actor;auto& family=*m->families.at(actor.source);
  if(family.retireBeforeRelease&&actor.registered){
   if(!registry.retire(pointer,actor.handle))return refuse(error,"retail pre-release registry retirement failed");
   actor.registered=false;
  }
  if(!family.release(pointer,actor.token,error))return false;
  if(actor.registered&&!registry.retire(pointer,actor.handle))return refuse(error,"retail released registry retirement failed");
  actor.registered=false;actor.actor=nullptr;
 }
 m->actors.clear();
 for(auto& source:m->sources)if(source.handle){
  if(!registry.retireGenerator(source.generator.get(),source.handle))return refuse(error,"retail source generator still bound");
  source.handle=0;
 }
 m->sources.clear();
 for(const auto& family:m->preparedFamilies)if(!family->cancel(error))return false;
 m->preparedFamilies.clear();
 m->prepared=false;m->cleaning=false;error.clear();return true;
}
} // namespace p2retail
bool pc_p2_retail_cave_native_retired(Creature* actor,std::string& error){
 for(auto* owner:p2retail::owners())if(!owner->retired(actor,error))return false;
 error.clear();return true;
}
