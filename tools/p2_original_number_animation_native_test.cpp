#include "pc_p2_original_number_animation.h"
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <limits>
using namespace p2originalnumber;
namespace {unsigned checks=0,failures=0;void check(bool b,const char* text){++checks;if(!b){++failures;std::printf("FAIL %s\n",text);}}bool near(float a,float b){return std::fabs(a-b)<0.00001f;}
std::string descriptor(){
 std::string result="P2_ORIGINAL_NUMBER_ANIMATION_1\nnumber 1\nsource c5041fdfbd8fa31b30f057eeb7db7450be63eba8e90d6329b93e053ade0d9c26\nduration 41\nangle_scale 1\nloop 10 30\ntracks 9\n";
 for(const char* name:{"sx","rx","tx","sy","ry","ty","sz","rz","tz"}){
  result+="track ";result+=name;result+=" 1\n0 ";result+=name[0]=='s'?"1":name[0]=='r'&&name[1]=='z'?"8192.9":"0";result+=" 0 0\n";
 }
 return result;
}}
int main(int argc,char** argv){
 Animation a;std::string error;const auto bytes=descriptor();
 check(a.read(bytes,error),"synthetic descriptor schema accepted, not source resource admission");
 std::array<float,12> matrix{};check(a.sample(0,matrix)&&near(matrix[0],0)&&near(matrix[1],-1)&&near(matrix[4],1)&&near(matrix[5],0),"rotation truncates before angleScale and yields quarterturn");
 const auto before=matrix;check(!a.sample(41,matrix)&&matrix==before,"source END frame is not an out of range pose");
 check(!a.sample(std::numeric_limits<float>::quiet_NaN(),matrix)&&matrix==before,"nonfinite sample preserves output");
 check(!a.read(bytes+"trailing",error)&&a.sample(0,matrix)&&near(matrix[1],-1),"failed descriptor read preserves prior clip");
 auto bad=bytes;bad.replace(bad.find("loop 10 30"),10,"loop 0 40");check(!a.read(bad,error),"unsupported source loop refused");
 CarryPlayer p;check(p.start(1.0f/30)&&near(p.step(),1)&&p.sampleFrame()==0,"pickup starts animation zero at literal thirtyfps increment");
 for(unsigned i=0;i<30;++i)check(p.advance(1.0f/60,true),"player advances with captured step");
 check(p.sampleFrame()==30,"loop end event waits for integer timer past30");
 check(p.advance(1.0f/60,true)&&p.sampleFrame()==10,"loop returns to10 and discards overshoot");
 p.finish();check(p.start(1.0f/60)&&p.sampleFrame()==10&&near(p.step(),1)&&p.finishing(),"quick repick retains finishing tail and cached step");
 for(unsigned i=0;i<30;++i)check(p.advance(1.0f/60,false),"finish leaves loop and advances towardEND");
 check(p.sampleFrame()==40&&p.finishing(),"finish reaches last actual pose beforeEND");
 check(p.advance(1.0f/60,false)&&p.sampleFrame()==0&&near(p.step(),0)&&!p.finishing(),"unpickedEND restarts zero and stops");
 check(p.start(2.0f/30)&&p.advance(0,true),"overshoot setup");for(unsigned i=0;i<15;++i)check(p.advance(0,true),"overshoot advances");
 check(p.sampleFrame()==10,"loop discards a twoframe step overshoot, not modulo arithmetic");
 p.finish();for(unsigned i=0;i<16;++i)check(p.advance(1.0f/60,true),"picked finish passesEND");
 check(!p.finishing()&&near(p.step(),0.5f),"pickedEND recomputes source step from currentdelta");
 const float old=p.timer();check(!p.advance(std::numeric_limits<float>::infinity(),false)&&p.timer()==old,"invaliddelta cannot mutate player");
 for(int i=1;i<argc;++i){std::ifstream in(argv[i],std::ios::binary);const std::string bank{std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>()};
  Animation actual;check(!bank.empty()&&actual.read(bank,error),"private converted descriptor parse");
  for(float frame:{0.0f,9.5f,10.0f,20.0f,30.0f,40.0f,40.5f})check(actual.sample(frame,matrix),"private source key sample is finite");
  check(actual.sample(0,matrix)&&near(matrix[0],1)&&near(matrix[5],1)&&near(matrix[10],1)&&near(matrix[3],0)&&near(matrix[7],0)&&near(matrix[11],0),"private source framezero identity");
 }
 std::printf("original_number_animation checks=%u failures=%u engine=0 asset_authentication=caller ppc_bit_equivalence=0\n",checks,failures);return failures?1:0;
}
