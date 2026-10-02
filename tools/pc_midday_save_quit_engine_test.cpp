#define SDL_MAIN_HANDLED
#include <SDL.h>
#include "pc_midday_save_quit.h"
#include "pc_midday_save_quit_native.h"
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
std::string notice;bool noticeError=false;int checks=0;
extern "C" void pc_settings_midday_notice(const char* message,bool error){notice=message?message:"";noticeError=error;}
void check(bool ok){++checks;if(!ok){std::cerr<<"native SaveQuit control failed "<<checks<<"\n";std::exit(1);}}
int rejectQuit(void*,SDL_Event* event){return event->type==SDL_QUIT?0:1;}
int main(int argc,char** argv){
 if(argc!=2)return 2;
 SDL_SetMainReady();check(SDL_Init(SDL_INIT_EVENTS)==0);
 check(!pc_midday_request_save_quit()&&noticeError&&notice.find("unavailable")!=std::string::npos);
 check(!pc_midday_save_quit_tick(1));SDL_Event event;check(SDL_PollEvent(&event)==0);
 using namespace pc_midday;Binding binding;binding.seed[0]=1;binding.session[0]=2;binding.content[0]=3;binding.schema[0]=4;
 Coverage coverage{{{1,1},{10,1}},{{10,1}},{1}};std::string error;
 auto dir=std::filesystem::absolute(argv[1])/std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
 check(configureNativeSaveQuit(dir,binding,1,[&](uint64_t generation,uint64_t frame,CaptureBundle& b,std::string&){
  b.snapshot={binding,generation,frame,0,{{1,{1,1},{9},{0}}},{{{10,1},{2,3}}}};b.coverage=coverage;return true;
 },error));
 SDL_SetEventFilter(rejectQuit,nullptr);check(pc_midday_request_save_quit()&&!noticeError);check(!pc_midday_save_quit_tick(22));
 check(noticeError&&notice.find("Checkpoint saved; quit failed")!=std::string::npos);
 Snapshot loaded;check(load(dir,binding,coverage,loaded,error)==LoadResult::Current&&loaded.frame==22&&loaded.generation==1);
 check(SDL_PollEvent(&event)==0);
 SDL_SetEventFilter(nullptr,nullptr);check(pc_midday_request_save_quit());check(pc_midday_save_quit_tick(23));
 check(!noticeError&&notice.find("Checkpoint saved. Quitting")!=std::string::npos);
 check(SDL_PollEvent(&event)==1&&event.type==SDL_QUIT);check(SDL_PollEvent(&event)==0);
 check(load(dir,binding,coverage,loaded,error)==LoadResult::Current&&loaded.frame==23&&loaded.generation==2);
 check(pc_midday_save_quit_tick(24));check(SDL_PollEvent(&event)==0);
 SDL_Quit();std::cout<<checks<<" native SaveQuit SDL event/notice controls PASS\n";
}
