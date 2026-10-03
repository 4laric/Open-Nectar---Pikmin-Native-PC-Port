#include "pc_p2_retail_floor_ledger.h"
#include <cstdlib>
#include <iostream>
using namespace p2retail;
struct Verifier final:SelectedLedgerVerifier {
 mutable unsigned calls=0;bool accept=true;
 bool verify(const LedgerCheckpointProof& p,const FloorLedger&,const LedgerFloor&,const SceneIdentity& s,std::string&)const override{
  ++calls;return accept&&p.generation==3&&p.sha256==std::string(64,'a')&&s.serial==29;
 }
};
int main(){
 unsigned checks=0;auto check=[&](bool value){++checks;if(!value){std::cerr<<"failed "<<checks<<'\n';std::exit(1);}};
 FloorLedger base;base.seed="real-seed";base.campaignSha256=std::string(64,'b');base.sessionSha256=std::string(64,'c');
 auto* desc=descriptor("tutorial_1");check(desc!=nullptr);auto* def=definition(*desc,1);check(def!=nullptr);
 LedgerFloor f;f.visit="visit1";f.cave=desc->cave;f.floor=1;f.epoch=7;f.nativeSerial=19;f.transitionGeneration=2;
 f.sourceSha256=desc->sourceSha256;f.catalogSha256=desc->catalogSha256;f.layoutSha256=std::string(64,'d');
 for(unsigned r=0;r<def->rows.size();++r)for(unsigned i=0;i<def->rows[r].minimum();++i){
  LedgerBirth b;b.identity={r,i,f.epoch,instanceKey(*desc,1,def->rows[r],i),base.nextActivation++};f.births.push_back(b);
 }
 base.floors.push_back(f);std::string error,bytes;check(writeLedger(base,bytes,error));FloorLedger decoded;
 check(readLedger(bytes,decoded,error));std::string again;check(writeLedger(decoded,again,error)&&again==bytes);
 auto refuse=[&](FloorLedger bad){std::string untouched="keep";check(!writeLedger(bad,untouched,error)&&untouched=="keep");};
 auto bad=base;bad.seed="";refuse(bad);
 bad=base;bad.sessionSha256="wrong";refuse(bad);
 bad=base;bad.nextActivation=1;refuse(bad);
 bad=base;bad.floors[0].sourceSha256=std::string(64,'e');refuse(bad);
 bad=base;bad.floors[0].catalogSha256=std::string(64,'e');refuse(bad);
 bad=base;bad.floors[0].layoutSha256="wrong";refuse(bad);
 bad=base;bad.floors[0].nativeSerial=0;refuse(bad);
 bad=base;bad.floors[0].transitionGeneration=0;refuse(bad);
 bad=base;bad.floors.push_back(f);refuse(bad);
 bad=base;bad.floors[0].births.pop_back();refuse(bad);
 bad=base;bad.floors[0].births[1].identity.activation=bad.floors[0].births[0].identity.activation;refuse(bad);
 bad=base;bad.floors[0].births[0].identity.epoch=8;refuse(bad);
 bad=base;bad.floors[0].births[0].identity.instance="foreign";refuse(bad);
 bad=base;bad.floors[0].births[0].receipt={2,9,std::string(64,'a')};refuse(bad);
 bad=base;bad.floors[0].births[0].state=BindingState::ConsumedTreasure;bad.floors[0].births[0].receipt={2,9,std::string(64,'a')};refuse(bad);
 bad=base;bad.floors[0].births[0].state=BindingState::SourceSuppressed;bad.floors[0].births[0].receipt={2,9,std::string(64,'a')};refuse(bad);
 for(auto& b:base.floors[0].births)if(def->rows[b.identity.row].kind=="loose_treasure"){
  b.state=BindingState::ConsumedTreasure;b.receipt={2,11,std::string(64,'a')};break;
 }
 check(writeLedger(base,bytes,error));check(readLedger(bytes,decoded,error));
 auto unsignedAlias=bytes;auto where=unsignedAlias.find("birth 0 0 7 ");
 check(where!=std::string::npos);unsignedAlias.replace(where,12,"birth -18446744073709551616 0 7 ");
 for(const auto& malformed:{bytes+" extra",bytes+"\n",unsignedAlias,bytes.substr(0,bytes.size()/2),std::string(1024*1024+1,'x')}){
  auto before=decoded;check(!readLedger(malformed,decoded,error));std::string a,b;
  check(writeLedger(before,a,error)&&writeLedger(decoded,b,error)&&a==b);
 }
 Verifier verifier;LedgerCheckpointProof proof{3,std::string(64,'a')};SelectedFloorAuthority owner(base,proof,verifier);
 SceneIdentity scene{base.seed,f.visit,f.layoutSha256,29};BirthIdentity out;
 check(owner.expectedBirth(*desc,1,scene,0,0,out,error)&&out==f.births[0].identity);
 auto saved=out;verifier.accept=false;check(!owner.expectedBirth(*desc,1,scene,0,0,out,error)&&out==saved);
 verifier.accept=true;scene.serial=30;check(!owner.expectedBirth(*desc,1,scene,0,0,out,error)&&out==saved);
 scene.serial=29;scene.layoutSha256=std::string(64,'e');check(!owner.expectedBirth(*desc,1,scene,0,0,out,error)&&out==saved);
 scene.layoutSha256=f.layoutSha256;scene.seed="foreign";check(!owner.expectedBirth(*desc,1,scene,0,0,out,error)&&out==saved);
 scene.seed=base.seed;SelectedFloorAuthority stale(base,{1,std::string(64,'a')},verifier);
 check(!stale.expectedBirth(*desc,1,scene,0,0,out,error)&&out==saved);
 check(verifier.calls>=3);
 std::cout<<checks<<" retail ledger controls PASS (mock verifier; no physical admission)\n";
}
