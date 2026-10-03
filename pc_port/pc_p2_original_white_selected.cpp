#include "pc_p2_original_white_selected.h"
#include <pc_randomizer.h>
#include <netplay/pc_netplay_sha256.h>
#include <utility>
namespace p2original {
namespace {
constexpr const char* prefix="p2-original/piki-bodies/white/";
struct Asset {const char* name;std::size_t bytes;const char* sha;};
// Actual source-white-bank01 conversion receipt. Different conversions require
// a new reviewed kit version; selected input hashes alone cannot infer source.
constexpr Asset assets[]={
 {"bank.txt",6658,"268e890b95615b0983f29dfb9aad013b5c4df6e98003ce620f26def96eaf0639"},
 {"pikiParms.txt",4938,"f22ae88fade54bf8f142ecc5aae4ce0c82078e6aed448d029f16b75e5a3d7996"},
 {"white_attack1_00.mod",11520,"ae52bbee62301c66526345769023a5147b485d25dcc11e7501c2450eb7bbd24b"},
 {"white_attack1_01.mod",11520,"d0d6d6e897d864828989d7e85dbfa0703c2aae397571ac2df1215e6082be5eff"},
 {"white_attack1_02.mod",11520,"3c78a99a54b38ac1a41c58fd316dd203bdb4b7f2a7c407503a6bfbf8834e4ce4"},
 {"white_attack1_03.mod",11520,"138e82cedc85fe71c0642cf7c34219634aa5b07b0b3c539876e0aa3bdf1eb4ed"},
 {"white_attack1_04.mod",11520,"64e2801ee1363dcf6a8118755b1c6ac6dbb7494b55a3a97c8be72bcdfe72a789"},
 {"white_attack1_05.mod",11520,"9cc30915ae18996e85af9cb5f6321ca85f27b6deb0184bb2ee8bcf40186cb089"},
 {"white_attack1_06.mod",11520,"922f8f2cf4ea602426736159789e86726b001306d313af8c7c5ddacffe2f1c53"},
 {"white_attack1_07.mod",11520,"ee5acb3c2f9260cb590c66cee618b5df2eec885525728537324293ae636cd804"},
 {"white_attack1_08.mod",11520,"fed2e01c1b52357d58539d24a35b4a751b55a59a68b208a93a428fc4354e1aa3"},
 {"white_attack1_09.mod",11520,"2b69f7f192cdf1a06c0377b73ba7bffe7d5dc91d3b5e4fffbbd493d6ca48fd2a"},
 {"white_attack1_10.mod",11520,"1bf5169af5fdfc6ff56130f324ede08977c3f8ee472f44daac176440395fce1e"},
 {"white_attack1_11.mod",11520,"b2af62ec09997a085c8b5207a8b5c31a0e3599b611048388de4ac24eca78bc8a"},
 {"white_happa_0.mod",1696,"cf92ec379ce0ea7c1174de6af4e97531aae7596b40e5e1c073d29c80e5e56a06"},
 {"white_happa_1.mod",3264,"ea46520fde740cd46026c252a4d937e7ac18746f4db359a32700cf0c52631c4f"},
 {"white_happa_2.mod",4064,"b9a54e452b0ab7db9b06e581136fa63f245a10597c5b3e32bf0fa6ad97d4f3ea"},
 {"white_wait_00.mod",11520,"f0bb1c8b8bafd5af458706cdd6771a2b1840e137b512b8c87a4b61109ff53b59"},
 {"white_wait_01.mod",11520,"45ae4e9f38278381cf5dc43139b3f50262498259bd3859a181258e2ebbd3936a"},
 {"white_wait_02.mod",11520,"ef8bd6cbcb5cba678f777311c168fe37b6eb392d474f9b5718ffee384dd3ec81"},
 {"white_wait_03.mod",11520,"31870a9153464c58c3967381a8caa5f2be7ccd321b5c5a1173f65fa10577946b"},
 {"white_wait_04.mod",11520,"3c8549eef70cf278f3ece08f12e6b50c3c6c49f50d5f6538403bfe9f06d6f5d1"},
 {"white_wait_05.mod",11520,"b13bb55c625a779d771b745f169d4c5cd7e877ce682f866b023027641b09e066"},
 {"white_wait_06.mod",11520,"3002817096f9d957fff3fbb8b7b01de8c4b3ec5404ee7d06bd5749e9bdb0d222"},
 {"white_wait_07.mod",11520,"2773db05010d4a433b9a4531c51f249d52e8df77a69e9dbca20dce0a1c574edf"},
 {"white_wait_08.mod",11520,"63a65da90d5c4a9d3eea09e25225179db18c9d6d5185b8ac242aaf5507d7cfca"},
 {"white_wait_09.mod",11520,"ff260ee2728c5c4bfeab78d2fcbca9ff868b74b81f31a381688d40ff6b47c05a"},
 {"white_wait_10.mod",11520,"ef8bd6cbcb5cba678f777311c168fe37b6eb392d474f9b5718ffee384dd3ec81"},
 {"white_wait_11.mod",11520,"d61af1ec212972c08c06ef0c6bd1f27086cd9ffabbc4b359201cf8e826dfda7a"},
 {"white_walk_00.mod",11520,"f0bb1c8b8bafd5af458706cdd6771a2b1840e137b512b8c87a4b61109ff53b59"},
 {"white_walk_01.mod",11520,"f431179b9879342dbcab1ff7a3bd74aa9727860acf04291b399643c6ba52b6f7"},
 {"white_walk_02.mod",11520,"e5b598afa5102fe201c645ca57b43dfaa9cbec0109d78732768dd8393a9eb4fd"},
 {"white_walk_03.mod",11520,"2130874cb98cc2bc453b1134d99896559cbfd5b6c807f2edfb5e8c8efca2480b"},
 {"white_walk_04.mod",11520,"34183236d1c1bf974b6e535bb3e07f948c5d603b667abaddcbb66604ae852f89"},
 {"white_walk_05.mod",11520,"d05c13acc2bc70a98a2cfa327b2ab6da0d443b5d999e6482435190e2420ddd74"},
 {"white_walk_06.mod",11520,"58853f0b490cd941e44f0324accbafc82b3477e1851743b3b53bfef9c36df619"},
 {"white_walk_07.mod",11520,"e6f6932ab6e89ec3fd5f78e0cb76703d14e4bb50801e6a4bf97ee99816e0104f"},
 {"white_walk_08.mod",11520,"3871c8647f28be2ffb0fcf0beb059dcc0a93d394d7887341b04fc751a7093d57"},
 {"white_walk_09.mod",11520,"f5d3eeb7aa636089c746e882b9ffb99ceeabaa55e86b72686d883fce89839bcb"},
 {"white_walk_10.mod",11520,"c6dac7c977dfc7126b2a6a936888dcc0cc9cfe139cd1435bc3e894e786a09dde"},
 {"white_walk_11.mod",11520,"e68f200357e402f4a42fb9842cd65a58981922c2a67614fdd9f285b6347baa19"},
};
std::string hash(const std::string& bytes){
 std::uint8_t digest[32];pc_netplay_sha::sha256(bytes.data(),bytes.size(),digest);
 constexpr char hex[]="0123456789abcdef";std::string result(64,'0');
 for(unsigned i=0;i<32;++i){result[2*i]=hex[digest[i]>>4];result[2*i+1]=hex[digest[i]&15];}
 return result;
}
}
bool whiteSelectedKitCurrent(const WhiteSelectedKit& kit)noexcept{
 try{return kit.selection_!=0&&pc_randomizer_original_session()
  &&pc_randomizer_original_selection_revision()==kit.selection_
  &&pc_randomizer_original_campaign()==kit.campaign_
  &&pc_randomizer_session_fingerprint()==kit.session_;}
 catch(...){return false;}
}
bool readWhiteSelectedKit(WhiteSelectedKit& out,std::string& error){
 if(!pc_randomizer_original_session()){error="source White kit requires authenticated OriginalSession";return false;}
 WhiteSelectedKit value;value.selection_=pc_randomizer_original_selection_revision();
 value.campaign_=pc_randomizer_original_campaign();value.session_=pc_randomizer_session_fingerprint();
 if(!whiteSelectedKitCurrent(value)){error="source White selection unavailable";return false;}
 for(const auto& asset:assets){
  const std::string role=std::string(prefix)+asset.name;std::string bytes;
  // This is the real selection-owned read, never a filesystem or preview fallback.
  if(!pc_randomizer_original_input(role,bytes,error))return false;
  if(bytes.size()!=asset.bytes||hash(bytes)!=asset.sha){error="source White converted asset mismatch: "+role;return false;}
  value.files_.emplace(role,std::move(bytes));
 }
 if(!parseWhiteBank(value.files_.at(std::string(prefix)+"bank.txt"),value.bank_,error))return false;
 if(!whiteSelectedKitCurrent(value)){error="source White selection changed during preflight";return false;}
 out=std::move(value);error.clear();return true;
}
}
