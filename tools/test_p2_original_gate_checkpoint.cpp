#include "pc_p2_original_gate_checkpoint.h"
#include "netplay/pc_netplay_sha256.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
using namespace p2original;
namespace {
unsigned checks=0;
void check(bool good,const char* name){++checks;if(!good){std::fprintf(stderr,"FAIL %s\n",name);std::exit(1);}}
void rehash(std::vector<std::uint8_t>& bytes){pc_netplay_sha::sha256(bytes.data(),bytes.size()-32,bytes.data()+bytes.size()-32);}
}
int main(int argc,char** argv){
 check(argc==2,"actual source manifest required");std::string error;std::vector<GateRecord> rows;
 check(readGates(argv[1],rows,error)&&rows.size()==3,"actual three Tutorial gate sources");
 std::ifstream file(argv[1],std::ios::binary);const std::string raw((std::istreambuf_iterator<char>(file)),{});
 std::vector<GateRecord> parsed;
 check(readGatesFromBytes(raw,parsed,error)&&parsed.size()==rows.size()
    &&gateDigest(parsed.front())==gateDigest(rows.front()),"same verified source buffer parser");
 auto old=parsed;
 check(!readGatesFromBytes(raw+"extra",parsed,error)&&parsed.size()==old.size()
    &&gateDigest(parsed.front())==gateDigest(old.front()),"invalid buffered source refuses without mutation");
 check(!readGatesFromBytes(std::string(4*1024*1024+1,'x'),parsed,error),"oversize buffered source refuses");
 // Codec-only counter probes. Production capture must obtain all stamps from
 // the real scene registry; these values never become an engine identity.
 const std::string campaign(64,'c');std::vector<GateCheckpointEntry> entries;
 for(unsigned i=0;i<rows.size();++i){GateCheckpointEntry value;
  value.identity={campaign,rows[i].uid,0,0x8000000000000001ULL+i,0xf000000000000007ULL+i};
  value.order=(i+2)%3;value.state=gateInitial(rows[i]);entries.push_back(value);
 }
 gateDamage(entries[1].state,5);
 gateDamage(entries[2].state,rows[2].segmentLife+1);gateExec(entries[2].state);
 entries[2].state.animationFrame=12.5f;gateDamage(entries[2].state,3);
 std::vector<std::uint8_t> bytes;check(gateCheckpointExport(campaign,rows,entries,bytes,error),"source-bound whole gate section export");
 check(bytes.size()==572,"bounded three-source section size");std::vector<GateCheckpointEntry> restored;
 check(gateCheckpointImport(campaign,rows,bytes,restored,error)&&restored.size()==3,"source-bound section import");
 for(unsigned order=0;order<3;++order){auto actual=std::find_if(entries.begin(),entries.end(),[&](const auto& v){return v.order==order;});
  check(restored[order].order==order&&restored[order].identity==actual->identity,"physical order distinct from exact source incarnation");
  check(restored[order].state.phase==actual->state.phase&&restored[order].state.health==actual->state.health
    &&restored[order].state.damage==actual->state.damage&&restored[order].state.animationFrame==actual->state.animationFrame,"pending HP/damage/phase/frame preserved");
 }
 auto prior=restored;auto refuses=[&](const auto& source,const auto& payload){
  check(!gateCheckpointImport(campaign,source,payload,restored,error),"malformed gate section refuses");
  check(restored.size()==prior.size()&&restored.front().identity==prior.front().identity
    &&restored.back().state.animationFrame==prior.back().state.animationFrame,"refusal preserves prior output");
 };
 for(std::size_t n=0;n<bytes.size();++n)refuses(rows,std::vector<std::uint8_t>(bytes.begin(),bytes.begin()+n));
 auto bad=bytes;bad.back()^=1;refuses(rows,bad);
 bad=bytes;bad.push_back(0);refuses(rows,bad);
 bad=bytes;bad[0]='X';rehash(bad);refuses(rows,bad);
 bad=bytes;bad[4]='a';rehash(bad);refuses(rows,bad);
 bad=bytes;bad[68]=4;rehash(bad);refuses(rows,bad);
 bad=bytes;bad[72+4]=1;rehash(bad);refuses(rows,bad); // source ordinal
 bad=bytes;std::memset(bad.data()+72+16,0,8);rehash(bad);refuses(rows,bad); // activation
 bad=bytes;bad[72+24]=3;rehash(bad);refuses(rows,bad); // physical order
 bad=bytes;bad[72+156+24]=0;rehash(bad);refuses(rows,bad); // duplicate order
 auto changed=rows;changed.front().segmentLife+=1;refuses(changed,bytes);
 changed=rows;changed.front().position[0]+=1;refuses(changed,bytes);
 auto incomplete=entries;incomplete.pop_back();auto retained=bytes;
 check(!gateCheckpointExport(campaign,rows,incomplete,bytes,error)&&bytes==retained,"missing actual source census export refuses atomically");
 incomplete=entries;incomplete[0].identity=entries[1].identity;
 check(!gateCheckpointExport(campaign,rows,incomplete,bytes,error)&&bytes==retained,"duplicate source incarnation refuses atomically");
 std::printf("PASS ORIGINAL_GATE_CHECKPOINT controls=%u literal_rows=3 native_allocations=0 source_stamp_fixture_only=1\n",checks);
}
