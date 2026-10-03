#include "pc_p2_retail_cave_blackpom.h"
#include "Pom.h"
namespace p2retail {
bool bindBlackPom(NativeFloor& floor,p2original::blackpom::Native& native,
                  BlackPomPopulation population,BlackPomConsumer consumer,std::string& error){
 if(!population||!consumer){error="retail BlackPom actual population/consumer unavailable";return false;}
 FamilyOps ops;
 ops.prepare=[&native](const std::vector<p2original::CatalogRow>& rows,std::string& e){
  unsigned count=0;for(const auto& row:rows)count+=row.enemy.count;return native.prepare(count,e);
 };
 ops.birth=[&native,population](const p2original::CatalogRow& row,const Snapshot& floor,
          Generator* generator,unsigned,const Vector3f& p,float yaw,Creature*& actor,bool& suppressed,std::string& e){
  p2original::blackpom::BirthContext context;
  if(!population(floor,context,e))return false;
  if(!context.section||context.inCave!=floor.inCave||context.story!=floor.story||context.cave!=floor.cave||
     context.floorIndex!=floor.floor-1||row.caveFloor!=floor.floor){
   e="retail BlackPom physical population scene differs";return false;
  }
  Pom* pom=nullptr;bool born=native.birth(generator,p,yaw,context,pom,suppressed,e);actor=pom;return born;
 };
 ops.bind=[&native](const p2original::CatalogRow&,Creature* actor,unsigned token,std::string& e){
  return native.bind(static_cast<Pom*>(actor),token,e);
 };
 ops.release=[&native](Creature* actor,unsigned,std::string& e){return native.release(static_cast<Pom*>(actor),e);};
 ops.cancel=[&native](std::string& e){return native.cancel(e);};ops.retireBeforeRelease=true;
 // Natural beforeKill already calls floor retirement then leaf nativeRetired.
 // Explicit Native.release takes its own cleanup path and bypasses onDeath.
 ops.retired=[](Creature*,std::string& e){e.clear();return true;};
 if(!floor.family(6,std::move(ops),error))return false;
 native.onDeath([&floor](Pom* actor,std::string& e){return floor.retired(actor,e);});
 native.onBind(std::move(consumer));return true;
}
}
