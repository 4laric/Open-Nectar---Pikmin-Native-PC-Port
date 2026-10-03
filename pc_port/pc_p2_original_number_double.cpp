#include "pc_p2_original_number_double.h"
#include "pc_p2_original_number_triangle.h"
#include <array>
#include <cstdint>
#include <cstring>
#include <limits>

namespace p2originalnumber { namespace doubleMath {
namespace {
static_assert(sizeof(double)==8 && std::numeric_limits<double>::is_iec559 &&
              std::numeric_limits<double>::digits==53,"IEEE binary64 required");
constexpr int base=-2148;
using Words=std::array<std::uint32_t,132>;
struct Decoded { std::uint64_t significand; int exponent; bool negative; };
bool decode(double value,Decoded& d) noexcept {
 std::uint64_t bits;std::memcpy(&bits,&value,sizeof bits);
 const unsigned e=static_cast<unsigned>((bits>>52)&2047);
 if(e==2047)return false;
 d={bits&0xfffffffffffffull,e ? static_cast<int>(e)-1075 : -1074,(bits>>63)!=0};
 if(e)d.significand|=1ull<<52;
 return true;
}
void insert(Words& w,std::uint32_t word,unsigned shift) noexcept {
 const unsigned i=shift/32,b=shift%32;
 w[i]|=word<<b;
 if(b && i+1<w.size())w[i+1]|=word>>(32-b);
}
int compare(const Words& a,const Words& b) noexcept {
 for(unsigned i=a.size();i-->0;)if(a[i]!=b[i])return a[i]>b[i]?1:-1;
 return 0;
}
void plus(Words& a,const Words& b) noexcept {
 std::uint64_t carry=0;
 for(unsigned i=0;i<a.size();++i){const std::uint64_t t=std::uint64_t(a[i])+b[i]+carry;a[i]=static_cast<std::uint32_t>(t);carry=t>>32;}
}
void minus(Words& a,const Words& b) noexcept {
 std::uint64_t borrow=0;
 for(unsigned i=0;i<a.size();++i){const std::uint64_t rhs=std::uint64_t(b[i])+borrow,lhs=a[i];a[i]=static_cast<std::uint32_t>(lhs-rhs);borrow=lhs<rhs;}
}
bool bit(const Words& w,unsigned at) noexcept {return ((w[at/32]>>(at%32))&1)!=0;}
bool below(const Words& w,unsigned limit) noexcept {
 for(unsigned i=0;i<limit/32;++i)if(w[i])return true;
 const unsigned n=limit%32;
 return n && (w[limit/32]&((std::uint32_t(1)<<n)-1));
}
void publish(std::uint64_t bits,double& out) noexcept {std::memcpy(&out,&bits,sizeof bits);}
}
bool sourceFma(double a,double b,double c,double& output) noexcept {
 float environmentControl;
 if(!triangle::sourceFma(0,0,0,environmentControl))return false;
 volatile double tiny=std::numeric_limits<double>::denorm_min(),one=1;
 if(!(tiny*one>0))return false;
 Decoded da,db,dc;if(!decode(a,da)||!decode(b,db)||!decode(c,dc))return false;
 Words product{},addend{};
 const std::uint64_t al=static_cast<std::uint32_t>(da.significand),ah=da.significand>>32;
 const std::uint64_t bl=static_cast<std::uint32_t>(db.significand),bh=db.significand>>32;
 const std::uint64_t low=al*bl,middle=ah*bl+al*bh+(low>>32),high=ah*bh+(middle>>32);
 const unsigned pshift=static_cast<unsigned>(da.exponent+db.exponent-base);
 insert(product,static_cast<std::uint32_t>(low),pshift);
 insert(product,static_cast<std::uint32_t>(middle),pshift+32);
 insert(product,static_cast<std::uint32_t>(high),pshift+64);
 insert(product,static_cast<std::uint32_t>(high>>32),pshift+96);
 const unsigned cshift=static_cast<unsigned>(dc.exponent-base);
 insert(addend,static_cast<std::uint32_t>(dc.significand),cshift);
 insert(addend,static_cast<std::uint32_t>(dc.significand>>32),cshift+32);
 bool negative=da.negative!=db.negative;
 if(negative==dc.negative)plus(product,addend);
 else {const int order=compare(product,addend);if(order<0){minus(addend,product);product=addend;negative=dc.negative;}else minus(product,addend);}
 int highest=-1;
 for(unsigned i=product.size();i-->0;)if(product[i]){unsigned word=product[i],n=0;while(word>>1){word>>=1;++n;}highest=static_cast<int>(32*i+n);break;}
 if(highest<0){
  // Exact cancellation is +0 under RN. Two zero operands retain -0 only when
  // both the product and addend have that sign.
  negative=(!da.significand || !db.significand) && !dc.significand &&
           (da.negative!=db.negative) && dc.negative;
  publish(negative?1ull<<63:0,output);return true;
 }
 int exponent=base+highest;
 const unsigned shift=exponent>=-1022 ? static_cast<unsigned>(highest-52) : 1074;
 std::uint64_t mantissa=0;
 for(unsigned i=0;i<53;++i)if(bit(product,shift+i))mantissa|=1ull<<i;
 if(shift && bit(product,shift-1) && (below(product,shift-1)||(mantissa&1)))++mantissa;
 std::uint64_t result;
 if(exponent>=-1022){
  if(mantissa==(1ull<<53)){mantissa>>=1;++exponent;}
  if(exponent>1023)return false;
  result=(std::uint64_t(exponent+1023)<<52)|(mantissa&0xfffffffffffffull);
 }else result=mantissa; // Rounding up to 2^52 encodes the smallest normal.
 if(negative)result|=1ull<<63;
 publish(result,output);return true;
}
} }
