#include "pc_p2_original_system_parameters.h"
#include <fstream>
#include <iostream>
#include <iterator>
using namespace p2original;
namespace {
unsigned checks=0;void check(bool c,const char* text){++checks;if(!c){std::cerr<<text<<'\n';std::exit(1);}}
std::string read(const char* path){std::ifstream in(path,std::ios::binary);check(bool(in),"actual staged byte file readable");return {std::istreambuf_iterator<char>(in),{}};}
std::string replace(std::string bytes,const std::string& before,const std::string& after){auto i=bytes.find(before);check(i!=std::string::npos,"test mutation target");bytes.replace(i,before.size(),after);return bytes;}
const std::string ai="gravity 560\ndopecount 10\ndebt 10000\ncamera_angle 290\nend\n";
const std::string timeText="# source schema\n{\n{fp00} 4 7\n{fp01} 4 19\n{fp02} 4 1560\n{fp03} 4 5.25\n{fp04} 4 7\n{fp05} 4 8\n{fp06} 4 15\n{fp07} 4 16.5\n{fp11} 4 17.5\n{fp08} 4 19\n{fp09} 4 18\n{fp10} 4 18.5\n{_eof}\n}\n";
void badAI(const std::string& bytes){AIConstantsParameters out;out.gravity=91;out.dopeCount=92;out.debt=93;out.cameraAngle=94;std::string e;
 check(!parseAIConstantsParameters(bytes,out,e)&&out.gravity==91&&out.dopeCount==92&&out.debt==93&&out.cameraAngle==94&&!e.empty(),"AI refusal transactional");}
void badTime(const std::string& bytes){TimeParameters out;out.dayStart=91;out.dayEnd=92;out.dayLengthSeconds=93;out.midEveningEnd=94;std::string e;
 check(!parseTimeParameters(bytes,out,e)&&out.dayStart==91&&out.dayEnd==92&&out.dayLengthSeconds==93&&out.midEveningEnd==94&&!e.empty(),"timeText refusal transactional");}
}
int main(int argc,char** argv){
 std::string e;AIConstantsParameters a;TimeParameters t;
 check(parseAIConstantsParameters(ai,a,e)&&a.gravity==560&&a.dopeCount==10&&a.debt==10000&&a.cameraAngle==290,"all source AI fields");
 check(parseAIConstantsParameters("# comment\ngravity +5.60e2 # comment\nend",a,e)&&a.gravity==560&&a.dopeCount==2&&a.debt==10000&&a.cameraAngle==180,"actual optional AI ctor defaults only");
 check(parseAIConstantsParameters("gravity 560\ncamera_angle -450\ndopecount 0\ndebt 0\nend",a,e)&&a.cameraAngle==-450&&a.debt==0&&a.dopeCount==0,"finite camera degrees not invented clamp; zero counts allowed");
 badAI("end");badAI("");badAI("gravity 560");badAI("gravity 560\nending");badAI("gravity 560\nend\nextra");badAI("gravity 560\ngravity 561\nend");
 for(const auto& value:{"nan","inf","-inf","0","-1","1e999","1e-999","560suffix","0x1p2","5,6","+","."})badAI("gravity "+std::string(value)+"\nend");
 for(const auto& tag:{"dopecount","debt","camera_angle"}){badAI("gravity 560\n"+std::string(tag)+" 1\n"+tag+" 2\nend");badAI("gravity 560\n"+std::string(tag)+"\nend");}
 badAI("gravity 560\ndopecount 2147483648\nend");badAI("gravity 560\ndebt -2147483649\nend");badAI("gravity 560\ndopecount 2.5\nend");badAI("gravity 560\ndebt -1\nend");badAI("gravity 560\nunknown 1\nend");
 badAI(std::string("gravity 560\nend\0",16));badAI("{ gravity 560\nend");badAI("gravity 560}\nend");badAI(std::string(65537,' '));
 for(std::size_t n=0;n<ai.find("end")+3;++n)badAI(ai.substr(0,n));
 check(parseTimeParameters(timeText,t,e)&&t.dayStart==7&&t.dayEnd==19&&t.dayLengthSeconds==1560&&t.morningStart==5.25f&&t.midMorning==7&&t.morningEnd==8
  &&t.eveningStart==15&&t.midEveningStart==16.5f&&t.midEveningEnd==17.5f&&t.eveningEnd==19&&t.sundownAlert==18&&t.countdown==18.5f,"all12 literal timeText fields; fp11 order");
 check(parseTimeParameters(replace(timeText,"{fp07} 4 16.5","{fp07} +4 +1.65e1"),t,e)&&t.midEveningStart==16.5f,"source decimal exponent/sign forms");
 for(const auto& id:{"fp00","fp01","fp02","fp03","fp04","fp05","fp06","fp07","fp11","fp08","fp09","fp10"}){
  const auto token=std::string("{")+id+"}";auto begin=timeText.find(token),end=timeText.find('\n',begin);
  auto missing=timeText;missing.erase(begin,end-begin+1);badTime(missing);
  badTime(replace(timeText,"{_eof}",token+" 4 7\n{_eof}"));
  const auto tagSize=token+" 4";badTime(replace(timeText,tagSize,token+" 8"));badTime(replace(timeText,tagSize,token+" -1"));
 }
 badTime("");badTime("{_eof}");badTime(replace(timeText,"{_eof}",""));badTime(timeText+"extra");badTime(replace(timeText,"{fp11}","{fp99}"));
 for(const auto& value:{"nan","inf","1e999","1e-999","7suffix","0x1p2","7,0","-1","25"})badTime(replace(timeText,"{fp00} 4 7","{fp00} 4 "+std::string(value)));
 badTime(replace(timeText,"{fp02} 4 1560","{fp02} 4 0"));badTime(replace(timeText,"{fp02} 4 1560","{fp02} 4 3601"));
 badTime(replace(timeText,"{fp01} 4 19","{fp01} 4 7"));badTime(replace(timeText,"{fp04} 4 7","{fp04} 4 5.25"));
 badTime(replace(timeText,"{fp05} 4 8","{fp05} 4 7"));badTime(replace(timeText,"{fp06} 4 15","{fp06} 4 8"));
 badTime(replace(timeText,"{fp07} 4 16.5","{fp07} 4 15"));badTime(replace(timeText,"{fp11} 4 17.5","{fp11} 4 16"));
 badTime(replace(timeText,"{fp08} 4 19","{fp08} 4 17.5"));
 check(parseTimeParameters(replace(timeText,"{fp11} 4 17.5","{fp11} 4 16.5"),t,e),"zero plateau has no actual denominator; accepted");
 badTime(timeText.substr(0,timeText.rfind('}')));badTime("}"+timeText);badTime(timeText+std::string(1,'\0'));
 for(std::size_t n=0;n<=timeText.rfind('}');++n)badTime(timeText.substr(0,n));
 check(!parseAIConstantsParameters("end",a,e)&&!e.empty(),"AI refusal supplies diagnostic");
 check(parseAIConstantsParameters(ai,a,e)&&e.empty(),"AI success after refusal clears stale diagnostic");
 check(!parseTimeParameters("{_eof}",t,e)&&!e.empty(),"Time refusal supplies diagnostic");
 check(parseTimeParameters(timeText,t,e)&&e.empty(),"Time success after refusal clears stale diagnostic");
 check(argc==3,"actual staged AI/timeText byte paths required");
 const auto actualAI=read(argv[1]),actualTime=read(argv[2]);
 check(parseAIConstantsParameters(actualAI,a,e)&&a.gravity==560&&a.dopeCount==10&&a.debt==10000&&a.cameraAngle==290,"actual staged AI source bytes");
 check(parseTimeParameters(actualTime,t,e)&&t.midEveningStart==16.5f&&t.midEveningEnd==17.5f&&t.dayLengthSeconds==1560,"actual staged Shift-JIS comments/timeText values");
 std::cout<<checks<<" strict source system parameter controls passed; raw bytes only, no selected authority or runtime\n";
}

