#include "pc_midday_save_quit.h"
#include "pc_midday_save_quit_native.h"
#include "settings/pc_settings.h"
#include <SDL.h>
namespace {
pc_midday::SaveQuitService& service(){static pc_midday::SaveQuitService s;return s;}
bool quitAfterSave(std::string& e){
    SDL_Event event{};event.type=SDL_QUIT;
    if(SDL_PushEvent(&event)!=1){e=SDL_GetError();if(e.empty())e="quit event rejected";return false;}
    return true;
}
}
namespace pc_midday {
bool configureNativeSaveQuit(const std::filesystem::path& dir,const Binding& b,uint64_t next,CaptureWorld capture,std::string& e){
    return service().configure(dir,b,next,std::move(capture),quitAfterSave,e);
}
}
bool pc_midday_request_save_quit(){
    std::string e;const bool accepted=service().request(e);
    pc_settings_midday_notice(accepted?service().notice().c_str():e.c_str(),!accepted);
    return accepted;
}
bool pc_midday_save_quit_tick(unsigned frame){
    if(service().outcome()!=pc_midday::SaveQuitOutcome::Pending)
        return service().outcome()==pc_midday::SaveQuitOutcome::SavedQuitRequested;
    const auto result=service().finishTick(frame);
    pc_settings_midday_notice(service().notice().c_str(),result!=pc_midday::SaveQuitOutcome::SavedQuitRequested);
    return result==pc_midday::SaveQuitOutcome::SavedQuitRequested;
}
