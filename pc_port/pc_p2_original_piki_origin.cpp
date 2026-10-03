#include "pc_p2_original_piki_origin.h"
#include "pc_p2_original_catalog.h"
#include <map>
namespace {
std::string fingerprint;
std::map<std::string,OriginalPikiSource> sources;
struct Body {OriginalPikiOrigin origin;OriginalPikiBodyState state;bool hasState=false;};
std::map<const Piki*,Body> bodies;
bool fp(const std::string& f){if(f.size()!=64)return false;for(char c:f)if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return false;return true;}
bool key(const std::string& s){
 if(s.empty()||s.size()>256)return false;
 auto hash=s.rfind('#');if(hash==std::string::npos||hash==0||hash+1==s.size())return false;
 unsigned index=0;for(std::size_t i=hash+1;i<s.size();++i){unsigned char c=s[i];if(c<'0'||c>'9')return false;index=index*10+(c-'0');if(index>65535)return false;}
 if(s.substr(hash+1)!=std::to_string(index))return false;
 std::size_t start=0;unsigned components=0;
 for(std::size_t i=0;i<=hash;++i)if(i==hash||s[i]=='/'){
  auto part=s.substr(start,i-start);if(part.empty()||part=="."||part=="..")return false;
  for(unsigned char c:part)if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c=='.'))return false;
  ++components;start=i+1;
 }
 return components>=2;
}
bool same(const OriginalPikiOrigin&a,const OriginalPikiOrigin&b){return a.sourceKey==b.sourceKey&&a.recordUid==b.recordUid&&a.attempt==b.attempt&&a.activation==b.activation&&a.catalogFingerprint==b.catalogFingerprint;}
bool valid(const OriginalPikiOrigin& o){auto i=sources.find(o.sourceKey);return !fingerprint.empty()&&o.catalogFingerprint==fingerprint&&o.activation&&i!=sources.end()&&i->second.uid==o.recordUid&&o.attempt<i->second.count;}
bool stateValid(const OriginalPikiBodyState& s){return s.species<=5&&(!s.wild||s.wasWild);}
bool stateSame(const OriginalPikiBodyState&a,const OriginalPikiBodyState&b){return a.species==b.species&&a.wild==b.wild&&a.wasWild==b.wasWild;}
bool attach(Piki* p,const OriginalPikiOrigin& o,const OriginalPikiBodyState* state=nullptr){
 if(!p||!valid(o)||bodies.count(p)||(state&&(!stateValid(*state)||sources.at(o.sourceKey).species!=state->species)))return false;
 for(const auto& body:bodies)if(same(body.second.origin,o))return false;
 Body value;value.origin=o;if(state){value.state=*state;value.hasState=true;}
 auto inserted=bodies.emplace(p,std::move(value));
 try {
  if(pc_p2_cave_campaign_party_associate_birth(p,o.sourceKey.c_str(),o.recordUid,o.attempt,o.activation,o.catalogFingerprint.c_str()))return true;
 } catch(...) {bodies.erase(inserted.first);throw;}
 bodies.erase(inserted.first);return false;
}
}
bool pc_p2_original_piki_origin_install(const std::string& f,const std::vector<OriginalPikiSource>& rows,std::string& e){
 if(!bodies.empty()||!fp(f)||rows.empty()||rows.size()>65536){e="original Piki authority requires empty old scene and a full bound catalog";return false;}
 std::map<std::string,OriginalPikiSource> next;std::map<std::uint32_t,bool> uids;
 for(const auto& row:rows)if(!key(row.sourceKey)||row.count>65535||row.species>5||row.uid!=p2original::originalGeneratorUid(row.sourceKey)||!uids.emplace(row.uid,true).second||!next.emplace(row.sourceKey,row).second){e="invalid/duplicate original Piki source or count";return false;}
 std::string nextFingerprint=f;sources.swap(next);fingerprint.swap(nextFingerprint);e.clear();return true;
}
bool pc_p2_original_piki_origin_associate_birth(Piki* p,const OriginalPikiOrigin& o){return attach(p,o);}
bool pc_p2_original_piki_origin_query(const Piki* p,OriginalPikiOrigin& out){auto i=bodies.find(p);if(i==bodies.end())return false;OriginalPikiOrigin next=i->second.origin;out=std::move(next);return true;}
bool pc_p2_original_piki_origin_restore_saved(Piki* p,const OriginalPikiOrigin& o){
 if(!p||!valid(o)||bodies.count(p))return false;
 std::uint64_t generation=0;std::uint8_t sha[32]={};
 if(!pc_p2_cave_campaign_survivor_permit(o.sourceKey,o.recordUid,o.attempt,o.activation,o.catalogFingerprint,&generation,sha)||!generation)return false;
 bool nonzero=false;for(auto byte:sha)nonzero|=byte!=0;if(!nonzero)return false;
 // No cached live pointer or invented generator; exactly this selected SAVE's
 // survivor ticket authorizes association to the already-created fresh body.
 return attach(p,o);
}
void pc_p2_original_piki_origin_forget(Piki* p){bodies.erase(p);}

bool pc_p2_original_piki_body_associate_birth(Piki* p,const OriginalPikiBody& body){
 // Fresh source setZikatu(true/false) never produces a previously-recruited body.
 if(body.state.wild!=body.state.wasWild)return false;
 return attach(p,body.origin,&body.state);
}
bool pc_p2_original_piki_body_query(const Piki* p,OriginalPikiBody& out){
 auto i=bodies.find(p);if(i==bodies.end()||!i->second.hasState)return false;
 OriginalPikiBody next;next.origin=i->second.origin;next.state=i->second.state;
 out=std::move(next);return true;
}
bool pc_p2_original_piki_body_restore_saved(Piki* p,const OriginalPikiBody& body){
 if(!p||!valid(body.origin)||!stateValid(body.state)||sources.at(body.origin.sourceKey).species!=body.state.species||bodies.count(p))return false;
 OriginalPikiBodyState selected;std::uint64_t generation=0;std::uint8_t sha[32]={};
 const auto& o=body.origin;
 if(!pc_p2_cave_campaign_survivor_body(o.sourceKey,o.recordUid,o.attempt,o.activation,
     o.catalogFingerprint,selected,&generation,sha)||!generation
     ||!stateValid(selected)||!stateSame(selected,body.state))return false;
 bool nonzero=false;for(auto byte:sha)nonzero|=byte!=0;if(!nonzero)return false;
 return attach(p,o,&body.state);
}
bool pc_p2_original_piki_body_recruited(Piki* p){
 auto i=bodies.find(p);if(i==bodies.end()||!i->second.hasState
     ||i->second.state.species>2||!i->second.state.wild)return false;
 i->second.state.wild=false;return true;
}
