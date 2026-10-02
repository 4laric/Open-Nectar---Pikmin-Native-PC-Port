#include "pc_midday_store.h"
#include <cstdio>
#include <cerrno>
#include <fstream>
#include <system_error>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#include <sys/stat.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace pc_midday {
namespace {
namespace fs=std::filesystem;
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
Bytes marker(uint64_t now,uint64_t previous){Bytes b={'P','C','C','O','M','M','I','T'};put(b,now);put(b,previous);auto h=digest(b);b.insert(b.end(),h.begin(),h.end());return b;}
bool committed(const fs::path& dir,uint64_t& now,uint64_t& previous,std::string& e){
    Bytes b;if(!read(dir/"CURRENT",b,e))return false;
    if(b.size()!=56 || !std::equal(b.begin(),b.begin()+8,"PCCOMMIT")){e="invalid commit record";return false;}
    Bytes body(b.begin(),b.begin()+24);auto h=digest(body);
    if(!std::equal(h.begin(),h.end(),b.begin()+24)){e="commit checksum mismatch";return false;}
    now=get(b,8);previous=get(b,16);
    if(!now || previous>=now){e="invalid committed generation order";return false;}return true;
}
bool readGeneration(const fs::path& dir,uint64_t n,const Binding& b,const Coverage& c,Snapshot& s,std::string& e){
    Bytes bytes;if(!read(generation(dir,n),bytes,e) || !decode(bytes,b,c,s,e))return false;
    if(s.generation!=n){e="generation filename/content mismatch";return false;}return true;
}
struct Lock {fs::path path;bool held=false;~Lock(){if(held){std::error_code ec;fs::remove(path,ec);}}};
}
bool publish(const std::filesystem::path& dir,const Snapshot& s,const Coverage& c,std::string& e,Boundary boundary){
    try {
    Bytes bytes;if(!encode(s,c,bytes,e))return false;
    std::error_code ec;fs::create_directories(dir,ec);if(ec){e="save directory create failed";return false;}
    Lock lock{dir/"WRITER.lock",false};lock.held=fs::create_directory(lock.path,ec);
    if(!lock.held || ec){e="checkpoint writer locked; explicit recovery required for stale lock";return false;}
    auto step=[&](const char* name){if(boundary && !boundary(name)){e=std::string("interrupted at ")+name;return false;}return true;};
    uint64_t now=0,previous=0;
    if(fs::exists(dir/"CURRENT")){
        if(!committed(dir,now,previous,e))return false;
        Snapshot current;if(!readGeneration(dir,now,s.binding,c,current,e))return false;
        if(s.generation<=now){e="generation must advance";return false;}
    }
    if(!writeNew(generation(dir,s.generation),bytes,e) || !syncDir(dir)){if(e.empty())e="generation directory sync failed";return false;}
    if(!step("generation-durable"))return false;
    Snapshot verified;if(!readGeneration(dir,s.generation,s.binding,c,verified,e))return false;
    if(!step("generation-verified"))return false;
    // Pending marker is generation-specific, exclusive and never overwrites a
    // foreign/orphan pending file. Only this transaction can rename its file.
    auto pending=dir/("commit-"+std::to_string(s.generation)+".pending");
    if(!writeNew(pending,marker(s.generation,now),e))return false;
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
