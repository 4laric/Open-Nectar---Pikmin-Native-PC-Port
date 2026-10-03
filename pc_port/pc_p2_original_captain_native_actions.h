#pragma once
#include "pc_p2_original_captain_actions_party.h"
#include "pc_p2_original_captain_motion.h"
#include "pc_p2_original_piki_runtime.h"
#include "pc_p2_original_piki_plate.h"
#include <memory>
namespace p2original { namespace captain { namespace nativeactions {
// Actual body owner implements only source fields/effects absent from the
// physical Navi and SourcePiki/Plate objects. No P1 control/whistle/Brain adapter.
struct Observation {actions::Vec3 cursor;float delta=0;bool movieActor=false;};
struct WhistleCandidate {actions::PikiHandle piki;Navi* captain=nullptr;};
class ActorSource {
public:
 virtual ~ActorSource()=default;
 virtual const LoadedScene& scene()const=0;
 virtual bool observe(const Navi&,Observation&,std::string&)const=0;
 virtual bool world(party::WorldFacts&,std::string&)const=0;
 virtual bool whistle(const Navi&,actions::WhistleFrame&,std::string&)const=0;
 virtual bool startWhistle(Navi&,std::string&)=0;
 virtual bool stopWhistle(Navi&,std::string&)=0;
 virtual bool updateWhistle(Navi&,actions::Vec3,bool,std::string&)=0;
 // Actual ordered source sphere/CellIterator census; no fabricated empty list.
 virtual bool whistleCandidates(const Navi&,std::vector<WhistleCandidate>&,std::string&)const=0;
 virtual bool holdFields(Navi&,float,float,float,std::string&)=0;
 virtual bool nextThrowPiki(Navi&,std::optional<actions::PikiHandle>,std::string&)=0;
 virtual bool feedback(Navi&,actions::Feedback,actions::PikiHandle,std::string&)=0;
 // Source pmTogglePlayer changes controller/camera ownership; source FSM
 // Follow/Change transitions remain with the typed Party caller afterwards.
 virtual bool togglePlayer(Navi&,Navi&,std::string&)=0;
 virtual bool changeVoice(Navi&,std::string&)=0;
 virtual bool dismissSound(Navi&,std::string&)=0;
 virtual bool disbandTimer(Navi&,unsigned,std::string&)=0;
 virtual bool followFrame(const Navi&,party::FollowFrame&,std::string&)const=0;
 virtual bool randomChoice(float&,std::string&)=0;
 virtual bool followFeedback(Navi&,party::FollowFeedback,std::string&)=0;
 virtual bool enemy(party::EnemyHandle,party::EnemyFrame&,std::string&)const=0;
 virtual bool followPunch(Navi&,party::EnemyHandle,actions::Vec3,std::string&)=0;
};
// Borrow the actual process-lifetime Services singleton and its actual Plate.
// Factory authenticates selected bytes and composition identity, not readiness.
// Every operation rechecks current canonical scene/World/actor and SDK lifetime.
class Bridge final:public actions::ActionSource,public party::PartySource {
public:
 static std::unique_ptr<Bridge> create(ActorSource&,piki::Services&,piki::PhysicalSource&,piki::Plate&,SourceBank&,std::string&);
 ~Bridge();
 const LoadedScene& scene()const override;
 const std::string& parameterBytes()const override;
 bool frame(const Navi&,actions::ActorFrame&,std::string&)const override;
 bool squad(const Navi&,std::vector<actions::PikiFrame>&,std::string&)const override;
 bool piki(const Navi&,actions::PikiHandle,actions::PikiFrame&,std::string&)const override;
 bool control(Navi&,std::string&)override;
 bool whistle(const Navi&,actions::WhistleFrame&,std::string&)const override;
 bool startWhistle(Navi&,std::string&)override;
 bool stopWhistle(Navi&,std::string&)override;
 bool updateWhistle(Navi&,actions::Vec3,bool,std::string&)override;
 bool callPikis(Navi&,std::string&)override;
 bool transitionPiki(Navi&,actions::PikiHandle,actions::PikiState,std::string&)override;
 bool positionPiki(Navi&,actions::PikiHandle,actions::Vec3,std::string&)override;
 bool sortFormation(Navi&,actions::PikiHandle,int,std::string&)override;
 bool holdFields(Navi&,float,float,float,std::string&)override;
 bool nextThrowPiki(Navi&,std::optional<actions::PikiHandle>,std::string&)override;
 bool findNextThrowPiki(Navi&,std::string&)override;
 bool throwPiki(Navi&,actions::PikiHandle,actions::Vec3,std::string&)override;
 bool feedback(Navi&,actions::Feedback,actions::PikiHandle,std::string&)override;
 bool world(party::WorldFacts&,std::string&)const override;
 bool captain(const Navi&,party::CaptainFacts&,actions::Vec3&,std::string&)const override;
 bool members(const Navi&,std::vector<party::Member>&,std::string&)const override;
 bool togglePlayer(Navi&,Navi&,std::string&)override;
 bool changeVoice(Navi&,std::string&)override;
 bool whistleMember(Navi&,actions::PikiHandle,bool,bool,std::string&)override;
 bool dismissSound(Navi&,std::string&)override;
 bool freeMember(Navi&,actions::PikiHandle,float,actions::Vec3,bool,std::string&)override;
 bool disbandTimer(Navi&,unsigned,std::string&)override;
 bool followFrame(const Navi&,party::FollowFrame&,std::string&)const override;
 bool moveRotation(Navi&,bool,std::string&)override;
 bool randomChoice(float&,std::string&)override;
 bool followFeedback(Navi&,party::FollowFeedback,std::string&)override;
 bool enemy(party::EnemyHandle,party::EnemyFrame&,std::string&)const override;
 bool followPunch(Navi&,party::EnemyHandle,actions::Vec3,std::string&)override;
private:
 struct Impl;std::unique_ptr<Impl> m;explicit Bridge(std::unique_ptr<Impl>);
};
}}}
