// Real campaign envelope parser/writer, with a synthetic 32 KiB card buffer.
// This component has no game actors and does not claim native gameplay SAVE.
#define PC_RANDOMIZER_NO_ORIGINAL_ENGINE 1
#define PIKMIN_P2_AUTHORED_CAVE_PROVIDER 1
#include "../pc_port/pc_randomizer.cpp"
#include "pc_p2_authored_cave_route.h"
#include "pc_p2_authored_piki_catalog.h"
#include <cassert>
#include <iostream>

void bytes(const std::filesystem::path& p,const std::string& b){std::filesystem::create_directories(p.parent_path());std::ofstream f(p,std::ios::binary);f.write(b.data(),b.size());assert(f.good());}
void be(std::string& raw,unsigned at,unsigned n){for(int i=3;i>=0;--i){raw[at+i]=char(n);n>>=8;}}
// Synthetic component records exercise the authenticated parser, not native births.
std::string generatorImage(){
    std::string raw(24+29*80+20*168,0);raw.replace(0,4,"1.0v");be(raw,20,49);
    for(unsigned i=0;i<20;++i){unsigned at=24+29*80+i*168;raw.replace(at,8,"    0.0v");
        unsigned uid=200+i;for(unsigned b=0;b<4;++b)raw[at+8+b]=char(uid>>(8*b));
        be(raw,at+12,15);raw.replace(at+72,4,"ikip");raw.replace(at+80,4,"p00\4");be(raw,at+84,2);
        raw.replace(at+88,4,"p01\4");be(raw,at+92,1);raw.replace(at+132,8,"nota0.0v");
        raw.replace(at+156,4,"p00\4");be(raw,at+160,1);
    }return raw;
}
std::string catalogImage(const P2AuthoredCaveRoute& r,const std::string& generator,int offsetDelta=0,int sourceDelta=0,int stableDelta=0,int species=1,int count=1){
    const auto digest=P2AuthoredCaveSession::hash(generator);std::ostringstream out;
    out<<"P2_AUTHORED_PIKI_GENERATORS 1 "<<r.seed<<' '<<r.token<<' '<<r.surface.stage<<' '<<r.surface.index
        <<' '<<r.surface.file<<' '<<r.surface.mapSha<<" stages/forest/default.gen "<<digest<<" 20";
    for(unsigned i=0;i<20;++i){unsigned offset=24+29*80+i*168,uid=0;
        for(unsigned b=0;b<4;++b)uid|=unsigned(static_cast<unsigned char>(generator[offset+8+b]))<<(8*b);
        const auto stable=pc_p2_authored_piki_catalog_uid(r.seed,r.token,digest,r.surface.stage,offset,uid);
        out<<' '<<int(offset)+offsetDelta<<' '<<int(uid)+sourceDelta<<' '<<std::uint64_t(stable)+stableDelta<<' '<<species<<' '<<count;
    }out<<" END\n";return out.str();
}
std::string cacheImage(){std::string b(P2CaveCacheBanks::imageSize,0);auto put=[&](unsigned p,unsigned n){for(int i=3;i>=0;--i){b[p+i]=char(n);n>>=8;}};put(4,P2CaveCacheBanks::heapSize);for(unsigned i=0;i<5;++i){unsigned p=8+P2CaveCacheBanks::heapSize+i*37;b[p]=char(255);put(p+1,i);}return b;}
int main(int argc,char** argv){
    if(argc==3&&std::string(argv[1])=="--randomizer-seed"){
        assert(pc_randomizer_init(argc,argv)&&pc_randomizer_authored_cave_route().present&&!pc_randomizer_resumed());
        std::cout<<"PASS actual bootstrap selected authored route before load; no native scene/body/SAVE\n";return 0;
    }
    assert(argc==2);const auto root=std::filesystem::absolute(argv[1])/std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());assert(!std::filesystem::exists(root));std::filesystem::create_directories(root);std::filesystem::current_path(root);
    P2AuthoredCaveRoute r;r.present=true;r.seed=42;r.token=std::string(32,'a');r.surface={1,1,"stages/forest.ini",P2AuthoredCaveSession::hash("surface"),{1,30,2}};r.floor={1,1,"stages/generated-forest.ini",P2AuthoredCaveSession::hash("floor"),{3,30,4}};r.exit={10,20,30};
    const auto generator=generatorImage();const auto catalog=catalogImage(r,generator);r.pikiGeneratorsSha=P2AuthoredCaveSession::hash(catalog);
    bytes(root/"assets/dataDir/stages/forest/default.gen",generator);bytes(root/"p2-authored-piki-generators.txt",catalog);
    std::ostringstream routeBytes;routeBytes<<"P2_AUTHORED_CAVE_ROUTE 2 "<<r.seed<<' '<<r.token<<' '<<r.pikiGeneratorsSha;r.surface.write(routeBytes);r.floor.write(routeBytes);r.exit.write(routeBytes);routeBytes<<" END\n";r.routeSha=P2AuthoredCaveSession::hash(routeBytes.str());
    bytes(root/"p2-authored-cave-route.txt",routeBytes.str());bytes(root/"assets"/"dataDir"/r.surface.file,"surface");bytes(root/"assets"/"dataDir"/r.floor.file,"floor");std::string error;assert(pc_p2_authored_cave_route_validate(r,root.generic_string(),error));
    auto altered=r;altered.floor.mapSha[0]=altered.floor.mapSha[0]=='a'?'b':'a';assert(!pc_p2_authored_cave_route_validate(altered,root.generic_string(),error));
    bytes(root/"assets"/"dataDir"/r.floor.file,"wrong floor");assert(!pc_p2_authored_cave_route_validate(r,root.generic_string(),error));bytes(root/"assets"/"dataDir"/r.floor.file,"floor");
    bytes(root/"p2-authored-cave-route.txt",routeBytes.str()+"trailing\n");assert(!pc_p2_authored_cave_route_validate(r,root.generic_string(),error));bytes(root/"p2-authored-cave-route.txt",routeBytes.str());
    auto trailing=r;trailing.routeSha=P2AuthoredCaveSession::hash(routeBytes.str()+"trailing\n");bytes(root/"p2-authored-cave-route.txt",routeBytes.str()+"trailing\n");assert(!pc_p2_authored_cave_route_validate(trailing,root.generic_string(),error));
    const std::string oversized(65537,'x');trailing.routeSha=P2AuthoredCaveSession::hash(oversized);bytes(root/"p2-authored-cave-route.txt",oversized);assert(!pc_p2_authored_cave_route_validate(trailing,root.generic_string(),error));bytes(root/"p2-authored-cave-route.txt",routeBytes.str());
    unsigned stable=0;const auto firstUid=pc_p2_authored_piki_catalog_uid(r.seed,r.token,P2AuthoredCaveSession::hash(generator),1,24+29*80,200);
    assert(pc_p2_authored_piki_catalog_bind(r,1,"default.gen",24+29*80,200,stable)&&stable==firstUid);
    assert(pc_p2_authored_piki_catalog_saved(r,firstUid,200));
    assert(pc_p2_authored_piki_catalog_contains(r,firstUid)&&!pc_p2_authored_piki_catalog_contains(r,0));
    auto foreignCatalog=r;foreignCatalog.pikiGeneratorsSha[0]=foreignCatalog.pikiGeneratorsSha[0]=='a'?'b':'a';
    assert(!pc_p2_authored_piki_catalog_contains(foreignCatalog,firstUid));
    assert(!pc_p2_authored_piki_catalog_bind(r,2,"default.gen",24+29*80,200,stable));
    assert(!pc_p2_authored_piki_catalog_bind(r,1,"init.gen",24+29*80,200,stable));
    assert(!pc_p2_authored_piki_catalog_bind(r,1,"default.gen",25+29*80,200,stable));
    assert(!pc_p2_authored_piki_catalog_bind(r,1,"default.gen",24+29*80,201,stable));
    assert(!pc_p2_authored_piki_catalog_saved(r,firstUid,201));
    auto rejectCatalog=[&](const std::string& changedGenerator,const std::string& changedCatalog){
        auto selected=r;selected.pikiGeneratorsSha=P2AuthoredCaveSession::hash(changedCatalog);
        bytes(root/"assets/dataDir/stages/forest/default.gen",changedGenerator);bytes(root/"p2-authored-piki-generators.txt",changedCatalog);
        assert(!pc_p2_authored_piki_catalog_validate(selected,root.generic_string(),error));
    };
    for(unsigned field:{12u,84u,92u,160u}){auto bad=generator;be(bad,24+29*80+field,0);rejectCatalog(bad,catalogImage(r,bad));}
    auto badCountTag=generator;badCountTag[24+29*80+156]='x';rejectCatalog(badCountTag,catalogImage(r,badCountTag));
    rejectCatalog(generator,catalogImage(r,generator,1));rejectCatalog(generator,catalogImage(r,generator,0,1));
    rejectCatalog(generator,catalogImage(r,generator,0,0,1));rejectCatalog(generator,catalogImage(r,generator,0,0,0,2));
    rejectCatalog(generator,catalogImage(r,generator,0,0,0,1,2));rejectCatalog(generator,catalog+"trailing");
    bytes(root/"assets/dataDir/stages/forest/default.gen",generator);bytes(root/"p2-authored-piki-generators.txt",catalog);
    assert(pc_p2_authored_cave_route_validate(r,root.generic_string(),error));
    authoredCaveRoute=r;generatedCave=true;enabled=true;fingerprint=std::string(64,'f');campaignDirectory=root/"campaign";generatedCaveBinding.seed=r.seed;generatedCaveBinding.token=r.token;
    std::string card(32768,0);assert(write_campaign_checkpoint(card.data(),1,false,nullptr,nullptr));CkptScan scan;assert(scanCampaignCheckpoint(scan)==kCkptOk&&!scan.authored.present&&scan.authoredRoute==r);
    auto changed=r;changed.exit.x+=1;authoredCaveRoute=changed;CkptScan mismatch;assert(scanCampaignCheckpoint(mismatch)==kCkptMismatch);authoredCaveRoute=r;
    auto routeSelected=r;authoredCaveRoute={};CkptScan unselected;assert(scanCampaignCheckpoint(unselected)==kCkptMismatch);authoredCaveRoute=routeSelected;
    // Prospective capture guards reject bad state without replacing old banks.
    P2CaveCacheBanks banks;assert(banks.enter(cacheImage())&&banks.captureFloor(cacheImage()));
    P2AuthoredCaveSession next;next.present=true;next.route=r;next.day=0;next.party.present=true;next.party.inside=true;next.party.nextKey=21;
    for(int slot=0;slot<2;++slot){P2CavePartyCaptain c;c.slot=slot;c.health=75;c.maxHealth=100;next.party.captains.push_back(c);}
    for(int i=0;i<20;++i){P2CavePartyBody b;b.species=i%3;b.growth=i%3;b.owner=i%2;b.player=i%2;b.key=i+1;b.health=5;b.maxHealth=10;next.party.bodies.push_back(b);}
    next.surfaceCacheSha=P2AuthoredCaveSession::bankHash(r.routeSha,"surface",banks.surface);next.floorCacheSha=P2AuthoredCaveSession::bankHash(r.routeSha,"floor",banks.floor);next.activeCacheSha=next.floorCacheSha;
    auto bad=next;bad.route.exit.x+=1;assert(!pc_randomizer_authored_cave_checkpoint_set(bad,banks)&&!generatedCaveCache.inside&&!authoredCaveSession.present);
    assert(pc_randomizer_authored_cave_checkpoint_set(next,banks));assert(write_campaign_checkpoint(card.data(),2,false,nullptr,nullptr));CkptScan living;assert(scanCampaignCheckpoint(living)==kCkptOk&&living.authored.party.bodies.size()==20);
    auto old=generatedCaveCache;bad=next;bad.floorCacheSha[0]='0';if(bad.floorCacheSha==next.floorCacheSha)bad.floorCacheSha[0]='1';assert(!pc_randomizer_authored_cave_checkpoint_set(bad,banks)&&generatedCaveCache.floor==old.floor);
    loadCampaignCheckpoint();assert(campaignGeneration==2&&authoredCaveSession.party.bodies.size()==20&&campaignBlock.size()==32768);
    // Legacy mode still writes and reads version 2 without authored records.
    authoredCaveRoute={};authoredCaveSession={};generatedCaveCache={};campaignDirectory=root/"legacy";assert(write_campaign_checkpoint(card.data(),1,false,nullptr,nullptr));CkptScan legacy;assert(scanCampaignCheckpoint(legacy)==kCkptOk&&!legacy.authoredRoute.present);
    std::cout<<"PASS actual authored/legacy checkpoint writer+scanner+adoption, Route2/catalog binding and semantic refusals, atomic20-party state; synthetic card only\n";
}
