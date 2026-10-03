#include "pc_p2_original_course.h"
#include "pc_p2_original_dispatch.h"
#include "pc_p2_original_gen_object.h"
#include "pc_p2_original_group_engine.h"
#include "pc_p2_original_pelplant_native.h"
#include "pc_p2_original_chappy_native.h"
#include "pc_p2_original_frog_native.h"
#include "pc_p2_original_uji_native.h"
#include "pc_p2_original_manifest.h"
#include "pc_p2_original_progress.h"
#include "Creature.h"
#include "teki.h"
#include <fstream>
#include <memory>
#include <cstdio>
#include <cstdlib>
namespace {
using namespace p2original;
struct Course {
 std::unique_ptr<pelplant::Native> plants;
 std::unique_ptr<chappy::Native> chappies;
 std::unique_ptr<frog::Native> frogs;
 std::unique_ptr<uji::Native> ujis;
 Dispatch dispatch;
 std::map<unsigned,GeneratorState> literal;
 std::set<const Generator*> shadows;
 bool started=false;
};
std::unique_ptr<Course> current;
bool fail(std::string& e,const char* text){e=text;return false;}
}
bool pc_p2_original_course_prepare(const std::string& fingerprint,const std::vector<CatalogRow>& rows,
 const std::vector<GeneratorState>& literal,std::function<bool(unsigned)> metColor,std::string& e){
 if(current||!metColor||rows.size()!=literal.size())return fail(e,"original course prepare requires unowned complete source inventory");
 auto next=std::make_unique<Course>();next->plants=std::make_unique<pelplant::Native>(std::move(metColor));
 next->chappies=std::make_unique<chappy::Native>();next->frogs=std::make_unique<frog::Native>();next->ujis=std::make_unique<uji::Native>();
 if(!next->dispatch.add(0,next->plants->provider(),[](const CatalogRow& r,std::string& e){pelplant::Initial value;return pelplant::decode(r,value,e);},e))return false;
 for(unsigned source:{2u,43u})if(!next->dispatch.add(source,next->chappies->provider(),chappy::admits,e))return false;
 for(unsigned source:{17u,18u})if(!next->dispatch.add(source,next->frogs->provider(),frog::capability,e))return false;
 for(unsigned source:{12u,13u,14u})if(!next->dispatch.add(source,next->ujis->provider(),uji::decode,e))return false;
 // Validate structural/source metadata atomically BEFORE publishing catalog.
 Catalog checked;
 if(!checked.install(fingerprint,rows,[&](const CatalogRow& r,std::string& e){return next->dispatch.capability(r,e);},e))return false;
 for(const auto& s:literal){auto* r=checked.find(s.uid);std::string bytes;
  if(!r||s.count!=r->enemy.count||s.epoch||s.activation||s.dayNum||s.deathCount
   ||!encodeOriginalState(fingerprint,s,bytes,e)||!next->literal.emplace(s.uid,s).second)
   return fail(e,"original literal inventory is incomplete, duplicated or contains runtime state");
 }
 if(!originalActors().install(fingerprint,rows,[&](const CatalogRow& r,std::string& e){return next->dispatch.capability(r,e);},e))return false;
 next->plants->onIdentity([](Creature* actor,std::string& identity,std::string& e){
  unsigned source=0,token=0;InstanceIdentity id;
  if(!originalActors().query(actor,source,token,&id)||source!=0)return fail(e,"original plant lost full course identity");
  identity=id.catalog+":"+std::to_string(id.generator)+":"+std::to_string(id.ordinal)+":"+std::to_string(id.epoch)+":"+std::to_string(id.activation);
  e.clear();return true;
 });
 next->plants->onDeath([](Creature* actor,std::string& e){
  if(!current||!current->started||!actor||!actor->mGenerator)return fail(e,"original plant death outside active course");
  bool handled=false;
  if(!pc_p2_original_generator_death(actor->mGenerator,actor,handled,e)||!handled)return false;
  // kill(false) calls Generator::informDeath itself. The source END above is
  // the one gameplay death; detach before disposal to prevent a second count.
  actor->mGenerator=nullptr;
  const unsigned token=pc_p2_original_actor_token(actor);
  if(!current->dispatch.release(actor,token,e))return false;
  pc_p2_original_native_retired(actor);return true;
 });
 current=std::move(next);e.clear();return true;
}
bool pc_p2_original_course_start(GeneratorList* list,std::string& e){
 if(!current||current->started||!list||!list->mGenListHead)return fail(e,"original course start requires prepared native generator list");
 std::vector<GroupBinding> bindings;std::map<unsigned,std::vector<Generator*>> inventory;
 // Validate the entire list before collect mutates compatibility observations.
 for(auto* node=list->mGenListHead->mChild;node;node=node->mNext){
  auto* g=static_cast<Generator*>(node);auto* object=dynamic_cast<GenObjectOriginalEnemy*>(g->mGenObject);
  if(!object)continue;
  auto found=current->literal.find(object->mState.uid);
  if(found==current->literal.end())return fail(e,"original native list has unknown source object");
  inventory[found->first].push_back(g);
 }
 if(inventory.size()!=current->literal.size())return fail(e,"original native list omitted source objects");
 std::set<const Generator*> shadows;
 for(const auto& entry:inventory){Generator* cached=nullptr;Generator* disc=nullptr;GroupBinding selected;
  for(auto* g:entry.second){GroupBinding b;
   if(!pc_p2_original_gen_object_collect(g,current->literal.at(entry.first),b,e))return false;
   if(g->readFromRam()){
    if(cached)return fail(e,"original native cache duplicates source UID");cached=g;selected=b;
   }else{
    if(disc)return fail(e,"original native disc duplicates source UID");disc=g;
    if(!cached)selected=b;
   }
  }
  if(cached&&disc)shadows.insert(disc); // exact literal validation already passed
  bindings.push_back(selected);
 }
 if(!pc_p2_original_course_install(bindings,current->dispatch,e))return false;
 current->shadows=std::move(shadows);current->started=true;e.clear();return true;
}
bool pc_p2_original_course_finish(std::string& e){
 if(!current){e.clear();return true;}
 if(current->started&&!pc_p2_original_course_unload(e))return false;
 current.reset();e.clear();return true;
}
bool pc_p2_original_course_prepared(){return bool(current);}
void pc_p2_original_course_retired(Creature* actor){if(current)current->dispatch.retired(actor);}
bool pc_p2_original_course_shadow(const Generator* g){return current&&current->shadows.count(g);}
bool pc_p2_original_course_load(const char* directory,const char* course,std::function<bool(unsigned)> metColor,std::string& e){
 if(!directory||!*directory||!course||!*course)return fail(e,"original private manifest requires selected surface course");
 const std::string selected=course;
 for(char c:selected)if(!((c>='a'&&c<='z')||c=='_'))return fail(e,"original manifest course path invalid");
 const std::string path=std::string(directory)+"/"+selected+".p2c";
 std::ifstream input(path,std::ios::binary|std::ios::ate);
 if(!input)return fail(e,"selected original course manifest missing");
 const auto size=input.tellg();if(size<=0||size>4*1024*1024)return fail(e,"original private manifest size invalid");
 std::string bytes(size_t(size),'\0');input.seekg(0);
 if(!input.read(bytes.data(),size))return fail(e,"original private manifest read failed");
 SourceManifest manifest;
 if(!readSourceManifest(bytes,selected,manifest,e))return false;
 if(!originalProgress().initialize(manifest.fingerprint,e))return false;
 return pc_p2_original_course_prepare(manifest.fingerprint,manifest.rows,manifest.literal,std::move(metColor),e);
}
bool pc_p2_original_course_use_models(std::string& e){
 if(!current){e.clear();return true;}
 if(!tekiMgr)return fail(e,"original course model admission lacks native manager");
 // Explicit early chassis reservation precedes startStage. Physical preflight
 // later verifies actual bank/corpse/drop resources and manager capacity.
 for(const auto& entry:originalActors().rows()){
  const unsigned source=entry.second.enemy.source;int type=-1;
  if(source==0)type=TEKI_Palm;
  else if(source==2||source==43)type=TEKI_Swallow;
  else if(source==17)type=TEKI_Frog;
  else if(source==18)type=TEKI_Frow;
  else if(uji::species(source))type=uji::nativeType(source);
  if(type<0)return fail(e,"original source has no early resource owner");
  tekiMgr->mUsingType[type]=true;
 }
 e.clear();return true;
}
bool pc_p2_original_course_boot(const char* directory,const char* course,std::string& e){
 return pc_p2_original_course_load(directory,course,pc_p2_original_progress_met,e);
}
void pc_p2_original_course_day_advanced(){
 const char* catalog=std::getenv("PIKMIN_P2_ORIGINAL_CATALOG");
 if(!catalog||!*catalog||!originalProgress().ready()||!originalProgress().context().story)return;
 std::string e;
 if(!originalProgress().nextDay(e)){
  std::fprintf(stderr,"P2_ORIGINAL_DAY_ADVANCE_FAIL %s\n",e.c_str());std::abort();
 }
}
