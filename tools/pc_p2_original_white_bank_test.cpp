#include "pc_p2_original_white_bank.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
namespace {
unsigned checks=0;
void require(bool condition,const char* message){++checks;if(!condition)throw std::runtime_error(message);}
std::string replace(std::string text,const std::string& from,const std::string& to){
 auto at=text.find(from);require(at!=std::string::npos,"test mutation target exists");text.replace(at,from.size(),to);return text;
}
}
int main(int argc,char** argv){try{
 require(argc==2,"actual source White bank path required");
 std::ifstream file(argv[1],std::ios::binary);require(bool(file),"actual bank readable");
 const std::string bytes((std::istreambuf_iterator<char>(file)),{});
 p2original::WhiteBank out;std::string error;
 require(p2original::parseWhiteBank(bytes,out,error),"actual converted bank parses");
 require(error.empty()&&out.clips[1].frames[11]==39&&out.clips[0].frames[11]==19,"actual source samples retained");
 const auto negative=[&](const std::string& input){
  out.modelSha="sentinel";out.clips[0].happa[0][0]=123;
  require(!p2original::parseWhiteBank(input,out,error),"invalid bank refuses");
  require(!error.empty()&&out.modelSha=="sentinel"&&out.clips[0].happa[0][0]==123,"refusal leaves output unchanged");
 };
 negative("");negative(std::string(32769,'x'));negative(bytes+std::string(1,'\0'));
 negative(replace(bytes,"P2_SOURCE_WHITE_BANK_1","P2_WHITE_1"));
 negative(replace(bytes,out.parameterSha,std::string(64,'a')));
 negative(replace(bytes,"a971c6ac48e04a6f99333cacc1f57faf1ebfa13db71c5a31e93c6ce293bb376d",std::string(64,'b')));
 negative(replace(bytes,"clip walk 40","clip walk 39"));
 negative(replace(bytes,"clip wait 20","clip walk 20"));
 negative(replace(bytes," 12 0 2 3"," 12 0 3 3"));
 negative(replace(bytes,"happa wait 1 ","happa wait 0 "));
 negative(replace(bytes,"-0.0039900748","nan"));
 negative(bytes.substr(0,bytes.size()/2));negative(bytes+" unexpected");
 require(p2original::parseWhiteBank(bytes,out,error),"valid retry succeeds");
 require(out.clips[2].name=="attack1"&&out.clips[2].duration==20,"third actual clip preserved");
 std::cout<<checks<<" source White bank controls PASS\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
