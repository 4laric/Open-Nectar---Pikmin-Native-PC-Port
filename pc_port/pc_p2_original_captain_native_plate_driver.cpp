#include "pc_p2_original_captain_native_plate_driver.h"
#include "pc_p2_original_captain_native_reader.h"
#include "pc_p2_original_piki_native_facts.h"
#include <algorithm>
#include <cstdlib>
namespace p2original {namespace piki {
Plate* nativePlate(const captain::LoadedScene&,std::string&);
PhysicalSource* nativePhysicalSource(const captain::LoadedScene&,std::string&);
}}
namespace p2original {namespace captain {namespace nativereader {
namespace {
bool fail(std::string& e,const char* s){e=s;return false;}
class NativeDriver final:public PlateDriver {
public:
 Reader* reader=nullptr;piki::Plate* plate=nullptr;const LoadedScene* scene=nullptr;
 std::uint64_t serial=0;std::string campaign,session,catalog;
 piki::Plate& storage()const override{if(!plate)std::abort();return *plate;}
 bool current(Navi* n,std::string& e)const {
  auto* s=pc_p2_original_captain_loaded_scene();auto* w=pc_p2_original_captain_world();
  if(!reader||!plate||!s||s!=scene||!w||s->incarnation()!=serial||w->incarnation()!=serial
   ||s->selectedCampaign()!=campaign||w->selectedCampaign()!=campaign||s->selectedFingerprint()!=session||w->selectedFingerprint()!=session
   ||s->sourceCatalog()!=catalog||w->sourceCatalog()!=catalog||pc_p2_original_captain_native_reader()!=reader
   ||piki::nativePlate(*s,e)!=plate||(w->phase()!=Phase::Loading&&w->phase()!=Phase::GameWorldActive)
   ||(n&&n!=s->captainAt(0)&&n!=s->captainAt(1)))return fail(e,"actual source CPlate driver lost sole Body/Reader owner");
  return !n||reader->currentBody(n,e);
 }
 bool execute(Navi* n,const cstick::Command& command,std::string& e)override {
  if(!n||!current(n,e))return false;
  bool result=false;
  switch(command.kind){
   case cstick::CommandKind::Refresh:result=plate->refresh(n,int(command.count),command.strength,e);break;
   case cstick::CommandKind::SetPos:result=plate->setPos(n,e);break;
   case cstick::CommandKind::SetPosGray:result=plate->setPosGray(n,e);break;
   case cstick::CommandKind::Rearrange:result=plate->rearrange(n,Vector3f(command.position.x,command.position.y,command.position.z),e);break;
  }
  return result&&current(n,e);
 }
 bool afterRefresh(Navi* n,cstick::AfterRefresh& out,std::string& e)const override {
  if(!n||!current(n,e))return false;
  piki::PlateState state;std::vector<piki::Frame> roster;
  auto* physical=piki::nativePhysicalSource(*scene,e);
  if(!physical||&physical->scene()!=scene||!plate->state(n,state,e)||!piki::roster(roster,e)||!current(n,e))return false;
  std::vector<piki::Frame> members;
  for(const auto& member:roster)if(member.captain==n&&member.formationSlot>=0)members.push_back(member);
  std::sort(members.begin(),members.end(),[](const piki::Frame& a,const piki::Frame& b){return a.formationSlot<b.formationSlot;});
  if(members.size()!=state.count)return fail(e,"source CPlate census differs from committed Body formation membership");
  cstick::AfterRefresh next;
  for(unsigned slot=0;slot<members.size();++slot){
   const auto& member=members[slot];piki::PhysicalFacts facts;
   if(member.formationSlot!=int(slot)||!piki::nativePhysicalFacts(member.handle,physical,facts,e)||!current(n,e))return false;
   // Literal whole CPlate iteration includes CF-dead members. Body-owned
   // physical lifetime authenticates them; no living/releasable filtering.
   actions::PikiFrame frame;frame.handle={member.handle.body,member.handle.lifetime};
   frame.position={member.position.x,member.position.y,member.position.z};frame.kind=member.species;frame.happa=member.happa;frame.captain=member.captain;frame.throwable=member.throwable;
   switch(member.state){case piki::State::Walk:frame.state=actions::PikiState::Walk;break;case piki::State::GoHang:frame.state=actions::PikiState::GoHang;break;case piki::State::Hanged:frame.state=actions::PikiState::Hanged;break;case piki::State::Flying:frame.state=actions::PikiState::Flying;break;case piki::State::LookAt:break;}
   next.members.push_back(frame);
  }
  if(state.maxPositionKnown)next.maxPositionOffset=actions::Vec3{state.maxPositionOffset.x,state.maxPositionOffset.y,state.maxPositionOffset.z};
  if(!current(n,e))return false;
  out=std::move(next);return true;
 }
};
NativeDriver driver;
}
bool bindNativePlateDriver(std::string& e){
 auto* reader=dynamic_cast<Reader*>(pc_p2_original_captain_native_reader());auto* scene=pc_p2_original_captain_loaded_scene();
 if(!reader||!scene||&reader->scene()!=scene)return fail(e,"actual CPlate driver requires initialized canonical captain Reader");
 auto* plate=piki::nativePlate(*scene,e);if(!plate)return false;
 if(driver.scene==scene && driver.serial==scene->incarnation() && driver.plate && driver.plate!=plate)
  return fail(e,"actual CPlate storage changed within retained source scene");
 // Rebinding uses only Body's stable sole storage after Reader has performed
 // checked retirement. The reader itself refuses retained scene replacement.
 driver.reader=reader;driver.plate=plate;driver.scene=scene;driver.serial=scene->incarnation();
 driver.campaign=scene->selectedCampaign();driver.session=scene->selectedFingerprint();driver.catalog=scene->sourceCatalog();
 return driver.current(nullptr,e)&&reader->bindPlate(driver,e);
}
}}}
