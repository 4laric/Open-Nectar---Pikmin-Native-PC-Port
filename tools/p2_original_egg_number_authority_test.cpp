#include "pc_p2_original_egg_number_journal.h"
#include <cassert>
#include <iostream>
#include <map>
#include <stdexcept>
using namespace p2originalresource;
struct Roots: NumberRootAuthority {
 std::map<SourceIdentity,unsigned> admitted;bool available=true;
 bool resolve(const SourceIdentity& id,unsigned& out,std::string& e)const override{
  auto i=admitted.find(id);if(!available||i==admitted.end()){e="selected source is unavailable";return false;}
  out=i->second;e.clear();return true;
 }
};
struct Birth:Engine {
 NumberJournalAuthority& authority;bool born=true,expectClosureRejection=false;unsigned expected;int draws=0,callbacks=0;
 Birth(NumberJournalAuthority& a,unsigned root):authority(a),expected(root){}
 float randFloat()noexcept override{++draws;return .2f;}
 int randInt(int n)noexcept override{assert(n==3);++draws;return 1;}
 bool sprayMade(HoneyKind)noexcept override{return false;}
 bool mititeManagerAvailable()noexcept override{return false;}
 bool birthHoney(HoneyKind,const ChildOutcome&)noexcept override{assert(false);return false;}
 bool birthMititeGroup(const ChildOutcome&)noexcept override{assert(false);return false;}
 bool birthPellet(const ChildOutcome& child)noexcept override{
  ++callbacks;unsigned type=99;ContentsRecord record;record.source.uid=999;std::string e;
  if(expectClosureRejection){assert(!authority.pending(child,type,record,e)&&type==99&&record.source.uid==999);return false;}
  assert(authority.pending(child,type,record,e)&&type==expected&&!record.complete&&!record.children.front().born);
  auto reject=[&](ChildOutcome bad){type=99;ContentsRecord unchanged;unchanged.source.uid=999;
   assert(!authority.pending(bad,type,unchanged,e)&&type==99&&unchanged.source.uid==999);};
  auto bad=child;bad.identity.source.epoch++;reject(bad);bad=child;bad.identity.source.activation++;reject(bad);
  bad=child;bad.identity.source.ordinal++;reject(bad);bad=child;bad.identity.source.uid++;reject(bad);
  bad=child;bad.identity.source.fingerprint[0]='b';reject(bad);bad=child;bad.identity.slot=1;reject(bad);
  bad=child;bad.identity.ancestry={{EmitterKind::EggMitite,0,0}};reject(bad);
  bad=child;bad.position.y++;reject(bad);bad=child;bad.velocity.y++;reject(bad);
  bad=child;bad.pelletColor=2;reject(bad);bad=child;bad.kind=ChildKind::PelletFive;reject(bad);
  bad=child;bad.facing=1;reject(bad);
  if(exactNumberFloat(child.position.x,0.0f)){bad=child;bad.position.x=-0.0f;reject(bad);}
  bad=child;bad.velocity.x=-0.0f;reject(bad);bad=child;bad.facing=-0.0f;reject(bad);bad=child;bad.mititeCount=10;reject(bad);
  bad=child;bad.attempted=false;reject(bad);bad=child;bad.born=true;reject(bad);bad=child;bad.consumed=true;reject(bad);
  type=99;ContentsRecord unchanged;unchanged.source.uid=999;
  assert(!authority.completed(child.identity,type,unchanged,e)&&type==99&&unchanged.source.uid==999);
  assert(!authority.consumeAccepted(child.identity,e)); // pending failure cannot mutate journal
  return born;
 }
};
int main(){
 assert(!exactNumberFloat(0.0f,-0.0f)); // journal callback policy; native object separately verifies actual physical origin
 EggContents journal;Roots roots;SourceIdentity source{std::string(64,'a'),0x52000100,0,0,1};roots.admitted[source]=37;
 bool capacity=true;int admissions=0;NumberJournalAuthority authority(journal,roots,[&](const ContentsRequirements& r,std::string& e){
  ++admissions;assert(r.pelletOne&&!r.pelletFive);if(!capacity){e="retained limit";return false;}e.clear();return true;});
 P2EggConfig config;config.forcedDropType=1;std::string e;Birth engine(authority,37);ContentsRecord result;
 capacity=false;assert(!authority.begin(source,37,config,{},e)&&!authority.active()&&!journal.find(source)&&engine.draws==0);
 capacity=true;assert(!authority.begin(source,16,config,{},e)&&!authority.active());
 assert(authority.begin(source,37,config,{},e));const int before=admissions;
 assert(!authority.begin(source,37,config,{},e)&&admissions==before&&authority.active());
 assert(journal.generate(source,config,{1,2,3},engine,result,e)&&engine.draws==2&&engine.callbacks==1);
 authority.end();assert(!authority.active());unsigned type=99;ContentsRecord completed;
 assert(authority.completed({source,0},type,completed,e)&&type==37&&completed.complete&&completed.children.front().born);
 // No pending scope survives parent generation.
 assert(!authority.pending(completed.children.front(),type,result,e));
 roots.available=false;assert(!authority.consumeAccepted({source,0},e)&&!journal.find(source)->children.front().consumed);
 roots.available=true;auto wrong=source;wrong.activation++;assert(!authority.consumeAccepted({wrong,0},e));
 assert(authority.consumeAccepted({source,0},e)&&journal.find(source)->children.front().consumed);
 assert(authority.consumeAccepted({source,0},e)); // idempotent publication, not a new grant
 SourceIdentity captured=source;captured.uid++;captured.ordinal=3;roots.admitted[captured]=16;
 Birth nullEngine(authority,16);nullEngine.born=false;assert(authority.begin(captured,16,config,{},e));
 assert(journal.generate(captured,config,{},nullEngine,result,e));authority.end();
 type=99;completed.source.uid=999;assert(!authority.completed({captured,0},type,completed,e)&&type==99&&completed.source.uid==999);
 assert(!authority.consumeAccepted({captured,0},e)&&!journal.find(captured)->children.front().consumed);
 assert(!authority.begin(captured,16,config,{},e)); // durable failed attempt is not retried
 SourceIdentity successful=captured;successful.ordinal++;roots.admitted[successful]=16;
 Birth carriedEngine(authority,16);assert(authority.begin(successful,16,config,{},e));
 assert(journal.generate(successful,config,{},carriedEngine,result,e));authority.end();
 assert(authority.completed({successful,0},type,completed,e)&&type==16&&completed.complete);
 assert(authority.consumeAccepted({successful,0},e)&&journal.find(successful)->children.front().consumed);
 SourceIdentity mismatch=successful;mismatch.ordinal++;roots.admitted[mismatch]=16;
 Birth mismatchEngine(authority,16);mismatchEngine.expectClosureRejection=true;
 assert(authority.begin(mismatch,16,config,{},e));auto five=config;five.forcedDropType=2;
 assert(journal.generate(mismatch,five,{},mismatchEngine,result,e)&&!result.children.front().born);authority.end();
 assert(!authority.completed({mismatch,0},type,completed,e));
 SourceIdentity throwing=mismatch;throwing.ordinal++;roots.admitted[throwing]=16;
 NumberJournalAuthority throws(journal,roots,[](const ContentsRequirements&,std::string&)->bool{throw std::runtime_error("preflight");});
 try{throws.begin(throwing,16,config,{},e);assert(false);}catch(const std::runtime_error&){}
 assert(!throws.active()&&!journal.find(throwing));
 NumberJournalAuthority missing(journal,roots,{});assert(!missing.begin(throwing,16,config,{},e));
 std::cout<<"P2_ORIGINAL_EGG_NUMBER_AUTHORITY_PASS\n";
}
