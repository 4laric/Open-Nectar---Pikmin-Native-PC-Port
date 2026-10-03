#pragma once
#include "pc_p2_retail_cave_context.h"
#include <sstream>
#include <limits>

namespace p2retail {
// Durable source census. Native actors and pointers never enter this codec.
struct LedgerReceipt {
 std::uint64_t generation=0,event=0;
 std::string sha256;
};
struct LedgerBirth {
 BirthIdentity identity;
 BindingState state=BindingState::Live;
 LedgerReceipt receipt;
};
struct LedgerFloor {
 std::string visit,cave,sourceSha256,catalogSha256,layoutSha256;
 unsigned floor=0;std::uint64_t epoch=0,nativeSerial=0,transitionGeneration=0;
 std::vector<LedgerBirth> births;
};
struct FloorLedger {
 std::string seed,campaignSha256,sessionSha256;
 std::uint64_t nextActivation=1;
 std::vector<LedgerFloor> floors;
};
inline bool ledgerToken(const std::string& value){
 if(value.empty()||value.size()>256)return false;
 for(unsigned char c:value)if(c<=32||c>=127)return false;
 return true;
}
inline bool validateLedger(const FloorLedger& ledger,std::string& error){
 auto fail=[&](const char* reason){error=reason;return false;};
 if(!ledgerToken(ledger.seed)||!hex64(ledger.campaignSha256)||!hex64(ledger.sessionSha256)||
    !ledger.nextActivation||ledger.floors.size()>105)
  return fail("retail ledger framing");
 std::set<std::string> visits;std::set<std::uint64_t> activations,epochs;
 std::size_t total=0;
 for(const auto& f:ledger.floors){
  const auto* cave=descriptor(f.cave);const auto* def=cave?definition(*cave,f.floor):nullptr;
  if(!def||def->first!=def->last||!ledgerToken(f.visit)||!f.epoch||!f.nativeSerial||!f.transitionGeneration||
     !epochs.insert(f.epoch).second||!visits.insert(f.visit+":"+f.cave+":"+std::to_string(f.floor)).second||
     f.sourceSha256!=cave->sourceSha256||f.catalogSha256!=cave->catalogSha256||!hex64(f.layoutSha256))
   return fail("retail ledger floor identity");
  if(f.births.size()>10000||(total+=f.births.size())>20000)return fail("retail ledger census bound");
  std::set<std::string> expected,seen;
  for(const auto& row:def->rows){
   if(row.weight())return fail("retail ledger weighted selection unavailable");
   for(unsigned ordinal=0;ordinal<row.minimum();++ordinal)
    expected.insert(instanceKey(*cave,f.floor,row,ordinal));
  }
  for(const auto& birth:f.births){
   const auto& id=birth.identity;
   if(id.row>=def->rows.size()||id.ordinal>=def->rows[id.row].minimum()||id.epoch!=f.epoch||
      !id.activation||id.activation>=ledger.nextActivation||!activations.insert(id.activation).second||
      id.instance!=instanceKey(*cave,f.floor,def->rows[id.row],id.ordinal)||!seen.insert(id.instance).second)
    return fail("retail ledger birth identity");
   const auto state=static_cast<unsigned>(birth.state);
   if(state>static_cast<unsigned>(BindingState::SourceSuppressed))return fail("retail ledger terminal state");
   const auto& receipt=birth.receipt;
   if(birth.state==BindingState::Live){
    if(receipt.generation||receipt.event||!receipt.sha256.empty())return fail("retail ledger live receipt");
   }else{
    if(!receipt.generation||!receipt.event||!hex64(receipt.sha256))return fail("retail ledger terminal receipt");
    const auto& row=def->rows[id.row];
    if((birth.state==BindingState::ConsumedTreasure&&row.kind!="loose_treasure")||
       (birth.state==BindingState::SourceSuppressed&&row.sourceId!=6&&row.sourceId!=7))
     return fail("retail ledger source absence kind");
   }
  }
  if(expected!=seen)return fail("retail ledger incomplete source census");
 }
 error.clear();return true;
}
inline bool writeLedger(const FloorLedger& ledger,std::string& bytes,std::string& error){
 if(!validateLedger(ledger,error))return false;
 std::ostringstream out;
 out<<"P2_RETAIL_LEDGER_1 "<<ledger.seed<<' '<<ledger.campaignSha256<<' '<<ledger.sessionSha256<<' '
    <<ledger.nextActivation<<' '<<ledger.floors.size()<<'\n';
 for(const auto& f:ledger.floors){
  out<<"floor "<<f.visit<<' '<<f.cave<<' '<<f.floor<<' '<<f.sourceSha256<<' '<<f.catalogSha256<<' '
     <<f.layoutSha256<<' '<<f.epoch<<' '<<f.nativeSerial<<' '<<f.transitionGeneration<<' '<<f.births.size()<<'\n';
  for(const auto& b:f.births){const auto& id=b.identity;
   out<<"birth "<<id.row<<' '<<id.ordinal<<' '<<id.epoch<<' '<<id.instance<<' '<<id.activation<<' '
      <<static_cast<unsigned>(b.state)<<' '<<b.receipt.generation<<' '<<b.receipt.event<<' '
      <<(b.receipt.sha256.empty()?"-":b.receipt.sha256)<<'\n';
  }
 }
 auto next=out.str();if(next.size()>1024*1024){error="retail ledger byte bound";return false;}
 bytes=std::move(next);error.clear();return true;
}
inline bool readLedger(const std::string& bytes,FloorLedger& ledger,std::string& error){
 auto fail=[&](const char* reason){error=reason;return false;};
 if(bytes.size()>1024*1024)return fail("retail ledger byte bound");
 std::istringstream in(bytes);FloorLedger next;std::string tag;unsigned count=0,total=0;
 if(!(in>>tag>>next.seed>>next.campaignSha256>>next.sessionSha256>>next.nextActivation>>count)||tag!="P2_RETAIL_LEDGER_1"||count>105)
  return fail("retail ledger framing");
 for(unsigned i=0;i<count;++i){LedgerFloor f;unsigned births=0;
  if(!(in>>tag>>f.visit>>f.cave>>f.floor>>f.sourceSha256>>f.catalogSha256>>f.layoutSha256>>f.epoch>>f.nativeSerial>>f.transitionGeneration>>births)||
     tag!="floor"||births>10000||(total+=births)>20000)return fail("retail ledger floor framing");
  for(unsigned j=0;j<births;++j){LedgerBirth b;auto& id=b.identity;unsigned state=0;
   if(!(in>>tag>>id.row>>id.ordinal>>id.epoch>>id.instance>>id.activation>>state>>
         b.receipt.generation>>b.receipt.event>>b.receipt.sha256)||tag!="birth"||state>3)
    return fail("retail ledger birth framing");
   b.state=static_cast<BindingState>(state);if(b.receipt.sha256=="-")b.receipt.sha256.clear();
   f.births.push_back(std::move(b));
  }
  next.floors.push_back(std::move(f));
 }
 if(in>>tag)return fail("retail ledger trailing bytes");
 if(!validateLedger(next,error))return false;
 // Canonical framing also rejects signed/overflow aliases accepted by the
 // standard stream's unsigned conversions, without altering selected bytes.
 std::string canonical;
 if(!writeLedger(next,canonical,error))return false;
 if(canonical!=bytes)return fail("retail ledger noncanonical bytes");
 ledger=std::move(next);error.clear();return true;
}
struct LedgerCheckpointProof {
 std::uint64_t generation=0;std::string sha256;
};
class SelectedLedgerVerifier {
public:
 virtual ~SelectedLedgerVerifier()=default;
 // Actual SAVE owner must check exact selected family bytes, generation/full
 // card SHA, source session and owned native scene on EVERY call. This seam
 // grants no physical authority by itself and has no permissive default.
 virtual bool verify(const LedgerCheckpointProof&,const FloorLedger&,const LedgerFloor&,
                     const SceneIdentity&,std::string&)const=0;
};
class SelectedFloorAuthority final:public FloorIdentityAuthority {
 FloorLedger mLedger;LedgerCheckpointProof mProof;const SelectedLedgerVerifier& mVerifier;
public:
 SelectedFloorAuthority(FloorLedger ledger,LedgerCheckpointProof proof,const SelectedLedgerVerifier& verifier)
  :mLedger(std::move(ledger)),mProof(std::move(proof)),mVerifier(verifier){}
 bool expectedBirth(const CaveDescriptor& cave,unsigned floor,const SceneIdentity& scene,
                    unsigned row,unsigned ordinal,BirthIdentity& out,std::string& error)const override{
  if(!mProof.generation||!hex64(mProof.sha256)||!scene.serial||scene.seed!=mLedger.seed||
     !hex64(scene.layoutSha256)||!validateLedger(mLedger,error)){
   error="retail ledger selected proof";return false;
  }
  const auto* canonical=descriptor(cave.cave);
  if(!canonical||cave.source!=canonical->source||cave.sourceSha256!=canonical->sourceSha256||
     cave.catalogSha256!=canonical->catalogSha256||cave.maxFloor!=canonical->maxFloor){
   error="retail ledger foreign descriptor";return false;
  }
  for(const auto& f:mLedger.floors)if(f.cave==cave.cave&&f.floor==floor&&f.visit==scene.visit&&f.layoutSha256==scene.layoutSha256){
   if(f.transitionGeneration>mProof.generation){error="retail ledger future transition";return false;}
   for(const auto& b:f.births)if(b.receipt.generation>mProof.generation){error="retail ledger future receipt";return false;}
   if(!mVerifier.verify(mProof,mLedger,f,scene,error))return false;
   for(const auto& birth:f.births)if(birth.identity.row==row&&birth.identity.ordinal==ordinal){
    out=birth.identity;error.clear();return true;
   }
   error="retail ledger missing selected birth";return false;
  }
  error="retail ledger foreign selected floor";return false;
 }
};
}
