#include "pc_p2_piki_jpa_bank.h"
#include "netplay/pc_netplay_sha256.h"
#include <algorithm>
namespace p2original { namespace pikiJPA {
namespace {
const ResourceRole roles[]={
 {"piki-0169.jpa",0x169,205468,304,"f57796cd2b1aa7dd9554293595ba16a048ce6ff32cc6ecedd45780b7b6df4562"},
 {"IP2_ringhalo_i.tex1",0,482880,4160,"82d8e6c36cdafad0d1392b9b902aa94c9f72616ff9f8ef6dbb5fd0ea1a5054f9"},
 {"piki-016a.jpa",0x16a,205772,304,"26eaf40991e8bc7b16825073bbd5f6ba7435bb225d76cd4381284fa776b50d8e"},
 {"piki-0281.jpa",0x281,206076,304,"f0a3c4e84720b6eda09ec111ad936e64602c6337d11a3267303f7447122509f4"},
 {"piki-016b.jpa",0x16b,206380,304,"fe10a9bebdcc09fc0b3f072b660b3782362944381ed72a3a0ceaca16381e097e"},
 {"piki-016c.jpa",0x16c,206684,304,"87cf8317855c777fa40aeb4ed7fe3e4c0a616df56f3f4074fa3e3a647ed8621a"},
 {"piki-016d.jpa",0x16d,206988,304,"ed18b8d93e185ce077de496f6216818b6ec8f6c0199e23e0e2ce7dcbee7d0df9"},
 {"piki-0172.jpa",0x172,208348,276,"be9276aa8b8c951ba480705841326f5b6e1fc804362152d065d8c22549fdb3b9"},
 {"IP2_firemsk1_ia.tex1",0,297632,1088,"802c5354db1f6d10bf0a6dc6c9dd44ecf488cb755b82f169022f59af4f66ef11"},
 {"IP2_ami2_i.tex1",0,445952,1088,"deb6a406cf184b327fe036e2bf4637d1b06b37d73095fbc72f24f1fc76122083"},
 {"piki-0173.jpa",0x173,208624,276,"ae1fc60271854b306d62d47e3d97dfa3a2fb768625989c0655c026bbada064be"},
 {"piki-0174.jpa",0x174,208900,276,"1004a7e1ea1d01ea2eb53409a36fc94d86c1f5b7ff587189cbb44b7de97d1c7b"},
 {"piki-0175.jpa",0x175,209176,276,"971fd4c1284d5d1eb19ee5f4f6be684d071a8afbe289b27395047645a263588c"},
 {"piki-0176.jpa",0x176,209452,276,"2b2051166d1b1a8c373d32ed234179ef9e187aa69c7c9267b57a67efc6e33cb2"},
 {"piki-0177.jpa",0x177,209728,316,"2aa39e477cf5a404ec2224f6f83cd1d3f08975763d07edbe07acbf582da2c262"},
 {"IP2_star5_i.tex1",0,264032,4160,"bdd22ded1e6467a6b1918fe3befd9808ea7839d2100d4314a26d6d685f1decf1"},
};
bool fullSha(const std::string& s){return s.size()==64&&std::all_of(s.begin(),s.end(),[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');});}
bool fail(std::string& e,const char* s){e=s;return false;}
bool hash(const std::vector<unsigned char>& b,const char* expected){
 unsigned char digest[32];pc_netplay_sha::sha256(b.data(),b.size(),digest);
 const char* h="0123456789abcdef";
 for(unsigned i=0;i<32;++i)if(h[digest[i]>>4]!=expected[2*i]||h[digest[i]&15]!=expected[2*i+1])return false;
 return true;
}
}
const ResourceRole* resourceRoles(std::size_t& n){n=sizeof(roles)/sizeof(*roles);return roles;}
bool Bank::load(const SelectedIdentity& selected,const std::vector<SelectedBytes>& inputs,std::string& e){
 if(!fullSha(selected.campaignSHA)||!fullSha(selected.packetSHA)||selected.session.empty()||selected.session.size()>256)
  return fail(e,"Piki JPA requires actual selected campaign, packet and session identity");
 if(inputs.empty()||inputs.size()>sizeof(roles)/sizeof(*roles))return fail(e,"Piki JPA selected role count invalid");
 std::map<std::string,std::vector<unsigned char>> checked;
 for(const auto& input:inputs){
  const auto* role=std::find_if(std::begin(roles),std::end(roles),[&](const ResourceRole& r){return input.role==r.role;});
  if(role==std::end(roles)||input.archiveMember!=sourceArchive||input.archiveSHA!=sourceArchiveSHA||input.memberOffset!=role->offset||input.memberBytes!=role->bytes)
   return fail(e,"Piki JPA selected role/provenance mismatch");
  if(input.bytes.size()!=role->bytes||!hash(input.bytes,role->sha256))return fail(e,"Piki JPA genuine selected bytes digest mismatch");
  if(!checked.emplace(input.role,input.bytes).second)return fail(e,"Piki JPA duplicate selected role");
 }
 for(const auto& input:inputs){
  if(input.role.size()<5||input.role.substr(input.role.size()-4)!=".jpa")continue;
  const unsigned pid=(unsigned(input.bytes[0])<<8)|input.bytes[1];
  const char* texture=(pid>=0x169&&pid<=0x16d)||pid==0x281?"IP2_ringhalo_i.tex1":pid==0x177?"IP2_star5_i.tex1":"IP2_firemsk1_ia.tex1";
  if(!checked.count(texture))return fail(e,"Piki JPA selected effect texture absent");
  if(pid>=0x172&&pid<=0x176&&!checked.count("IP2_ami2_i.tex1"))return fail(e,"Piki JPA source stripe secondary texture absent");
 }
 mSelected=selected;mBytes=std::move(checked);e.clear();return true;
}
const std::vector<unsigned char>* Bank::bytes(const std::string& role)const {auto i=mBytes.find(role);return i==mBytes.end()?nullptr:&i->second;}
unsigned haloId(unsigned species){const unsigned ids[]={0x16a,0x16b,0x16d,0x169,0x16c,0x281};return species<6?ids[species]:0;}
unsigned blurId(unsigned species){const unsigned ids[]={0x173,0x174,0x176,0x172,0x175};return species<5?ids[species]:0;}
} }
