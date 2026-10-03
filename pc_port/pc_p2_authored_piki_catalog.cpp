#include "pc_p2_authored_piki_catalog.h"
#include "pc_randomizer_spawn_catalog.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <set>
namespace {
struct Row {unsigned offset=0,source=0,stable=0;};
struct Catalog {P2AuthoredCaveRoute route;std::string file;std::vector<Row> rows;};
Catalog selected;
bool readBytes(const std::filesystem::path& path,std::string& out,std::size_t limit){
    std::ifstream in(path,std::ios::binary);if(!in)return false;
    std::string next;char buffer[4096];
    while(in.read(buffer,sizeof(buffer))||in.gcount()){
        const auto count=static_cast<std::size_t>(in.gcount());
        if(count>limit-next.size())return false;
        next.append(buffer,count);
    }
    if(!in.eof()||next.empty())return false;
    out=std::move(next);return true;
}
}
unsigned pc_p2_authored_piki_catalog_uid(std::uint64_t seed,const std::string& token,const std::string& generatorSha,int stage,unsigned offset,unsigned sourceUid){
    const auto digest=P2AuthoredCaveSession::hash("AUTHORED_PIKI_GENERATOR1/"+std::to_string(seed)+"/"+token+"/"+generatorSha+"/"+std::to_string(stage)+"/"+std::to_string(offset)+"/"+std::to_string(sourceUid));
    return 0xa0000000u|(static_cast<unsigned>(std::stoul(digest.substr(0,8),nullptr,16))&0x0fffffffu);
}
bool pc_p2_authored_piki_catalog_validate(const P2AuthoredCaveRoute& route,const std::string& runRoot,std::string& error){
    auto refuse=[&](const char* why){error=why;return false;};
    std::string raw;
    if(!route.present||!route.valid()||!readBytes(std::filesystem::path(runRoot)/"p2-authored-piki-generators.txt",raw,65536)
        ||P2AuthoredCaveSession::hash(raw)!=route.pikiGeneratorsSha)return refuse("authored Pikmin catalog digest mismatch");
    std::istringstream in(raw);std::string tag,version,seedText,token,surface,surfaceSha,file,fileSha,end,extra;
    std::uint64_t seed=0;int stage=-1,index=-1;unsigned count=0;
    if(!(in>>tag>>version>>seedText>>token>>stage>>index>>surface>>surfaceSha>>file>>fileSha>>count)
        ||tag!="P2_AUTHORED_PIKI_GENERATORS"||version!="1"||!p2CaveSeedUint64(seedText,seed)
        ||seed!=route.seed||token!=route.token||stage!=route.surface.stage||index!=route.surface.index
        ||surface!=route.surface.file||surfaceSha!=route.surface.mapSha||count!=20
        ||!P2AuthoredCaveStage::digest(fileSha))return refuse("authored Pikmin catalog route mismatch");
    auto expected=std::filesystem::path(route.surface.file);expected.replace_extension();expected/="default.gen";
    if(file!=expected.generic_string())return refuse("foreign authored Pikmin generator file");
    std::string generator;
    if(!readBytes(std::filesystem::path("assets/dataDir")/file,generator,16*1024*1024)
        ||P2AuthoredCaveSession::hash(generator)!=fileSha||generator.size()<24||generator.compare(0,4,"1.0v"))
        return refuse("authored Pikmin generator bytes mismatch");
    Catalog next;next.route=route;next.file="default.gen";
    auto word=[&](std::size_t at){unsigned value=0;for(unsigned b=0;b<4;++b)value=(value<<8)|static_cast<unsigned char>(generator[at+b]);return value;};
    if(generator.size()<24+20*168||word(20)!=49)return refuse("authored Pikmin generator header mismatch");
    const auto first=generator.size()-20*168;
    std::set<unsigned> offsets,sources,stable;
    for(unsigned i=0;i<count;++i){
        Row row;unsigned species=0,bodyCount=0;
        if(!(in>>row.offset>>row.source>>row.stable>>species>>bodyCount)||species!=1||bodyCount!=1
            ||row.offset!=first+i*168||!row.source
            ||generator.compare(row.offset,8,"    0.0v")||generator.compare(row.offset+72,4,"ikip")
            ||(word(row.offset+12)&15)!=15
            ||generator.compare(row.offset+80,4,"p00\4")||word(row.offset+84)!=2
            ||generator.compare(row.offset+88,4,"p01\4")||word(row.offset+92)!=1
            ||generator.compare(row.offset+132,8,"nota0.0v")||generator.compare(row.offset+156,4,"p00\4")||word(row.offset+160)!=1
            ||!offsets.insert(row.offset).second||!sources.insert(row.source).second||!stable.insert(row.stable).second)
            return refuse("invalid authored Pikmin generator row");
        unsigned native=0;for(unsigned b=0;b<4;++b)native|=unsigned(static_cast<unsigned char>(generator[row.offset+8+b]))<<(8*b);
        if(native!=row.source||row.stable!=pc_p2_authored_piki_catalog_uid(seed,token,fileSha,stage,row.offset,row.source))
            return refuse("authored Pikmin literal identity mismatch");
        for(const auto& ordinary:randomizerSpawnSlots)if(ordinary.uid==row.stable)return refuse("authored Pikmin identity collides with enemy catalog");
        next.rows.push_back(row);
    }
    if(!(in>>end)||end!="END"||(in>>extra))return refuse("trailing authored Pikmin catalog fields");
    unsigned observed=0;for(std::size_t at=0;(at=generator.find("ikip",at))!=std::string::npos;++at){
        ++observed;if(at<72||!offsets.count(static_cast<unsigned>(at-72)))return refuse("unlisted authored Pikmin record");
    }
    if(observed!=20)return refuse("authored Pikmin record census mismatch");
    selected=std::move(next);error.clear();return true;
}
bool pc_p2_authored_piki_catalog_bind(const P2AuthoredCaveRoute& route,int stage,const char* file,int offset,unsigned sourceUid,unsigned& stableUid){
    if(!route.present||!(route==selected.route)||stage!=route.surface.stage||!file||file!=selected.file||offset<0)return false;
    for(const auto& row:selected.rows)if(row.offset==static_cast<unsigned>(offset)&&row.source==sourceUid){stableUid=row.stable;return true;}
    return false;
}
bool pc_p2_authored_piki_catalog_saved(const P2AuthoredCaveRoute& route,unsigned stableUid,unsigned sourceUid){
    if(!route.present||!(route==selected.route)||!stableUid||!sourceUid)return false;
    for(const auto& row:selected.rows)if(row.stable==stableUid&&row.source==sourceUid)return true;
    return false;
}
bool pc_p2_authored_piki_catalog_contains(const P2AuthoredCaveRoute& route,unsigned stableUid){
    if(!route.present||!(route==selected.route)||!stableUid)return false;
    for(const auto& row:selected.rows)if(row.stable==stableUid)return true;
    return false;
}
