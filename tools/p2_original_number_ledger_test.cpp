#include "pc_p2_original_number_ledger.h"
#include <cstdio>
#include <limits>
using namespace p2originalnumber;
using namespace p2originalresource;
namespace {
ChildOutcome pending(const std::string& catalog,unsigned uid=1){ChildOutcome c;c.identity={{catalog,uid,7,0,1},0};c.kind=ChildKind::PelletOne;c.position={1,2,3};c.velocity={4,5,6};c.facing=0.25f;c.pelletColor=1;c.attempted=true;return c;}
ContentsRecord complete(const ChildOutcome& child){ContentsRecord j;j.source=child.identity.source;j.type=child.kind==ChildKind::PelletOne?P2EggDropType::OnePellets:P2EggDropType::FivePellets;j.complete=true;j.children.push_back(child);j.children[0].born=true;return j;}
// Exercises the real engine-independent producer ABI and journal transition.
// Opaque int is a binding key, never a fake native Pellet or source authority.
class CallbackEngine final:public Engine {
public:
 Ledger& ledger;EggContents& producer;int key=0;unsigned calls=0;std::uint64_t handle=999;bool nullAllocation=false,sawPending=false;
 CallbackEngine(Ledger& l,EggContents& p):ledger(l),producer(p){}
 float randFloat()noexcept override{return .25f;}
 int randInt(int)noexcept override{return 2;}
 bool sprayMade(HoneyKind)noexcept override{return false;}
 bool mititeManagerAvailable()noexcept override{return false;}
 bool birthPellet(const ChildOutcome& c)noexcept override{
  ++calls;const auto rows=producer.snapshot();sawPending=rows.size()==1&&!rows[0].complete&&rows[0].children.size()==1&&rows[0].children[0].attempted&&!rows[0].children[0].born;
  BirthPlan plan;std::string e;if(!ledger.preflightBirth(c,37,plan,e))return false;
  return ledger.bindBirth(plan,nullAllocation?nullptr:&key,handle,e);
 }
 bool birthHoney(HoneyKind,const ChildOutcome&)noexcept override{return false;}
 bool birthMititeGroup(const ChildOutcome&)noexcept override{return false;}
};
}
int main(){
 unsigned checks=0,failures=0;auto check=[&](bool b,const char* name){++checks;if(!b){++failures;std::printf("FAIL %s\n",name);}};
 const std::string catalog(64,'a'),otherCatalog(64,'b');std::string e;Ledger l(catalog);auto c=pending(catalog);BirthPlan plan;plan.receipt.rootSourceType=999;
 check(l.preflightBirth(c,37,plan,e)&&plan.receipt.rootSourceType==37&&plan.receipt.birthPayload.identity.source.epoch==0&&l.report().receipts.empty(),"read-only direct Egg37 preflight preserves genuine epoch0");
 int pellet=0,other=0;std::uint64_t h=999,k=999;
 check(!l.bindBirth(plan,nullptr,h,e)&&h==999&&l.report().receipts.empty(),"null native allocation no binding/history/handle effect");
 check(l.bindBirth(plan,&pellet,h,e)&&l.owns(&pellet)&&l.report().liveBodies==1,"actual nonnull key binds retained payload");
 check(!l.bindBirth(plan,&other,k,e)&&k==999&&l.report().receipts.size()==1,"same logical child cannot bind twice");
 auto second=pending(catalog,2);second.kind=ChildKind::PelletFive;BirthPlan secondPlan;
 check(l.preflightBirth(second,16,secondPlan,e),"captured Qurione16 Five preflight");
 check(!l.bindBirth(secondPlan,&pellet,k,e)&&k==999&&l.report().receipts.size()==1,"same live pointer refuses second child atomically");
 auto j=complete(c);Receipt r;r.rootSourceType=999;
 check(l.query(&pellet,h,j,37,r,e)==QueryResult::Present&&r.rootSourceType==37&&!r.consumed&&r.birthPayload.position.x==1,"completed successful exact journal queried");
 r.rootSourceType=999;check(l.query(&pellet,h+1,j,37,r,e)==QueryResult::Unavailable&&r.rootSourceType==999&&l.owns(&pellet),"stale labelled handle remains unavailable rather than ordinary");
 check(l.query(&other,h,j,37,r,e)==QueryResult::Missing&&r.rootSourceType==999,"ordinary key missing and output unchanged");
 k=999;check(!l.handle(&other,k)&&k==999&&l.handle(&pellet,k)&&k==h,"nonallocating handle readers");
 auto checkJournal=[&](const ContentsRecord& bad,unsigned root,const char* name){Receipt untouched;untouched.rootSourceType=999;check(l.query(&pellet,h,bad,root,untouched,e)==QueryResult::Unavailable&&untouched.rootSourceType==999&&!l.report().receipts[0].consumed,name);};
 auto bad=j;bad.complete=false;checkJournal(bad,37,"incomplete producer journal unavailable");
 bad=j;bad.children[0].born=false;checkJournal(bad,37,"failed native birth journal unavailable");
 bad=j;bad.children[0].attempted=false;checkJournal(bad,37,"nonattempted producer journal unavailable");
 bad=j;bad.children[0].consumed=true;checkJournal(bad,37,"producer consumed without ledger commit unavailable");
 bad=j;bad.source.activation++;checkJournal(bad,37,"full parent tuple mismatch unavailable");
 bad=j;bad.children[0].identity.source.ordinal++;checkJournal(bad,37,"full child ordinal mismatch unavailable");
 bad=j;bad.children[0].identity.source.uid++;checkJournal(bad,37,"full child UID mismatch unavailable");
 bad=j;bad.children[0].identity.source.activation++;checkJournal(bad,37,"full child activation mismatch unavailable");
 bad=j;bad.children[0].identity.source.epoch++;checkJournal(bad,37,"full child epoch mismatch unavailable");
 bad=j;bad.children[0].identity.source.fingerprint=otherCatalog;checkJournal(bad,37,"crosscatalog child unavailable");
 bad=j;bad.children[0].pelletColor=2;checkJournal(bad,37,"RGB payload mismatch unavailable");
 bad=j;bad.children[0].position.x++;checkJournal(bad,37,"position payload mismatch unavailable");
 bad=j;bad.children[0].position.x=std::numeric_limits<float>::quiet_NaN();checkJournal(bad,37,"nonfinite journal payload unavailable");
 bad=j;bad.children[0].velocity.z++;checkJournal(bad,37,"velocity payload mismatch unavailable");
 bad=j;bad.children[0].facing++;checkJournal(bad,37,"facing payload mismatch unavailable");
 bad=j;bad.children[0].mititeCount=1;checkJournal(bad,37,"non-numeric payload count mismatch unavailable");
 bad=j;bad.children[0].kind=ChildKind::PelletFive;checkJournal(bad,37,"One vs Five kind mismatch unavailable");
 bad=j;bad.type=P2EggDropType::FivePellets;checkJournal(bad,37,"journal drop type mismatch unavailable");
 bad=j;bad.children.push_back(bad.children[0]);checkJournal(bad,37,"duplicate journal child unavailable");
 checkJournal(j,16,"actual root16 cannot replace retained root37");
 ConsumePlan consume;consume.handle=999;
 bad=j;bad.complete=false;
 check(!l.prepareConsume(&pellet,h,bad,37,consume,e)&&consume.handle==999,"failed consume preflight output unchanged");
 check(l.prepareConsume(&pellet,h,j,37,consume,e)&&!l.report().receipts[0].consumed,"consume prepare read-only");
 check(!l.commitConsumed(consume,j,37,e)&&!l.report().receipts[0].consumed,"commit before real producer consumption refused");
 auto consumedJournal=j;consumedJournal.children[0].consumed=true;
 bad=consumedJournal;bad.children[0].pelletColor=2;
 check(!l.commitConsumed(consume,bad,37,e)&&!l.report().receipts[0].consumed,"consumed mismatched producer payload cannot grant");
 auto tampered=consume;tampered.receipt.birthPayload.identity.source.uid++;
 check(!l.commitConsumed(tampered,consumedJournal,37,e)&&!l.report().receipts[0].consumed,"tampered consume plan refuses atomically");
 tampered=consume;tampered.receipt.birthPayload.born=true;
 check(!l.commitConsumed(tampered,consumedJournal,37,e),"pending receipt plan flags cannot be forged");
 check(l.commitConsumed(consume,consumedJournal,37,e),"completed consumed producer proof commits once");
 check(l.query(&pellet,h,consumedJournal,37,r,e)==QueryResult::Present&&r.consumed,"consumed diagnostic query agrees with journal");
 check(l.query(&pellet,h,j,37,r,e)==QueryResult::Unavailable&&l.owns(&pellet),"producer journal rollback cannot reopen consumed body");
 ConsumePlan untouchedPlan;untouchedPlan.handle=999;
 check(!l.prepareConsume(&pellet,h,consumedJournal,37,untouchedPlan,e)&&untouchedPlan.handle==999&&!l.commitConsumed(consume,consumedJournal,37,e),"duplicate consume no replay");
 check(!l.unload(e)&&!l.freshSession(otherCatalog,e),"live numeric body refuses unload/session replacement");
 check(!l.retire(&pellet,h+1,e)&&l.retire(&pellet,h,e)&&l.retire(&pellet,h,e)&&l.unload(e),"retire exact binding and duplicate cleanup harmless");
 check(l.report().receipts.size()==1&&l.report().receipts[0].consumed&&!l.bindBirth(plan,&pellet,k,e),"consumed history persists retirement/unload");
 check(l.bindBirth(secondPlan,&pellet,k,e)&&k!=h,"pool reuse fresh RAM handle and root16 retained");
 auto fiveJournal=complete(second);
 check(l.query(&pellet,k,fiveJournal,16,r,e)==QueryResult::Present&&r.rootSourceType==16&&r.birthPayload.kind==ChildKind::PelletFive,"root16 Five exact retained receipt");
 check(!l.commitConsumed(consume,consumedJournal,37,e)&&!l.retire(&pellet,h,e),"old consume/death handles cannot touch reused pool slot");
 check(l.retire(&pellet,k,e)&&l.unload(e)&&l.report().receipts.size()==2,"unconsumed retired birth prevents second body as well");
 check(!l.bindBirth(secondPlan,&other,h,e),"retired unconsumed logical birth history persists");
 check(l.freshSession(otherCatalog,e)&&l.report().receipts.empty(),"explicit fresh catalog resets dead retained history");
 BirthPlan untouched;untouched.receipt.rootSourceType=999;
 check(!l.preflightBirth(c,37,untouched,e)&&untouched.receipt.rootSourceType==999,"old catalog source cannot enter fresh ledger");
 auto fresh=pending(otherCatalog,1);check(l.preflightBirth(fresh,37,plan,e)&&l.bindBirth(plan,&pellet,h,e)&&h>k,"fresh session never rewinds RAM handles");

 Ledger validation(catalog);
 auto checkBirth=[&](ChildOutcome badChild,unsigned root,const char* name){BirthPlan output;output.receipt.rootSourceType=999;check(!validation.preflightBirth(badChild,root,output,e)&&output.receipt.rootSourceType==999&&validation.report().receipts.empty(),name);};
 checkBirth(c,26,"unqualified root source type rejected");
 auto invalid=c;invalid.attempted=false;checkBirth(invalid,37,"birth requires pending actual attempted callback");
 invalid=c;invalid.born=true;checkBirth(invalid,37,"already born producer entry not a birth callback");
 invalid=c;invalid.consumed=true;checkBirth(invalid,37,"consumed entry not a birth callback");
 invalid=c;invalid.identity.slot=1;checkBirth(invalid,37,"numeric Egg childslot must0");
 invalid=c;invalid.identity.ancestry={{EmitterKind::EggMitite,0,0}};checkBirth(invalid,37,"emitted-member ancestry is not admitted direct Egg number");
 invalid=c;invalid.identity.source.activation=0;checkBirth(invalid,37,"full producer activation required");
 invalid=c;invalid.identity.source.fingerprint="bad";checkBirth(invalid,37,"full catalog fingerprint required");
 invalid=c;invalid.kind=ChildKind::Nectar;checkBirth(invalid,37,"Honey not numeric body");
 invalid=c;invalid.pelletColor=3;checkBirth(invalid,37,"numeric RGB payload bounded");
 invalid=c;invalid.mititeCount=10;checkBirth(invalid,37,"no fake group numeric payload");
 invalid=c;invalid.position.y=std::numeric_limits<float>::quiet_NaN();checkBirth(invalid,37,"position finite");
 invalid=c;invalid.velocity.x=std::numeric_limits<float>::infinity();checkBirth(invalid,37,"velocity finite");
 invalid=c;invalid.facing=-std::numeric_limits<float>::infinity();checkBirth(invalid,37,"facing finite");
 Ledger invalidCatalog("bad");check(!invalidCatalog.preflightBirth(c,37,plan,e),"invalid configured catalog fails closed");

 // Actual pure producer invokes pending callback before publishing complete
 // born=true. Only its real consume journal transition authorizes ledgercommit.
 Ledger producerLedger(catalog);EggContents producer;CallbackEngine callback(producerLedger,producer);P2EggConfig config;config.forcedDropType=1;ContentsRecord produced;
 SourceIdentity identity{catalog,100,0,0,1};
 check(producer.generate(identity,config,{0,0,0},callback,produced,e)&&callback.calls==1&&callback.sawPending&&produced.complete&&produced.children[0].born,"real pure producer pendingcallback->completed born journal");
 check(producerLedger.prepareConsume(&callback.key,callback.handle,produced,37,consume,e),"actual completed producer receipt prepare");
 check(producer.consume(produced.children[0].identity,e),"caller invokes actual producer consumption once");
 const auto producerRows=producer.snapshot();
 check(producerLedger.commitConsumed(consume,producerRows[0],37,e)&&producerLedger.query(&callback.key,callback.handle,producerRows[0],37,r,e)==QueryResult::Present&&r.consumed,"actual producer transition agrees with retained consumed receipt");
 Ledger failedLedger(catalog);EggContents failedProducer;CallbackEngine failed(failedLedger,failedProducer);failed.nullAllocation=true;
 check(failedProducer.generate(identity,config,{0,0,0},failed,produced,e)&&failed.calls==1&&!produced.children[0].born&&failed.handle==999&&failedLedger.report().receipts.empty(),"producer-owned null allocation publishes failed once with no ledger birth");
 check(failedProducer.generate(identity,config,{0,0,0},failed,produced,e)&&failed.calls==1,"producer journal duplicate never retries failed pool allocation");

 Ledger bounded(catalog);bool filled=true;
 check(bounded.remainingCapacity()==retainedLimit,"fresh retained capacity is available without copying report");
 for(std::size_t i=0;i<retainedLimit;++i){auto entry=pending(catalog,static_cast<unsigned>(1000+i));if(!bounded.preflightBirth(entry,37,plan,e)||!bounded.bindBirth(plan,&other,k,e)||!bounded.retire(&other,k,e)){filled=false;break;}}
 check(filled&&bounded.unload(e),"retained metadata includes all retired births to65536 bound");
 check(bounded.remainingCapacity()==0,"retired receipts retain capacity until explicit fresh session");
 check(!bounded.preflightBirth(pending(catalog,999999),37,untouched,e)&&untouched.receipt.rootSourceType==999,"retained metadata overflow refuses before native allocation");
 static_assert(noexcept(l.owns(nullptr)),"persistent owned reader noexcept");static_assert(noexcept(l.handle(nullptr,h)),"persistent handle reader noexcept");
 std::printf("original_number_ledger checks=%u failures=%u engine=0 source_authority=caller producer_api=actual codec=0\n",checks,failures);return failures?1:0;
}
