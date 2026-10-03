#include "pc_p2_original_piki_native_facts.h"
#include "system.h"
#include <iostream>
#include <cstdlib>
using namespace p2original;
using namespace p2original::piki;
void* System::alloc(size_t bytes){return std::malloc(bytes);}
namespace {
unsigned checks=0;
void check(bool ok,const char* text){++checks;if(!ok){std::cerr<<text<<'\n';std::exit(1);}}
Navi* first=reinterpret_cast<Navi*>(0x1000);Navi* second=reinterpret_cast<Navi*>(0x2000);
bool live=true,installed=true;
class Scene final:public captain::LoadedScene {
public:
 std::string campaign="campaign",fingerprint="seed",catalog="catalog";std::uint64_t epoch=7;
 const std::string& selectedCampaign()const override{return campaign;}
 const std::string& selectedFingerprint()const override{return fingerprint;}
 const std::string& sourceCatalog()const override{return catalog;}
 MoviePlayer* moviePlayer()const override{return nullptr;}
 std::uint64_t incarnation()const override{return epoch;}
 Navi* captainAt(unsigned i)const override{return i==0?first:i==1?second:nullptr;}
} scene;
class World final:public captain::World {
public:
 std::string campaign="campaign",fingerprint="seed",catalog="catalog";std::uint64_t epoch=7;
 captain::Phase current=captain::Phase::Loading;
 const std::string& selectedCampaign()const override{return campaign;}
 const std::string& selectedFingerprint()const override{return fingerprint;}
 const std::string& sourceCatalog()const override{return catalog;}
 std::uint64_t incarnation()const override{return epoch;}
 captain::Phase phase()const override{return current;}
 captain::Demo demo()const override{return captain::Demo::Absent;}
 Navi* captainAt(unsigned i)const override{return i==0?first:i==1?second:nullptr;}
} world;
}
const captain::LoadedScene* pc_p2_original_captain_loaded_scene(){return installed?&scene:nullptr;}
const captain::World* pc_p2_original_captain_world(){return installed?&world:nullptr;}
bool pc_p2_original_captain_actor_alive(const Navi*){return live;}
bool pc_p2_original_piki_body_current(const Piki*,std::uint64_t)noexcept{return false;}
bool pc_p2_original_piki_body_handle(const Piki*,OriginalPikiBodyHandle&){return false;}
const std::string& pc_p2_original_piki_catalog_fingerprint()noexcept{static const std::string none;return none;}
bool pc_p2_original_piki_recruit_pair_ready()noexcept{return false;}
PcP2SourceBodyKind pc_p2_source_body_query(const Piki*,PcP2SourceBody&){return PcP2SourceBodyKind::None;}
bool pc_p2_source_body_admitted(const PcP2SourceBody&,const std::string&,const std::string&,std::string&){return false;}
int main(){
 std::string e;SceneBinding b;b.incarnation=99;
 installed=false;check(!nativeSceneBinding(b,false,e)&&b.incarnation==99,"absent real scene unchanged");installed=true;
 check(nativeSceneBinding(b,false,e)&&b.incarnation==7,"Loading source binding");
 check(nativeCaptainFacts(b,first,false,e),"actual source captain Loading");
 live=false;check(!nativeCaptainFacts(b,first,false,e),"source CF-dead refuses acquisition");
 check(nativeCaptainFacts(b,first,true,e),"cleanup permits retained CF-dead captain");live=true;
 check(!nativeCaptainFacts(b,reinterpret_cast<Navi*>(0x3000),false,e),"foreign P1 captain refuses");
 ++scene.epoch;check(!nativeSceneCurrent(b,true,e),"different scene incarnation refuses");--scene.epoch;
 world.catalog="other";check(!nativeSceneBinding(b,true,e),"catalog mismatch refuses");world.catalog="catalog";
 world.campaign="other";check(!nativeSceneBinding(b,true,e),"campaign mismatch refuses");world.campaign="campaign";
 world.fingerprint="other";check(!nativeSceneBinding(b,true,e),"seed mismatch refuses");world.fingerprint="seed";
 world.current=captain::Phase::Inactive;check(!nativeSceneBinding(b,false,e),"inactive acquire refuses");
 check(nativeSceneBinding(b,true,e)&&nativeCaptainFacts(b,first,true,e),"inactive exact roster cleanup");
 world.current=captain::Phase::GameWorldActive;
 NativeBodyFacts body;body.happa=8;check(!nativeBodyFacts({},body,e)&&body.happa==8,"null body unchanged");
 check(!nativeBodyFacts({reinterpret_cast<Piki*>(0x4000),77},body,e)&&body.happa==8,"stale lifetime unchanged before dereference");
 PhysicalFacts physical;physical.frozen=true;check(!nativePhysicalFacts({},nullptr,physical,e)&&physical.frozen,"missing actual physical producer explicitly refuses unchanged");
 second=first;check(!nativeSceneBinding(b,true,e),"duplicate captain roster refuses");
 std::cout<<checks<<" actual native facts TU component controls passed (synthetic scene; no physical actor grant)\n";
}
