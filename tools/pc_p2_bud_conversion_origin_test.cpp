#include "pc_p2_bud_conversion_origin.h"
#include "pc_p2_bud_conversion_producer.h"
#include "pc_p2_source_body.h"
#include "pc_p2_original_piki_recruit.h"
#include "pc_p2_original_progress.h"
#include "pc_p2_original_source_uid.h"
#include <cstdio>
#include <cstdlib>
#include <new>
// Opaque pointers are pure-control identities, never dereferenced/native actors.
class Pom {}; class Piki {}; class PikiHeadItem {};
bool pc_p2_cave_campaign_party_associate_birth(Piki*,const char*,std::uint32_t,std::uint32_t,std::uint64_t,const char*){return true;}
bool pc_p2_cave_campaign_survivor_permit(const std::string&,std::uint32_t,std::uint32_t,std::uint64_t,const std::string&,std::uint64_t*,std::uint8_t[32]){return false;}
bool pc_p2_cave_campaign_survivor_body(const std::string&,std::uint32_t,std::uint32_t,std::uint64_t,const std::string&,OriginalPikiBodyState&,std::uint64_t*,std::uint8_t[32]){return false;}
static bool denyAllocation=false;
void* operator new(std::size_t n){if(denyAllocation)throw std::bad_alloc();if(void* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}
using namespace p2budorigin;
static int checks=0;
static void check(bool ok,const char* what){++checks;if(!ok){std::fprintf(stderr,"FAIL %d: %s\n",checks,what);std::exit(1);}}
static std::string hash(char c){return std::string(64,c);}
static unsigned headColour(const PikiHeadItem*)noexcept{return 3;}
static unsigned failures=0;
static void fatal(const char*)noexcept{++failures;}
struct MockAuthority final:Authority {
 p2originalcheckpoint::Proof proof;bool current=true,allow=true;mutable unsigned recordReads=0;unsigned donorSpecies=1;std::string donorKey="party:1";
 MockAuthority(){proof.generation=1;proof.sha[0]=42;}
 bool selected(p2originalcheckpoint::Proof& out,std::string&)const override{out=proof;return current;}
 bool context(std::string& c,std::string& s,std::string&)const override{c=hash('a');s=hash('b');return allow;}
 bool bud(const Pom*,unsigned,const p2original::InstanceIdentity&,FloorIdentity& f,std::string&)const override{f={hash('a'),hash('b'),"forest_1","visit:1",hash('c'),hash('d'),hash('e'),"bud:1",1,2,0,1,7};return allow;}
 bool donor(const Piki*,Donor& d,std::string&)const override{d={DonorKind::Party,hash('a'),donorKey,donorSpecies};return allow;}
 bool record(const Record&,std::string&)const override{++recordReads;return allow;}
 bool stillCurrent(const p2originalcheckpoint::Proof& p)const noexcept override{return current&&p==proof;}
 bool saved(const Snapshot&,const p2originalcheckpoint::Proof& p,std::string&)const override{return allow&&p==proof;}
};
int main(){
 MockAuthority a;Registry r;std::string e;Pom pom;Piki donor,body;PikiHeadItem head,head2;
 p2original::InstanceIdentity bud{hash('f'),19,0,1,9};
 check(r.bind(a,a.proof,e),"bind explicit mock selected proof (not native acceptance)");
 Snapshot empty;check(r.snapshot(empty,e)&&empty.emissions.empty(),"empty bound snapshot has context");
 std::string emptyBytes;check(encodeSnapshot(empty,emptyBytes,e),"empty family canonical framing");
 PendingEmission p;check(r.prepare(&pom,1,bud,&donor,3,false,p,e)&&p.ordinal()==1,"reserve first head before donor consumption");
 Record untouched;check(!r.head(&head,untouched),"reservation does not publish head");
 Snapshot before;check(r.snapshot(before,e)&&before.emissions.empty(),"reservation has no durable donor tombstone");
 const char* reason=nullptr;denyAllocation=true;bool adopted=r.adopt(std::move(p),&head,1,3,false,reason);denyAllocation=false;
 check(adopted&&!reason,"head callback adoption allocates nothing");
 Record first;check(r.head(&head,first)&&first.identity.emission==1,"physical head has typed identity");
 auto forged=first;forged.donor.logical="party:forged";check(!r.admitted(forged,e),"valid but foreign donor payload is not a live registry record");
 std::string bytes;check(encode(first,bytes,e),"record codec");Record decoded;check(decode(bytes,decoded,e)&&decoded.identity==first.identity,"record roundtrip");
 auto sentinel=decoded;check(!decode(bytes+"x",decoded,e)&&decoded.identity==sentinel.identity,"trailing bytes leave output unchanged");
 check(!decode(bytes.substr(0,bytes.size()-1),decoded,e),"missing canonical newline refused");
 Record bad=first;bad.species=1;check(!encode(bad,bytes,e),"source6 cannot silently become RGB");
 bad=first;bad.refunded=true;check(!encode(bad,bytes,e),"refund must agree actual donor colour");
 bad=first;bad.donor.kind=DonorKind(4);check(!encode(bad,bytes,e),"unknown donor kind refused");
 bad=first;bad.identity.floor.activation=0;check(!encode(bad,bytes,e),"zero floor activation refused");
 bad=first;bad.identity.bud.generator=0;check(!encode(bad,bytes,e),"actual bud UID required");
 PendingEmission replay;check(!r.prepare(&pom,1,bud,&donor,3,false,replay,e),"consumed canonical donor cannot replay");
 PendingEmission transfer;check(!r.prepareTransfer(&head,1,transfer,e),"wrong physical colour cannot pluck");
 check(r.prepareTransfer(&head,3,transfer,e),"typed head transfer preallocates");
 denyAllocation=true;bool bound=r.adoptBody(std::move(transfer),&body,reason);denyAllocation=false;
 check(bound&&!reason,"body adoption allocates nothing");Record savedBody;check(!r.head(&head,savedBody)&&r.body(&body,savedBody)&&savedBody.identity==first.identity,"Head-to-pluck Body preserves full identity");
 r.forget(&head);check(r.body(&body,savedBody),"old head kill cannot retire transferred body");
 Snapshot live;auto reads=a.recordReads;check(r.snapshot(live,e)&&a.recordReads>reads,"snapshot rechecks each live selected family record");
 std::string ledger;check(encodeSnapshot(live,ledger,e),"body ledger codec");Snapshot roundtrip;check(decodeSnapshot(ledger,roundtrip,e)&&roundtrip.emissions[0].lifecycle==Lifecycle::Body,"body lifecycle roundtrip");
 auto gap=live;gap.emissions[0].record.identity.emission=2;check(!encodeSnapshot(gap,bytes,e),"emission gaps refused");
 auto duplicate=live;duplicate.emissions.push_back(duplicate.emissions[0]);check(!encodeSnapshot(duplicate,bytes,e),"duplicate identity/donor refused");
 a.allow=false;Snapshot unchanged=live;check(!r.snapshot(unchanged,e)&&unchanged.emissions.size()==1,"refused family leaves snapshot unchanged");a.allow=true;
 a.current=false;check(r.body(&body,savedBody),"expired authority retains typed label");check(!r.admitted(savedBody,e),"expired label cannot fall back to ordinary admission");a.current=true;
 PendingEmission refund;a.donorSpecies=3;a.donorKey="party:2";check(r.prepare(&pom,1,bud,&donor,3,true,refund,e)&&refund.ordinal()==2,"same-colour refund reserves real second emission");
 check(!r.adopt(std::move(refund),&head2,1,3,true,reason)&&refund.ready(),"wrong core ordinal preserves reservation");
 check(r.adopt(std::move(refund),&head2,2,3,true,reason)&&r.head(&head2,decoded)&&decoded.refunded,"refunded conversion still emits actual head");
 check(r.snapshot(live,e)&&live.emissions.size()==2,"refunded output is in durable history");
 a.proof.generation=2;a.proof.sha[0]=43;check(!r.snapshot(unchanged,e),"old proof invalid after SAVE");
 check(r.adoptSelected(a,live,a.proof,e)&&r.snapshot(unchanged,e),"real-owner selected family proof refresh retains live graph");
 auto changed=live;changed.emissions[0].lifecycle=Lifecycle::Retired;check(!r.adoptSelected(a,changed,a.proof,e),"SAVE refresh rejects changed graph");
 Registry restored;check(!restored.restore(a,live,a.proof,{},{{first.identity,&body}},e),"restore requires every saved head");
 check(!restored.restore(a,live,a.proof,{{live.emissions[1].record.identity,&head2}},{{first.identity,reinterpret_cast<Piki*>(&head2)}},e),"same physical root cannot be both head and body");
 check(restored.restore(a,live,a.proof,{{decoded.identity,&head2}},{{first.identity,&body}},e),"complete provisional bindings restore atomically");
 check(restored.body(&body,savedBody)&&restored.head(&head2,decoded),"fresh mapping preserves source identity");
 check(!restored.restore(a,live,a.proof,{{decoded.identity,&head2}},{{first.identity,&body}},e),"cannot restore over live registry");
 restored.sceneExit();check(!restored.body(&body,savedBody)&&!restored.head(&head2,decoded),"scene exit removes stale native pointers");
 check(restored.bind(a,a.proof,e),"same campaign scene may rebind proof");
 PendingEmission oldDonor;check(!restored.prepare(&pom,1,bud,&donor,3,true,oldDonor,e),"scene exit preserves donor tombstone");
 Registry older;check(older.restore(a,live,a.proof,{{live.emissions[1].record.identity,&head2}},{{first.identity,&body}},e),"authenticated selected older graph can restore in new provisional world");
 // Two pre-consumption reservations cannot overwrite another committed output.
 Registry race;check(race.bind(a,a.proof,e),"race test owner");PendingEmission p1,p2;a.donorSpecies=1;a.donorKey="party:3";
 check(race.prepare(&pom,1,bud,&donor,3,false,p1,e),"reservation one");a.donorKey="party:4";check(race.prepare(&pom,1,bud,&donor,3,false,p2,e),"reservation two before first commit");
 check(race.adopt(std::move(p1),&head,1,3,false,reason),"first output commit");check(!race.adopt(std::move(p2),&head2,1,3,false,reason),"stale preallocated state cannot erase committed graph");
 // Live capacity is enforced BEFORE producer consumes a donor.
 Registry capacity;check(capacity.bind(a,a.proof,e),"capacity owner");PikiHeadItem heads[101];
 for(unsigned i=0;i<100;++i){a.donorKey="party:capacity:"+std::to_string(i);PendingEmission next;check(capacity.prepare(&pom,1,bud,&donor,3,false,next,e)&&capacity.adopt(std::move(next),&heads[i],i+1,3,false,reason),"100 literal live slots");}
 a.donorKey="party:capacity:100";PendingEmission excess;check(!capacity.prepare(&pom,1,bud,&donor,3,false,excess,e),"101st reservation refused before consumption");
 Registry callback;check(callback.bind(a,a.proof,e),"real-ABI adapter owner");Producer producer(callback,a,headColour,fatal);std::string receipt,oldReceipt;
 a.donorKey="party:callback";check(Producer::snapshotCallback(&pom,bud,1,&donor,receipt,&producer),"snapshot callback reserves before donor consumption");oldReceipt=receipt;
 check(Producer::snapshotCallback(&pom,bud,1,&donor,receipt,&producer)&&receipt!=oldReceipt,"capacity retry replaces stale reversible reservation");
 denyAllocation=true;Producer::headCallback(&pom,bud,1,1,&head,false,receipt,&producer);denyAllocation=false;
 check(!failures&&callback.head(&head,decoded),"actual callback ABI adopts without allocation");
 denyAllocation=true;Producer::headCallback(&pom,bud,1,1,&head2,false,oldReceipt,&producer);denyAllocation=false;
 check(failures==1&&!callback.head(&head2,decoded),"old local receipt cannot replay or silently gain ownership");
 // Native converted-donor kill occurs synchronously BEFORE output callback.
 PendingEmission move;check(callback.prepareTransfer(&head,3,move,e)&&callback.adoptBody(std::move(move),&body,reason),"converted donor body");
 a.donorSpecies=3;a.donorKey="conversion:callback";
 check(Producer::snapshotCallback(&pom,bud,1,&body,receipt,&producer),"converted donor reservation includes pending retirement");
 callback.forget(&body);
 denyAllocation=true;Producer::headCallback(&pom,bud,1,2,&head2,true,receipt,&producer);denyAllocation=false;
 check(failures==1&&callback.head(&head2,decoded)&&decoded.refunded&&!callback.body(&body,savedBody),"native kill-before-output and refund preserve new head ownership");
 check(callback.snapshot(live,e)&&live.emissions[0].lifecycle==Lifecycle::Retired&&live.emissions[1].lifecycle==Lifecycle::Head,"donor terminal and output live states both persisted");
 producer.cancel();check(callback.head(&head2,decoded),"cancel never erases committed output");
 PendingEmission finalTransfer;check(callback.prepareTransfer(&head2,3,finalTransfer,e)&&callback.adoptBody(std::move(finalTransfer),&body,reason),"plucked converted body for common typed census");
 registry().swap(callback);PcP2SourceBody typed;check(pc_p2_source_body_query(&body,typed)==PcP2SourceBodyKind::BudConversion&&typed.state.species==3&&!typed.state.wild&&!typed.state.wasWild,"converted body is a distinct nonwild discriminator");
 Piki ordinary;auto prior=typed;check(pc_p2_source_body_query(&ordinary,typed)==PcP2SourceBodyKind::None&&typed.kind==prior.kind,"ordinary body stays unlabelled and output unchanged");
 const auto genCatalog=hash('7');const std::string key="tutorial/defaultgen.txt#5";
 check(pc_p2_original_piki_origin_install(genCatalog,{{key,p2original::originalSourceCatalogUid(key),1,1}},e),"independent immutable GenPiki catalog");
 check(p2original::originalProgress().initialize(hash('a'),e)&&pc_p2_original_piki_recruit_bind(hash('a'),genCatalog,e),"explicit distinct campaign/GenPiki authority pair");
 check(pc_p2_original_piki_recruit_allowed(&body,1,false,true,e),"source day0 nonwild converted body permits Louie");
 check(!pc_p2_original_piki_recruit_allowed(&body,0,false,true,e),"source day0 nonwild converted body excludes Olimar before reunion");
 check(pc_p2_original_piki_contact_owner_allowed(&body,0,1,false,e),"common source discriminator permits canonically accepted contact despite P1 colour owner");
 check(!pc_p2_original_piki_contact_owner_allowed(&body,0,1,true,e),"VS ordinary ownership remains strict");
 auto progress=p2original::originalProgress().snapshot();check(pc_p2_original_piki_recruit_accepted(&body,1,false,true,e)&&p2original::originalProgress().snapshot().met==progress.met,"converted recruitment cannot fabricate RGB first-met");
 a.current=false;check(pc_p2_source_body_query(&body,typed)==PcP2SourceBodyKind::BudConversion&&!pc_p2_original_piki_recruit_allowed(&body,1,false,true,e),"expired converted proof remains labelled but refuses native event");a.current=true;
 check(!pc_p2_source_body_admitted(typed,hash('c'),genCatalog,e),"foreign campaign refused independently of GenPiki catalog");
 registry().sceneExit();check(pc_p2_source_body_query(&body,typed)==PcP2SourceBodyKind::None,"common scene teardown forgets old pool address");
 std::printf("BUD_ORIGIN_CONTROLS PASS %d (mock proof/opaque actors only)\n",checks);return 0;
}
