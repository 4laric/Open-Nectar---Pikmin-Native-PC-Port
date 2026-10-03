#include "pc_p2_original_piki_animator.h"
#include "Piki.h"
#include "Shape.h"
#include "Graphics.h"
#include "Camera.h"
#include "Matrix4f.h"
#include <cmath>
namespace p2original { namespace piki {
bool NativeAnimator::draw(Handle h,Graphics& gfx,std::string& e)const{
 Frame f;AnimatedPose sampled;
 if(!gfx.mCamera||!frame(h,f,e)||!pose(h,sampled,e))return false;
 if(f.species>2||h.body->mHappa<0||h.body->mHappa>2||!std::isfinite(h.body->mFaceDirection)){
  e="invalid source RGB pose facts";return false;
 }
 Shape* growth=sampled.happaShapes[h.body->mHappa];
 if(!sampled.shape||!growth){e="source pose has no actual selected Shape";return false;}
 // Retail Piki::getBaseScale is literal1 for RGB. Source pose geometry is
 // already skinned by the authenticated exporter; its native joint0 is rigid.
 Matrix4f world,view,stem,stemWorld,stemView;
 world.makeSRT(Vector3f(1,1,1),Vector3f(0,h.body->mFaceDirection,0),f.position);
 gfx.mCamera->mLookAtMtx.multiplyTo(world,view);
 stem.makeIdentity();
 for(unsigned row=0;row<3;++row)for(unsigned col=0;col<4;++col)stem.mMtx[row][col]=sampled.happa[row*4+col];
 world.multiplyTo(stem,stemWorld);gfx.mCamera->mLookAtMtx.multiplyTo(stemWorld,stemView);
 // Rendering updates only the bank's native presentation matrices. Source
 // physics/Brain, held handles, effect anchors and P1 animation stay untouched.
 sampled.shape->updateAnim(gfx,view,nullptr,h.body);
 sampled.shape->drawshape(gfx,*gfx.mCamera,nullptr);
 growth->updateAnim(gfx,stemView,nullptr,h.body);
 growth->drawshape(gfx,*gfx.mCamera,nullptr);
 return true;
}
} }
