#include "pc_midday_codec.h"
#include "pc_midday_store.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <chrono>
#include <thread>
#include <functional>
#include <cerrno>
#include <exception>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#endif
using namespace pc_midday;
namespace fs=std::filesystem;
int checks=0;
void check(bool yes,const char* label){++checks;if(!yes)throw std::runtime_error(label);}
void write(const fs::path& p,const Bytes& b){std::ofstream f(p,std::ios::binary|std::ios::trunc);f.write(reinterpret_cast<const char*>(b.data()),b.size());if(!f)throw std::runtime_error("test write failed");}
Bytes readBytes(const fs::path& p){std::ifstream f(p,std::ios::binary);if(!f)throw std::runtime_error("test read failed");return Bytes(std::istreambuf_iterator<char>(f),{});}
void seal(Bytes& b){b.resize(b.size()-32);auto h=digest(b);b.insert(b.end(),h.begin(),h.end());}
Snapshot fixture(){Snapshot s;s.binding.seed[0]=1;s.binding.session[0]=2;s.binding.content[0]=3;s.binding.schema[0]=4;s.generation=1;s.frame=12345;s.actors={{2,{1,1},{0,255,3},{1}},{1,{1,1},{9},{2,0}}};s.sections={{{10,1},{2,3,4}}};return s;}
int child(const fs::path& exe,const fs::path& dir,const std::string& point,std::function<void()> observe={}){
#ifdef _WIN32
    std::wstring command=L"\""+exe.wstring()+L"\" \""+dir.wstring()+L"\" "+std::wstring(point.begin(),point.end());
    STARTUPINFOW startup{};startup.cb=sizeof(startup);PROCESS_INFORMATION process{};
    if(!CreateProcessW(exe.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&process))throw std::runtime_error("child create failed");
    try{if(observe)observe();}catch(...){TerminateProcess(process.hProcess,99);WaitForSingleObject(process.hProcess,10000);CloseHandle(process.hThread);CloseHandle(process.hProcess);throw;}
    DWORD wait=WaitForSingleObject(process.hProcess,10000),code=0;
    if(wait!=WAIT_OBJECT_0){TerminateProcess(process.hProcess,99);WaitForSingleObject(process.hProcess,10000);}
    GetExitCodeProcess(process.hProcess,&code);CloseHandle(process.hThread);CloseHandle(process.hProcess);
    return int(code);
#else
    pid_t pid=fork();if(pid<0)throw std::runtime_error("fork failed");
    if(!pid){execl(exe.c_str(),exe.c_str(),dir.c_str(),point.c_str(),nullptr);_Exit(99);}
    int status=0;
    auto waitBounded=[&](std::chrono::seconds limit){
        const auto deadline=std::chrono::steady_clock::now()+limit;
        for(;;){
            const pid_t waited=waitpid(pid,&status,WNOHANG);
            if(waited==pid)return true;
            if(waited<0&&errno!=EINTR)throw std::runtime_error("child status unknown: waitpid failed (errno="+std::to_string(errno)+")");
            if(std::chrono::steady_clock::now()>=deadline)return false;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    };
    auto stopAndReap=[&](){
        if(kill(pid,SIGKILL)!=0&&errno!=ESRCH)throw std::runtime_error("child termination unknown: kill failed (errno="+std::to_string(errno)+")");
        if(!waitBounded(std::chrono::seconds(1)))throw std::runtime_error("child remains unreaped after bounded SIGKILL wait");
    };
    try{if(observe)observe();}catch(...){
        const auto original=std::current_exception();
        try{stopAndReap();}catch(const std::exception& failure){
            std::throw_with_nested(std::runtime_error(std::string("observer failed; child cleanup unknown: ")+failure.what()));
        }
        std::rethrow_exception(original);
    }
    bool completed=false;
    try{completed=waitBounded(std::chrono::seconds(10));}catch(...){
        const auto original=std::current_exception();
        try{stopAndReap();}catch(const std::exception& failure){
            std::throw_with_nested(std::runtime_error(std::string("child wait failed; cleanup unknown: ")+failure.what()));
        }
        std::rethrow_exception(original);
    }
    if(!completed){stopAndReap();return 99;}
    return WIFEXITED(status)?WEXITSTATUS(status):99;
#endif
}
int main(int argc,char** argv){try{
    if(argc==3){auto s=fixture();s.generation=2;Coverage c{{{1,1},{10,1}},{{10,1}},{1,2}};std::string e;
        if(std::string(argv[2]).find("recovery-")==0){RecoveryPlan plan;auto dir=fs::absolute(argv[1]);
            if(!inspectRecovery(dir,1,s.binding,c,plan,e))return 96;
            recover(dir,plan,s.binding,c,e,[&](const char* at){if(std::string(at)==argv[2])std::_Exit(86);return true;});return 98;
        }
        if(std::string(argv[2])=="live-writer"){
            auto dir=fs::absolute(argv[1]);bool ok=publish(dir,s,c,e,[&](const char* at){
                if(std::string(at)!="generation-durable")return true;
                write(dir/"LIVE",{1});auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
                while(!fs::exists(dir/"RELEASE")){if(std::chrono::steady_clock::now()>=deadline)return false;std::this_thread::sleep_for(std::chrono::milliseconds(10));}return true;
            });return ok?0:97;
        }
        publish(fs::absolute(argv[1]),s,c,e,[&](const char* at){if(std::string(at)==argv[2])std::_Exit(86);return true;});return 98;}
    if(argc!=2)throw std::runtime_error("new private output directory required");
    fs::path root=fs::absolute(argv[1]);if(fs::exists(root))throw std::runtime_error("refuse existing test output");fs::create_directories(root);
    bool observerRethrown=false;
    try{child(fs::absolute(argv[0]),root/"observer-failure","live-writer",[]{throw std::runtime_error("intentional observer failure");});}
    catch(const std::runtime_error& failure){observerRethrown=std::string(failure.what())=="intentional observer failure";}
    check(observerRethrown,"observer exception rethrown after child cleanup");
    auto s=fixture();Coverage c{{{1,1},{10,1}},{{10,1}},{1,2}};std::string e;Bytes b;Snapshot out;out.generation=777;
    check(encode(s,c,b,e),"encode");check(decode(b,s.binding,c,out,e)&&out.frame==12345&&out.actors[0].id==1,"roundtrip sorted IDs");
    auto reordered=s;std::reverse(reordered.actors.begin(),reordered.actors.end());Bytes other;check(encode(reordered,c,other,e)&&other==b,"canonical actor order");
    auto bad=s;bad.actors[0].id=1;check(!encode(bad,c,other,e),"duplicate ID");bad=s;bad.actors[0].id=0;check(!encode(bad,c,other,e),"zero ID");
    bad=s;bad.actors[0].references={999};check(!encode(bad,c,other,e),"dangling reference");bad=s;bad.actors[0].adapter.version=2;check(!encode(bad,c,other,e),"unknown adapter version");
    bad=s;bad.actors.pop_back();check(!encode(bad,c,other,e),"unsupported actor cannot silently disappear");
    bad=s;bad.sections.clear();check(!encode(bad,c,other,e),"required RNG clock/audio/ledger section missing");
    bad=s;bad.sections.push_back(bad.sections[0]);check(!encode(bad,c,other,e),"duplicate global section");
    bad=s;bad.generation=0;check(!encode(bad,c,other,e),"zero generation");bad=s;bad.dayEndGeneration=1;check(!encode(bad,c,other,e),"dayend precedence");
    bad=s;bad.binding.session={};check(!encode(bad,c,other,e),"anonymous binding refused");
    for(size_t n:{size_t(0),size_t(8),b.size()-1}){Bytes t(b.begin(),b.begin()+n);out.generation=777;check(!decode(t,s.binding,c,out,e)&&out.generation==777,"truncation leaves output untouched");}
    auto t=b;t[200]^=1;check(!decode(t,s.binding,c,out,e),"checksum corruption");
    t=b;t[8]=2;seal(t);check(!decode(t,s.binding,c,out,e)&&e.find("version")!=std::string::npos,"future version with valid checksum");
    for(int i=0;i<4;++i){auto bind=s.binding;Digest* fields[]={&bind.seed,&bind.session,&bind.content,&bind.schema};(*fields[i])[0]^=1;check(!decode(b,bind,c,out,e),"identity binding mismatch");}
    t=b;for(int i=0;i<4;++i)t[164+i]=255;seal(t);check(!decode(t,s.binding,c,out,e),"malicious actor count rejected");
    t=b;t.insert(t.end()-32,0);seal(t);check(!decode(t,s.binding,c,out,e),"trailing bytes rejected");
    auto sha=digest({'a','b','c'});check(sha[0]==0xba&&sha[31]==0xad,"SHA256 known vector edges");
    check(publish(root/"normal",s,c,e),"first durable save");
    check(load(root/"normal",s.binding,c,out,e)==LoadResult::Current&&out.generation==1,"load committed generation");
    auto legacy=root/"legacy-commit";check(publish(legacy,s,c,e),"legacy commit base");
    Bytes legacyMarker={'P','C','C','O','M','M','I','T'};
    for(uint64_t value:{uint64_t(1),uint64_t(0)})for(int i=0;i<8;++i)legacyMarker.push_back(uint8_t(value>>(i*8)));
    auto legacySum=digest(legacyMarker);legacyMarker.insert(legacyMarker.end(),legacySum.begin(),legacySum.end());write(legacy/"CURRENT",legacyMarker);
    check(load(legacy,s.binding,c,out,e)==LoadResult::Current&&out.generation==1,"original commit framing remains readable");
    auto legacyNext=s;legacyNext.generation=2;check(publish(legacy,legacyNext,c,e),"legacy current upgrades by normal publication");
    check(!publish(root/"normal",s,c,e),"same generation refused");
    s.generation=2;s.frame=54321;check(publish(root/"normal",s,c,e),"second durable save");
    check(load(root/"normal",s.binding,c,out,e,false,2)==LoadResult::Invalid,"later dayend refuses stale midday");
    write(root/"normal/generation-2.checkpoint",{1,2,3});out.generation=777;
    check(load(root/"normal",s.binding,c,out,e)==LoadResult::RecoveryAvailable&&out.generation==777,"corrupt current explicit recovery only");
    check(load(root/"normal",s.binding,c,out,e,true)==LoadResult::Recovered&&out.generation==1,"previous-good explicit recovery");
    check(load(root/"normal",s.binding,c,out,e,true,1)==LoadResult::Invalid,"stale previous save excluded after dayend");
    s.generation=3;check(!publish(root/"normal",s,c,e),"do not silently replace corrupt committed save");
    RecoveryPlan rollback;
    check(inspectRecovery(root/"normal",1,s.binding,c,rollback,e),"inspect committed previous-good recovery");
    auto originalCurrent=readBytes(root/"normal/CURRENT");write(root/"normal/CURRENT",{9});
    check(!recover(root/"normal",rollback,s.binding,c,e),"changed current record invalidates approved recovery");
    write(root/"normal/CURRENT",originalCurrent);
    check(recover(root/"normal",rollback,s.binding,c,e),"explicit previous-good repair publication continuity");
    check(load(root/"normal",s.binding,c,out,e)==LoadResult::Current&&out.generation==1,"recovered marker selects chosen bytes");
    s.generation=2;check(!publish(root/"normal",s,c,e),"recovery high-water refuses reused incarnation generation");
    s.generation=3;check(publish(root/"normal",s,c,e),"new save after explicit previous-good recovery");
    write(root/"normal/CURRENT",{4});check(load(root/"normal",s.binding,c,out,e)==LoadResult::Invalid,"corrupt commit preserves evidence refuses guess");
    check(!inspectRecovery(root/"normal",3,s.binding,c,rollback,e),"corrupt commit selection lacks automatic committed provenance");
    check(inspectRecovery(root/"normal",3,s.binding,c,rollback,e,0,true)&&recover(root/"normal",rollback,s.binding,c,e),"explicit selected snapshot repairs corrupt CURRENT with evidence retained");
    for(const char* point:{"generation-durable","generation-verified","commit-durable","commit-published"}){
        auto dir=root/point;s.generation=1;check(publish(dir,s,c,e),"fault base save");s.generation=2;
        check(!publish(dir,s,c,e,[&](const char* at){return std::string(at)!=point;}),"injected publication interruption");
        check(load(dir,s.binding,c,out,e)==LoadResult::Current&&out.generation==(std::string(point)=="commit-published"?2u:1u),"interruption leaves complete old or new generation");
        check(fs::exists(dir/"generation-1.checkpoint"),"previous bytes retained");
        auto killed=root/(std::string(point)+"-process-exit");s.generation=1;check(publish(killed,s,c,e),"process interruption base");
        check(child(fs::absolute(argv[0]),killed,point)==86,"real fresh writer process killed at boundary");
        check(fs::exists(killed/"WRITER.lock"),"actual interruption preserves stale ownership lock");
        check(load(killed,s.binding,c,out,e)==LoadResult::Current&&out.generation==(std::string(point)=="commit-published"?2u:1u),"fresh process interrupted publication old/new only");
        s.generation=3;check(!publish(killed,s,c,e),"interrupted writer lock blocks next writer until explicit recovery");
        RecoveryPlan plan;
        check(inspectRecovery(killed,out.generation,s.binding,c,plan,e),"native exact stopped-owner proof prepares recovery");
        auto owner=readBytes(killed/"WRITER.lock/owner");auto changed=owner;changed[32]^=1;seal(changed);write(killed/"WRITER.lock/owner",changed);
        check(!recover(killed,plan,s.binding,c,e),"foreign host/changed lock refuses recovery");write(killed/"WRITER.lock/owner",owner);
        check(recover(killed,plan,s.binding,c,e),"stopped writer lock retired preserving owner evidence");
        s.generation=plan.highWater+1;check(publish(killed,s,c,e),"recovered stopped writer permits higher generation save");
    }
    auto live=root/"live-writer";s.generation=1;check(publish(live,s,c,e),"live writer base");
    bool sawLive=false;
    int liveExit=child(fs::absolute(argv[0]),live,"live-writer",[&]{
        auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
        while(!fs::exists(live/"LIVE")&&std::chrono::steady_clock::now()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(10));
        if(!fs::exists(live/"LIVE"))return;
        RecoveryPlan plan;sawLive=!inspectRecovery(live,1,s.binding,c,plan,e)&&fs::exists(live/"WRITER.lock/owner");
        auto fake=root/"alive-owner-no-gate";fs::create_directories(fake/"WRITER.lock");write(fake/"WRITER.lock/owner",readBytes(live/"WRITER.lock/owner"));
        write(fake/"CURRENT",readBytes(live/"CURRENT"));write(fake/"generation-1.checkpoint",readBytes(live/"generation-1.checkpoint"));
        sawLive=sawLive&&!inspectRecovery(fake,1,s.binding,c,plan,e)&&fs::exists(fake/"WRITER.lock/owner");
        write(live/"RELEASE",{1});
    });check(sawLive&&liveExit==0,"live native owner/gate refuse recovery without altering lock");
    for(const char* point:{"recovery-owner-durable","recovery-evidence-durable","recovery-commit-durable","recovery-commit-published"}){
        auto dir=root/point;s.generation=1;check(publish(dir,s,c,e),"interrupted recovery base1");s.generation=2;check(publish(dir,s,c,e),"interrupted recovery base2");write(dir/"generation-2.checkpoint",{99});
        check(child(fs::absolute(argv[0]),dir,point)==86,"actual recovery process terminates at durability boundary");
        check(fs::exists(dir/"WRITER.lock/owner"),"interrupted recovery leaves durable recovery-owner identity");
        auto status=load(dir,s.binding,c,out,e);check(status==(std::string(point)=="recovery-commit-published"?LoadResult::Current:LoadResult::RecoveryAvailable),"interrupted recovery never exposes partial committed data");
        RecoveryPlan retry;check(inspectRecovery(dir,1,s.binding,c,retry,e)&&recover(dir,retry,s.binding,c,e),"stopped interrupted recovery can be explicitly retried");
        s.generation=retry.highWater+1;check(publish(dir,s,c,e),"new save after recovery process interruption retry");
    }
#ifdef _WIN32
    auto denied=root/"deny-atomic-replace";s.generation=1;check(publish(denied,s,c,e),"locked publication base");
    HANDLE held=CreateFileW((denied/"CURRENT").c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    check(held!=INVALID_HANDLE_VALUE,"lock real commit file");s.generation=2;
    check(!publish(denied,s,c,e),"real denied atomic rename returns failure");CloseHandle(held);
    check(load(denied,s.binding,c,out,e)==LoadResult::Current&&out.generation==1,"failed rename retains previous current");
#endif
    s.generation=1;auto locked=root/"locked";fs::create_directories(locked/"WRITER.lock");check(!publish(locked,s,c,e)&&fs::exists(locked/"WRITER.lock"),"foreign stale writer lock untouched");
    check(!inspectRecovery(locked,1,s.binding,c,rollback,e,0,true)&&fs::exists(locked/"WRITER.lock"),"empty legacy lock is unknown never deleted");
    auto orphan=root/"orphan";fs::create_directories(orphan);write(orphan/"generation-1.checkpoint",{99});check(load(orphan,s.binding,c,out,e)==LoadResult::Missing,"uncommitted orphan excluded");check(!publish(orphan,s,c,e),"orphan generation never overwritten");
    std::cout<<"PASS "<<checks<<" native codec/store controls\n";return 0;
}catch(const std::exception& x){std::cerr<<"FAIL after "<<checks<<": "<<x.what()<<"\n";return 1;}}
