#pragma once
#include <cstdint>
#include <string>
#include <thread>
namespace p2retail {class SceneContext;class SceneRuntime;}
class StageInfo;class MapMgr;class RouteMgr;class Navi;
namespace p2original {
namespace piki {class NativeBodyFactory;}
namespace captain {class LoadedScene;}
// The retained scalar portion of actual Source System, not the host/P1 timer.
// Its private constructor ports factor1/dt1/60. A genuine BaseGameSection init
// event applies factor2/dt2/60 BEFORE the native captain roster exists. Only
// after that event may a consumer read dt. These are source event ports, not
// wall-clock measurement or gameplay/emission/World readiness permissions.
class SystemClock final {
public:
 SystemClock(const SystemClock&)=delete;SystemClock& operator=(const SystemClock&)=delete;
 bool readDeltaTime(float&,std::string&)const;
private:
 friend class piki::NativeBodyFactory;friend class p2retail::SceneRuntime;
 SystemClock():mDeltaTime(1.0f/60.0f),mFrameRate(1.0f),mThread(std::this_thread::get_id()){}
 bool sourceStageOwned(const p2retail::SceneContext&,std::string&);
 bool sourceBaseGameSectionInit(std::string&);
 // Attach later at the actual loaded-source event. This must never become an
 // artificial prerequisite for the earlier BaseGameSection init/camera dt.
 bool sourceLoadedScene(std::string&);
 // Actual BaseGameSection::doUpdate bracket; no dt change in either event.
 // The open bracket is retained cleanup bookkeeping, not a retail clock flag.
 bool sourceBaseGameSectionUpdateBegin(std::string&);
 bool sourceBaseGameSectionUpdateEnd(std::string&);
 bool sourceStageReleased(std::string&);
 float mDeltaTime,mFrameRate;
 const p2retail::SceneContext* mStage=nullptr;
 StageInfo* mStageInfo=nullptr;MapMgr* mMap=nullptr;RouteMgr* mRoutes=nullptr;
 const std::thread::id mThread;
 std::uint64_t mSerial=0,mRevision=0;
 std::string mCampaign,mSession,mVisit,mLayout;
 const captain::LoadedScene* mScene=nullptr;Navi* mCaptains[2]={nullptr,nullptr};
 bool mSectionInitialized=false,mFrameOpen=false;
 mutable bool mBusy=false,mReentered=false;
 class Operation;
 bool stageCurrent(bool cleanup,std::string&)const;
 bool sceneCurrent(std::string&)const;
 bool exact(bool cleanup,std::string&)const;
};
}
