#pragma once
#include "pc_p2_original_piki_native_facts.h"
#include <array>
namespace p2original { namespace piki {
struct PlateParameters {float startingOffset=0,lengthLimit=0,maxPositionSize=0;};
bool parsePlateParameters(const std::string&,PlateParameters&,std::string&);
// Actual Navi::updateCPlate owner supplies its already evaluated source pose.
// Never derive these decisions from P1 CircleMove/controller/state aliases.
struct PlatePose {
 Vector3f position,velocity;
 float angle=0,scale=0,moveStrength=0;
 bool gray=false;
};
struct PlateState {
 unsigned count=0,activeCount=0,shrinkTimer=0;
 std::array<unsigned,3> happaCounts{};
 float baseRadius=0,moveRadius=0,maxRadius=0,angle=0;
 Vector3f baseOffset,maxPositionOffset,velocity;
};
class PlateSource {
public:
 virtual ~PlateSource()=default;
 virtual const captain::LoadedScene& scene()const=0;
 // Retail CPlate::Parms is constructed in Game/CPlate.h, not loaded from
 // NaviParms text. Read the actual selected source instance/parameter owner.
 virtual bool readParameters(const Navi*,PlateParameters&,std::string&)const=0;
 virtual bool readPose(const Navi*,PlatePose&,std::string&)const=0;
 // Genuine ActFormation::setFormed tutorial/movie/timer owner. Not CPlate.
 virtual bool setFormed(Handle,Navi*,std::string&)=0;
};
// Source CPlate storage only; never writes native P1 squad/circleMove state.
// The single Services composer owns a stable Plate and source producer.
class Plate {
public:
 explicit Plate(PlateSource& source):mSource(source){}
 bool allocateSlot(Handle,Navi*,int&,std::string&);
 bool releaseSlot(Handle,Navi*,int,std::string&);
 bool canReleaseSlot(Handle,Navi*,int,std::string&)const;
 bool slotPosition(Handle,Navi*,int,Vector3f&,std::string&)const;
 bool sortSlot(Handle,Navi*,int,int happa,std::string&);
 bool formed(Handle,Navi*,std::string&);
 bool refresh(Navi*,std::string&);
 bool rearrange(Navi*,const Vector3f&,std::string&);
 bool shrink(Navi*,std::string&);
 bool update(Navi*,std::string&);
 // Called after genuine source growth (retail CPlate::changeFlower).
 bool changeFlower(Handle,Navi*,std::string&);
 bool growthCounts(Navi*,std::array<unsigned,3>&,std::string&)const;
 bool state(Navi*,PlateState&,std::string&)const;
 // Refuses live slots; caller first retires SourcePiki resources/associations.
 bool reset(std::string&);
 unsigned retainedSlots()const noexcept;
 unsigned retainedListeners()const noexcept{return retainedSlots();}
private:
 struct Slot {Handle handle;unsigned species=0,happa=0;Vector3f relative,position;};
 struct Group {
  SceneBinding binding;Navi* captain=nullptr;PlateParameters parameters;
  std::array<Slot,100> slots{};std::array<unsigned,3> happaCounts{};
  unsigned count=0,activeCount=0,shrinkTimer=0;
  float baseRadius=10,moveRadius=10,maxRadius=0,angle=0;
  Vector3f baseOffset,maxPositionOffset,velocity;
 };
 PlateSource& mSource;std::array<Group,2> mGroups{};mutable bool mBusy=false,mReentered=false;
 class Operation;
 class ReadOperation;
 bool group(Navi*,bool cleanup,Group*&,std::string&);
 bool validate(const Group&,bool cleanup,std::string&)const;
 bool publish(Group&,Group&,bool cleanup,std::string&);
 static bool geometry(Group&,const PlatePose&,std::string&);
};
} }
