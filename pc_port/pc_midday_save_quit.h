#pragma once
#include "pc_midday_store.h"
#include <thread>
namespace pc_midday {
struct CaptureBundle {Snapshot snapshot;Coverage coverage;};
using CaptureWorld=std::function<bool(uint64_t generation,uint64_t frame,CaptureBundle&,std::string&)>;
using QuitAfterSave=std::function<bool(std::string&)>;
enum class SaveQuitOutcome {Idle,Pending,Refused,SavedQuitRequested,SavedQuitFailed};
// Gameplay-thread service. Capture executes synchronously at the completed tick
// fence, so no subsequent gameplay tick/input callback runs during publication.
// The provider separately owns coherent logical audio capture.
// The complete-world provider must reject unsupported families/globals. A codec
// validator alone cannot establish gameplay coverage or restore support.
class SaveQuitService {
    const std::thread::id owner_=std::this_thread::get_id();
    std::filesystem::path directory_;
    Binding binding_;
    uint64_t nextGeneration_=0;
    CaptureWorld capture_;
    QuitAfterSave quit_;
    bool pending_=false,busy_=false,storageNeedsRecovery_=false;
    SaveQuitOutcome outcome_=SaveQuitOutcome::Idle;
    std::string notice_;
public:
    SaveQuitService()=default;
    SaveQuitService(const SaveQuitService&)=delete;
    SaveQuitService& operator=(const SaveQuitService&)=delete;
    // Only scene bootstrap installs a fully covered capture+restore provider.
    // nextGeneration is derived from the durable store's high-water generation.
    bool configure(const std::filesystem::path&,const Binding&,uint64_t nextGeneration,CaptureWorld,QuitAfterSave,std::string&);
    bool request(std::string&);
    SaveQuitOutcome finishTick(uint64_t frame,Boundary faultBoundary={});
    SaveQuitOutcome outcome()const {return outcome_;} // owner thread only
    const std::string& notice()const {return notice_;} // owner thread only
};
// Native scene bootstrap registration. No registration occurs until the real
// capture/restore backend covers the entire world and accepted mode/profile.
bool configureNativeSaveQuit(const std::filesystem::path&,const Binding&,uint64_t,
                             CaptureWorld,std::string&);
}
