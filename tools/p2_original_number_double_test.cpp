#include "pc_p2_original_number_double.h"
#include <cfenv>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#if defined(__SSE2__) || defined(_M_X64)
#include <xmmintrin.h>
#endif
namespace D=p2originalnumber::doubleMath;
static unsigned controls=0,generated=0;
static double value(std::uint64_t b){double x;std::memcpy(&x,&b,8);return x;}
static std::uint64_t bits(double x){std::uint64_t b;std::memcpy(&b,&x,8);return b;}
static constexpr std::uint64_t sentinel=0x40123456789abcdeull;
static void check(bool ok,const char* label){++controls;if(!ok)throw std::runtime_error(label);}
static void test(std::uint64_t a,std::uint64_t b,std::uint64_t c,bool expectedSuccess,std::uint64_t expected){
 double out=value(sentinel);const bool success=D::sourceFma(value(a),value(b),value(c),out);
 ++controls;
 if(success!=expectedSuccess||bits(out)!=(expectedSuccess?expected:sentinel)){
  std::cerr<<"FMA mismatch a="<<std::hex<<a<<" b="<<b<<" c="<<c<<" expected="<<expected<<" success="<<expectedSuccess<<" actual="<<bits(out)<<" success="<<success<<std::dec<<'\n';
  throw std::runtime_error("exact FMA result/refusal mismatch");
 }
}
static void literals(){
 // Independent Fraction/RNE controls: exact operand-bit decoding, rational
 // product+sum, integer quotient/remainder tie rounding. No host FMA oracle.
 constexpr std::uint64_t one=0x3ff0000000000000ull,half=0x3fe0000000000000ull,neg=1ull<<63,max=0x7fefffffffffffffull;
 test(one,one,0,true,one);test(one,one,neg|one,true,0);
 test(one+1,one-1,neg|one,true,0x3c9ffffffffffffeull);
 test(one+1,one,neg|one,true,0x3cb0000000000000ull);
 test(max,0x4000000000000000ull,neg|max,true,max);
 test(neg|max,0x4000000000000000ull,max,true,neg|max);
 test(0x7fe0000000000000ull,0x4000000000000000ull,neg|max,true,0x7ca0000000000000ull);
 test(one,one,0x3ca0000000000000ull,true,one);
 test(one+1,one,0x3ca0000000000000ull,true,one+2);
 test(one,one,0x3ca0000000000001ull,true,one+1);
 test(one,one,0x3c9fffffffffffffull,true,one);
 test(1,half,0,true,0);test(neg|1,half,0,true,neg);
 test(1,half,1,true,2);test(3,half,0,true,2);
 test(0x0010000000000000ull,half,0,true,0x0008000000000000ull);
 test(0x000fffffffffffffull,one,1,true,0x0010000000000000ull);
 test(max,one,0x7c8fffffffffffffull,true,max);
 test(max,one,0x7c90000000000000ull,false,0);
 test(max,one,0x7c90000000000001ull,false,0);
 test(neg|max,one,neg|0x7c90000000000000ull,false,0);
 const std::uint64_t zeros[]={0,neg},multipliers[]={one,neg|one,0,neg};
 for(std::uint64_t a:zeros)for(std::uint64_t b:multipliers)for(std::uint64_t c:zeros){
  const bool negative=((a^b)&neg)&&c==neg;test(a,b,c,true,negative?neg:0);
 }
 const std::uint64_t invalid[]={0x7ff0000000000000ull,0xfff0000000000000ull,0x7ff8000000000001ull,0x7ff0000000000001ull};
 for(std::uint64_t bad:invalid){test(bad,one,0,false,0);test(one,bad,0,false,0);test(one,one,bad,false,0);}
}
static void environment(){
#if defined(__MINGW32__) && !defined(_RC_NEAR)
 constexpr int nearest=0,downward=0x100,upward=0x200,towardZero=0x300;
#else
 constexpr int nearest=FE_TONEAREST,downward=FE_DOWNWARD,upward=FE_UPWARD,towardZero=FE_TOWARDZERO;
#endif
 const int original=std::fegetround();check(original==nearest,"test requires initial nearest mode");
 for(int mode:{downward,upward,towardZero}){
  check(std::fesetround(mode)==0,"set directed mode");double out=value(sentinel);const bool ok=D::sourceFma(1,2,3,out);const int after=std::fegetround();std::fesetround(original);
  check(!ok&&bits(out)==sentinel&&after==mode,"directed rounding refusal does not mutate environment/output");
 }
#if defined(__SSE2__) || defined(_M_X64)
 const unsigned csr=_mm_getcsr();
 for(unsigned flags:{0x8000u,0x0040u,0x8040u}){
  _mm_setcsr(csr|flags);double out=value(sentinel);const bool ok=D::sourceFma(1,2,3,out);const unsigned after=_mm_getcsr();_mm_setcsr(csr);
  check(!ok&&bits(out)==sentinel&&(after&flags)==flags,"FTZ/DAZ refusal preserves environment/output");
 }
#endif
 test(0x3ff0000000000000ull,0x4000000000000000ull,0x4008000000000000ull,true,0x4014000000000000ull);
}
static void goldens(const char* path){
 std::ifstream stream(path);check(bool(stream),"generated Fraction controls file available");
 std::string a,b,c,want;
 while(stream>>a>>b>>c>>want){++generated;test(std::stoull(a,nullptr,16),std::stoull(b,nullptr,16),std::stoull(c,nullptr,16),want!="refuse",want=="refuse"?0:std::stoull(want,nullptr,16));}
 check(stream.eof()&&generated>10000,"complete broad generated controls consumed");
}
int main(int argc,char**argv){try{literals();environment();if(argc==2)goldens(argv[1]);else check(argc==1,"usage: number-double-test [Fraction-goldens.tsv]");std::cout<<"double controls="<<controls<<" generated="<<generated<<" actual_world=0 gameplay=0 save=0 hardware=0\n";return 0;}catch(const std::exception&e){std::cerr<<"FAIL after "<<controls<<" generated="<<generated<<": "<<e.what()<<'\n';return 1;}}
