#pragma once
#include "pc_p2_original_captain_throw.h"
#include <array>
namespace p2original { namespace captain { namespace party {
using actions::Vec3;
struct CaptainFacts {
 bool alive=false,controller=false,movieActor=false;
 StateId state=StateId::Dead;
};
struct WorldFacts {
 bool active=false,softPaused=false,multiplayer=false,demoInactive=false;
 bool switchUnlocked=false,reunited=false;
 unsigned mode=0,day=0;
};
bool switchAllowed(const WorldFacts&,const CaptainFacts& other);
bool whistleAllowed(const WorldFacts&,const CaptainFacts& recipient);
bool needsChange(StateId);
struct Member {
 actions::PikiHandle handle;
 Vec3 position;
 unsigned kind=0;
 bool alive=false,releasable=false;
};
struct Group { unsigned count=0; Vec3 center; float radius=0; };
// Retail Navi::releasePikis: full 3D mean and four sequential separation
// passes, including each of the eight source kinds. Not a position teleport.
bool dismissGroups(const std::vector<Member>&,Vec3 captain,Vec3 other,
 bool otherAlive,std::array<Group,8>&,std::string&);
// Canonical original party owner. These operations must use actual source
// CPlate/Brain/InteractFue semantics and exact live handles; P1 acts are not
// compatible implementations. scene() is pointer-identical to LoadedScene.
class PartySource {
public:
 virtual ~PartySource()=default;
 virtual const LoadedScene& scene()const=0;
 virtual bool world(WorldFacts&,std::string&)const=0;
 virtual bool captain(const Navi&,CaptainFacts&,Vec3&,std::string&)const=0;
 virtual bool members(const Navi&,std::vector<Member>&,std::string&)const=0;
 virtual bool togglePlayer(Navi&,Navi&,std::string&)=0;
 virtual bool changeVoice(Navi&,std::string&)=0;
 virtual bool follow(Navi&,bool newToParty,std::string&)=0;
 virtual bool whistleMember(Navi& caller,actions::PikiHandle,bool combine,
  bool newToParty,std::string&)=0;
 virtual bool dismissSound(Navi&,std::string&)=0;
 virtual bool freeMember(Navi&,actions::PikiHandle,float radius,Vec3 center,
  bool sourceDismiss,std::string&)=0;
 virtual bool disbandTimer(Navi&,unsigned,std::string&)=0;
};
// Actual source native bridge: rechecks roster/world on every operation.
bool whistleCaptain(Navi* recipient,Navi* caller,bool combine,bool newToParty,std::string&);
bool dismissCaptain(Navi*,std::string&);
bool releasePikis(Navi*,bool& released,std::string&);
bool switchCaptain(Navi*,std::string&);
} // party
void registerPartyStates(NaviStateMachine&);
// SourceBank owner delivers actual authored keys; this consumes only Change.
bool partyKey(Navi*,int authoredKey);
} }
const p2original::captain::party::PartySource* pc_p2_original_captain_party_source(const Navi*);
