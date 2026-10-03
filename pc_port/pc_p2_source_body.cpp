#include "pc_p2_source_body.h"
PcP2SourceBodyKind pc_p2_source_body_query(const Piki* p,PcP2SourceBody& out){
 OriginalPikiBody gen;p2budorigin::Record bud;
 const bool hasGen=pc_p2_original_piki_body_query(p,gen);
 const bool hasBud=p2budorigin::registry().body(p,bud);
 if(hasGen&&hasBud)return PcP2SourceBodyKind::Unavailable;
 PcP2SourceBody next;
 if(hasGen){next.kind=PcP2SourceBodyKind::GenPiki;next.state=gen.state;next.genPiki=std::move(gen);}
 else if(hasBud){next.kind=PcP2SourceBodyKind::BudConversion;next.state={static_cast<std::uint8_t>(bud.species),false,false};next.conversion=std::move(bud);}
 else {OriginalPikiOrigin legacy;if(pc_p2_original_piki_origin_query(p,legacy))return PcP2SourceBodyKind::Unavailable;return PcP2SourceBodyKind::None;}
 out=std::move(next);return out.kind;
}
bool pc_p2_source_body_admitted(const PcP2SourceBody& body,const std::string& campaign,const std::string& catalog,std::string& e){
 if(body.kind==PcP2SourceBodyKind::GenPiki)return body.genPiki.origin.catalogFingerprint==catalog;
 if(body.kind==PcP2SourceBodyKind::BudConversion){
  if(body.conversion.identity.floor.campaign!=campaign){e="converted source body campaign mismatch";return false;}
  return p2budorigin::registry().admitted(body.conversion,e);
 }
 e="source body discriminator unavailable";return false;
}
bool pc_p2_source_body_recruited(Piki* p){PcP2SourceBody body;const auto kind=pc_p2_source_body_query(p,body);
 if(kind==PcP2SourceBodyKind::GenPiki)return pc_p2_original_piki_body_recruited(p);
 // Retail FakePiki::onInit clears flags; a converted, plucked body is not wild.
 return kind==PcP2SourceBodyKind::BudConversion&&!body.state.wild;
}
