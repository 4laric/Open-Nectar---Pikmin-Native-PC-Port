#include "pc_p2_source_camera_math.h"
#include <cmath>

namespace p2original { namespace cameraMath {
namespace {
constexpr float pi=3.14159265358979323846f;
volatile bool disableCulling; // Source process-static zero initialization.
bool finite(Vec v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
float dot(Vec a,Vec b){return a.x*b.x+a.y*b.y+a.z*b.z;}
Vec negate(Vec v){return {-v.x,-v.y,-v.z};}
Vec at(Vec p,Vec v,float distance){return {v.x*distance+p.x,v.y*distance+p.y,v.z*distance+p.z};}
Plane plane(Vec p,Vec n){return {n,dot(n,p)};}
bool valid(const Frustum& f){for(const auto& p:f.plane)if(!finite(p.normal)||!std::isfinite(p.offset))return false;return true;}
// SDK RotAxisRad shape, including axis normalization. Host sqrt/sin/cos are
// deliberately not advertised as PPC frsqrte/paired-single bit-exact emulation.
Vec rotate(Vec axis,float angle,Vec v){
 const float inv=1.0f/std::sqrt(dot(axis,axis));
 const float x=axis.x*inv,y=axis.y*inv,z=axis.z*inv;
 const float s=std::sin(angle),c=std::cos(angle),t=1.0f-c;
 const float m00=x*x*t+c,m01=x*y*t-z*s,m02=x*z*t+y*s;
 const float m10=x*y*t+z*s,m11=y*y*t+c,m12=y*z*t-x*s;
 const float m20=x*z*t-y*s,m21=y*z*t+x*s,m22=z*z*t+c;
 return {m00*v.x+m01*v.y+m02*v.z,m10*v.x+m11*v.y+m12*v.z,m20*v.x+m21*v.y+m22*v.z};
}
}
bool derive(const Matrix& m,const Projection& p,const Vec* jst,Frustum& output) noexcept {
 for(const auto& row:m.row)for(float f:row)if(!std::isfinite(f))return false;
 if(!std::isfinite(p.viewAngle)||p.viewAngle<=0||p.viewAngle>=180||
    !std::isfinite(p.aspect)||p.aspect<=0||!std::isfinite(p.nearDistance)||
    !std::isfinite(p.farDistance)||p.nearDistance<=0||p.farDistance<=p.nearDistance)return false;
 // Matrixf::structView names xx,yx,zx on row 0; xy,yy,zy on row 1.
 // Source side/up/view therefore come from ROWS, not storage columns.
 const Vec side={-m.row[0][0],-m.row[0][1],-m.row[0][2]};
 const Vec up={m.row[1][0],m.row[1][1],m.row[1][2]};
 const Vec view={-m.row[2][0],-m.row[2][1],-m.row[2][2]};
 if(dot(side,side)==0||dot(up,up)==0||dot(view,view)==0)return false;
 const Vec translation={-m.row[0][3],-m.row[1][3],-m.row[2][3]};
 Vec position={m.row[0][0]*translation.x+m.row[1][0]*translation.y+m.row[2][0]*translation.z,
               m.row[0][1]*translation.x+m.row[1][1]*translation.y+m.row[2][1]*translation.z,
               m.row[0][2]*translation.x+m.row[1][2]*translation.y+m.row[2][2]*translation.z};
 if(jst)position=*jst;
 if(!finite(position))return false;
 const float angle=pi*(p.viewAngle/360.0f);
 // Source getFOV calls double tan/atan with an explicit f32 tan result.
 const float tangent=static_cast<float>(std::tan(static_cast<double>(angle)));
 const float fov=static_cast<float>(std::atan(static_cast<double>(p.aspect*tangent)));
 Frustum result;
 result.plane[0]=plane(position,rotate(side,pi-angle,up));
 result.plane[1]=plane(position,rotate(side,angle,up));
 result.plane[2]=plane(position,rotate(up,-fov,side));
 result.plane[3]=plane(position,rotate(up,pi+fov,side));
 result.plane[4]=plane(at(position,view,p.farDistance),negate(view));
 result.plane[5]=plane(at(position,view,p.nearDistance),view);
 if(!valid(result))return false;
 output=result;return true;
}
bool sphereVisible(const Frustum& f,const Vec& pos,float radius) noexcept {
 for(const auto& p:f.plane)if(dot(pos,p.normal)-p.offset < -radius)return false;
 return true;
}
bool viewable(const Viewport& v) noexcept {
 if(v.flags&1)return false;
 if(v.bounds2.right-v.bounds2.left<1.0f||v.bounds2.bottom-v.bounds2.top<1.0f)return false;
 return true;
}
float aspect(const Bounds& b) noexcept {
 const float x=b.right-b.left,y=b.bottom-b.top;
 return x==0||y==0?1.0f:x/y;
}
bool processDisableCulling() noexcept {return disableCulling;}
bool cull(const Viewport* const* views,std::size_t count,bool disabled,
          const Vec& position,float radius,bool& output) noexcept {
 if(count>=4||(count&&!views)||!finite(position)||!std::isfinite(radius)||radius<0)return false;
 // Validate the entire referenced list before short circuiting. This boundary
 // refuses a missing owner instead of silently ignoring that viewport.
 for(std::size_t i=0;i<count;++i){
  if(!views[i]||!views[i]->camera||!valid(*views[i]->camera))return false;
  const Bounds& b=views[i]->bounds2;
  if(!std::isfinite(b.left)||!std::isfinite(b.top)||!std::isfinite(b.right)||!std::isfinite(b.bottom))return false;
 }
 if(disabled){output=false;return true;}
 bool visible=false;
 for(std::size_t i=0;i<count;++i)if(viewable(*views[i])&&sphereVisible(*views[i]->camera,position,radius)){visible=true;break;}
 output=!visible;return true;
}
} }
