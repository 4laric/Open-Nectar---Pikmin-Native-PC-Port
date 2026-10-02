#include "pc_midday_save_quit.h"
#include <limits>
namespace pc_midday {
namespace {
bool same(const Binding& a,const Binding& b){return a.seed==b.seed&&a.session==b.session&&a.content==b.content&&a.schema==b.schema;}
bool nonzero(const Digest& d){for(uint8_t b:d)if(b)return true;return false;}
}
bool SaveQuitService::configure(const std::filesystem::path& dir,const Binding& binding,uint64_t next,CaptureWorld capture,QuitAfterSave quit,std::string& e){
    if(std::this_thread::get_id()!=owner_){e="save configuration requires gameplay thread";return false;}
    if(busy_||pending_||capture_){e="save provider already installed or request active";return false;}
    if(!dir.is_absolute()||!next||next==std::numeric_limits<uint64_t>::max()||!capture||!quit||
       !nonzero(binding.seed)||!nonzero(binding.session)||!nonzero(binding.content)||!nonzero(binding.schema)){
        e="save provider requires absolute store, valid binding and bounded generation";return false;
    }
    directory_=dir;binding_=binding;nextGeneration_=next;capture_=std::move(capture);quit_=std::move(quit);
    e.clear();return true;
}
bool SaveQuitService::request(std::string& e){
    if(std::this_thread::get_id()!=owner_){e="Save & Quit requires gameplay thread";return false;}
    if(busy_||pending_||outcome_==SaveQuitOutcome::SavedQuitRequested){e="Save & Quit request already active";return false;}
    if(!capture_){e="Mid-day save unavailable: complete world restore is not enabled for this scene";notice_=e;outcome_=SaveQuitOutcome::Refused;return false;}
    if(storageNeedsRecovery_){e="Previous save publication failed or was uncertain; recover the checkpoint store before retrying";notice_=e;outcome_=SaveQuitOutcome::Refused;return false;}
    if(nextGeneration_==std::numeric_limits<uint64_t>::max()){e="checkpoint generation exhausted";notice_=e;outcome_=SaveQuitOutcome::Refused;return false;}
    pending_=true;outcome_=SaveQuitOutcome::Pending;notice_="Saving at the completed gameplay tick...";e.clear();return true;
}
SaveQuitOutcome SaveQuitService::finishTick(uint64_t frame,Boundary boundary){
    if(std::this_thread::get_id()!=owner_||busy_||!pending_)return outcome_;
    pending_=false;busy_=true;
    struct Guard {bool& busy;~Guard(){busy=false;}} guard{busy_};
    bool committed=false;
    try {
        CaptureBundle bundle;std::string e;
        if(!capture_(nextGeneration_,frame,bundle,e)){
            notice_="Save refused: "+e;outcome_=SaveQuitOutcome::Refused;return outcome_;
        }
        if(!same(bundle.snapshot.binding,binding_)||bundle.snapshot.generation!=nextGeneration_||bundle.snapshot.frame!=frame){
            notice_="Save refused: capture changed binding, generation or tick";outcome_=SaveQuitOutcome::Refused;return outcome_;
        }
        // A failed publish can leave an immutable orphan or an uncertain CURRENT.
        // Do not blindly reuse its generation or overwrite evidence on retry.
        storageNeedsRecovery_=true;
        if(!publish(directory_,bundle.snapshot,bundle.coverage,e,std::move(boundary))){
            notice_="Save failed: "+e;outcome_=SaveQuitOutcome::Refused;return outcome_;
        }
        storageNeedsRecovery_=false;committed=true;++nextGeneration_;
        if(!quit_(e)){notice_="Checkpoint saved; quit failed: "+e;outcome_=SaveQuitOutcome::SavedQuitFailed;return outcome_;}
        notice_="Checkpoint saved. Quitting...";outcome_=SaveQuitOutcome::SavedQuitRequested;
    }catch(const std::exception& failure){
        notice_=std::string(committed?"Checkpoint saved; quit failed: ":"Save refused: ")+failure.what();
        outcome_=committed?SaveQuitOutcome::SavedQuitFailed:SaveQuitOutcome::Refused;
    }catch(...){notice_=committed?"Checkpoint saved; quit failed":"Save refused: unknown capture/publication failure";outcome_=committed?SaveQuitOutcome::SavedQuitFailed:SaveQuitOutcome::Refused;}
    return outcome_;
}
}
