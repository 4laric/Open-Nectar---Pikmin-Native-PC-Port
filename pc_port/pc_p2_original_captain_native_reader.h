#pragma once
#include "pc_p2_original_piki_captain_reader.h"
#include "pc_p2_original_piki_plate.h"
#include "pc_p2_original_captain_cstick.h"
#include "pc_p2_original_captain_native_control.h"
#include <memory>
namespace p2original {namespace captain {namespace nativereader {
// Actual Body-owned CPlate composition executes these source commands. This
// provider is borrowed only AFTER Body construction, so Reader can exist first.
// execute applies literal Refresh/SetPos/Gray/Rearrange to its real source Plate;
// afterRefresh observes the exact full census and current maxPositionOffset.
class PlateDriver {
public:
 virtual ~PlateDriver()=default;
 virtual piki::Plate& storage()const=0;
 virtual bool execute(Navi*,const cstick::Command&,std::string&)=0;
 virtual bool afterRefresh(Navi*,cstick::AfterRefresh&,std::string&)const=0;
};
class Plan {
public:
 bool resetsSceneAnimationTimer()const{return source_.resetSceneAnimationTimer;}
private:
 Navi* actor_=nullptr;const NaviState* state_=nullptr;const World* world_=nullptr;
 const LoadedScene* scene_=nullptr;SourceBank* bank_=nullptr;PlateDriver* plate_=nullptr;
 std::uint64_t incarnation_=0,revision_=0,motionGeneration_=0;cstick::Plan source_;
 friend class Reader;
};
class Reader final:public piki::NativeCaptainReader,public piki::PlateSource {
public:
 Reader();~Reader();
 const LoadedScene& scene()const override;
 const std::string& naviParameterBytes()const override;
 piki::PlateSource& plateSource()const override;
 bool frame(const Navi*,piki::CaptainFrame&,std::string&)const override;
 bool readParameters(const Navi*,piki::PlateParameters&,std::string&)const override;
 bool readPose(const Navi*,piki::PlatePose&,std::string&)const override;
 bool setFormed(piki::Handle,Navi*,std::string&)override;
 // Actual Loading hook after canonical real two-body reset and SourceBank
 // binding. A new per-lifetime source field object runs PelletView ctor(null),
 // then Navi::onInit. Repeated onInit preserves PelletView's existing pointer.
 bool initializeAfterBodyReset(std::string&);
 bool bindPlate(PlateDriver&,std::string&);
 // Read-only exact body lifetime check for the ordered child driver.
 bool currentBody(const Navi*,std::string&)const;
 bool prepareCStick(Navi*,const nativecontrol::Request&,Plan&,std::string&)const;
 // Every commit preflights actual nativecontrol Effects.commit in flight before
 // any prefix write; active clock reset precedes Refresh at navi.cpp4871.
 bool commitCStick(const Plan&,std::string&);
 // Source prefix field writes precede actual CPlate callbacks. On later
 // refusal those genuine partial writes remain; no later suffix is executed.
 // Root additionally checks concrete SDK/body/resource retirement. These
 // checks cover Reader's actual borrowed Plate and Services consumer references.
 bool canRetire(std::string&)const;
 bool retireAfterBodyConsumers(std::string&);
private:
 struct Impl;std::unique_ptr<Impl> m;
 friend piki::NativeCaptainReader* borrow();
};
// Process-stable concrete reader object; accessor is null until genuine Loading
// initialization. Neither accessor nor Plate binding activates source gameplay.
Reader& instance();
piki::NativeCaptainReader* borrow();
}}}
p2original::piki::NativeCaptainReader* pc_p2_original_captain_native_reader();
