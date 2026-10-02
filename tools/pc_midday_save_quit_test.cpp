#include "pc_midday_save_quit.h"
#include <cstdlib>
#include <chrono>
#include <iostream>
#include <stdexcept>
using namespace pc_midday;
int checks=0;void check(bool b){++checks;if(!b){std::cerr<<"SaveQuit failed "<<checks<<"\n";std::exit(1);}}
int main(int argc,char** argv){
    if(argc!=2)return 2;
    const auto base=std::filesystem::absolute(argv[1])/std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    // Caller supplies a new private directory; never remove an existing save.
    if(std::filesystem::exists(base)){std::cerr<<"fixture directory already exists\n";return 2;}
    std::filesystem::create_directories(base);
    Binding binding;binding.seed[0]=1;binding.session[0]=2;binding.content[0]=3;binding.schema[0]=4;
    Coverage coverage{{{1,1},{10,1}},{{10,1}},{1}};
    auto capture=[&](uint64_t generation,uint64_t frame,CaptureBundle& b,std::string&){
        b.snapshot={binding,generation,frame,0,{{1,{1,1},{9},{0}}},{{{10,1},{2,3}}}};b.coverage=coverage;return true;};
    int quit=0;std::string e;auto quits=[&](std::string&){++quit;return true;};
    SaveQuitService absent;check(!absent.request(e)&&absent.outcome()==SaveQuitOutcome::Refused);
    SaveQuitService good;check(good.configure(base/"good",binding,1,capture,quits,e));check(!good.configure(base/"other",binding,1,capture,quits,e));
    check(good.request(e));check(!good.request(e));check(good.finishTick(321)==SaveQuitOutcome::SavedQuitRequested&&quit==1);
    Snapshot loaded;check(load(base/"good",binding,coverage,loaded,e)==LoadResult::Current&&loaded.generation==1&&loaded.frame==321);
    check(good.finishTick(322)==SaveQuitOutcome::SavedQuitRequested&&quit==1);check(!good.request(e));
    SaveQuitService malformed;auto bad=[&](uint64_t g,uint64_t f,CaptureBundle& b,std::string& err){capture(g,f,b,err);b.snapshot.binding.seed[0]=99;return true;};
    check(malformed.configure(base/"bad",binding,1,bad,quits,e));check(malformed.request(e));check(malformed.finishTick(3)==SaveQuitOutcome::Refused&&quit==1&&!std::filesystem::exists(base/"bad"));
    SaveQuitService failed;check(failed.configure(base/"writefailed",binding,1,capture,quits,e));check(failed.request(e));
    check(failed.finishTick(4,[](const char*){return false;})==SaveQuitOutcome::Refused&&quit==1);check(!failed.request(e));
    SaveQuitService quitFailed;check(quitFailed.configure(base/"quitfailed",binding,1,capture,[](std::string& err){err="event queue unavailable";return false;},e));
    check(quitFailed.request(e));check(quitFailed.finishTick(8)==SaveQuitOutcome::SavedQuitFailed);
    check(load(base/"quitfailed",binding,coverage,loaded,e)==LoadResult::Current&&loaded.generation==1);
    check(quitFailed.request(e));check(quitFailed.finishTick(9)==SaveQuitOutcome::SavedQuitFailed);
    check(load(base/"quitfailed",binding,coverage,loaded,e)==LoadResult::Current&&loaded.generation==2);
    SaveQuitService reentrant;bool refused=false;auto nested=[&](uint64_t g,uint64_t f,CaptureBundle& b,std::string& err){refused=!reentrant.request(err);return capture(g,f,b,err);};
    check(reentrant.configure(base/"reentrant",binding,1,nested,quits,e));check(reentrant.request(e));check(reentrant.finishTick(10)==SaveQuitOutcome::SavedQuitRequested&&refused&&quit==2);
    SaveQuitService throwing;check(throwing.configure(base/"throw",binding,1,[](uint64_t,uint64_t,CaptureBundle&,std::string&)->bool{throw std::runtime_error("unsupported scene");},quits,e));
    check(throwing.request(e));check(throwing.finishTick(11)==SaveQuitOutcome::Refused&&quit==2&&!std::filesystem::exists(base/"throw"));
    SaveQuitService committedError;check(committedError.configure(base/"committed-error",binding,1,capture,[](std::string&)->bool{throw std::runtime_error("quit failure");},e));
    check(committedError.request(e));check(committedError.finishTick(12)==SaveQuitOutcome::SavedQuitFailed);
    check(load(base/"committed-error",binding,coverage,loaded,e)==LoadResult::Current&&loaded.frame==12);
    SaveQuitService uncertain;check(uncertain.configure(base/"uncertain",binding,1,capture,quits,e));check(uncertain.request(e));
    check(uncertain.finishTick(13,[](const char* point){return std::string(point)!="commit-published";})==SaveQuitOutcome::Refused&&quit==2);
    check(load(base/"uncertain",binding,coverage,loaded,e)==LoadResult::Current&&loaded.frame==13);check(!uncertain.request(e));
    SaveQuitService unsupported;check(unsupported.configure(base/"unsupported",binding,1,[](uint64_t,uint64_t,CaptureBundle&,std::string& err){err="enemy adapter missing";return false;},quits,e));
    check(unsupported.request(e));check(unsupported.finishTick(14)==SaveQuitOutcome::Refused&&quit==2&&!std::filesystem::exists(base/"unsupported"));
    std::cout<<checks<<" actual-store SaveQuit controls PASS: "<<base.string()<<"\n";
}
