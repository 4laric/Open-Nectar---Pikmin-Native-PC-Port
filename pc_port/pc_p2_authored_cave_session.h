#pragma once
#include "pc_p2_cave_campaign_cache.h"
#include "pc_p2_cave_campaign_party.h"
#include "pc_p2_cave_seed_binding.h"
#include "netplay/pc_netplay_sha256.h"

// The selected authored route is separate from the original retail context.
// This descriptor neither installs a scene nor grants a native actor lifetime.
struct P2AuthoredCaveStage {
    int stage=-1,index=-1;
    std::string file,mapSha;
    P2CavePartyPoint landing;
    static bool digest(const std::string& s){return s.size()==64&&s.find_first_not_of("0123456789abcdef")==std::string::npos;}
    bool valid()const{
        if(stage<0||stage>=5||index<0||index>=5||file.size()>255||file.rfind("stages/",0)!=0
            ||file.find("..")!=std::string::npos||file.find('\\')!=std::string::npos||!digest(mapSha)||!landing.valid())return false;
        for(unsigned char c:file)if(c<=32||c>=127)return false;
        return true;
    }
    bool read(std::istream& in){return bool(in>>stage>>index>>file>>mapSha)&&landing.read(in)&&valid();}
    void write(std::ostream& out)const{out<<' '<<stage<<' '<<index<<' '<<file<<' '<<mapSha;landing.write(out);}
    bool operator==(const P2AuthoredCaveStage& s)const{return stage==s.stage&&index==s.index&&file==s.file&&mapSha==s.mapSha
        &&landing.x==s.landing.x&&landing.y==s.landing.y&&landing.z==s.landing.z;}
};
struct P2AuthoredCaveRoute {
    bool present=false;
    std::uint64_t seed=0;
    std::string token,routeSha,pikiGeneratorsSha;
    P2AuthoredCaveStage surface,floor;
    P2CavePartyPoint exit;
    bool valid()const{
        if(!present)return token.empty()&&routeSha.empty()&&pikiGeneratorsSha.empty();
        return token.size()==32&&token.find_first_not_of("0123456789abcdef")==std::string::npos
            &&P2AuthoredCaveStage::digest(routeSha)&&P2AuthoredCaveStage::digest(pikiGeneratorsSha)&&surface.valid()&&floor.valid()&&exit.valid()
            &&floor.file=="stages/generated-forest.ini"&&surface.file!=floor.file
            &&surface.stage==floor.stage&&surface.index==floor.index;
    }
    bool matches(const P2CaveSeedBinding& selected)const{return present&&valid()&&seed==selected.seed&&token==selected.token;}
    bool operator==(const P2AuthoredCaveRoute& r)const{return present==r.present&&seed==r.seed&&token==r.token&&routeSha==r.routeSha&&pikiGeneratorsSha==r.pikiGeneratorsSha&&surface==r.surface&&floor==r.floor&&exit.x==r.exit.x&&exit.y==r.exit.y&&exit.z==r.exit.z;}
    bool read(std::istream& in,bool markerConsumed=false){
        P2AuthoredCaveRoute next;std::string tag,version,seedText;
        if(markerConsumed)tag="AUTHORED_CAVE_ROUTE";else if(!(in>>tag))return false;
        if(!(in>>version>>seedText>>next.token>>next.routeSha>>next.pikiGeneratorsSha)||tag!="AUTHORED_CAVE_ROUTE"||version!="2"
            ||!p2CaveSeedUint64(seedText,next.seed)||!next.surface.read(in)||!next.floor.read(in)||!next.exit.read(in))return false;
        next.present=true;
        if(!next.valid())return false;
        *this=std::move(next);return true;
    }
    void write(std::ostream& out)const{out<<std::setprecision(std::numeric_limits<float>::max_digits10)<<" AUTHORED_CAVE_ROUTE 2 "<<seed<<' '<<token<<' '<<routeSha<<' '<<pikiGeneratorsSha;surface.write(out);floor.write(out);exit.write(out);}
};
struct P2AuthoredCaveSession {
    bool present=false;
    P2AuthoredCaveRoute route;
    int day=-1;
    std::string surfaceCacheSha,floorCacheSha,activeCacheSha;
    P2CaveCampaignParty party;
    static std::string hash(const std::string& bytes){
        unsigned char digest[32];pc_netplay_sha::sha256(bytes.data(),bytes.size(),digest);std::string out;
        for(auto b:digest){out+="0123456789abcdef"[b>>4];out+="0123456789abcdef"[b&15];}return out;
    }
    static std::string bankHash(const std::string& route,const char* realm,const std::string& bytes){return bytes.empty()?"-":hash(route+"/"+realm+"/"+bytes);}
    bool valid()const{
        if(!present)return !route.present&&route.valid()&&day==-1&&surfaceCacheSha.empty()&&floorCacheSha.empty()&&activeCacheSha.empty()&&!party.present&&party.valid();
        return route.present&&route.valid()&&day>=0&&day<30&&party.present&&party.resumeLiving&&party.valid()
            &&(surfaceCacheSha=="-"||P2AuthoredCaveStage::digest(surfaceCacheSha))
            &&(floorCacheSha=="-"||P2AuthoredCaveStage::digest(floorCacheSha))&&P2AuthoredCaveStage::digest(activeCacheSha);
    }
    bool matches(const P2AuthoredCaveRoute& selected,const P2CaveCacheBanks& banks)const{
        return present&&valid()&&route==selected&&banks.valid()&&party.inside==banks.inside
            &&surfaceCacheSha==bankHash(route.routeSha,"surface",banks.surface)&&floorCacheSha==bankHash(route.routeSha,"floor",banks.floor);
    }
    bool activeCacheMatches(const std::string& nativeCache)const{
        return present&&valid()&&P2CaveCacheBanks::imageValid(nativeCache)
            &&activeCacheSha==bankHash(route.routeSha,party.inside?"floor":"surface",nativeCache);
    }
    bool read(std::istream& in){
        P2AuthoredCaveSession next;std::string tag,version;int flag=-1;
        if(!(in>>tag>>version>>flag)||tag!="AUTHORED_CAVE_SESSION"||version!="1"||(flag!=0&&flag!=1))return false;
        if(flag){if(!next.route.read(in)||!(in>>next.day>>next.surfaceCacheSha>>next.floorCacheSha>>next.activeCacheSha)||!next.party.read(in))return false;next.present=true;}
        if(!next.valid())return false;
        *this=std::move(next);return true;
    }
    void write(std::ostream& out)const{
        out<<std::setprecision(std::numeric_limits<float>::max_digits10)<<" AUTHORED_CAVE_SESSION 1 "<<int(present);
        if(present){route.write(out);out<<' '<<day<<' '<<surfaceCacheSha<<' '<<floorCacheSha<<' '<<activeCacheSha;party.write(out);}
    }
};
