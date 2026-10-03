#include "pc_p2_retail_starting_piki.h"
#include "pc_p2_retail_start.h"
#include "netplay/pc_netplay_sha256.h"
#include <sstream>
#include <locale>
#include <set>
namespace p2retail { namespace {
bool fail(std::string& e,const char* s){e=s;return false;}
bool digest(const std::string& s){return s.size()==64&&s.find_first_not_of("0123456789abcdef")==std::string::npos;}
std::string hash(const std::string& s){unsigned char d[32];pc_netplay_sha::sha256(s.data(),s.size(),d);std::string h;for(auto c:d){h+="0123456789abcdef"[c>>4];h+="0123456789abcdef"[c&15];}return h;}
}
bool parseStartingPiki(const std::string& b,const std::string& mb,const std::string& provenance,StartingPikiInputs& out,std::string& e){
 if(b.empty()||b.size()>65536||provenance.empty()||provenance.size()>16384)return fail(e,"starting Piki input size");
 StartingPikiInputs next;std::istringstream in(b);in.imbue(std::locale::classic());std::string tag,campaign,catalog;unsigned count=0;
 if(!(in>>tag>>count)||tag!="P2_RETAIL_STARTING_PIKI_1"||count!=20)return fail(e,"starting Piki framing/count");
 auto field=[&](const char* name,std::string& value){return bool(in>>tag>>value)&&tag==name&&digest(value);};
 if(!field("campaign",campaign)||!(in>>tag>>next.cave>>next.floor)||tag!="floor"||next.cave!="tutorial_1"||(next.floor!=1&&next.floor!=2)||
    !field("plan",next.planSha)||!field("start",next.startSha)||!field("baseline",next.baselineSha)||!field("provenance",next.provenanceSha)||
    !field("manifest",next.manifestSha)||!field("catalog",catalog))return fail(e,"starting Piki bindings");
 unsigned members=0;std::string color;
 if(!(in>>tag>>color>>members)||tag!="bank"||color!="red"||members!=364)return fail(e,"starting Piki bank version/count");
 std::string previous;
 for(unsigned i=0;i<members;++i){std::string role,sha;if(!(in>>role>>sha)||!digest(sha))return fail(e,"starting Piki bank entry");
  const std::string prefix="p2-original/piki-bodies/red/";
  if(role.compare(0,prefix.size(),prefix)||role.size()<=prefix.size()||role.size()>160||(!previous.empty()&&role<=previous))return fail(e,"starting Piki bank role/order");
  const auto name=role.substr(prefix.size());if(name=="."||name==".."||name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-.")!=std::string::npos)return fail(e,"starting Piki bank filename");
  next.bank.emplace(role,sha);previous=role;
 }
 if(!(in>>tag)||tag!="END"||(in>>tag)||!next.bank.count("p2-original/piki-bodies/red/bank.txt")||!next.bank.count("p2-original/piki-bodies/red/motion-registry.json"))return fail(e,"starting Piki bank closure");
 const auto red=next.bank.find("p2-original/piki-bodies/red/red.bmd");if(red==next.bank.end()||red->second!="4ad910ab6dec0722180c117539d3467b705b94dbe322eae40809e3345a44e358")return fail(e,"starting Piki genuine red model binding");
 if(hash(mb)!=next.manifestSha||hash(provenance)!=next.provenanceSha||!p2original::readPikiManifest(mb,next.manifest,e)||next.manifest.campaign!=campaign||next.manifest.catalog!=catalog||next.manifest.rows.size()!=20)return fail(e,"starting Piki manifest/provenance digest");
 const auto expectedProvenance="P2_DEVELOPMENT_STARTUP20_1\nbaseline "+next.baselineSha+"\nplan "+next.planSha+"\nstart "+next.startSha+"\nfixture 20 red Free-Bore\ngrid 2 10 8 -36 32 30\nplacement absolute-map-start-xz-slot-y-plus30\nground validate-dry-support-refuse-no-relocation\nclaim engineered-not-story-acquisition\n";
 if(provenance!=expectedProvenance)return fail(e,"starting Piki provenance policy");
 std::set<std::string> keys;for(unsigned i=0;i<20;++i)keys.insert("development/startup20.txt#"+std::to_string(i));
 for(const auto& row:next.manifest.rows){if(!keys.erase(row.sourceKey)||row.sourceSha!=next.provenanceSha||row.spawn.count!=1||row.spawn.species!=1||row.spawn.wildParameter!=0||row.reserved!=0||row.resurrectionDays!=0||row.dayLimit!=-1||row.recordVersion!="v0.3"||row.objectVersion!="0001"||row.spawn.offset!=std::array<float,3>{})return fail(e,"starting Piki authored roster");}
 if(!keys.empty())return fail(e,"starting Piki missing identity");out=std::move(next);e.clear();return true;
}
bool validateStartingPikiPlacement(const StartingPikiInputs& inputs,const SourceStart& start,std::string& e){
 if(inputs.manifest.rows.size()!=20)return fail(e,"starting Piki placement count");
 std::set<unsigned> seen;
 for(const auto& row:inputs.manifest.rows){const auto prefix=std::string("development/startup20.txt#");if(row.sourceKey.compare(0,prefix.size(),prefix))return fail(e,"starting Piki placement key");unsigned i=0;const auto suffix=row.sourceKey.substr(prefix.size());if(suffix.empty()||suffix.size()>2||suffix.find_first_not_of("0123456789")!=std::string::npos)return fail(e,"starting Piki placement index");for(char c:suffix)i=i*10+unsigned(c-'0');if(i>=20||suffix!=std::to_string(i)||!seen.insert(i).second)return fail(e,"starting Piki placement identity");const std::array<float,3> expected={float(start.mapStart[0]-36.0+double(i%10)*8.0),float(start.slotPosition[1]+30.0),float(start.mapStart[2]+32.0+double(i/10)*8.0)};if(row.spawn.position!=expected)return fail(e,"starting Piki authored absolute position differs");}
 e.clear();return true;
}
}
