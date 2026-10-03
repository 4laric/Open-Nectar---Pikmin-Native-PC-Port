#include "pc_p2_original_number_room.h"
#include "pc_p2_original_number_triangle.h"
#include <cmath>
#include <cstdint>
#include <cstring>
namespace p2originalnumber { namespace room {
namespace {
float bits(std::uint32_t value){float f;std::memcpy(&f,&value,4);return f;}
bool finite(Vec3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
bool fail(std::string& e,const char* text){e=text;return false;}
// Quarter indices of JMath's primary initializer, independently checked with
// libm and 100-digit decimal Taylor evaluation of EXACT binary32 TAU40c90fdb.
// This is not an extracted/emulated original runtime table qualification.
bool sincos(float angle,float& sine,float& cosine){
 const bool negative=angle<0;const float magnitude=negative?-angle:angle;
 const unsigned index=unsigned(int(magnitude*bits(0x43a2f983)))&2047u;
 const std::uint32_t sinBits[]={0x00000000,0x3f800000,0xb3bbbd2e,0xbf800000};
 const std::uint32_t cosBits[]={0x3f800000,0xb33bbd2e,0xbf800000,0x340ccde3};
 if(index%512)return false;
 sine=bits(sinBits[index/512]);if(negative)sine=-sine;cosine=bits(cosBits[index/512]);return true;
}
}
// Primary source pins: matMath.cpp8cfcab079bccf43da2e67ae839558ba8e12bd3c911cd90c6c63c39688b0d0a78;
// JMath.hde12e8cd0411116cbd659310c266df6d75879959cfe9be193e1f11783c0f837e;
// mtxvec.c58779d3cc20d2cdde7a6cf5c5d02f4a994bb02c9efe9f0066cb2990b960b3880.
bool make(unsigned turn,float centreX,float centreZ,Matrix3x4& out,std::string& e){
 if(turn>3||!std::isfinite(centreX)||!std::isfinite(centreZ))return fail(e,"source room authoring input invalid");
 const float direction=-90.0f*float(turn);
 const float degreeProduct=bits(0x3bb60b61)*direction;
 const float angle=bits(0x40490fdb)*degreeProduct;
 float sy=0,cy=0;if(!sincos(angle,sy,cy))return fail(e,"source room angle differs from quarter LUT policy");
 const float sx=0,cx=1,sz=0,cz=1;
 const float cxcz=cx*cz,cxsz=cx*sz,sxsy=sx*sy,sxsz=sx*sz,sxcz=sx*cz;
 Matrix3x4 m;
 m[0]=cy*cz;m[4]=cy*sz;m[8]=-sy;
 if(!triangle::sourceFma(sxsy,cz,-cxsz,m[1])||!triangle::sourceFma(sxsy,sz,cxcz,m[5])||
    !triangle::sourceFma(cxcz,sy,sxsz,m[2])||!triangle::sourceFma(cxsz,sy,-sxcz,m[6]))return fail(e,"source room fused arithmetic unavailable");
 m[9]=sx*cy;m[10]=cx*cy;
 m[3]=centreX*170.0f;m[7]=0;m[11]=centreZ*170.0f;
 for(float value:m)if(!std::isfinite(value))return fail(e,"source room matrix arithmetic overflow");
 out=m;e.clear();return true;
}
bool transformVertex(const Matrix3x4& m,Vec3 v,Vec3& out,std::string& e){
 if(!finite(v))return fail(e,"source room vertex nonfinite");
 for(float value:m)if(!std::isfinite(value))return fail(e,"source room matrix nonfinite");
 Vec3 candidate;float* values[]={&candidate.x,&candidate.y,&candidate.z};
 for(unsigned row=0;row<3;++row){const unsigned i=row*4;
  const float lane0=m[i]*v.x,lane1=m[i+1]*v.y;
  float combined0,combined1;
  if(!triangle::sourceFma(m[i+2],v.z,lane0,combined0)||!triangle::sourceFma(m[i+3],1.0f,lane1,combined1))return fail(e,"source room fused vertex arithmetic unavailable");
  *values[row]=combined0+combined1;
 }
 if(!finite(candidate))return fail(e,"source room transformed vertex overflow");
 out=candidate;e.clear();return true;
}
}}
