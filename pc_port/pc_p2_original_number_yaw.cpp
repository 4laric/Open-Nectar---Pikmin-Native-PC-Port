#include "pc_p2_original_number_yaw.h"
#include "pc_p2_original_number_trig.h"
#include "pc_p2_original_number_triangle.h"
#include <cmath>
namespace p2originalnumber { namespace bodyYaw {
namespace {
float mul(float a,float b){volatile float value=a*b;return value;}
bool finite(Vec3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
bool fail(std::string& error,const char* reason){error=reason;return false;}
}
// Original GPVE01 makeSRT 804282D8/0x288 code SHA-256
// 863fb87c29bdee1d1b7b9024919506a3ca6c0a8884dec784f327a6dbe7e1cab8.
// FakePiki::updateTrMatrix 8013ED50/0x78 SHA-256
// 849bd819b3b152e94b2b01e0687839dae1bd6fc320e64feb9bd129767626ed11.
bool bodySRT(float face,Vec3 position,Matrix& output,std::string& error){
 if(!std::isfinite(face)||!finite(position))return fail(error,"source body SRT input nonfinite");
 float sx,cx,sy,cy,sz,cz;
 if(!trig::lookup(0,sx,cx,error)||!trig::lookup(face,sy,cy,error)||
    !trig::lookup(0,sz,cz,error))return false;
 // Preserve all original separately rounded products, including zero signs,
 // then only the four written fmadds/fmsubs and final literal scale products.
 const float cysz=mul(cy,sz),sxcy=mul(sx,cy),cxsz=mul(cx,sz),sxsy=mul(sx,sy);
 const float sxsz=mul(sx,sz),cxcy=mul(cx,cy),cycz=mul(cy,cz),cxcz=mul(cx,cz),sxcz=mul(sx,cz);
 float xz,yz,xy,yy;
 if(!triangle::sourceFma(cxcz,sy,sxsz,xz)||!triangle::sourceFma(cxsz,sy,-sxcz,yz)||
    !triangle::sourceFma(sxsy,cz,-cxsz,xy)||!triangle::sourceFma(sxsy,sz,cxcz,yy))
  return fail(error,"source body SRT fused arithmetic unavailable");
 Matrix candidate={mul(1,cycz),mul(1,xy),mul(1,xz),position.x,
                   mul(1,cysz),mul(1,yy),mul(1,yz),position.y,
                   mul(1,-sy),mul(1,sxcy),mul(1,cxcy),position.z};
 for(float value:candidate)if(!std::isfinite(value))return fail(error,"source body SRT output nonfinite");
 output=candidate;error.clear();return true;
}
bool worldRoot(float face,Vec3 position,Vec3 local,Vec3& output,std::string& error){
 if(!finite(local))return fail(error,"source root local input nonfinite");
 Matrix matrix;
 if(!bodySRT(face,position,matrix,error))return false;
 // PSMTXConcat 800EA300: translation lane starts with separately rounded X,
 // then the Y and Z fused terms, then Unit01's literal1 * body translation.
 // PSMTXMultVec has a different paired summation order and cannot substitute.
 Vec3 candidate;float* rows[]={&candidate.x,&candidate.y,&candidate.z};
 for(unsigned row=0;row<3;++row){const unsigned i=4*row;
  float value=mul(matrix[i],local.x);
  if(!triangle::sourceFma(matrix[i+1],local.y,value,value)||
     !triangle::sourceFma(matrix[i+2],local.z,value,value)||
     !triangle::sourceFma(matrix[i+3],1,value,*rows[row]))
   return fail(error,"source root concatenation arithmetic refused");
 }
 if(!finite(candidate))return fail(error,"source root concatenation output nonfinite");
 output=candidate;error.clear();return true;
}
} }
