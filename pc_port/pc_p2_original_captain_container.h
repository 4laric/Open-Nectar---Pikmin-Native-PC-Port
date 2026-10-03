#pragma once
#include "pc_p2_original_captain_throw.h"
#include <array>
namespace Screen {class Game2DMgr;}
namespace Game {class CameraMgr;}
class Creature;
namespace p2original {namespace captain {namespace items {
using actions::Vec3;using actions::PikiHandle;
struct OnyonHandle {Creature* actor=nullptr;std::uint64_t lifetime=0;};
struct OnyonFrame {OnyonHandle handle;int type=0;std::uint32_t whitesQueued=0,purplesQueued=0;};
struct ScreenHandle {Screen::Game2DMgr* actor=nullptr;std::uint64_t lifetime=0;};
struct ContainerFacts {std::array<int,5> stored{},squad{};int partyTotal=0,mapCount=0,zikatuCount=0;bool whiteOwned=false,purpleOwned=false,debtPaid=false;};
struct MenuData {int color=0,inOnyon=0,currField=128000,inSquad=0,maxOnField=100,inParty=0,onMap=0,maxPikis=100;};
struct ShipMenu {MenuData white,purple;bool whiteOwned=false,purpleOwned=false,debtPaid=false;};
enum class MenuCheck:int {Error=-1,Open=0,Cancel=1,Confirmed=2};
// Exact current source Section/Game2DMgr, GameStat/PlayData, Onyon and Piki
// brain owner. Menu opening/result APIs are actual rendered menu lifecycle,
// not script answers, synthetic deltas, P1 GoalItem or Suck action delegates.
class ContainerSource {
public:
 virtual ~ContainerSource()=default;
 virtual const LoadedScene& scene()const=0;
 virtual bool screen(ScreenHandle&,std::string&)const=0;
 virtual bool onyon(OnyonHandle,OnyonFrame&,std::string&)const=0;
 virtual bool facts(const Navi&,ContainerFacts&,std::string&)const=0;
 virtual bool setGamePad(ScreenHandle,Navi&,std::string&)=0;
 virtual bool openOnyon(ScreenHandle,const MenuData&,bool& opened,std::string&)=0;
 virtual bool openShip(ScreenHandle,const ShipMenu&,bool& opened,std::string&)=0;
 virtual bool check(ScreenHandle,bool ship,MenuCheck&,std::string&)const=0;
 virtual bool result(ScreenHandle,bool ship,int& whiteOrOnyon,int& purple,std::string&)const=0;
 virtual bool freeze(bool,std::string&)=0; // source setFrozen(...,"open-cont")
 virtual bool moviePause(bool,std::string&)=0; // setMoviePause(...,"open-cont")
 virtual bool velocities(Navi&,Vec3 actual,Vec3 target,std::string&)=0;
 virtual bool squad(const Navi&,std::vector<actions::PikiFrame>&,std::string&)const=0;
 virtual bool enterPiki(Navi&,PikiHandle,OnyonHandle,std::string&)=0; // actual brain ACT_Enter(CreatureActionArg(onyon))
 virtual bool exitPikis(OnyonHandle,int count,int sourceColor,std::string&)=0;
};
struct HoneyHandle {Creature* actor=nullptr;std::uint64_t lifetime=0;};
enum class Honey:int {Yellow=0,Red=1,Black=2};
struct HoneyFrame {HoneyHandle handle;Vec3 position;Honey type=Honey::Yellow;bool honey=false,alive=false,shrinking=false;};
struct CameraHandle {Game::CameraMgr* actor=nullptr;std::uint64_t lifetime=0;};
// Actual Honey lifetime/retention/InteractAbsorb, source camera and campaign
// owner. Retained dead Honey metadata must remain accessible through END.
// Existing P1 NaviReceiver/Manager::start(Navi*) cannot implement this provider.
class AbsorbSource {
public:
 virtual ~AbsorbSource()=default;
 virtual const LoadedScene& scene()const=0;
 virtual bool camera(CameraHandle&,std::string&)const=0;
 virtual bool honey(HoneyHandle,HoneyFrame&,std::string&)const=0;
 virtual bool retain(HoneyHandle,std::string&)=0;
 virtual bool release(HoneyHandle,std::string&)=0;
 virtual bool drinkSound(Navi&,std::string&)=0;
 virtual bool turnTo(Navi&,Vec3,std::string&)=0;
 virtual bool lockCamera(CameraHandle,Navi&,bool,std::string&)=0;
 virtual bool startNearLow(CameraHandle,Navi&,std::string&)=0; // CAMDEMO_NearLow0
 virtual bool finishCamera(CameraHandle,Navi&,std::string&)=0;
 virtual bool velocities(Navi&,Vec3 actual,Vec3 target,std::string&)=0;
 virtual bool absorb(Navi&,HoneyHandle,bool& accepted,std::string&)=0; // literal InteractAbsorb; result ignored by Navi
 // Source incDopeCount(honeyType!=HONEY_R); actual physical source child +
 // captain-index owner credits/dedupes campaign inventory, never this leaf.
 virtual bool creditSpray(Navi&,HoneyHandle,int sourceSprayIndex,std::string&)=0;
};
bool beginContainer(Navi*,OnyonHandle,std::string&);
bool beginAbsorb(Navi*,HoneyHandle,std::string&);
} void registerContainerAbsorbStates(NaviStateMachine&);
}}
p2original::captain::items::ContainerSource* pc_p2_original_captain_container_source(const Navi*);
p2original::captain::items::AbsorbSource* pc_p2_original_captain_absorb_source(const Navi*);
bool pc_p2_original_captain_container_absorb_preflight(Navi*,p2original::captain::StateId,std::string&);
bool pc_p2_original_captain_container_absorb_advance_animation(Navi*,float,std::string&);
