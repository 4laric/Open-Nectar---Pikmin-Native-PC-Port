#include "pc_p2_original_white_selected.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
// SDK read boundary controls only. These mocks do not prove native Bundle
// authentication, Shape construction, scene lifetime or physical White birth.
namespace {
std::map<std::string,std::string> files;
std::string refuseRole;
std::uint64_t revision=1;
bool selected=true,changeRevision=false;
unsigned reads=0,checks=0;
void require(bool value,const char* error){++checks;if(!value)throw std::runtime_error(error);}
}
bool pc_randomizer_original_session(){return selected;}
std::uint64_t pc_randomizer_original_selection_revision()noexcept{return revision;}
std::string pc_randomizer_original_campaign(){return std::string(64,'a');}
std::string pc_randomizer_session_fingerprint(){return std::string(64,'b');}
bool pc_randomizer_original_input(const std::string& role,std::string& out,std::string& error){
 ++reads;if(role==refuseRole||!files.count(role)){error="controlled selected read refusal";return false;}
 out=files.at(role);if(changeRevision&&reads==41)++revision;error.clear();return true;
}
int main(int argc,char** argv){try{
 require(argc==2,"actual source kit directory required");
 for(const auto& entry:std::filesystem::directory_iterator(argv[1])){
  const auto name=entry.path().filename().string();
  if(entry.path().extension()!=".mod"&&name!="bank.txt"&&name!="pikiParms.txt")continue;
  std::ifstream in(entry.path(),std::ios::binary);require(bool(in),"actual kit file readable");
  files.emplace("p2-original/piki-bodies/white/"+name,std::string((std::istreambuf_iterator<char>(in)),{}));
 }
 require(files.size()==41,"exact actual resource closure");
 p2original::WhiteSelectedKit kit;std::string error;
 require(!p2original::whiteSelectedKitCurrent(kit),"empty kit cannot become current");
 require(p2original::readWhiteSelectedKit(kit,error),"actual converted kit preflight succeeds");
 require(reads==41&&kit.files().size()==41&&kit.bank().clips[1].duration==40,"all actual bytes retained");
 const auto negative=[&](){
  reads=0;require(!p2original::readWhiteSelectedKit(kit,error),"invalid selected kit refuses");
  require(!error.empty()&&kit.selection()==1&&kit.files().size()==41,"failed preflight leaves previous kit unchanged");
 };
 selected=false;negative();require(reads==0,"ordinary session reads no kit");selected=true;
 revision=0;negative();require(reads==0,"unselected revision reads no kit");revision=1;
 refuseRole="p2-original/piki-bodies/white/white_wait_00.mod";negative();refuseRole.clear();
 const std::string role="p2-original/piki-bodies/white/white_walk_00.mod";
 files[role][20]^=1;negative();files[role][20]^=1;
 const std::string bank="p2-original/piki-bodies/white/bank.txt";
 files[bank][300]^=1;negative();files[bank][300]^=1;
 const std::string parms="p2-original/piki-bodies/white/pikiParms.txt";
 files[parms][100]^=1;negative();files[parms][100]^=1;
 changeRevision=true;negative();require(!p2original::whiteSelectedKitCurrent(kit),"expired selection invalidates old kit");
 changeRevision=false;reads=0;require(p2original::readWhiteSelectedKit(kit,error),"new actual selection can preflight");
 require(kit.selection()==2&&p2original::whiteSelectedKitCurrent(kit),"new selection retained without role fallback");
 std::cout<<checks<<" source White selected-read controls PASS (mock SDK; real kit bytes)\n";return 0;
 }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
