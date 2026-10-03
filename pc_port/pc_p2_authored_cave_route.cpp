#include "pc_p2_authored_cave_route.h"
#include <filesystem>
#include <fstream>
#include <sstream>
namespace {
bool bytes(const std::filesystem::path& path,std::string& out,std::uintmax_t limit){
    std::error_code ec;const auto size=std::filesystem::file_size(path,ec);
    if(ec||!size||size>limit)return false;
    std::ifstream in(path,std::ios::binary);
    out.assign(std::istreambuf_iterator<char>(in),{});
    return !in.bad()&&out.size()==size;
}
}
bool pc_p2_authored_cave_route_validate(const P2AuthoredCaveRoute& selected,
    const std::string& runRoot,std::string& error){
    auto refuse=[&](const char* why){error=why;return false;};
    if(!selected.present||!selected.valid())return refuse("invalid selected authored route");
    std::string raw;
    if(!bytes(std::filesystem::path(runRoot)/"p2-authored-cave-route.txt",raw,65536)
        ||P2AuthoredCaveSession::hash(raw)!=selected.routeSha)
        return refuse("authored route bytes differ from selected digest");
    P2AuthoredCaveRoute actual;std::string magic,version,seedText,end,extra;
    std::istringstream in(raw);
    if(!(in>>magic>>version>>seedText>>actual.token)
        ||magic!="P2_AUTHORED_CAVE_ROUTE"||version!="1"
        ||!p2CaveSeedUint64(seedText,actual.seed)||!actual.surface.read(in)
        ||!actual.floor.read(in)||!actual.exit.read(in)||!(in>>end)
        ||end!="END"||(in>>extra))return refuse("malformed authored route input");
    actual.present=true;actual.routeSha=selected.routeSha;
    if(!actual.valid()||!(actual==selected))return refuse("authored route fields differ from selected contract");
    // DVDOpen resolves these same native paths relative to the process's assets
    // directory. The host supplies runRoot only for the selected sidecar.
    for(const auto* stage:{&selected.surface,&selected.floor}){
        std::string ini;
        if(!bytes(std::filesystem::path("assets")/stage->file,ini,16*1024*1024)
            ||P2AuthoredCaveSession::hash(ini)!=stage->mapSha)
            return refuse("authored stage INI bytes differ from selected digest");
    }
    error.clear();return true;
}
