#include "pc_p2_original_number_yaw.h"
#include <cfenv>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace Y=p2originalnumber::bodyYaw;
static unsigned checks=0,generated=0;
static float f(std::uint32_t b){float v;std::memcpy(&v,&b,4);return v;}
static std::uint32_t bits(float v){std::uint32_t b;std::memcpy(&b,&v,4);return b;}
static void check(bool ok,const char* label){++checks;if(!ok)throw std::runtime_error(label);}
static Y::Matrix marker(){Y::Matrix out;for(unsigned i=0;i<12;++i)out[i]=f(0x40123456+i);return out;}
static bool same(const Y::Matrix&a,const Y::Matrix&b){for(unsigned i=0;i<12;++i)if(bits(a[i])!=bits(b[i]))return false;return true;}
static bool same(Y::Vec3 a,Y::Vec3 b){return bits(a.x)==bits(b.x)&&bits(a.y)==bits(b.y)&&bits(a.z)==bits(b.z);}
static void matrixBits(const Y::Matrix&m,const std::uint32_t* expected){for(unsigned i=0;i<12;++i){if(bits(m[i])!=expected[i])std::cerr<<"matrix row="<<generated<<" word="<<i<<" actual="<<std::hex<<bits(m[i])<<" expected="<<expected[i]<<std::dec<<'\n';check(bits(m[i])==expected[i],"full original SRT matrix bits");}}
static void literals(){
 // Independently transcribed DOL makeSRT8042847c..80428530 schedule;
 // SHA863fb87c29bdee1d1b7b9024919506a3ca6c0a8884dec784f327a6dbe7e1cab8.
 // Values from independent Fraction instruction oracle, not ideal rotation.
 const std::uint32_t identity[12]={0x3f800000,0,0,0,0,0x3f800000,0,0,0x80000000,0,0x3f800000,0};
 const std::uint32_t negative[12]={0x3f0af1b7,0x80000000,0xbf570348,0,0,0x3f800000,0x80000000,0,0x3f570348,0,0x3f0af1b7,0};
 std::string e;auto m=marker();for(float face:{0.f,-0.f,f(0x40c90fdb)}){check(Y::bodySRT(face,{0,0,0},m,e),"zero/wrap SRT available");matrixBits(m,identity);}
 check(Y::bodySRT(-1,{0,0,0},m,e),"negative face available");matrixBits(m,negative);
 // Root interface uses SDK PSMTXConcat800EA300 translation column, ordered
 // rounded X product -> Y FMA -> Z FMA -> translation FMA (Unit01).
 // Pure yaw's sparse rows may coincide with MultVec on these inputs; that
 // coincidence does not qualify a generic SDK phase replacement.
 Y::Vec3 out{9,8,7};check(Y::worldRoot(0,{4,5,6},{1,2,3},out,e)&&same(out,{5,7,9}),"SDK concat identity translation");
 for(std::uint32_t bad:{0x7fc00000u,0x7f800000u,0xff800000u,0x7f7fffffu,0x4b000000u}){m=marker();const auto prior=m;check(!Y::bodySRT(f(bad),{1,2,3},m,e)&&same(m,prior),"invalid face matrix failure atomic");out={9,8,7};check(!Y::worldRoot(f(bad),{1,2,3},{4,5,6},out,e)&&same(out,{9,8,7}),"invalid face world failure atomic");}
 for(unsigned i=0;i<3;++i){Y::Vec3 pos{1,2,3};float* p[]={&pos.x,&pos.y,&pos.z};*p[i]=f(0x7fc00000);m=marker();const auto prior=m;check(!Y::bodySRT(1,pos,m,e)&&same(m,prior),"nonfinite position matrix failure atomic");Y::Vec3 local{1,2,3};float* l[]={&local.x,&local.y,&local.z};*l[i]=f(0x7f800000);out={9,8,7};check(!Y::worldRoot(1,{4,5,6},local,out,e)&&same(out,{9,8,7}),"nonfinite local world failure atomic");}
 check(Y::bodySRT(0,{f(0x7f7fffff),0,0},m,e)&&bits(m[3])==0x7f7fffff,"finite max position preserved");out={9,8,7};check(!Y::worldRoot(0,{f(0x7f7fffff),0,0},{f(0x7f7fffff),0,0},out,e)&&same(out,{9,8,7}),"concat overflow refuses atomically");
#if defined(__MINGW32__) && !defined(_RC_NEAR)
 constexpr int downward=0x100;
#else
 constexpr int downward=FE_DOWNWARD;
#endif
 const int mode=std::fegetround();check(std::fesetround(downward)==0,"set directed rounding");m=marker();const auto prior=m;const bool accepted=Y::bodySRT(1,{1,2,3},m,e);const int after=std::fegetround();std::fesetround(mode);check(!accepted&&same(m,prior)&&after==downward,"directed rounding output/environment unchanged");
}
static void goldens(const char* path){
 std::ifstream input(path);check(bool(input),"independent Fraction goldens available");input>>std::hex;std::uint32_t face;
 while(input>>face){++generated;std::uint32_t p[3],v[3],m[12],w[3];std::string status;
  for(auto& x:p)input>>x;
  for(auto& x:v)input>>x;
  for(auto& x:m)input>>x;
  input>>status;
  for(auto& x:w)input>>x;
  check(bool(input)&&(status=="present"||status=="refuse"),"complete oracle row");std::string e;auto actual=marker();const Y::Vec3 pos{f(p[0]),f(p[1]),f(p[2])},local{f(v[0]),f(v[1]),f(v[2])};
  check(Y::bodySRT(f(face),pos,actual,e),"generated SRT success");matrixBits(actual,m);
  Y::Vec3 world{9,8,7};const bool success=Y::worldRoot(f(face),pos,local,world,e);
  check(success==(status=="present"),"concat world availability");
  if(success){check(bits(world.x)==w[0],"concat world X exact bits");check(bits(world.y)==w[1],"concat world Y exact bits");check(bits(world.z)==w[2],"concat world Z exact bits");}else check(same(world,{9,8,7}),"generated overflow rollback");
 }
 check(input.eof()&&generated>6000,"all broad oracle rows consumed");
}
int main(int argc,char**argv){try{literals();if(argc==2)goldens(argv[1]);else check(argc==1,"usage: yaw-test [Fraction-goldens.tsv]");std::cout<<"bodyYaw checks="<<checks<<" generated="<<generated<<" actual_world=0 owner=0 gameplay=0 save=0 hardware=0\n";return 0;}catch(const std::exception&e){std::cerr<<"FAIL checks="<<checks<<" generated="<<generated<<": "<<e.what()<<'\n';return 1;}}
