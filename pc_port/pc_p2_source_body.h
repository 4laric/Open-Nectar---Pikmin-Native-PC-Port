#pragma once
#include "pc_p2_original_piki_origin.h"
#include "pc_p2_bud_conversion_origin.h"
enum class PcP2SourceBodyKind { None,GenPiki,BudConversion,Unavailable };
struct PcP2SourceBody {
 PcP2SourceBodyKind kind=PcP2SourceBodyKind::None;
 OriginalPikiBodyState state;
 OriginalPikiBody genPiki;
 p2budorigin::Record conversion;
};
// Tag is returned even if source admission later refuses. Never turn an expired
// labelled source body into an ordinary-P1 fallback. Failed output is unchanged.
PcP2SourceBodyKind pc_p2_source_body_query(const Piki*,PcP2SourceBody&);
bool pc_p2_source_body_admitted(const PcP2SourceBody&,const std::string& campaign,
                              const std::string& genPikiCatalog,std::string&);
bool pc_p2_source_body_recruited(Piki*);
