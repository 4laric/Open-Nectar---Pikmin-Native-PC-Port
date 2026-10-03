#include "pc_p2_original_white_bank.h"
#include <cmath>
#include <locale>
#include <sstream>
#include <utility>
namespace p2original {
namespace {
constexpr const char* model="a971c6ac48e04a6f99333cacc1f57faf1ebfa13db71c5a31e93c6ce293bb376d";
constexpr const char* parms="f22ae88fade54bf8f142ecc5aae4ce0c82078e6aed448d029f16b75e5a3d7996";
constexpr const char* names[]={"wait","walk","attack1"};
constexpr const char* sources[]={
 "a6d37d0f572e21b0016e235e7b398b4bc6b0a30a276cd1b9bc7d15001baec2e3",
 "a7213c06444fd702de2503573b6c67691f90d0be80b1dd61c7651deaa307eb93",
 "378f09e7ff1abbab15bb1ef33fcaf113b5b25f06f5fabfbdd81e9a6508d47765"};
constexpr unsigned samples[2][12]={{0,2,3,5,7,9,10,12,14,16,17,19},{0,4,7,11,14,18,21,25,28,32,35,39}};
}
bool parseWhiteBank(const std::string& bytes,WhiteBank& out,std::string& error){
 const auto fail=[&](const char* why){error=why;return false;};
 if(bytes.empty()||bytes.size()>32768||bytes.find('\0')!=std::string::npos)
  return fail("invalid source White bank extent");
 std::istringstream in(bytes);in.imbue(std::locale::classic());
 WhiteBank value;std::string word;
 if(!(in>>word>>value.modelSha>>value.parameterSha)||word!="P2_SOURCE_WHITE_BANK_1"||value.modelSha!=model||value.parameterSha!=parms)
  return fail("source White model/parameter provenance mismatch");
 for(unsigned c=0;c<3;++c){
  auto& clip=value.clips[c];unsigned count=0;
  if(!(in>>word>>clip.name>>clip.duration>>clip.sourceSha>>count)||word!="clip"||clip.name!=names[c]||clip.duration!=(c==1?40u:20u)||clip.sourceSha!=sources[c]||count!=12)
   return fail("source White clip provenance/layout mismatch");
  for(unsigned i=0;i<12;++i)if(!(in>>clip.frames[i])||clip.frames[i]!=samples[c==1?1:0][i])
   return fail("source White sample frame mismatch");
  for(unsigned i=0;i<12;++i){
   std::string name;unsigned index=0;
   if(!(in>>word>>name>>index)||word!="happa"||name!=clip.name||index!=i)
    return fail("source White Happa sample topology mismatch");
   for(auto& entry:clip.happa[i])if(!(in>>entry)||!std::isfinite(entry))
    return fail("nonfinite source White Happa transform");
  }
 }
 if(in>>word)return fail("trailing source White bank data");
 if(!in.eof())return fail("invalid source White bank stream");
 out=std::move(value);error.clear();return true;
}
}
