#include "pc_midday_codec.h"
#include "pc_midday_store.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#endif
using namespace pc_midday;
namespace fs=std::filesystem;
int checks=0;
void check(bool yes,const char* label){++checks;if(!yes)throw std::runtime_error(label);}
void write(const fs::path& p,const Bytes& b){std::ofstream f(p,std::ios::binary|std::ios::trunc);f.write(reinterpret_cast<const char*>(b.data()),b.size());if(!f)throw std::runtime_error("test write failed");}
void seal(Bytes& b){b.resize(b.size()-32);auto h=digest(b);b.insert(b.end(),h.begin(),h.end());}
Snapshot fixture(){Snapshot s;s.binding.seed[0]=1;s.binding.session[0]=2;s.binding.content[0]=3;s.binding.schema[0]=4;s.generation=1;s.frame=12345;s.actors={{2,{1,1},{0,255,3},{1}},{1,{1,1},{9},{2,0}}};s.sections={{{10,1},{2,3,4}}};return s;}
int child(const fs::path& exe,const fs::path& dir,const std::string& point){
#ifdef _WIN32
    std::wstring command=L"\""+exe.wstring()+L"\" \""+dir.wstring()+L"\" "+std::wstring(point.begin(),point.end());
    STARTUPINFOW startup{};startup.cb=sizeof(startup);PROCESS_INFORMATION process{};
    if(!CreateProcessW(exe.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&process))throw std::runtime_error("child create failed");
    DWORD wait=WaitForSingleObject(process.hProcess,10000),code=0;
    if(wait!=WAIT_OBJECT_0){TerminateProcess(process.hProcess,99);WaitForSingleObject(process.hProcess,10000);}
    GetExitCodeProcess(process.hProcess,&code);CloseHandle(process.hThread);CloseHandle(process.hProcess);
    return int(code);
#else
    pid_t pid=fork();if(pid<0)throw std::runtime_error("fork failed");
    if(!pid){execl(exe.c_str(),exe.c_str(),dir.c_str(),point.c_str(),nullptr);_Exit(99);}
    int status=0;if(waitpid(pid,&status,0)!=pid)throw std::runtime_error("wait failed");
    return WIFEXITED(status)?WEXITSTATUS(status):99;
#endif
}
int main(int argc,char** argv){try{
    if(argc==3){auto s=fixture();s.generation=2;Coverage c{{{1,1},{10,1}},{{10,1}},{1,2}};std::string e;
        publish(fs::absolute(argv[1]),s,c,e,[&](const char* at){if(std::string(at)==argv[2])std::_Exit(86);return true;});return 98;}
    if(argc!=2)throw std::runtime_error("new private output directory required");
    fs::path root=fs::absolute(argv[1]);if(fs::exists(root))throw std::runtime_error("refuse existing test output");fs::create_directories(root);
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
    check(!publish(root/"normal",s,c,e),"same generation refused");
    s.generation=2;s.frame=54321;check(publish(root/"normal",s,c,e),"second durable save");
    check(load(root/"normal",s.binding,c,out,e,false,2)==LoadResult::Invalid,"later dayend refuses stale midday");
    write(root/"normal/generation-2.checkpoint",{1,2,3});out.generation=777;
    check(load(root/"normal",s.binding,c,out,e)==LoadResult::RecoveryAvailable&&out.generation==777,"corrupt current explicit recovery only");
    check(load(root/"normal",s.binding,c,out,e,true)==LoadResult::Recovered&&out.generation==1,"previous-good explicit recovery");
    check(load(root/"normal",s.binding,c,out,e,true,1)==LoadResult::Invalid,"stale previous save excluded after dayend");
    s.generation=3;check(!publish(root/"normal",s,c,e),"do not silently replace corrupt committed save");
    write(root/"normal/CURRENT",{4});check(load(root/"normal",s.binding,c,out,e)==LoadResult::Invalid,"corrupt commit preserves evidence refuses guess");
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
    }
#ifdef _WIN32
    auto denied=root/"deny-atomic-replace";s.generation=1;check(publish(denied,s,c,e),"locked publication base");
    HANDLE held=CreateFileW((denied/"CURRENT").c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    check(held!=INVALID_HANDLE_VALUE,"lock real commit file");s.generation=2;
    check(!publish(denied,s,c,e),"real denied atomic rename returns failure");CloseHandle(held);
    check(load(denied,s.binding,c,out,e)==LoadResult::Current&&out.generation==1,"failed rename retains previous current");
#endif
    s.generation=1;auto locked=root/"locked";fs::create_directories(locked/"WRITER.lock");check(!publish(locked,s,c,e)&&fs::exists(locked/"WRITER.lock"),"foreign stale writer lock untouched");
    auto orphan=root/"orphan";fs::create_directories(orphan);write(orphan/"generation-1.checkpoint",{99});check(load(orphan,s.binding,c,out,e)==LoadResult::Missing,"uncommitted orphan excluded");check(!publish(orphan,s,c,e),"orphan generation never overwritten");
    std::cout<<"PASS "<<checks<<" native codec/store controls\n";return 0;
}catch(const std::exception& x){std::cerr<<"FAIL after "<<checks<<": "<<x.what()<<"\n";return 1;}}
