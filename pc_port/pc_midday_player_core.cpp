#include "pc_midday_player_core.h"
#include <algorithm>
#include <cstring>
namespace pc_midday {
namespace {
constexpr size_t Limit=4096;
bool fail(std::string& e,const char* text){e=text;return false;}
bool graphValid(const PlayerGraph& g){return g.end>=g.start&&g.entries.size()==size_t(g.end-g.start)+1&&g.entries.size()<=Limit;}
bool valid(const PlayerCoreFields& v,std::string& e){
 if(!graphValid(v.hour)||!graphValid(v.day))return fail(e,"player graph topology mismatch");
 for(const auto& f:v.courses){if(f.entries>Limit||f.bits.size()!=(f.entries?size_t(f.entries/8)+1:0))return fail(e,"course flag allocation mismatch");}
 return true;
}
void put(Bytes& b,uint32_t n,unsigned size){for(unsigned i=0;i<size;++i)b.push_back(uint8_t(n>>(8*i)));}
void signedPut(Bytes& b,int32_t n){uint32_t u;std::memcpy(&u,&n,4);put(b,u,4);}
struct Reader {
 const Bytes& b;size_t at=12;bool ok=true;
 uint32_t get(unsigned n){if(n>b.size()-std::min(at,b.size())){ok=false;return 0;}uint32_t v=0;for(unsigned i=0;i<n;++i)v|=uint32_t(b[at++])<<(8*i);return v;}
 int32_t signedGet(){uint32_t u=get(4);int32_t v;std::memcpy(&v,&u,4);return v;}
 bool boolean(){auto v=get(1);if(v>1)ok=false;return v!=0;}
};
void graphPut(Bytes& b,const PlayerGraph& g){put(b,g.start,2);put(b,g.end,2);for(const auto& row:g.entries)for(int32_t n:row)signedPut(b,n);}
bool graphGet(Reader& r,PlayerGraph& g){g.start=uint16_t(r.get(2));g.end=uint16_t(r.get(2));if(!r.ok||g.end<g.start||size_t(g.end-g.start)+1>Limit)return false;const size_t count=size_t(g.end-g.start)+1;if(count>(r.b.size()-r.at)/12)return false;g.entries.resize(count);for(auto& row:g.entries)for(auto& n:row)n=r.signedGet();return r.ok;}
}
bool encodePlayerCore(const PlayerCoreFields& v,Bytes& out,std::string& e){
 if(!valid(v,e))return false;
 Bytes b={'P','C','P','L','A','Y','0','1'};put(b,1,4);
 for(int32_t n:{v.sprouted,v.lostBattle,v.leftBehind,v.totalPlucked,v.totalRegisteredParts,v.totalParts,v.currentParts,v.requiredParts,v.totalDead,v.totalBorn,v.living})signedPut(b,n);
 for(uint8_t n:{v.shipUpgrade,v.shipEffect,v.container,v.displayPiki,v.unused186})put(b,n,1);
 for(bool n:{v.extinctionPlayed,v.tutorial,v.naviPilot,v.dayEnd,v.challenge})put(b,n?1:0,1);
 put(b,v.lastUpdated,2);
 for(const auto* a:{&v.collectedByDay,&v.partsToNext})b.insert(b.end(),a->begin(),a->end());
 b.insert(b.end(),v.stageParts.begin(),v.stageParts.end());
 graphPut(b,v.hour);graphPut(b,v.day);
 for(const auto& f:v.courses){put(b,f.entries,2);b.insert(b.end(),f.bits.begin(),f.bits.end());}
 out.swap(b);e.clear();return true;
}
bool decodePlayerCore(const Bytes& b,PlayerCoreFields& out,std::string& e){
 const Bytes magic={'P','C','P','L','A','Y','0','1'};
 if(b.size()<12||b.size()>MaxBytes||!std::equal(magic.begin(),magic.end(),b.begin()))return fail(e,"player core framing mismatch");
 Reader r{b};r.at=8;if(r.get(4)!=1)return fail(e,"player core version mismatch");PlayerCoreFields v;
 for(auto* n:{&v.sprouted,&v.lostBattle,&v.leftBehind,&v.totalPlucked,&v.totalRegisteredParts,&v.totalParts,&v.currentParts,&v.requiredParts,&v.totalDead,&v.totalBorn,&v.living})*n=r.signedGet();
 for(auto* n:{&v.shipUpgrade,&v.shipEffect,&v.container,&v.displayPiki,&v.unused186})*n=uint8_t(r.get(1));
 for(auto* n:{&v.extinctionPlayed,&v.tutorial,&v.naviPilot,&v.dayEnd,&v.challenge})*n=r.boolean();
 v.lastUpdated=uint16_t(r.get(2));
 for(auto* a:{&v.collectedByDay,&v.partsToNext})for(auto& n:*a)n=uint8_t(r.get(1));
 for(auto& n:v.stageParts)n=uint8_t(r.get(1));
 if(!r.ok||!graphGet(r,v.hour)||!graphGet(r,v.day))return fail(e,"player core truncated/invalid graph");
 for(auto& f:v.courses){f.entries=uint16_t(r.get(2));if(!r.ok||f.entries>Limit)return fail(e,"course flag count exceeds bound");size_t count=f.entries?size_t(f.entries/8)+1:0;if(count>b.size()-r.at)return fail(e,"truncated course flags");f.bits.assign(b.begin()+r.at,b.begin()+r.at+count);r.at+=count;}
 if(!r.ok||r.at!=b.size()||!valid(v,e))return fail(e,"player core trailing/noncanonical state");
 out=std::move(v);e.clear();return true;
}
bool validatePlayerCoreTopology(const PlayerCoreFields& v,const PlayerCoreTopology& t,std::string& e){
 if(!valid(v,e))return false;
 if(v.hour.start!=t.hourStart||v.hour.end!=t.hourEnd||v.day.start!=t.dayStart||v.day.end!=t.dayEnd)return fail(e,"player graphs differ from source factory topology");
 for(size_t i=0;i<5;++i)if(v.courses[i].entries!=t.courseEntries[i])return fail(e,"player course flags differ from source factory topology");
 e.clear();return true;
}
bool preparePlayerCore(PlayerCoreFields& out,const Bytes& b,const PlayerCoreTopology& t,const RestoreGate& g,std::string& e){
 if(!g.freshProcess||!g.paused||!g.zeroInput||!g.birthEffectsSuppressed||!g.rewardsSuppressed||!g.rngDrawsSuppressed||!g.audioVoicesSuppressed)return fail(e,"player preparation requires full fresh paused fence");
 PlayerCoreFields staged;
 if(!decodePlayerCore(b,staged,e)||!validatePlayerCoreTopology(staged,t,e))return false;
 out=std::move(staged);return true;
}
}
