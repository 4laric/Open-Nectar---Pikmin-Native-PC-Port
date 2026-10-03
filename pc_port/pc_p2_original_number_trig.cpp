/*
 * Adapted bounded kernels/reduction from Sun fdlibm (1993):
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 * Developed at SunSoft, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this software is freely
 * granted, provided that this notice is preserved.
 *
 * Arithmetic schedules follow original GPVE01 revision0 DOL instructions,
 * not automatic contraction of fdlibm C. See private yaw audit receipt.
 */
#include "pc_p2_original_number_trig.h"
#include "pc_p2_original_number_double.h"
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>

namespace p2originalnumber { namespace trig {
namespace {
static_assert(sizeof(double)==8 && std::numeric_limits<double>::digits==53 &&
              std::numeric_limits<double>::is_iec559,"IEEE binary64 required");
static_assert(sizeof(float)==4 && std::numeric_limits<float>::digits==24 &&
              std::numeric_limits<float>::is_iec559,"IEEE binary32 required");
double bits(std::uint64_t b) { double d; std::memcpy(&d,&b,8); return d; }
std::uint32_t high(double d) { std::uint64_t b; std::memcpy(&b,&d,8); return static_cast<std::uint32_t>(b>>32); }
double sub(double a,double b) { volatile double r=a-b; return r; }
double mul(double a,double b) { volatile double r=a*b; return r; }
double div(double a,double b) { volatile double r=a/b; return r; }
struct Arithmetic {
 bool good=true;
 double f(double a,double b,double c) { double r=0; if(!doubleMath::sourceFma(a,b,c,r)) good=false; return r; }
 double ms(double a,double b,double c) { return f(a,b,-c); }
 double nms(double a,double b,double c) { return -ms(a,b,c); }
};
double kernelSin(double x,double y,bool tail,Arithmetic& a) {
 // __kernel_sin 800CEC7C/A0. Tiny branch preserves signed zero.
 if((high(x)&0x7fffffffu)<0x3e400000u) return x;
 const double s1=bits(0xbfc5555555555549ULL),s2=bits(0x3f8111111110f8a6ULL);
 const double s3=bits(0xbf2a01a019c161d5ULL),s4=bits(0x3ec71de357b1fe7dULL);
 const double s5=bits(0xbe5ae5e68a2b9cebULL),s6=bits(0x3de5d93a5acfd57cULL);
 const double z=mul(x,x),v=mul(z,x);
 const double r=a.f(z,a.f(z,a.f(z,a.f(s6,z,s5),s4),s3),s2);
 if(!tail) return a.f(v,a.f(z,r,s1),x);
 double t=a.ms(.5,y,mul(v,r));
 t=a.ms(z,t,y); t=a.nms(s1,v,t);
 return sub(x,t);
}
double kernelCos(double x,double y,Arithmetic& a) {
 // __kernel_cos 800CDD34/F4.
 const auto ix=high(x)&0x7fffffffu;
 if(ix<0x3e400000u) return 1.;
 const double c1=bits(0x3fa555555555554cULL),c2=bits(0xbf56c16c16c15177ULL);
 const double c3=bits(0x3efa01a019cb1590ULL),c4=bits(0xbe927e4f809c52adULL);
 const double c5=bits(0x3e21ee9ebdb4b1c4ULL),c6=bits(0xbda8fae9be8838d4ULL);
 const double z=mul(x,x);
 const double r=mul(z,a.f(z,a.f(z,a.f(z,a.f(z,a.f(c6,z,c5),c4),c3),c2),c1));
 const double correction=a.ms(z,r,mul(x,y));
 if(ix<0x3fd33333u) return sub(1.,a.ms(.5,z,correction));
 const double qx=ix>0x3fe90000u ? .28125 : bits(static_cast<std::uint64_t>(ix-0x00200000u)<<32);
 const double hz=a.ms(.5,z,qx);
 return sub(sub(1.,qx),sub(hz,correction));
}
int reduce(double x,double& y0,double& y1,Arithmetic& a) {
 // Bounded nonnegative initializer domain only. Original rem_pio2 800CD994/3A0.
 const auto ix=high(x);
 const double p1=bits(0x3ff921fb54400000ULL),t1=bits(0x3dd0b4611a626331ULL);
 const double p2=bits(0x3dd0b4611a600000ULL),t2=bits(0x3ba3198a2e037073ULL);
 const double p3=bits(0x3ba3198a2e000000ULL),t3=bits(0x397b839a252049c1ULL);
 if(ix<=0x3fe921fbu) { y0=x; y1=0.; return 0; }
 if(ix<0x4002d97cu) {
  double z=sub(x,p1);
  if(ix!=0x3ff921fbu) { y0=sub(z,t1); y1=sub(sub(z,y0),t1); }
  else { z=sub(z,p2); y0=sub(z,t2); y1=sub(sub(z,y0),t2); }
  return 1;
 }
 const int n=static_cast<int>(a.f(bits(0x3fe45f306dc9c883ULL),x,.5));
 const double fn=n;
 double r=a.nms(p1,fn,x),w=mul(t1,fn);
 y0=sub(r,w);
 constexpr std::uint32_t np[4]={0x3ff921fb,0x400921fb,0x4012d97c,0x401921fb};
 if(n<1 || n>4) { a.good=false; return 0; }
 if(ix==np[n-1]) {
  const int j=static_cast<int>(ix>>20);
  if(j-static_cast<int>((high(y0)>>20)&0x7ff)>16) {
   const double t=r,product=mul(p2,fn);
   r=sub(r,product); w=a.ms(t2,fn,sub(sub(t,r),product)); y0=sub(r,w);
   if(j-static_cast<int>((high(y0)>>20)&0x7ff)>49) {
    const double old=r,product3=mul(p3,fn);
    r=sub(r,product3); w=a.ms(t3,fn,sub(sub(old,r),product3)); y0=sub(r,w);
   }
  }
 }
 y1=sub(sub(r,y0),w);
 return n;
}
bool pair(double x,float& s,float& c) {
 Arithmetic a;
 double y0=0,y1=0; const int n=reduce(x,y0,y1,a);
 double sd=0,cd=0;
 if(n==0) { sd=kernelSin(y0,y1,high(x)>0x3fe921fbu,a); cd=kernelCos(y0,y1,a); }
 else if(n==1) { sd=kernelCos(y0,y1,a); cd=-kernelSin(y0,y1,true,a); }
 else if(n==2) { sd=-kernelSin(y0,y1,true,a); cd=-kernelCos(y0,y1,a); }
 else if(n==3) { sd=-kernelCos(y0,y1,a); cd=kernelSin(y0,y1,true,a); }
 else { sd=kernelSin(y0,y1,true,a); cd=kernelCos(y0,y1,a); }
 volatile float sr=static_cast<float>(sd),cr=static_cast<float>(cd);
 if(!a.good || !std::isfinite(sr) || !std::isfinite(cr)) return false;
 s=sr;c=cr;return true;
}
bool environment() { double d; return doubleMath::sourceFma(0.,0.,0.,d); }
}
bool buildTable(Table& output,std::string& error) {
 if(!environment()) { error="Original trigonometry arithmetic environment unavailable"; return false; }
 Table candidate{};
 for(unsigned i=0;i<2048;++i) {
  const double x=div(mul(static_cast<double>(i),bits(0x401921fb60000000ULL)),2048.);
  if(!pair(x,candidate[i][0],candidate[i][1])) { error="Original trigonometry initializer arithmetic refused"; return false; }
 }
 output=candidate;error.clear();return true;
}
bool lookup(float face,float& sine,float& cosine,std::string& error) {
 if(!environment() || !std::isfinite(face)) { error="Original trigonometry face/environment refused"; return false; }
 // Actual source f32 multiply then fctiwz and mask. Oversized conversions are
 // refused rather than inventing source behavior for invalid signed-int range.
 volatile float scaled=(face<0.f ? -face : face)*325.9493f;
 if(!std::isfinite(scaled) || static_cast<double>(scaled)>=2147483648.) {
  error="Original trigonometry face conversion out of range"; return false;
 }
 struct Cache { Table table{}; bool valid=false; Cache() { std::string e; valid=buildTable(table,e); } };
 static const Cache cache;
 if(!cache.valid) { error="Original trigonometry cached initializer refused"; return false; }
 const auto index=static_cast<unsigned>(static_cast<int>(scaled))&0x7ffu;
 const float s=face<0.f ? -cache.table[index][0] : cache.table[index][0];
 const float c=cache.table[index][1];sine=s;cosine=c;error.clear();return true;
}
} }
