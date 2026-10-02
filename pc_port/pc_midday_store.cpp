#include "pc_midday_store.h"
#include <cstdio>
#include <cerrno>
#include <fstream>
#include <system_error>
#include <sstream>
#include <limits>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#include <sys/stat.h>
#else
#include <fcntl.h>
#include <unistd.h>
#include <sys/file.h>
#endif

namespace pc_midday {
namespace {
namespace fs=std::filesystem;
struct Gate {
#ifdef _WIN32
    HANDLE handle=INVALID_HANDLE_VALUE;
    bool acquire(const fs::path& dir){handle=CreateFileW((dir/"WRITER.guard").c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);return handle!=INVALID_HANDLE_VALUE;}
    ~Gate(){if(handle!=INVALID_HANDLE_VALUE)CloseHandle(handle);}
#else
    int handle=-1;
    bool acquire(const fs::path& dir){handle=open((dir/"WRITER.guard").c_str(),O_CREAT|O_RDWR,0600);return handle>=0 && flock(handle,LOCK_EX|LOCK_NB)==0;}
    ~Gate(){if(handle>=0)close(handle);}
#endif
};
fs::path generation(const fs::path& dir,uint64_t n){return dir/("generation-"+std::to_string(n)+".checkpoint");}
bool read(const fs::path& p,Bytes& b,std::string& e){
    std::error_code ec;auto n=fs::file_size(p,ec);
    if(ec || n>MaxBytes){e="missing/oversize checkpoint file";return false;}
    std::ifstream f(p,std::ios::binary);Bytes t(size_t(n),0);
    if(!f || !f.read(reinterpret_cast<char*>(t.data()),std::streamsize(n))){e="checkpoint read failed";return false;}
    b=std::move(t);return true;
}
bool syncDir(const fs::path& p){
#ifdef _WIN32
    // MoveFileEx WRITE_THROUGH supplies the publication flush on Windows. No
    // directory FlushFileBuffers guarantee is invented here.
    (void)p;return true;
#else
    int fd=open(p.c_str(),O_RDONLY|O_DIRECTORY);if(fd<0)return false;
    bool ok=fsync(fd)==0;return close(fd)==0 && ok;
#endif
}
bool prepareDirectory(const fs::path& dir,std::string& e){
    std::vector<fs::path> created;auto path=fs::absolute(dir);
    while(!fs::exists(path)){
        created.push_back(path);auto parent=path.parent_path();
        if(parent==path){e="save path has no existing ancestor";return false;}path=parent;
    }
    std::error_code ec;fs::create_directories(dir,ec);
    if(ec){e="save directory create failed";return false;}
    for(const auto& p:created)if(!syncDir(p)||!syncDir(p.parent_path())){e="save directory ancestry sync failed";return false;}
    return true;
}
bool writeNew(const fs::path& p,const Bytes& b,std::string& e){
#ifdef _WIN32
    int fd=_wopen(p.c_str(),_O_CREAT|_O_EXCL|_O_WRONLY|_O_BINARY,_S_IREAD|_S_IWRITE);
    FILE* f=fd<0?nullptr:_fdopen(fd,"wb");
#else
    int fd=open(p.c_str(),O_CREAT|O_EXCL|O_WRONLY,0600);
    FILE* f=fd<0?nullptr:fdopen(fd,"wb");
#endif
    if(!f){
        if(fd>=0){
#ifdef _WIN32
            _close(fd);
#else
            close(fd);
#endif
        }
        e="exclusive checkpoint create failed; preserve existing files";return false;
    }
    bool ok=fwrite(b.data(),1,b.size(),f)==b.size() && fflush(f)==0;
#ifdef _WIN32
    if(ok)ok=_commit(_fileno(f))==0;
#else
    if(ok)ok=fsync(fileno(f))==0;
#endif
    if(fclose(f)!=0)ok=false;
    if(!ok)e="checkpoint durable write failed";
    return ok;
}
bool replace(const fs::path& from,const fs::path& to){
#ifdef _WIN32
    return MoveFileExW(from.c_str(),to.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
#else
    return rename(from.c_str(),to.c_str())==0;
#endif
}
void put(Bytes& b,uint64_t n){for(int i=0;i<8;++i)b.push_back(uint8_t(n>>(i*8)));}
uint64_t get(const Bytes& b,size_t p){uint64_t n=0;for(int i=0;i<8;++i)n|=uint64_t(b[p+i])<<(i*8);return n;}
Bytes marker(uint64_t now,uint64_t previous,uint64_t highWater){Bytes b={'P','C','C','O','M','M','0','2'};put(b,now);put(b,previous);put(b,highWater);auto h=digest(b);b.insert(b.end(),h.begin(),h.end());return b;}
bool committed(const fs::path& dir,uint64_t& now,uint64_t& previous,std::string& e,uint64_t* highWater=nullptr){
    Bytes b;if(!read(dir/"CURRENT",b,e))return false;
    bool legacy=b.size()==56 && std::equal(b.begin(),b.begin()+8,"PCCOMMIT");
    if(!legacy && !(b.size()==64 && std::equal(b.begin(),b.begin()+8,"PCCOMM02"))){e="invalid commit record";return false;}
    size_t end=legacy?24:32;Bytes body(b.begin(),b.begin()+end);auto h=digest(body);
    if(!std::equal(h.begin(),h.end(),b.begin()+end)){e="commit checksum mismatch";return false;}
    now=get(b,8);previous=get(b,16);
    uint64_t water=legacy?now:get(b,24);
    if(!now || previous>=now || water<now){e="invalid committed generation order";return false;}
    if(highWater)*highWater=water;
    return true;
}
bool readGeneration(const fs::path& dir,uint64_t n,const Binding& b,const Coverage& c,Snapshot& s,std::string& e){
    Bytes bytes;if(!read(generation(dir,n),bytes,e) || !decode(bytes,b,c,s,e))return false;
    if(s.generation!=n){e="generation filename/content mismatch";return false;}return true;
}
std::string hex(const Digest& h){static const char* digits="0123456789abcdef";std::string s;for(auto v:h){s+=digits[v>>4];s+=digits[v&15];}return s;}
uint64_t processBirth(uint64_t pid){
#ifdef _WIN32
    HANDLE p=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,DWORD(pid));if(!p)return 0;
    FILETIME born{},exit{},kernel{},user{};bool ok=GetProcessTimes(p,&born,&exit,&kernel,&user)!=0;CloseHandle(p);
    return ok?(uint64_t(born.dwHighDateTime)<<32)|born.dwLowDateTime:0;
#elif defined(__linux__)
    std::ifstream f("/proc/"+std::to_string(pid)+"/stat");std::string line;std::getline(f,line);auto close=line.rfind(')');if(close==std::string::npos)return 0;
    std::istringstream fields(line.substr(close+2));std::string field;
    for(int n=3;n<=22;++n)if(!(fields>>field))return 0;
    try{return std::stoull(field);}catch(...){return 0;}
#else
    (void)pid;return 0;
#endif
}
Digest localIdentity(){
#ifdef _WIN32
    wchar_t name[256];DWORD n=256;if(!GetComputerNameW(name,&n))return {};
    const auto* bytes=reinterpret_cast<const uint8_t*>(name);return digest(Bytes(bytes,bytes+n*sizeof(wchar_t)));
#elif defined(__linux__)
    std::ifstream f("/proc/sys/kernel/random/boot_id");std::string boot;std::getline(f,boot);if(boot.empty())return {};
    std::error_code ec;auto ns=fs::read_symlink("/proc/self/ns/pid",ec);if(ec)return {};
    boot+=ns.string();return digest(Bytes(boot.begin(),boot.end()));
#else
    return {};
#endif
}
Bytes ownerRecord(){
#ifdef _WIN32
    uint64_t pid=GetCurrentProcessId(),platform=1;
#else
    uint64_t pid=uint64_t(getpid()),platform=2;
#endif
    auto host=localIdentity();auto birth=processBirth(pid);if(!birth || host==Digest{})return {};
    Bytes b={'P','C','O','W','N','E','R','1'};put(b,platform);put(b,pid);put(b,birth);b.insert(b.end(),host.begin(),host.end());auto h=digest(b);b.insert(b.end(),h.begin(),h.end());return b;
}
bool stoppedOwner(const Bytes& b,std::string& e){
    if(b.size()!=96 || !std::equal(b.begin(),b.begin()+8,"PCOWNER1")){e="unknown/incomplete writer identity";return false;}
    Bytes body(b.begin(),b.begin()+64);auto sum=digest(body);
    if(!std::equal(sum.begin(),sum.end(),b.begin()+64)){e="writer identity checksum mismatch";return false;}
    auto local=ownerRecord();if(local.empty() || get(local,8)!=get(b,8) || !std::equal(local.begin()+32,local.begin()+64,b.begin()+32)){e="foreign/unknown writer platform or host";return false;}
    uint64_t pid=get(b,16),born=get(b,24);if(!pid || !born || pid>std::numeric_limits<uint32_t>::max()){e="invalid writer PID/birth";return false;}
#ifdef _WIN32
    HANDLE p=OpenProcess(SYNCHRONIZE|PROCESS_QUERY_LIMITED_INFORMATION,FALSE,DWORD(pid));
    if(!p){if(GetLastError()==ERROR_INVALID_PARAMETER)return true;e="writer liveness unknown";return false;}
    FILETIME create{},exit{},kernel{},user{};
    bool known=GetProcessTimes(p,&create,&exit,&kernel,&user)!=0;
    uint64_t actual=(uint64_t(create.dwHighDateTime)<<32)|create.dwLowDateTime;
    DWORD wait=WaitForSingleObject(p,0);CloseHandle(p);
    if(!known){e="writer creation identity unknown";return false;}
    if(actual!=born || wait==WAIT_OBJECT_0)return true;
#elif defined(__linux__)
    auto actual=processBirth(pid);if(actual && actual!=born)return true;
    if(!actual){std::error_code ec;bool present=fs::exists("/proc/"+std::to_string(pid),ec);if(!ec&&!present)return true;e="writer liveness unknown";return false;}
#endif
    e="writer is alive or liveness unknown";return false;
}
struct Lock {
    fs::path path;bool held=false;
    ~Lock(){if(held){std::error_code ec;fs::remove(path/"owner",ec);fs::remove(path,ec);}}
    bool acquire(const fs::path& dir,std::string& e){
        path=dir/"WRITER.lock";std::error_code ec;held=fs::create_directory(path,ec);
        if(!held||ec){e="checkpoint writer locked; explicit recovery required for stale lock";return false;}
        auto owner=ownerRecord();if(owner.empty()){e="local durable writer identity unavailable";return false;}
        if(!writeNew(path/"owner",owner,e)||!syncDir(path)||!syncDir(dir)){if(e.empty())e="writer identity sync failed";return false;}return true;
    }
};
bool inspectPlan(const fs::path& dir,uint64_t selected,const Binding& binding,const Coverage& coverage,
                 RecoveryPlan& out,std::string& e,uint64_t dayEnd,bool allowUncommitted){
    RecoveryPlan plan;plan.selectedGeneration=selected;plan.dayEndGeneration=dayEnd;
    plan.explicitUncommittedSelection=allowUncommitted;
    if(!selected || selected<=dayEnd){e="selected checkpoint is superseded or invalid";return false;}
    if(fs::exists(dir/"WRITER.lock")){
        Bytes owner;if(!read(dir/"WRITER.lock/owner",owner,e)||!stoppedOwner(owner,e))return false;
        plan.lockOwner=digest(owner);
    }
    uint64_t now=0,previous=0,water=0;std::string markerError;
    plan.currentMissing=!fs::exists(dir/"CURRENT");
    bool trusted=false;
    if(!plan.currentMissing){
        Bytes current;if(!read(dir/"CURRENT",current,e))return false;plan.currentRecord=digest(current);
        trusted=committed(dir,now,previous,markerError,&water);
    }
    if(!(trusted && (selected==now || selected==previous)) && !allowUncommitted){
        e="selected generation lacks valid committed provenance; explicit uncommitted-selection acknowledgement required";return false;
    }
    Bytes bytes;Snapshot checked;
    if(!read(generation(dir,selected),bytes,e)||!decode(bytes,binding,coverage,checked,e)||checked.generation!=selected){if(e.empty())e="selected generation identity mismatch";return false;}
    plan.selectedCheckpoint=digest(bytes);plan.highWater=std::max(water,selected);
    size_t entries=0;
    for(const auto& entry:fs::directory_iterator(dir)){
        if(++entries>100000){e="recovery inventory exceeds bound";return false;}
        auto name=entry.path().filename().string();const std::string prefix="generation-",suffix=".checkpoint";
        if(name.size()<=prefix.size()+suffix.size()||name.compare(0,prefix.size(),prefix)!=0||name.compare(name.size()-suffix.size(),suffix.size(),suffix)!=0)continue;
        auto number=name.substr(prefix.size(),name.size()-prefix.size()-suffix.size());
        if(number.find_first_not_of("0123456789")!=std::string::npos)continue;
        try{auto n=std::stoull(number);plan.highWater=std::max(plan.highWater,uint64_t(n));}
        catch(...){e="recovery generation counter overflow";return false;}
    }
    if(plan.highWater==std::numeric_limits<uint64_t>::max()){e="generation counter exhausted";return false;}
    out=plan;e.clear();return true;
}
bool same(const RecoveryPlan& a,const RecoveryPlan& b){
    return a.selectedGeneration==b.selectedGeneration&&a.highWater==b.highWater&&a.dayEndGeneration==b.dayEndGeneration&&
        a.lockOwner==b.lockOwner&&a.currentRecord==b.currentRecord&&a.selectedCheckpoint==b.selectedCheckpoint&&
        a.currentMissing==b.currentMissing&&a.explicitUncommittedSelection==b.explicitUncommittedSelection;
}
bool preserve(const fs::path& path,const Bytes& bytes,std::string& e){
    if(!fs::exists(path))return writeNew(path,bytes,e);
    Bytes old;if(!read(path,old,e)||old!=bytes){e="existing recovery evidence differs; preserve both";return false;}return true;
}
}
bool inspectRecovery(const fs::path& dir,uint64_t selected,const Binding& b,const Coverage& c,
                     RecoveryPlan& out,std::string& e,uint64_t dayEnd,bool allowUncommitted){
    try{
        if(!fs::is_directory(dir)){e="no recovery directory";return false;}
        Gate gate;if(!gate.acquire(dir)){e="writer gate is live or unavailable";return false;}
        return inspectPlan(dir,selected,b,c,out,e,dayEnd,allowUncommitted);
    }catch(const std::exception& x){e=std::string("recovery inspection failed: ")+x.what();return false;}
}
bool recover(const fs::path& dir,const RecoveryPlan& approved,const Binding& b,const Coverage& c,
             std::string& e,Boundary boundary){
    try{
        Gate gate;if(!gate.acquire(dir)){e="writer gate is live or unavailable";return false;}
        RecoveryPlan fresh;
        if(!inspectPlan(dir,approved.selectedGeneration,b,c,fresh,e,approved.dayEndGeneration,approved.explicitUncommittedSelection))return false;
        if(!same(fresh,approved)){e="recovery plan changed; inspect and approve again";return false;}
        auto step=[&](const char* name){if(boundary&&!boundary(name)){e=std::string("interrupted at ")+name;return false;}return true;};
        if(approved.lockOwner!=Digest{}){
            auto archive=dir/("recovered-lock-"+hex(approved.lockOwner));
            if(fs::exists(archive)){e="existing retired lock evidence; explicit inspection required";return false;}
            fs::rename(dir/"WRITER.lock",archive);
            if(!syncDir(dir)){e="retired lock sync failed";return false;}
        }
        Lock lock;if(!lock.acquire(dir,e))return false;
        if(!step("recovery-owner-durable"))return false;
        if(!approved.currentMissing){
            Bytes old;if(!read(dir/"CURRENT",old,e)||digest(old)!=approved.currentRecord){e="current record changed during recovery";return false;}
            if(!preserve(dir/("recovered-current-"+hex(approved.currentRecord)+".evidence"),old,e)||!syncDir(dir)){if(e.empty())e="recovery evidence sync failed";return false;}
        }
        if(!step("recovery-evidence-durable"))return false;
        auto pending=dir/("recovery-"+hex(approved.selectedCheckpoint)+"-"+std::to_string(approved.highWater)+".pending");
        if(!preserve(pending,marker(approved.selectedGeneration,0,approved.highWater),e))return false;
        if(!step("recovery-commit-durable"))return false;
        if(!replace(pending,dir/"CURRENT")||!syncDir(dir)){e="recovery publication uncertain or failed; inspect before continuing";return false;}
        if(!step("recovery-commit-published"))return false;
        e.clear();return true;
    }catch(const std::exception& x){e=std::string("checkpoint recovery failed: ")+x.what();return false;}
}
bool publish(const std::filesystem::path& dir,const Snapshot& s,const Coverage& c,std::string& e,Boundary boundary){
    try {
    Bytes bytes;if(!encode(s,c,bytes,e))return false;
    if(!prepareDirectory(dir,e))return false;
    Gate gate;if(!gate.acquire(dir)){e="checkpoint process-lifetime writer gate busy or unavailable";return false;}
    Lock lock;if(!lock.acquire(dir,e))return false;
    auto step=[&](const char* name){if(boundary && !boundary(name)){e=std::string("interrupted at ")+name;return false;}return true;};
    uint64_t now=0,previous=0,highWater=0;
    if(fs::exists(dir/"CURRENT")){
        if(!committed(dir,now,previous,e,&highWater))return false;
        Snapshot current;if(!readGeneration(dir,now,s.binding,c,current,e))return false;
        if(s.generation<=highWater){e="generation must exceed committed high-water mark";return false;}
    }
    if(!writeNew(generation(dir,s.generation),bytes,e) || !syncDir(dir)){if(e.empty())e="generation directory sync failed";return false;}
    if(!step("generation-durable"))return false;
    Snapshot verified;if(!readGeneration(dir,s.generation,s.binding,c,verified,e))return false;
    if(!step("generation-verified"))return false;
    // Pending marker is generation-specific, exclusive and never overwrites a
    // foreign/orphan pending file. Only this transaction can rename its file.
    auto pending=dir/("commit-"+std::to_string(s.generation)+".pending");
    if(!writeNew(pending,marker(s.generation,now,s.generation),e))return false;
    if(!step("commit-durable"))return false;
    if(!replace(pending,dir/"CURRENT")){e="commit atomic publication failed";return false;}
    if(!syncDir(dir)){e="commit directory sync failed; outcome requires inspection";return false;}
    if(!step("commit-published"))return false;
    e.clear();return true;
    } catch(const std::exception& x){e=std::string("checkpoint publication failed: ")+x.what();return false;}
}
LoadResult load(const std::filesystem::path& dir,const Binding& b,const Coverage& c,Snapshot& out,std::string& e,bool acceptPrevious,uint64_t latestDayEndGeneration){
    try {
    if(!fs::exists(dir/"CURRENT")){e="no committed checkpoint (orphans are not saves)";return LoadResult::Missing;}
    uint64_t now=0,previous=0;if(!committed(dir,now,previous,e))return LoadResult::Invalid;
    if(now<=latestDayEndGeneration){e="mid-day checkpoint superseded by later day-end generation";return LoadResult::Invalid;}
    Snapshot s;if(readGeneration(dir,now,b,c,s,e)){out=std::move(s);return LoadResult::Current;}
    auto original=e;std::string oldError;
    if(previous>latestDayEndGeneration && readGeneration(dir,previous,b,c,s,oldError)){
        e="current checkpoint invalid: "+original+"; previous-good recovery requires explicit consent";
        if(!acceptPrevious)return LoadResult::RecoveryAvailable;
        out=std::move(s);return LoadResult::Recovered;
    }
    e="current checkpoint invalid: "+original+"; no verified previous-good checkpoint";return LoadResult::Invalid;
    } catch(const std::exception& x){e=std::string("checkpoint load failed: ")+x.what();return LoadResult::Invalid;}
}
}
