#include "pc_p2_original_session.h"
#include <filesystem>
#include <iostream>
#include <stdexcept>
namespace {
unsigned checks=0;
void require(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}
std::string read(const std::filesystem::path& file){std::ifstream in(file,std::ios::binary);require(bool(in),"actual input readable");return std::string((std::istreambuf_iterator<char>(in)),{});}
}
int main(int argc,char** argv){try{
 require(argc==3,"actual kit and fresh private input root required");
 auto kit=std::filesystem::absolute(argv[1]),root=std::filesystem::absolute(argv[2]);
 require(!std::filesystem::exists(root),"private control root must be fresh");
 p2originalsession::Bundle expected;expected.campaign=std::string(64,'a');
 std::filesystem::create_directories(root/"p2-original/piki-bodies/white");
 for(const auto& item:std::filesystem::directory_iterator(kit)){
  const auto name=item.path().filename().string();
  if(item.path().extension()!=".mod"&&name!="bank.txt"&&name!="pikiParms.txt")continue;
  const std::string role="p2-original/piki-bodies/white/"+name;
  std::filesystem::copy_file(item.path(),root/role);
  expected.files[role]=p2treasureplacements::hash(read(item.path()));
 }
 require(expected.files.size()==41,"actual kit closure copied");
 // Descriptor parsing controls only: unrelated required roles are synthetic,
 // so complete course verification/OriginalSession activation is NOT claimed.
 for(const char* name:{"tutorial.p2c","forest.p2c","yakushima.p2c","last.p2c","campaign.p2pk","calendar.p2sc","stages.txt"})
  expected.files[std::string("p2-original/")+name]=std::string(64,'b');
 std::string descriptor="P2_ORIGINAL_SESSION 1 "+expected.campaign+" "+std::to_string(expected.files.size())+"\n";
 for(const auto& file:expected.files)descriptor+=file.first+" "+file.second+"\n";
 descriptor+="END\n";p2originalsession::Bundle selected;
 require(p2originalsession::parse(descriptor,p2treasureplacements::hash(descriptor),expected.campaign,selected),"retained descriptor parse");
 std::filesystem::current_path(root);
 const std::string role="p2-original/piki-bodies/white/bank.txt";
 std::string output="sentinel",error;
 require(p2originalsession::input(selected,role,output,error)&&output==read(kit/"bank.txt"),"real selected input read authenticates exact bytes");
 output="sentinel";
 require(!p2originalsession::input(selected,"p2-original/piki-bodies/white/not-selected.mod",output,error)&&output=="sentinel","unselected file refuses atomically");
 require(!p2originalsession::input(selected,"p2-original/../bank.txt",output,error)&&output=="sentinel","traversal refuses atomically");
 {std::ofstream changed(role,std::ios::binary|std::ios::app);changed<<'x';}
 require(!p2originalsession::input(selected,role,output,error)&&output=="sentinel","actual changed input refuses atomically");
 std::filesystem::remove(root/"p2-original/piki-bodies/white/white_wait_00.mod");
 require(!p2originalsession::input(selected,"p2-original/piki-bodies/white/white_wait_00.mod",output,error)&&output=="sentinel","actual missing input refuses atomically");
 auto changedDescriptor=descriptor;changedDescriptor[0]='X';
 p2originalsession::Bundle unchanged=selected;
 require(!p2originalsession::parse(changedDescriptor,p2treasureplacements::hash(descriptor),expected.campaign,unchanged)&&unchanged.files==selected.files,"descriptor authentication refuses unchanged output");
 std::cout<<checks<<" real selected-file SDK controls PASS (course activation untested)\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
