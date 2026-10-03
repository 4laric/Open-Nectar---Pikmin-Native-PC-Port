#include "pc_p2_original_captain_down.h"
#include "pc_p2_original_captain_down_sequence.h"
#include "pc_p2_original_captain_motion.h"
#include "Navi.h"
#include <fstream>
#include <iterator>
#include <iostream>
#include <stdexcept>
#include <limits>
#include <algorithm>
using namespace p2original::captain;
// Strong providers below are test doubles. They exercise the ACTUAL production
// down.cpp translation unit, not source-session authority or ordinary gameplay.
namespace {
Navi olimar,louie,outsider;int ownerToken=0,foreignToken=0;
struct Scene final:LoadedScene {
 std::string campaign="test-source-campaign",fingerprint="test-session-fingerprint",catalog="test-source-catalog";
 std::uint64_t epoch=1;Navi* actors[2]={&olimar,&louie};
 MoviePlayer* player=reinterpret_cast<MoviePlayer*>(&ownerToken);
 const std::string& selectedCampaign()const override{return campaign;}
 const std::string& selectedFingerprint()const override{return fingerprint;}
 const std::string& sourceCatalog()const override{return catalog;}
 std::uint64_t incarnation()const override{return epoch;}
 MoviePlayer* moviePlayer()const override{return player;}
 Navi* captainAt(unsigned i)const override{return i<2?actors[i]:nullptr;}
} scene,wrongScene;
struct TestWorld final:World {
 std::string campaign=scene.campaign,fingerprint=scene.fingerprint,catalog=scene.catalog;
 std::uint64_t epoch=1;Navi* actors[2]={&olimar,&louie};Phase game=Phase::GameWorldActive;
 const std::string& selectedCampaign()const override{return campaign;}
 const std::string& selectedFingerprint()const override{return fingerprint;}
 const std::string& sourceCatalog()const override{return catalog;}
 std::uint64_t incarnation()const override{return epoch;}
 Phase phase()const override{return game;}
 Demo demo()const override{return Demo::Inactive;}
 Navi* captainAt(unsigned i)const override{return i<2?actors[i]:nullptr;}
} world;
struct TestSection final:DownSection {
 const LoadedScene* identity=&::scene;
 bool permit=true,failStart=false,failCommand=false,failFinishBegin=false,failLaydown=false,failFinished=false,failInactive=false;
 int preflights=0,started=0,commands=0,cameras=0,actorCommands=0,finishBegin=0,laydowns=0,finishedCalls=0,inactives=0;
 std::vector<std::string> order;std::vector<bool> both;std::vector<Navi*> targets;
 std::vector<unsigned> bitsBeforeLaydown,bitsAtFinished;
 const LoadedScene& scene()const override{return *identity;}
 SectionKind kind()const override{return SectionKind::Cave;}
 bool result(bool failed,std::string& e,const char* reason){if(failed)e=reason;return !failed;}
 bool preflight(Navi&,std::string& e)const override{++const_cast<TestSection*>(this)->preflights;if(!permit)e="test section preflight refused";return permit;}
 bool movieStarted(Navi& n,std::string& e)override{++started;targets.push_back(&n);order.push_back("start");return result(failStart,e,"test section start refused");}
 bool movieCommand(Navi&,unsigned c,std::string& e)override{++commands;order.push_back("command"+std::to_string(c));return result(failCommand,e,"test section command refused");}
 bool cameraCommand(Navi&,unsigned,std::string&)override{++cameras;order.push_back("camera");return true;}
 bool actorCommand(Navi&,unsigned,std::string&)override{++actorCommands;order.push_back("actor");return true;}
 bool beginFinishing(Navi&,std::string& e)override{++finishBegin;order.push_back("finishing");return result(failFinishBegin,e,"test finishing begin refused");}
 bool laydownAndInformDeath(Navi&,std::string& e)override{++laydowns;order.push_back("laydown");unsigned bits=999;pc_p2_original_captain_down_dead_bits(bits);bitsBeforeLaydown.push_back(bits);return result(failLaydown,e,"test laydown refused");}
 bool finished(Navi&,bool all,std::string& e)override{++finishedCalls;both.push_back(all);order.push_back("finished");unsigned bits=999;pc_p2_original_captain_down_dead_bits(bits);bitsAtFinished.push_back(bits);return result(failFinished,e,"test finished result refused");}
 bool inactive(Navi&,std::string& e)override{++inactives;order.push_back("inactive");return result(failInactive,e,"test inactive refused");}
} section;
struct BankControl {
 bool ready=true,bound=true,sourceAvailable=true,start=true;
 unsigned failUpdate=0;int starts=0,updates=0;float lastFrame=0;
 std::string bytes;std::vector<float> frames;
} bankControl;
const LoadedScene* loaded=&scene;const World* activeWorld=&world;DownSection* sectionProvider=&section;
SourceBank* bankProvider=nullptr;
bool aliveProvider=true,alive[2]={true,true};int lifetimeQueries=0;
int checks=0;
void check(bool condition,const std::string& label){++checks;if(!condition)throw std::runtime_error("check "+std::to_string(checks)+": "+label);}
std::string error;
void reset(){
 ++scene.epoch;world.epoch=scene.epoch;world.campaign=scene.campaign;world.fingerprint=scene.fingerprint;world.catalog=scene.catalog;world.actors[0]=&olimar;world.actors[1]=&louie;world.game=Phase::GameWorldActive;
 loaded=&scene;activeWorld=&world;sectionProvider=&section;section=TestSection{};
 bankControl.ready=true;bankControl.bound=true;bankControl.sourceAvailable=true;bankControl.start=true;bankControl.failUpdate=0;bankControl.starts=0;bankControl.updates=0;bankControl.lastFrame=0;bankControl.frames.clear();
 aliveProvider=true;alive[0]=alive[1]=true;olimar.health=louie.health=50;
 Demo d=Demo::Absent;check(!pc_p2_original_captain_down_demo(d)&&d==Demo::Absent,"retired scene query leaves output unchanged");
 unsigned bits=88;check(pc_p2_original_captain_down_dead_bits(bits)&&bits==0,"new incarnation clears actual durable bits");
}
void studio(unsigned updates,float dt){for(unsigned i=0;i<updates;++i)pc_p2_original_captain_down_update(scene.player,dt);}
void finish(Navi* n){check(pc_p2_original_captain_down_begin(n,error),error);studio(207,.016f);pc_p2_original_captain_down_update(scene.player,2);pc_p2_original_captain_down_update(scene.player,0);}
}
namespace p2original { namespace captain {
// Definitions satisfy the genuine SourceBank class interface only in this
// standalone controls executable. No geometry/presentation/resource owner is
// substituted in the game executable.
struct SourceBank::Impl {};
SourceBank::SourceBank():m(new Impl){}SourceBank::~SourceBank()=default;
bool SourceBank::ready()const{return bankControl.ready;}
bool SourceBank::state(const Navi* n,MotionState& out,std::string& e)const{if(!bankControl.bound||(n!=&olimar&&n!=&louie)){e="test bank actor absent";return false;}out={};return true;}
bool SourceBank::sourceBytes(SourceResource resource,std::string& out,std::string& e)const{if(resource!=SourceResource::DownStudio||!bankControl.sourceAvailable){e="test source bytes absent";return false;}out=bankControl.bytes;return true;}
bool SourceBank::startDownMovie(Navi*,std::string& e){++bankControl.starts;if(!bankControl.start)e="test bank start failed";return bankControl.start;}
bool SourceBank::updateDownMovie(Navi*,float frame,std::string& e){++bankControl.updates;bankControl.lastFrame=frame;bankControl.frames.push_back(frame);if(bankControl.failUpdate&&frame==bankControl.failUpdate){e="test bank Studio update failed";return false;}return true;}
}}
const LoadedScene* pc_p2_original_captain_loaded_scene(){return loaded;}
const World* pc_p2_original_captain_world(){return activeWorld;}
DownSection* pc_p2_original_captain_down_section(){return sectionProvider;}
SourceBank* pc_p2_original_captain_source_bank(){return bankProvider;}
bool pc_p2_original_captain_actor_lifetime(const Navi* n,bool& out){++lifetimeQueries;if(!aliveProvider)return false;if(n==&olimar){out=alive[0];return true;}if(n==&louie){out=alive[1];return true;}return false;}
int main(int argc,char** argv){try{
 check(argc==2,"requires private canonical demo.stb argument");std::ifstream file(argv[1],std::ios::binary);bankControl.bytes=std::string((std::istreambuf_iterator<char>(file)),{});check(bankControl.bytes.size()==884,"actual private 884-byte Studio resource");
 check(bool(p2original::down::Sequence::authenticate(bankControl.bytes.data(),bankControl.bytes.size(),error)),error);SourceBank bank;bankProvider=&bank;
 reset();loaded=nullptr;check(!pc_p2_original_captain_down_preflight(&olimar,error),"missing canonical loaded scene");unsigned out=99;check(!pc_p2_original_captain_down_dead_bits(out)&&out==99,"missing canonical deadbits output unchanged");loaded=&scene;
 activeWorld=nullptr;check(!pc_p2_original_captain_down_preflight(&olimar,error),"missing actual world");activeWorld=&world;
 sectionProvider=nullptr;check(!pc_p2_original_captain_down_preflight(&olimar,error),"missing actual section");sectionProvider=&section;
 bankProvider=nullptr;check(!pc_p2_original_captain_down_preflight(&olimar,error),"missing actual bank");bankProvider=&bank;
 section.identity=&wrongScene;check(!pc_p2_original_captain_down_preflight(&olimar,error),"section wrong scene pointer");section.identity=&scene;
 world.fingerprint="wrong";check(!pc_p2_original_captain_down_preflight(&olimar,error),"world descriptor mismatch");world.fingerprint=scene.fingerprint;
 world.campaign="wrong";check(!pc_p2_original_captain_down_preflight(&olimar,error),"world campaign mismatch");world.campaign=scene.campaign;
 world.catalog="wrong";check(!pc_p2_original_captain_down_preflight(&olimar,error),"world catalog mismatch");world.catalog=scene.catalog;
 ++world.epoch;check(!pc_p2_original_captain_down_preflight(&olimar,error),"world incarnation mismatch");world.epoch=scene.epoch;
 world.actors[1]=&outsider;check(!pc_p2_original_captain_down_preflight(&olimar,error),"world roster mismatch");world.actors[1]=&louie;
 bankControl.ready=false;check(!pc_p2_original_captain_down_preflight(&olimar,error),"unprepared source bank");bankControl.ready=true;
 bankControl.bound=false;check(!pc_p2_original_captain_down_preflight(&olimar,error),"bank lacks actor binding");bankControl.bound=true;
 bankControl.sourceAvailable=false;check(!pc_p2_original_captain_down_preflight(&olimar,error),"no actual resource bytes");bankControl.sourceAvailable=true;
 auto canonicalBytes=bankControl.bytes;bankControl.bytes[0]^=1;check(!pc_p2_original_captain_down_begin(&olimar,error)&&section.started==0&&bankControl.starts==0,"mutated raw Studio rejects before side effects");bankControl.bytes=canonicalBytes;
 aliveProvider=false;check(!pc_p2_original_captain_down_preflight(&olimar,error),"missing genuine lifetime provider");aliveProvider=true;alive[0]=false;check(!pc_p2_original_captain_down_preflight(&olimar,error),"actual source lifetime already dead");alive[0]=true;
 check(!pc_p2_original_captain_down_preflight(&outsider,error),"outsider actor refuses");world.game=Phase::Inactive;check(!pc_p2_original_captain_down_preflight(&olimar,error),"inactive source world refuses");world.game=Phase::GameWorldActive;
 section.permit=false;check(!pc_p2_original_captain_down_begin(&olimar,error)&&bankControl.starts==0,"actual section preflight refusal");section.permit=true;
 check(pc_p2_original_captain_down_begin(&olimar,error),error);Demo d=Demo::Absent;check(pc_p2_original_captain_down_demo(d)&&d==Demo::Playing,"actual movie starts Playing");check(!pc_p2_original_captain_down_begin(&louie,error)&&bankControl.starts==1,"concurrent actor movie refuses");
 pc_p2_original_captain_down_update(reinterpret_cast<MoviePlayer*>(&foreignToken),1);check(bankControl.updates==0,"nonowner MoviePlayer cannot advance Studio");
 studio(196,0);check(bankControl.lastFrame==196&&section.commands==0&&section.finishBegin==0,"Studio exactly196 actual updates independent zero delta");
 pc_p2_original_captain_down_update(scene.player,100);check(bankControl.lastFrame==197&&section.commands==1&&section.order.back()=="command0","actual source command0 at197 despite giant dt");
 studio(10,.00001f);check(bankControl.lastFrame==207&&section.finishBegin==1&&section.finishedCalls==0,"actual source exhaustion207 before finishing callback");
 for(unsigned i=0;i<bankControl.frames.size();++i)check(bankControl.frames[i]==float(i+1),"each actual update advances exactlyone Studio tick");
 pc_p2_original_captain_down_update(scene.player,.9f);check(section.laydowns==0,"Finishing equality1.1 does not deliver callback");
 pc_p2_original_captain_down_update(scene.player,.0001f);check(section.laydowns==1&&section.finishedCalls==1&&section.inactives==0&&!section.both.back(),"strict finishing<1.1 callback and survivor dispatch");
 unsigned bits=99;check(pc_p2_original_captain_down_dead_bits(bits)&&bits==1,"durable slotbit after actual laydown acceptance");
 check(section.bitsBeforeLaydown.back()==0&&section.bitsAtFinished.back()==1,"source durable commit occurs after accepted laydown and before finished");
 check(alive[0]&&alive[1]&&olimar.health==50&&louie.health==50,"Down movie never writes sourceAlive/HP; genuine Dead owner outside TU");
 pc_p2_original_captain_down_update(scene.player,5);check(section.inactives==1&&section.laydowns==1&&pc_p2_original_captain_down_demo(d)&&d==Demo::Inactive,"later update delivers distinct inactive onlyonce");
 studio(20,1);check(section.laydowns==1&&section.inactives==1&&bankControl.updates==207,"no refire or bank advancement after inactive");
 finish(&louie);check(section.both.back()&&section.finishedCalls==2&&pc_p2_original_captain_down_dead_bits(bits)&&bits==3,"second captain exact sequence produces bothDown census");
 check(section.bitsBeforeLaydown.back()==1&&section.bitsAtFinished.back()==3,"second down retains first bit and commits second before dispatch");
 reset();check(pc_p2_original_captain_down_begin(&olimar,error),error);pc_p2_original_captain_down_update(scene.player,std::numeric_limits<float>::quiet_NaN());check(bankControl.updates==0&&pc_p2_original_captain_down_demo(d)&&d==Demo::Playing,"invalid delta refuses and holds Playing");bits=77;check(!pc_p2_original_captain_down_dead_bits(bits)&&bits==77,"failed downbits query unchanged");studio(220,1);check(bankControl.updates==0&&section.commands==0&&section.laydowns==0,"failed movie never advances/refires");check(!pc_p2_original_captain_down_begin(&louie,error),"failed active movie cannot restart");
 reset();check(pc_p2_original_captain_down_begin(&olimar,error),error);pc_p2_original_captain_down_update(scene.player,-1);check(bankControl.updates==0&&pc_p2_original_captain_down_demo(d)&&d==Demo::Playing,"negative delta refuses");
 reset();bankControl.failUpdate=197;check(pc_p2_original_captain_down_begin(&olimar,error),error);studio(220,.01f);check(bankControl.updates==197&&section.commands==0&&section.finishBegin==0,"actual presentation failure blocks same-tick callbacks and future advancement");
 reset();section.failCommand=true;check(pc_p2_original_captain_down_begin(&olimar,error),error);studio(220,.01f);check(bankControl.updates==197&&section.commands==1&&section.finishBegin==0,"section command failure never retries/refires");
 reset();section.failLaydown=true;check(pc_p2_original_captain_down_begin(&olimar,error),error);studio(207,.01f);pc_p2_original_captain_down_update(scene.player,2);check(section.laydowns==1&&section.finishedCalls==0,"rejected laydown blocks durable commit/finished callback");bits=44;check(!pc_p2_original_captain_down_dead_bits(bits)&&bits==44,"refused laydown output unchanged");studio(10,2);check(section.laydowns==1&&section.inactives==0,"failed laydown cannot refire/inactivate");
 reset();bankControl.start=false;check(!pc_p2_original_captain_down_begin(&olimar,error)&&section.started==0,"bank presentation start fails before section/movie binding");studio(210,.01f);check(bankControl.updates==0&&section.commands==0,"unaccepted bank start cannot advance/refire");
 reset();section.failStart=true;check(!pc_p2_original_captain_down_begin(&olimar,error)&&section.started==1,"section movie start failure refuses clock binding");studio(210,.01f);check(bankControl.updates==0&&section.commands==0,"unaccepted section start cannot advance/refire");
 reset();section.failFinishBegin=true;check(pc_p2_original_captain_down_begin(&olimar,error),error);studio(220,.01f);check(bankControl.updates==207&&section.finishBegin==1&&section.laydowns==0,"beginFinishing callback failure stops at207 once");
 reset();section.failFinished=true;check(pc_p2_original_captain_down_begin(&olimar,error),error);studio(207,.01f);pc_p2_original_captain_down_update(scene.player,2);check(section.laydowns==1&&section.finishedCalls==1&&section.bitsBeforeLaydown.back()==0&&section.bitsAtFinished.back()==1,"finished refusal occurs after durable accepted laydown");studio(10,2);check(section.finishedCalls==1&&section.inactives==0,"failed result cannot refire/inactivate");bits=66;check(!pc_p2_original_captain_down_dead_bits(bits)&&bits==66,"failed result query preserves output");
 reset();section.failInactive=true;check(pc_p2_original_captain_down_begin(&olimar,error),error);studio(207,.01f);pc_p2_original_captain_down_update(scene.player,2);pc_p2_original_captain_down_update(scene.player,0);studio(10,2);check(section.inactives==1&&section.laydowns==1&&pc_p2_original_captain_down_demo(d)&&d==Demo::Playing,"inactive callback failure stays Playing and never refires");
 reset();check(pc_p2_original_captain_down_begin(&olimar,error),error);studio(5,.01f);++scene.epoch;world.epoch=scene.epoch;pc_p2_original_captain_down_update(scene.player,1);check(bankControl.updates==5&&section.laydowns==0,"stale incarnation retires active owner");d=Demo::Absent;check(!pc_p2_original_captain_down_demo(d)&&d==Demo::Absent,"retired demo query unchanged");bits=44;check(pc_p2_original_captain_down_dead_bits(bits)&&bits==0,"new incarnation does not inherit dead bits");
 std::cout<<"P2_ORIGINAL_DOWN_ACTUAL_TU_CONTROLS_PASS checks="<<checks<<" privateStudioBytes=884 gameplay=UNTESTED providers=DOUBLES\n";
 return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
