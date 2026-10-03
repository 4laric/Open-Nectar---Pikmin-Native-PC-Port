#include "pc_p2_original_number_trig.h"
#include <cfenv>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
namespace {
#include "p2_original_number_trig_goldens.h"
int checks=0,failures=0;
void check(bool v,const char* name) { ++checks;if(!v) { ++failures;std::cerr<<name<<'\n'; } }
std::uint32_t bits(float f) { std::uint32_t b;std::memcpy(&b,&f,4);return b; }
}
int main(int argc,char** argv) {
 using namespace p2originalnumber::trig;
 if(argc>2) { std::cerr<<"Expected optional oracle-table.bin argument\n";return 2; }
 std::ifstream in; if(argc==2) in.open(argv[1],std::ios::binary);
 Table table;std::string error;
 check(buildTable(table,error),"build");check(error.empty(),"success clears error");
 for(unsigned i=0;i<2048;++i) for(unsigned c=0;c<2;++c) {
  std::uint32_t expected=oracleWords[2*i+c];
  if(argc==2) { unsigned char b[4]{};in.read(reinterpret_cast<char*>(b),4);
   expected=b[0]|(std::uint32_t(b[1])<<8)|(std::uint32_t(b[2])<<16)|(std::uint32_t(b[3])<<24);
   check(bool(in),"external oracle component available"); }
  check(bits(table[i][c])==expected,"exact oracle table component");
 }
 if(argc==2) check(in.peek()==std::char_traits<char>::eof(),"bounded oracle size");
 float s=7,c=8;
 check(lookup(0,s,c,error)&&bits(s)==0&&bits(c)==0x3f800000,"zero lookup");
 check(lookup(-0.f,s,c,error)&&bits(s)==0,"negative zero uses nonnegative branch");
 const float face=1.2345f;
 volatile float scaled=face*325.9493f;
 const unsigned index=static_cast<unsigned>(static_cast<int>(scaled))&0x7ff;
 check(lookup(face,s,c,error)&&bits(s)==bits(table[index][0])&&bits(c)==bits(table[index][1]),"literal truncation lookup");
 check(lookup(-face,s,c,error)&&bits(s)==bits(-table[index][0])&&bits(c)==bits(table[index][1]),"negative sine branch");
 volatile float wrapFace=7.f, wrapScaled=wrapFace*325.9493f;
 const unsigned wrapIndex=static_cast<unsigned>(static_cast<int>(wrapScaled))&0x7ff;
 check(lookup(wrapFace,s,c,error)&&bits(s)==bits(table[wrapIndex][0]),"mask wraps beyond one turn");
 for(float bad:{std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::max()}) {
  s=7;c=8;check(!lookup(bad,s,c,error)&&s==7&&c==8&&!error.empty(),"invalid input atomic");
 }
 const int prior=std::fegetround();
#if defined(__MINGW32__) && !defined(_RC_UP)
 constexpr int upward=0x200; // verified CRT value when engine float.h shadows CRT
#else
 constexpr int upward=FE_UPWARD;
#endif
 if(std::fesetround(upward)==0) {
  s=7;c=8;check(!lookup(face,s,c,error)&&s==7&&c==8,"directed rounding lookup atomic");
  Table sentinel;for(auto& row:sentinel)row={7,8};
  check(!buildTable(sentinel,error)&&sentinel.front()[0]==7&&sentinel.back()[1]==8,"directed rounding table atomic");
 }
 std::fesetround(prior);
 check(lookup(face,s,c,error),"cache usable after refused environment");
 std::cout<<"original_number_trig checks="<<checks<<" failures="<<failures<<" hardware=0 owner=0 gameplay=0 save=0\n";
 return failures?1:0;
}
